#include "editor/semantichighlightingengine.h"

#include "apppreferences.h"
#include "editor/backgroundworkscheduler.h"
#include "editor/customeditor.h"
#include "themecolors.h"

#include <NotePeek/syntaxstyler.h>
#include <NotePeek/syntaxtheme.h>

#include <QElapsedTimer>
#include <QEvent>
#include <QFutureWatcher>
#include <QScrollBar>
#include <QtConcurrent>
#include <Qsci/qscilexer.h>

namespace {

NotePeek::SyntaxTheme currentSyntaxTheme()
{
    const bool systemDark = ThemeColors::isDarkMode();
    return NotePeek::SyntaxThemeCatalog::theme(
        AppPreferences::instance().currentSyntaxThemeId(systemDark), systemDark);
}

LexerFactory::Language effectiveLanguage(CustomEditor *editor, const QString &filePath)
{
    const LexerFactory::Language pathLanguage = LexerFactory::languageFromFilePath(filePath);
    if (pathLanguage != LexerFactory::Unknown || !editor)
        return pathLanguage;
    const QVariant detected = editor->property("notepeekContentLanguage");
    const int value = detected.isValid() ? detected.toInt() : int(LexerFactory::Unknown);
    if (value <= int(LexerFactory::Unknown) || value > int(LexerFactory::IGNORE))
        return LexerFactory::Unknown;
    return static_cast<LexerFactory::Language>(value);
}

} // namespace

SemanticHighlightingEngine::SemanticHighlightingEngine(QObject *parent)
    : QObject(parent)
    , m_provider(std::make_shared<NotePeek::BasicSemanticTokenProvider>())
{
}

SemanticHighlightingEngine::~SemanticHighlightingEngine()
{
    for (State *state : std::as_const(m_states))
        cancel(state);
}

void SemanticHighlightingEngine::attach(CustomEditor *editor)
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

    connect(&state->timer, &QTimer::timeout, this, [this, state]() { start(state); });
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

void SemanticHighlightingEngine::setPath(CustomEditor *editor, const QString &path)
{
    if (State *state = m_states.value(editor))
    {
        state->path = path;
        state->language = effectiveLanguage(editor, path);
        schedule(state, 0);
    }
}

void SemanticHighlightingEngine::refreshTheme(CustomEditor *editor)
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

bool SemanticHighlightingEngine::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();
    if (type != QEvent::Show && type != QEvent::Hide && type != QEvent::Wheel
        && type != QEvent::KeyRelease && type != QEvent::MouseButtonRelease)
        return QObject::eventFilter(watched, event);

    State *state = m_eventTargets.value(watched);
    if (state && type == QEvent::Show)
        schedule(state, 0);
    else if (state && type == QEvent::Hide)
    {
        state->timer.stop();
        cancel(state);
    }
    else if (state)
        schedule(state, kScrollDebounceMs);
    return QObject::eventFilter(watched, event);
}

void SemanticHighlightingEngine::schedule(State *state, int delayMs)
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
    if (state->timer.isActive())
        delayMs = qMax(delayMs, state->timer.remainingTime());
    state->timer.start(qMax(0, delayMs));
}

void SemanticHighlightingEngine::cancel(State *state)
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

NotePeek::SemanticEntryState SemanticHighlightingEngine::entryStateForLine(
    CustomEditor *editor, int line)
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
        return NotePeek::SemanticEntryState::Code;
    }
}

QString SemanticHighlightingEngine::viewportText(CustomEditor *editor,
                                                  int firstLine, int lastLine)
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

void SemanticHighlightingEngine::start(State *state)
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
    const int lastLine = qMin(editor->lines() - 1, firstVisible + visibleLines + kLookAheadLines);
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
        if (requestId == state->requestId)
            state->cancellation.reset();
        if (state->editor)
        {
            state->editor->setProperty("semanticAnalysisInFlight", false);
            state->editor->setProperty("semanticWorkerNanoseconds", result.elapsedNanoseconds);
        }
        if (requestId == state->requestId && state->editor && state->editor->isVisible()
            && !result.canceled && result.revision == state->editor->documentRevision()
            && result.lastLine >= result.firstLine)
        {
            if (state->editor->semanticTokenCount() + result.tokens.size() > kMaxCachedTokens)
                state->editor->evictSemanticTokensOutside(result.firstLine, result.lastLine,
                                                          kMaxCachedTokens - result.tokens.size());
            QElapsedTimer applyTimer;
            applyTimer.start();
            state->editor->applySemanticTokenDelta(result.revision, result.firstLine,
                                                    result.lastLine, result.tokens);
            state->editor->setProperty("semanticApplyNanoseconds", applyTimer.nsecsElapsed());
        }
        if (state->rerun && state->editor && state->editor->isVisible())
            schedule(state, 0);
    });
    watcher->setFuture(QtConcurrent::run(BackgroundWorkScheduler::interactivePool(),
                                         [provider, document, control]() {
        return provider->analyze(document, control);
    }));
}
