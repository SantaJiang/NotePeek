#include "editor/editormanager.h"
#include "apppreferences.h"
#include "themecolors.h"
#include <QFileInfo>
#include <QFontDatabase>
#include <QSet>
#include <QTimer>
#include <QThreadPool>
#include <QThread>
#include <QFutureWatcher>
#include <QScrollBar>
#include <QEvent>
#include <QtConcurrent>
#include <Qsci/qscilexer.h>
#include <memory>

#include "decorators/highlightedscrollbardecorator.h"
#include "editor/customeditor.h"
#include "editor/documentregistry.h"
#include <NotePeek/lexerfactory.h>
#include <NotePeek/syntaxstyler.h>
#include <NotePeek/syntaxtheme.h>
#include <NotePeek/semantictokens.h>
#include "editor/performanceconstants.h"

namespace {
NotePeek::SyntaxTheme currentSyntaxTheme()
{
    return NotePeek::SyntaxThemeCatalog::theme(AppPreferences::instance().syntaxThemeId(),
                                               ThemeColors::isDarkMode());
}

long toScintillaColor(const QColor &color)
{
    return (color.blue() << 16) | (color.green() << 8) | color.red();
}

bool shouldUseSoftWrap(const QString &filePath, LexerFactory::Language lang)
{
    // A document with no path yet ("untitled") has nothing to infer from, so it
    // follows the user's configured default: plain text (wrapped, no line
    // numbers) unless code mode was chosen.
    if (filePath.isEmpty())
    {
        return !AppPreferences::instance().untitledUsesCodeMode();
    }

    switch (lang)
    {
    case LexerFactory::MARKDOWN:
    case LexerFactory::TEX:
    case LexerFactory::PO:
        return true;
    default:
        break;
    }

    if (lang != LexerFactory::Unknown)
    {
        return false;
    }

    const QFileInfo fileInfo(filePath);
    const QString suffix = fileInfo.suffix().toLower();
    static const QSet<QString> proseExtensions = {
        QStringLiteral("txt"),
        QStringLiteral("text"),
        QStringLiteral("rst"),
        QStringLiteral("adoc"),
        QStringLiteral("asciidoc"),
        QStringLiteral("org")
    };
    if (proseExtensions.contains(suffix))
    {
        return true;
    }

    const QString fileName = fileInfo.fileName().toLower();
    static const QSet<QString> proseFileNames = {
        QStringLiteral("readme"),
        QStringLiteral("license"),
        QStringLiteral("copying"),
        QStringLiteral("notice"),
        QStringLiteral("changelog"),
        QStringLiteral("authors"),
        QStringLiteral("todo")
    };
    return proseFileNames.contains(fileName);
}

LexerFactory::Language effectiveLanguage(CustomEditor *editor, const QString &filePath)
{
    const LexerFactory::Language pathLanguage = LexerFactory::languageFromFilePath(filePath);
    if (pathLanguage != LexerFactory::Unknown || !editor)
        return pathLanguage;

    const QVariant detected = editor->property("notepeekContentLanguage");
    if (!detected.isValid())
        return LexerFactory::Unknown;
    const int value = detected.toInt();
    if (value <= int(LexerFactory::Unknown) || value > int(LexerFactory::IGNORE))
        return LexerFactory::Unknown;
    return static_cast<LexerFactory::Language>(value);
}

bool insertedTextContainsAbnormallyLongLine(const char *text, int length)
{
    if (!text || length <= PerformanceConstants::AbnormallyLongLineBytes)
        return false;

    int lineLength = 0;
    for (int index = 0; index < length; ++index)
    {
        if (text[index] == '\n' || text[index] == '\r')
        {
            lineLength = 0;
            continue;
        }
        if (++lineLength > PerformanceConstants::AbnormallyLongLineBytes)
            return true;
    }
    return false;
}

qsizetype editorLineContentLength(CustomEditor *editor, int line)
{
    if (!editor || line < 0 || line >= editor->lines())
        return 0;
    const long start = editor->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, line);
    const long end = editor->SendScintilla(QsciScintillaBase::SCI_GETLINEENDPOSITION, line);
    return qsizetype(qMax(0L, end - start));
}

} // namespace

class SemanticHighlightingEngine final : public QObject
{
public:
    explicit SemanticHighlightingEngine(QObject *parent = nullptr)
        : QObject(parent)
        , m_provider(std::make_shared<NotePeek::BasicSemanticTokenProvider>())
    {
        // A single low-priority worker keeps CPU and memory bounded. Requests
        // are viewport-sized, so parallelism would add contention without
        // improving perceived latency.
        m_pool.setMaxThreadCount(1);
        m_pool.setExpiryTimeout(5000);
        m_pool.setThreadPriority(QThread::LowPriority);
    }

    ~SemanticHighlightingEngine() override
    {
        for (State *state : std::as_const(m_states))
            cancel(state);
        m_pool.clear();
        m_pool.waitForDone();
    }

    void attach(CustomEditor *editor)
    {
        if (!editor || m_states.contains(editor))
            return;
        auto *state = new State(this);
        state->editor = editor;
        state->timer.setSingleShot(true);
        state->timer.setInterval(kEditDebounceMs);
        m_states.insert(editor, state);
        m_eventTargets.insert(editor, state);
        m_eventTargets.insert(editor->viewport(), state);
        m_eventTargets.insert(editor->verticalScrollBar(), state);
        editor->installEventFilter(this);
        editor->viewport()->installEventFilter(this);
        editor->verticalScrollBar()->installEventFilter(this);
        editor->setProperty("semanticStartedRequestCount", 0);
        editor->setProperty("semanticCanceledRequestCount", 0);

        connect(&state->timer, &QTimer::timeout, this, [this, state]() {
            start(state);
        });
        connect(editor, &QsciScintillaBase::SCN_MODIFIED, this,
                [this, state](int, int modificationType, const char *, int, int,
                              int, int, int, int, int) {
            if (modificationType & (QsciScintillaBase::SC_MOD_INSERTTEXT
                                    | QsciScintillaBase::SC_MOD_DELETETEXT))
                schedule(state, kEditDebounceMs);
        });
        connect(editor, &QObject::destroyed, this, [this, editor, state]() {
            cancel(state);
            m_eventTargets.remove(editor);
            m_states.remove(editor);
            state->deleteLater();
        });
        const auto removeEventTarget = [this](QObject *target) {
            connect(target, &QObject::destroyed, this,
                    [this, target]() { m_eventTargets.remove(target); });
        };
        removeEventTarget(editor->viewport());
        removeEventTarget(editor->verticalScrollBar());
    }

    void setPath(CustomEditor *editor, const QString &path)
    {
        if (State *state = m_states.value(editor))
        {
            state->path = path;
            state->language = effectiveLanguage(editor, path);
            schedule(state, 0);
        }
    }

    void refreshTheme(CustomEditor *editor)
    {
        if (!editor)
            return;
        if (!AppPreferences::instance().semanticHighlightingEnabled())
        {
            editor->clearSemanticTokens();
            if (State *state = m_states.value(editor))
                cancel(state);
            return;
        }
        editor->setSemanticTokenColors(currentSyntaxTheme().syntaxColors);
        if (State *state = m_states.value(editor))
            schedule(state, 0);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        const QEvent::Type type = event->type();
        if (type != QEvent::Show && type != QEvent::Hide
            && type != QEvent::Wheel && type != QEvent::KeyRelease
            && type != QEvent::MouseButtonRelease)
            return QObject::eventFilter(watched, event);

        State *state = m_eventTargets.value(watched);
        if (state && type == QEvent::Show)
            schedule(state, 0);
        else if (state && type == QEvent::Hide)
        {
            state->timer.stop();
            cancel(state);
        }
        else if (state && (type == QEvent::Wheel || type == QEvent::KeyRelease
                           || type == QEvent::MouseButtonRelease))
        {
            // One request per input event or completed scroll-bar drag. Never
            // subscribe to valueChanged: pixel/kinetic scrolling can emit it
            // dozens of times for a single gesture and measurably hurts paint.
            schedule(state, kScrollDebounceMs);
        }
        return QObject::eventFilter(watched, event);
    }

private:
    struct State : QObject
    {
        explicit State(QObject *parent) : QObject(parent), timer(this) {}
        QPointer<CustomEditor> editor;
        QTimer timer;
        QString path;
        LexerFactory::Language language = LexerFactory::Unknown;
        quint64 requestId = 0;
        bool inFlight = false;
        bool rerun = false;
        std::shared_ptr<std::atomic_bool> cancellation;
    };

    static constexpr int kEditDebounceMs = 120;
    static constexpr int kScrollDebounceMs = 45;
    static constexpr int kLookBehindLines = 40;
    static constexpr int kLookAheadLines = 80;
    static constexpr int kMaxCachedTokens = 12000;
    static constexpr qint64 kWorkerBudgetNs = 2 * 1000 * 1000;

    void schedule(State *state, int delayMs)
    {
        if (!state || !state->editor)
            return;
        if (!AppPreferences::instance().semanticHighlightingEnabled())
        {
            state->timer.stop();
            cancel(state);
            state->editor->clearSemanticTokens();
            return;
        }
        if (!state->editor->isVisible())
            return;
        if (state->inFlight)
        {
            state->rerun = true;
            cancel(state);
            return;
        }
        // A single keystroke raises both SCN_MODIFIED (kEditDebounceMs) and
        // KeyRelease (kScrollDebounceMs). Restarting the timer unconditionally
        // would let the shorter scroll debounce overwrite the edit debounce, so
        // the deadline is never pulled earlier than one already pending.
        if (state->timer.isActive())
            delayMs = qMax(delayMs, state->timer.remainingTime());
        state->timer.start(qMax(0, delayMs));
    }

    void cancel(State *state)
    {
        if (!state || !state->cancellation)
            return;
        const bool wasCanceled = state->cancellation->exchange(true, std::memory_order_relaxed);
        if (!wasCanceled && state->editor)
        {
            const int count = state->editor->property("semanticCanceledRequestCount").toInt();
            state->editor->setProperty("semanticCanceledRequestCount", count + 1);
        }
    }

    // A viewport snapshot normally starts in the middle of the file. Ask the
    // lexer what is still open at that first character, otherwise the analyzer
    // restarts in the Code state and paints the inside of a long block comment
    // or multi-line string as if it were code.
    static NotePeek::SemanticEntryState entryStateForLine(CustomEditor *editor, int line)
    {
        QsciLexer *lexer = editor ? editor->lexer() : nullptr;
        if (!lexer || line <= 0)
            return NotePeek::SemanticEntryState::Code;
        const long position = editor->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, line);
        const int style = int(editor->SendScintilla(QsciScintillaBase::SCI_GETSTYLEAT, position));
        switch (NotePeek::SyntaxStyler::roleForStyle(lexer, style))
        {
        case NotePeek::SyntaxRole::Comment:
        case NotePeek::SyntaxRole::Documentation:
            return NotePeek::SemanticEntryState::BlockComment;
        case NotePeek::SyntaxRole::String:
            return NotePeek::SemanticEntryState::MultiLineString;
        default:
            // Includes the case where Scintilla has not styled this position
            // yet and reports style 0; treating that as code matches the
            // lexical fallback the editor is already showing.
            return NotePeek::SemanticEntryState::Code;
        }
    }

    static QString viewportText(CustomEditor *editor, int firstLine, int lastLine)
    {
        QString snapshot;
        for (int line = firstLine; line <= lastLine; ++line)
        {
            const QString lineText = editor->text(line);
            snapshot.append(lineText);
            if (line < lastLine && !lineText.endsWith(QLatin1Char('\n')))
                snapshot.append(QLatin1Char('\n'));
        }
        return snapshot;
    }

    void start(State *state)
    {
        if (!state || !state->editor || !state->editor->isVisible() || state->inFlight)
            return;
        if (!AppPreferences::instance().semanticHighlightingEnabled())
        {
            state->editor->clearSemanticTokens();
            return;
        }
        CustomEditor *editor = state->editor;
        const LexerFactory::Language language = state->language;
        if (language == LexerFactory::Unknown || !editor->lexer())
        {
            editor->clearSemanticTokens();
            return;
        }

        const int firstVisible = int(editor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE));
        const int visibleLines = qMax(1, int(editor->SendScintilla(QsciScintillaBase::SCI_LINESONSCREEN)));
        const int firstLine = qMax(0, firstVisible - kLookBehindLines);
        const int lastLine = qMin(editor->lines() - 1,
                                  firstVisible + visibleLines + kLookAheadLines);
        if (lastLine < firstLine)
            return;

        editor->setSemanticTokenColors(currentSyntaxTheme().syntaxColors);
        NotePeek::SemanticDocument document;
        document.uri = state->path;
        document.languageId = LexerFactory::languageName(language);
        document.revision = editor->documentRevision();
        document.firstLine = firstLine;
        document.entryState = entryStateForLine(editor, firstLine);
        document.text = viewportText(editor, firstLine, lastLine);
        editor->setProperty("semanticSnapshotBytes", document.text.size() * int(sizeof(QChar)));

        state->inFlight = true;
        editor->setProperty("semanticAnalysisInFlight", true);
        editor->setProperty("semanticStartedRequestCount",
                            editor->property("semanticStartedRequestCount").toInt() + 1);
        state->rerun = false;
        const quint64 requestId = ++state->requestId;
        state->cancellation = std::make_shared<std::atomic_bool>(false);
        NotePeek::SemanticAnalysisControl control;
        control.cancellation = state->cancellation;
        control.budgetNanoseconds = kWorkerBudgetNs;
        const auto provider = m_provider;
        QPointer<State> guardedState(state);
        auto *watcher = new QFutureWatcher<NotePeek::SemanticAnalysisResult>(this);
        connect(watcher, &QFutureWatcher<NotePeek::SemanticAnalysisResult>::finished,
                this, [this, watcher, guardedState, requestId]() {
            const NotePeek::SemanticAnalysisResult result = watcher->result();
            watcher->deleteLater();
            if (!guardedState)
                return;
            State *state = guardedState;
            state->inFlight = false;
            if (state->editor)
            {
                state->editor->setProperty("semanticAnalysisInFlight", false);
                state->editor->setProperty("semanticWorkerNanoseconds", result.elapsedNanoseconds);
            }
            // requestId can only differ if a second request was started while
            // this one was in flight, which start() currently prevents. Keep
            // the guard for future providers that allow overlap, but fall
            // through to the rerun check so a queued refresh is never dropped.
            if (requestId == state->requestId && state->editor && state->editor->isVisible()
                && !result.canceled
                && result.revision == state->editor->documentRevision()
                && result.lastLine >= result.firstLine)
            {
                // Evicting only the lines furthest from this window keeps the
                // visible region styled; clearing everything would briefly drop
                // the whole viewport back to lexical colours.
                if (state->editor->semanticTokenCount() + result.tokens.size() > kMaxCachedTokens)
                    state->editor->evictSemanticTokensOutside(result.firstLine, result.lastLine,
                                                              kMaxCachedTokens - result.tokens.size());
                QElapsedTimer applyTimer;
                applyTimer.start();
                state->editor->applySemanticTokenDelta(result.revision,
                                                        result.firstLine,
                                                        result.lastLine,
                                                        result.tokens);
                state->editor->setProperty("semanticApplyNanoseconds", applyTimer.nsecsElapsed());
            }
            if (state->rerun && state->editor && state->editor->isVisible())
                schedule(state, 0);
        });
        watcher->setFuture(QtConcurrent::run(&m_pool, [provider, document, control]() {
            return provider->analyze(document, control);
        }));
    }

    QThreadPool m_pool;
    std::shared_ptr<const NotePeek::SemanticTokenProvider> m_provider;
    QHash<CustomEditor *, State *> m_states;
    QHash<QObject *, State *> m_eventTargets;
};

EditorManager::EditorManager(QObject *parent)
    : QObject(parent)
    // Owned solely by the unique_ptr; giving it a QObject parent as well would
    // put two independent lifetimes on the same object.
    , m_semanticEngine(std::make_unique<SemanticHighlightingEngine>())
{
}

EditorManager::~EditorManager()
{
    // Clean up all editors
    for (auto &editor : m_editors)
    {
        if (!editor.isNull())
        {
            editor->deleteLater();
        }
    }
    m_editors.clear();
}

CustomEditor *EditorManager::createEditor(const QString &name)
{
    CustomEditor *editor = new CustomEditor();
    editor->setWindowTitle(name.isEmpty() ? QStringLiteral("untitled") : name);
    setupEditor(editor);
    connect(editor, &CustomEditor::filesDropped,  this, &EditorManager::filesDropped);

    m_editors.append(editor);
    emit editorCreated(editor);
    return editor;
}

CustomEditor *EditorManager::createEditorSharingDocument(CustomEditor *sourceEditor, const QString &name)
{
    if (!sourceEditor)
    {
        return nullptr;
    }

    CustomEditor *editor = createEditor(name.isEmpty() ? sourceEditor->windowTitle() : name);
    editor->setDocument(sourceEditor->document());
    editor->shareDocumentSessionFrom(sourceEditor);
    editor->setFilePath(sourceEditor->filePath());
    editor->setProperty("notepeekContentLanguage",
                        sourceEditor->property("notepeekContentLanguage"));
    updateEditorSyntax(editor, sourceEditor->filePath());
    updateLineNumberMarginWidth(editor);
    return editor;
}

CustomEditor *EditorManager::createEditorForFile(const QString &filePath, bool tryToCreate)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() && !tryToCreate)
    {
        return nullptr;
    }

    // Check if file is already open
    CustomEditor *existingEditor = getEditorByFilePath(filePath);
    if (existingEditor)
    {
        return existingEditor;
    }

    CustomEditor *editor = createEditor(fileInfo.fileName());

    // Load file content
    const bool isLargeFile = fileInfo.size() > PerformanceConstants::LargeFileSize;
    editor->setLargeFileMode(isLargeFile);

    if (isLargeFile)
    {
        editor->setLexer(nullptr);
        editor->setWrapMode(CustomEditor::WrapNone);
        editor->setMarginWidth(1, 0);
        editor->setUpdatesEnabled(false);

        const qint64 fileSize = fileInfo.size();
        const quint64 allocationSize = static_cast<quint64>(fileSize) + 1000U;
        if (fileSize > 0
            && allocationSize <= std::numeric_limits<unsigned long>::max())
        {
            editor->SendScintilla(QsciScintillaBase::SCI_ALLOCATE,
                                  static_cast<unsigned long>(allocationSize));
        }

        editor->setUpdatesEnabled(true);
    }
    // For ordinary files the lexer is attached only after the asynchronous
    // load and bulk insertion complete. Styling an empty document now and then
    // re-styling every inserted chunk adds work without producing visible UI.

    return editor;
}

CustomEditor *EditorManager::getEditorByFilePath(const QString &filePath)
{
    purgeOldEditorPointers();
    return DocumentRegistry::findEditorByFilePath(getEditors(), filePath);
}

QList<CustomEditor *> EditorManager::getEditors() const
{
    QList<CustomEditor *> list;
    for (const QPointer<CustomEditor> &editor : m_editors)
    {
        if (editor)
        {
            list.append(editor);
        }
    }
    return list;
}

void EditorManager::forgetEditor(CustomEditor *editor)
{
    if (!editor)
        return;

    for (int i = m_editors.size() - 1; i >= 0; --i)
    {
        if (m_editors.at(i).data() == editor)
        {
            m_editors.removeAt(i);
        }
    }
}

void EditorManager::setupEditor(CustomEditor *editor)
{
    // Basic editor setup
    editor->setMargins(2);
    editor->setMarginType(0, CustomEditor::NumberMargin);
    editor->setMarginWidth(0, 0);
    editor->setFolding(CustomEditor::NoFoldStyle, 1);
    // Start with the minimum scroll width so an empty document shows no horizontal
    // scrollbar; with width tracking enabled it grows as the content widens
    editor->setScrollWidth(1);
    editor->setScrollWidthTracking(true);
    editor->setTabWidth(4);
    editor->setIndentationWidth(4);
    editor->setCaretLineVisible(true);
    editor->setBraceMatching(CustomEditor::NoBraceMatch);
    editor->setWrapMode(CustomEditor::WrapNone);
    editor->SendScintilla(QsciScintillaBase::SCI_SETIDLESTYLING,
                          QsciScintillaBase::SC_IDLESTYLING_TOVISIBLE);

    // Set UTF-8 encoding
    editor->setUtf8(true);

    // Start in the typing mode chosen in Preferences ▸ General ▸ Typing.
    editor->setOverwriteMode(AppPreferences::instance().overwriteMode());

    // No file path yet: seed with the code font.  updateEditorSyntax() applies
    // the correct text/code font once the path (and therefore the lexer) is known.
    const QFont initialFont = fontForPath(QString());
    editor->setFont(initialFont);
    setMarginsFont(editor, initialFont);

    // Apply theme-aware colors (margin background/foreground, caret line, etc.)
    applyEditorColors(editor);

    // Connect signals.  Scintilla only reports savepoint transitions itself;
    // this forwards the modify state after text changes too, but only when the
    // state actually flipped — the downstream slots (tab title, watcher checks)
    // are not cheap enough to run on every keystroke.
    auto lastModificationState = std::make_shared<bool>(editor->isModified());
    connect(editor, &CustomEditor::modificationChanged, this,
            [editor, lastModificationState](bool) {
        // CustomEditor can intentionally remain dirty after Scintilla reaches
        // its internal save point (for example when that snapshot failed to
        // reach disk), so use the editor's effective state rather than the raw
        // Scintilla signal argument.
        *lastModificationState = editor->isModified();
    });
    connect(editor, &CustomEditor::textChanged, this, [editor, lastModificationState]() {
        const bool modified = editor->isModified();
        if (*lastModificationState == modified)
            return;
        *lastModificationState = modified;
        emit editor->modificationChanged(modified);
    });
    connect(editor, &CustomEditor::linesChanged, this, [this, editor]() {
        updateLineNumberMarginWidth(editor);
    });
    connect(editor, &QsciScintillaBase::SCN_MODIFIED, this,
            [this, editor](int position, int modificationType, const char *text, int length, int,
                           int, int, int, int, int) {
        if ((modificationType & (QsciScintillaBase::SC_MOD_INSERTTEXT
                                 | QsciScintillaBase::SC_MOD_DELETETEXT)) == 0
            || editor->isLargeFileMode())
        {
            return;
        }

        bool shouldDegrade = editor->length() > PerformanceConstants::LargeFileSize;
        if (!shouldDegrade && (modificationType & QsciScintillaBase::SC_MOD_INSERTTEXT))
        {
            const int firstLine = int(editor->SendScintilla(
                QsciScintillaBase::SCI_LINEFROMPOSITION, position));
            const int lastLine = int(editor->SendScintilla(
                QsciScintillaBase::SCI_LINEFROMPOSITION, position + qMax(0, length)));
            shouldDegrade = editorLineContentLength(editor, firstLine)
                                > PerformanceConstants::AbnormallyLongLineBytes
                            || editorLineContentLength(editor, lastLine)
                                > PerformanceConstants::AbnormallyLongLineBytes
                            || insertedTextContainsAbnormallyLongLine(text, length);
        }
        if (shouldDegrade)
            promoteToLargeFileMode(editor);
    });

    m_semanticEngine->attach(editor);

    updateLineNumberMarginWidth(editor);
    setupScrollBarDecorator(editor);
}

void EditorManager::purgeOldEditorPointers()
{
    QMutableListIterator<QPointer<CustomEditor>> it(m_editors);
    while (it.hasNext())
    {
        QPointer<CustomEditor> pointer = it.next();
        if (pointer.isNull())
        {
            it.remove();
        }
    }
}

void EditorManager::setupScrollBarDecorator(CustomEditor *editor)
{
    // Parented to editor, retrieved later via editor->findChild<HighlightedScrollBarDecorator*>()
    new HighlightedScrollBarDecorator(editor);
}

void EditorManager::updateLineNumberMarginWidth(CustomEditor *editor)
{
    if (!editor) return;

    if (editor->wrapMode() != CustomEditor::WrapNone)
    {
        editor->setMarginWidth(0, 0);
        return;
    }

    int lines = editor->lines();
    int digits = 1;
    while (lines >= 10)
    {
        lines /= 10;
        digits++;
    }

    // Keep the gutter stable through four-digit documents.  Beyond that it
    // continues to grow one digit at a time as before.
    const int visibleDigits = qMax(digits, 4);
    const QString sample = QString(visibleDigits, QLatin1Char('9')) + QLatin1Char(' ');
    const int width = editor->SendScintilla(QsciScintillaBase::SCI_TEXTWIDTH,
                                            QsciScintillaBase::STYLE_LINENUMBER,
                                            sample.toUtf8().constData());
    editor->setMarginWidth(0, width);
}

QFont EditorManager::fontForPath(const QString &filePath) const
{
    // Prose files (Markdown, plain text, README/LICENSE, …) use the text font;
    // everything else uses the code font.  This mirrors the wrap/line-number
    // display policy so the two fonts line up with how the file is presented.
    const LexerFactory::Language lang = LexerFactory::languageFromFilePath(filePath);
    const bool prose = shouldUseSoftWrap(filePath, lang);
    return prose ? AppPreferences::instance().textFont()
                 : AppPreferences::instance().codeFont();
}

QFont EditorManager::fontForEditor(CustomEditor *editor, const QString &filePath) const
{
    const LexerFactory::Language language = effectiveLanguage(editor, filePath);
    return shouldUseSoftWrap(filePath, language)
               ? AppPreferences::instance().textFont()
               : AppPreferences::instance().codeFont();
}

void EditorManager::setMarginsFont(CustomEditor *editor, const QFont &font)
{
    if (!editor) return;

    editor->setMarginsFont(font);
}

void EditorManager::updateEditorSyntax(CustomEditor *editor, const QString &filePath)
{
    updateEditorSyntax(editor, filePath, effectiveLanguage(editor, filePath));
}

void EditorManager::updateEditorSyntax(CustomEditor *editor,
                                       const QString &filePath,
                                       LexerFactory::Language detectedLanguage)
{
    if (!editor) return;
    if (detectedLanguage < LexerFactory::Unknown || detectedLanguage > LexerFactory::IGNORE)
        detectedLanguage = LexerFactory::Unknown;
    const LexerFactory::Language pathLanguage = LexerFactory::languageFromFilePath(filePath);
    const LexerFactory::Language lang = pathLanguage == LexerFactory::Unknown
                                            ? detectedLanguage
                                            : pathLanguage;
    if (pathLanguage == LexerFactory::Unknown && lang != LexerFactory::Unknown)
        editor->setProperty("notepeekContentLanguage", int(lang));
    else
        editor->setProperty("notepeekContentLanguage", QVariant());
    QsciLexer *oldLexer = editor->lexer();
    if (editor->isLargeFileMode())
    {
        editor->setProperty("semanticFilePath", filePath);
        editor->setProperty("notepeekLexerLanguage", -1);
        editor->setProperty("notepeekLargeFilePolicyApplied", true);
        if (oldLexer)
        {
            editor->setLexer(nullptr);
            oldLexer->deleteLater();
        }
        applyDisplayPolicy(editor, filePath);
        editor->setMarginWidth(1, 0);
        applyEditorColors(editor);
        m_semanticEngine->setPath(editor, filePath);
        return;
    }

    const QFont font = shouldUseSoftWrap(filePath, lang)
                           ? AppPreferences::instance().textFont()
                           : AppPreferences::instance().codeFont();
    const int previousLanguage = editor->property("notepeekLexerLanguage").toInt();
    const bool hasRecordedLanguage = editor->property("notepeekLexerLanguage").isValid();
    const bool sameLexer = hasRecordedLanguage && previousLanguage == int(lang)
                           && ((oldLexer != nullptr) == (lang != LexerFactory::Unknown));
    const QFont previousFont = editor->property("notepeekAppliedFont").value<QFont>();
    const bool fontChanged = !editor->property("notepeekAppliedFont").isValid()
                             || previousFont != font;

    editor->setProperty("semanticFilePath", filePath);
    editor->setProperty("notepeekLargeFilePolicyApplied", false);
    editor->setProperty("notepeekLexerLanguage", int(lang));
    editor->setProperty("notepeekAppliedFont", font);

    if (sameLexer)
    {
        if (fontChanged)
        {
            if (oldLexer)
                applyUniformStyleFont(oldLexer, font);
            else
                editor->setFont(font);
            setMarginsFont(editor, font);
        }
        applyDisplayPolicy(editor, filePath);
        applyEditorColors(editor);
        m_semanticEngine->setPath(editor, filePath);
        return;
    }

    QsciLexer* lexer = LexerFactory::createLexer(lang);

    if(lexer)
    {
        applyLexerTheme(lexer);

        lexer->setParent(editor);
        editor->setLexer(lexer);

        // Force the editor font onto every style *after* attaching the lexer.
        // setLexer() (through detachLexer) issues SCI_STYLERESETDEFAULT +
        // SCI_STYLECLEARALL, which resets all styles to the Qt application UI
        // font.  QScintilla only restores the styles the lexer actually
        // describes, so the remaining predefined styles keep the (often taller)
        // UI font.  Scintilla derives the line height from the tallest style,
        // so those stray styles silently inflate the vertical line spacing on
        // every re-lex (e.g. saving an already-open file).  Applying the font
        // now — with the lexer's fontChanged signal connected to the editor —
        // pushes the configured font to all styles and keeps the spacing stable.
        // Note: QsciScintilla::setFont() is a no-op while a lexer is attached,
        // so the per-style lexer API is the only way to change the font here.
        applyUniformStyleFont(lexer, font);

        if (oldLexer && oldLexer != lexer)
        {
            oldLexer->deleteLater();
        }
        editor->setAutoIndent(AppPreferences::instance().smartIndentationEnabled());
    }
    else
    {
        editor->setLexer(nullptr);
        if (oldLexer)
        {
            oldLexer->deleteLater();
        }
        editor->setAutoIndent(false);
        editor->setFont(font);
    }
    setMarginsFont(editor, font);
    editor->setProperty("notepeekAppliedFont", font);
    applyDisplayPolicy(editor, filePath);
    // Re-apply margin colors after lexer change (lexer can override margin colors)
    applyEditorColors(editor);
    m_semanticEngine->setPath(editor, filePath);
}

void EditorManager::promoteToLargeFileMode(CustomEditor *editor)
{
    if (!editor)
        return;

    editor->setLargeFileMode(true);
    for (const QPointer<CustomEditor> &candidatePointer : std::as_const(m_editors))
    {
        CustomEditor *candidate = candidatePointer.data();
        if (!candidate || !candidate->sharesDocumentSessionWith(editor))
            continue;
        candidate->setLargeFileMode(true);
        if (!candidate->property("notepeekLargeFilePolicyApplied").toBool())
            updateEditorSyntax(candidate, candidate->filePath());
    }
}

void EditorManager::applyDisplayPolicy(CustomEditor *editor, const QString &filePath)
{
    if (!editor)
    {
        return;
    }

    if (editor->isLargeFileMode())
    {
        editor->setCodeEditingFeaturesEnabled(false);
        editor->setBracketAssistanceEnabled(false);
        editor->setDocumentCompletionEnabled(false);
        editor->setBraceMatching(CustomEditor::NoBraceMatch);
        editor->setWrapMode(CustomEditor::WrapNone);
        editor->setFolding(CustomEditor::NoFoldStyle, 1);
        editor->setAutoIndent(false);
        editor->setIndentationGuides(false);
        editor->setTabIndents(false);
        editor->setBackspaceUnindents(false);
        if (editor->isListActive())
            editor->cancelList();
        editor->setAutoCompletionSource(CustomEditor::AcsNone);
        editor->setAutoCompletionThreshold(-1);
        editor->SendScintilla(QsciScintillaBase::SCI_SETIDLESTYLING,
                              QsciScintillaBase::SC_IDLESTYLING_NONE);
        updateLineNumberMarginWidth(editor);
        editor->SendScintilla(QsciScintillaBase::SCI_SETMARGINRIGHT, 0UL, 0L);
        return;
    }

    const LexerFactory::Language lang = effectiveLanguage(editor, filePath);
    const bool softWrap = shouldUseSoftWrap(filePath, lang);
    const bool codeFeatures = editor->lexer() && !softWrap;
    const AppPreferences &prefs = AppPreferences::instance();
    // QsciLexerCustom subclasses style their text themselves and do not
    // produce Scintilla fold levels unless they implement that explicitly.
    const bool foldingAvailable = codeFeatures && editor->lexer()->lexer()
                                  && prefs.codeFoldingEnabled();
    editor->setCodeEditingFeaturesEnabled(codeFeatures);
    editor->setBracketAssistanceEnabled(codeFeatures && prefs.bracketAssistanceEnabled());
    editor->setDocumentCompletionEnabled(codeFeatures && prefs.documentCompletionEnabled());
    editor->setBraceMatching(codeFeatures && prefs.bracketAssistanceEnabled()
                                 ? CustomEditor::SloppyBraceMatch
                                 : CustomEditor::NoBraceMatch);
    editor->setWrapMode(softWrap ? CustomEditor::WrapWord : CustomEditor::WrapNone);
    editor->setFolding(foldingAvailable ? CustomEditor::BoxedTreeFoldStyle
                                        : CustomEditor::NoFoldStyle,
                       1);
    const bool smartIndentation = codeFeatures && prefs.smartIndentationEnabled();
    editor->setAutoIndent(smartIndentation);
    editor->setIndentationGuides(smartIndentation);
    editor->setTabIndents(smartIndentation);
    editor->setBackspaceUnindents(smartIndentation);
    const bool documentCompletion = codeFeatures && prefs.documentCompletionEnabled();
    if (!documentCompletion && editor->isListActive())
        editor->cancelList();
    editor->setAutoCompletionSource(documentCompletion ? CustomEditor::AcsDocument
                                                        : CustomEditor::AcsNone);
    editor->setAutoCompletionThreshold(documentCompletion ? 3 : -1);
    editor->setAutoCompletionUseSingle(CustomEditor::AcusNever);
    editor->SendScintilla(QsciScintillaBase::SCI_SETIDLESTYLING,
                          QsciScintillaBase::SC_IDLESTYLING_TOVISIBLE);
    editor->setMarginWidth(0, softWrap ? 0 : 30);
    if (!softWrap)
    {
        updateLineNumberMarginWidth(editor);
    }
    editor->SendScintilla(QsciScintillaBase::SCI_SETMARGINRIGHT, 0UL, softWrap ? 5L : 0L);
}

void EditorManager::applyCodeFeaturePreferences()
{
    for (auto &editorPtr : m_editors)
    {
        if (!editorPtr)
            continue;

        CustomEditor *editor = editorPtr;
        editor->setOverwriteMode(AppPreferences::instance().overwriteMode());
        if (!AppPreferences::instance().codeFoldingEnabled()
            && editor->folding() != CustomEditor::NoFoldStyle)
        {
            editor->clearFolds();
        }
        applyDisplayPolicy(editor, editor->filePath());
        applyEditorColors(editor);
    }
}

void EditorManager::reapplyThemeToAllEditors()
{
    for (auto &editorPtr : m_editors)
    {
        if (editorPtr)
        {
            reapplyEditorSyntax(editorPtr);
            applyEditorColors(editorPtr);
            if (auto *decorator = editorPtr->findChild<HighlightedScrollBarDecorator*>())
            {
                decorator->applyThemeColors();
            }
        }
    }

    // QScintilla resets styles while reattaching lexers.  A queued second pass
    // makes the final theme colours win after Qt/macOS has finished delivering
    // palette and color-scheme notifications.
    QTimer::singleShot(0, this, [this]() {
        for (auto &editorPtr : m_editors)
        {
            if (editorPtr)
            {
                applyEditorColors(editorPtr);
                if (auto *decorator = editorPtr->findChild<HighlightedScrollBarDecorator*>())
                {
                    decorator->applyThemeColors();
                }
                editorPtr->viewport()->update();
            }
        }
    });
}

void EditorManager::applyUniformStyleFont(QsciLexer *lexer, const QFont &font)
{
    if (!lexer) return;

    // The lexer's fontChanged signal is connected to the editor by setLexer(),
    // so setting the font here reaches every Scintilla style (not just the ones
    // the lexer describes) and keeps the line height tied to the editor font.
    for (int i = 0; i <= QsciScintillaBase::STYLE_MAX; ++i)
    {
        lexer->setFont(font, i);
    }
}

void EditorManager::applyFontPreferences()
{
    for (auto &editorPtr : m_editors)
    {
        if (!editorPtr)
        {
            continue;
        }

        CustomEditor *editor = editorPtr;
        const QFont font = fontForEditor(editor, editor->filePath());

        if (QsciLexer *lexer = editor->lexer())
        {
            // QsciScintilla::setFont() does nothing while a lexer is attached,
            // so push the font through the lexer's per-style API.  This changes
            // only the font of each style and leaves the syntax colours intact.
            applyUniformStyleFont(lexer, font);
        }
        else
        {
            editor->setFont(font);
        }
        setMarginsFont(editor, font);
        updateLineNumberMarginWidth(editor);
    }
}

void EditorManager::reapplyEditorSyntax(CustomEditor *editor)
{
    if (!editor) return;

    QsciLexer *lexer = editor->lexer();
    if (lexer)
    {
        applyLexerTheme(lexer);
        // Re-set the lexer to trigger QScintilla internal refresh
        editor->setLexer(lexer);
        // Re-setting the lexer resets all styles to the UI font (see
        // updateEditorSyntax); restore the editor font across every style so a
        // theme change does not inflate the line spacing.
        applyUniformStyleFont(lexer, fontForEditor(editor, editor->filePath()));
    }
    else
    {
        // No lexer: color/paper are set by applyEditorColors
    }
    const NotePeek::SyntaxTheme theme = currentSyntaxTheme();
    editor->setSemanticTokenColors(theme.syntaxColors);
    m_semanticEngine->refreshTheme(editor);
}

void EditorManager::applyEditorColors(CustomEditor *editor)
{
    if (!editor) return;

    const NotePeek::SyntaxTheme theme = currentSyntaxTheme();
    const QColor marginBackground = theme.editorColor(QStringLiteral("marginBackground"), ThemeColors::Editor::marginBg());
    const QColor marginForeground = theme.editorColor(QStringLiteral("marginForeground"), ThemeColors::Editor::marginFg());
    const QColor background = theme.editorColor(QStringLiteral("background"), ThemeColors::Editor::background());

    // setMarginsBackgroundColor() uses STYLE_LINENUMBER (style 33) which is
    // the correct way to set the background of a NumberMargin.  In contrast,
    // setMarginBackgroundColor(margin, ...) uses SCI_SETMARGINBACKN which only
    // works for SC_MARGIN_COLOUR type margins.
    editor->setMarginsBackgroundColor(marginBackground);
    editor->setMarginsForegroundColor(marginForeground);
    editor->setFoldMarginColors(marginBackground, marginBackground);
    editor->setIndentationGuidesBackgroundColor(background);
    editor->setIndentationGuidesForegroundColor(theme.editorColor(QStringLiteral("indentGuide"), ThemeColors::Editor::indentGuide()));
    for (int marker = QsciScintillaBase::SC_MARKNUM_FOLDEREND;
         marker <= static_cast<int>(QsciScintillaBase::SC_MARKNUM_FOLDEROPEN);
         ++marker)
    {
        editor->setMarkerForegroundColor(marginBackground, marker);
        editor->setMarkerBackgroundColor(theme.editorColor(QStringLiteral("foldMarker"), ThemeColors::Editor::foldMarker()), marker);
    }
    const QColor caret = theme.editorColor(QStringLiteral("caret"), ThemeColors::Editor::caretFg());
    editor->setCaretForegroundColor(caret);
    editor->SendScintilla(QsciScintillaBase::SCI_SETADDITIONALCARETFORE,
                          toScintillaColor(caret));
    editor->setCaretLineBackgroundColor(theme.editorColor(QStringLiteral("caretLine"), ThemeColors::Editor::caretLineBg()));
    editor->setMatchedBraceForegroundColor(theme.editorColor(QStringLiteral("matchedBraceForeground"), ThemeColors::Editor::matchedBraceFg()));
    editor->setMatchedBraceBackgroundColor(theme.editorColor(QStringLiteral("matchedBraceBackground"), ThemeColors::Editor::matchedBraceBg()));
    applyBaseEditorColors(editor);
    editor->viewport()->update();
}

void EditorManager::applyBaseEditorColors(CustomEditor *editor)
{
    if (!editor) return;

    const NotePeek::SyntaxTheme theme = currentSyntaxTheme();
    const QColor foreground = theme.editorColor(QStringLiteral("foreground"), ThemeColors::Editor::foreground());
    const QColor background = theme.editorColor(QStringLiteral("background"), ThemeColors::Editor::background());
    const QColor marginForeground = theme.editorColor(QStringLiteral("marginForeground"), ThemeColors::Editor::marginFg());
    const QColor marginBackground = theme.editorColor(QStringLiteral("marginBackground"), ThemeColors::Editor::marginBg());
    const long fore = toScintillaColor(foreground);
    const long back = toScintillaColor(background);
    const long marginFore = toScintillaColor(marginForeground);
    const long marginBack = toScintillaColor(marginBackground);

    editor->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, 0, fore);
    editor->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciScintillaBase::STYLE_DEFAULT, fore);
    editor->SendScintilla(QsciScintillaBase::SCI_STYLESETBACK, 0, back);
    editor->SendScintilla(QsciScintillaBase::SCI_STYLESETBACK, QsciScintillaBase::STYLE_DEFAULT, back);
    editor->SendScintilla(QsciScintillaBase::SCI_STYLESETFORE, QsciScintillaBase::STYLE_LINENUMBER, marginFore);
    editor->SendScintilla(QsciScintillaBase::SCI_STYLESETBACK, QsciScintillaBase::STYLE_LINENUMBER, marginBack);
}

void EditorManager::applyLexerTheme(QsciLexer *lexer)
{
    if (!lexer) return;

    const NotePeek::SyntaxTheme theme = currentSyntaxTheme();
    NotePeek::SyntaxStyler::applyTheme(
        lexer, theme.syntaxColors,
        theme.editorColor(QStringLiteral("background"), ThemeColors::Lexer::defaultPaper()),
        theme.editorColor(QStringLiteral("foreground"), ThemeColors::Lexer::defaultColor()));
}
