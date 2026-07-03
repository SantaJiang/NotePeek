#include "editor/editormanager.h"
#include "themecolors.h"
#include <QFileInfo>
#include <QFontDatabase>
#include <QSet>
#include <QTimer>
#include <Qsci/qscilexer.h>
#include "decorators/highlightedscrollbardecorator.h"
#include "editor/customeditor.h"
#include "editor/documentregistry.h"
#include "editor/lexerfactory.h"

namespace {

long toScintillaColor(const QColor &color)
{
    return (color.blue() << 16) | (color.green() << 8) | color.red();
}

bool shouldUseSoftWrap(const QString &filePath, LexerFactory::Language lang)
{
    switch (lang)
    {
    case LexerFactory::MARKDOWN:
    case LexerFactory::TEX:
    case LexerFactory::PO:
        return true;
    default:
        break;
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

} // namespace

EditorManager::EditorManager(QObject *parent) : QObject(parent)
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
    editor->setWindowTitle(name.isEmpty() ? tr("Untitled") : name);
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
    editor->setFilePath(sourceEditor->filePath());
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
    bool isLargeFile = (fileInfo.size() > LARGE_FILE_SIZE);

    if (isLargeFile)
    {
        editor->setLexer(nullptr);
        editor->setEolMode(CustomEditor::EolWindows);
        editor->setWrapMode(CustomEditor::WrapNone);
        editor->setMarginWidth(1, 0);
        editor->setUpdatesEnabled(false);

        qint64 fileSize = fileInfo.size();
        if (fileSize > 0 && fileSize < std::numeric_limits<unsigned long>::max() - 1000)
        {
            editor->SendScintilla(QsciScintillaBase::SCI_ALLOCATE,
                                  static_cast<unsigned long>(fileSize + 1000));
        }

        editor->setUpdatesEnabled(true);
    }
    else
    {
        updateEditorSyntax(editor, filePath);
    }

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
    editor->setMargins(1);
    editor->setMarginType(0, CustomEditor::NumberMargin);
    editor->setMarginWidth(0, 30);
    // Start with the minimum scroll width so an empty document shows no horizontal
    // scrollbar; with width tracking enabled it grows as the content widens
    editor->setScrollWidth(1);
    editor->setScrollWidthTracking(true);
    editor->setTabWidth(4);
    editor->setCaretLineVisible(true);
    editor->setBraceMatching(CustomEditor::SloppyBraceMatch);
    editor->setWrapMode(CustomEditor::WrapNone);

    // Set UTF-8 encoding
    editor->setUtf8(true);

    setEditorFont(editor);
    setMarginsFont(editor);

    // Apply theme-aware colors (margin background/foreground, caret line, etc.)
    applyEditorColors(editor);

    // Connect signals
    connect(editor, &CustomEditor::textChanged, this, [editor]() {
        emit editor->modificationChanged(editor->isModified());
    });
    connect(editor, &CustomEditor::linesChanged, this, [this, editor]() {
        updateLineNumberMarginWidth(editor);
    });

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

    const int visibleDigits = qMax(digits, 2);
    const QString sample = QString(visibleDigits, QLatin1Char('9')) + QLatin1Char(' ');
    const int width = editor->SendScintilla(QsciScintillaBase::SCI_TEXTWIDTH,
                                            QsciScintillaBase::STYLE_LINENUMBER,
                                            sample.toUtf8().constData());
    editor->setMarginWidth(0, width);
}

void EditorManager::setEditorFont(CustomEditor *editor)
{
    if (!editor)
        return;

#if defined(Q_OS_WIN)
    QFont font("Source Code Pro", 10);
    editor->setFont(font);
#elif defined(Q_OS_MAC)
    QFont font("Menlo", 12);
    editor->setFont(font);
#elif defined(Q_OS_LINUX)
    QFont font;
    QFontDatabase fontDB;
    if (fontDB.hasFamily("Noto Sans Mono"))
        font = QFont("Noto Sans Mono", 10);
    else
        font = QFont("Source Code Pro", 10);
    editor->setFont(font);
#endif
}

void EditorManager::setMarginsFont(CustomEditor *editor)
{
    if (!editor) return;

    QFont marginFont = editor->font();
    editor->setMarginsFont(marginFont);
}

void EditorManager::updateEditorSyntax(CustomEditor *editor, const QString &filePath)
{
    if (!editor) return;

    QsciLexer *oldLexer = editor->lexer();
    LexerFactory::Language lang = LexerFactory::languageFromFilePath(filePath);
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
        // pushes the monospace font to all styles and keeps the spacing stable.
        applyUniformStyleFont(editor, lexer);

        if (oldLexer && oldLexer != lexer)
        {
            oldLexer->deleteLater();
        }
        editor->setAutoIndent(true);
    }
    else
    {
        editor->setLexer(nullptr);
        if (oldLexer)
        {
            oldLexer->deleteLater();
        }
        editor->setAutoIndent(false);
        setEditorFont(editor);
    }
    setMarginsFont(editor);
    applyDisplayPolicy(editor, filePath);
    // Re-apply margin colors after lexer change (lexer can override margin colors)
    applyEditorColors(editor);
}

void EditorManager::applyDisplayPolicy(CustomEditor *editor, const QString &filePath)
{
    if (!editor)
    {
        return;
    }

    const LexerFactory::Language lang = LexerFactory::languageFromFilePath(filePath);
    const bool softWrap = shouldUseSoftWrap(filePath, lang);
    editor->setWrapMode(softWrap ? CustomEditor::WrapWord : CustomEditor::WrapNone);
    editor->setMarginWidth(0, softWrap ? 0 : 30);
    if (!softWrap)
    {
        updateLineNumberMarginWidth(editor);
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

void EditorManager::applyUniformStyleFont(CustomEditor *editor, QsciLexer *lexer)
{
    if (!editor || !lexer) return;

    // The lexer's fontChanged signal is connected to the editor by setLexer(),
    // so setting the font here reaches every Scintilla style (not just the ones
    // the lexer describes) and keeps the line height tied to the editor font.
    const QFont editorFont = editor->font();
    for (int i = 0; i <= QsciScintillaBase::STYLE_MAX; ++i)
    {
        lexer->setFont(editorFont, i);
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
        applyUniformStyleFont(editor, lexer);
    }
    else
    {
        // No lexer: color/paper are set by applyEditorColors
    }
}

void EditorManager::applyEditorColors(CustomEditor *editor)
{
    if (!editor) return;

    using namespace ThemeColors::Editor;

    // setMarginsBackgroundColor() uses STYLE_LINENUMBER (style 33) which is
    // the correct way to set the background of a NumberMargin.  In contrast,
    // setMarginBackgroundColor(margin, ...) uses SCI_SETMARGINBACKN which only
    // works for SC_MARGIN_COLOUR type margins.
    editor->setMarginsBackgroundColor(marginBg());
    editor->setMarginsForegroundColor(marginFg());
    editor->setCaretForegroundColor(caretFg());
    editor->SendScintilla(QsciScintillaBase::SCI_SETADDITIONALCARETFORE,
                          toScintillaColor(caretFg()));
    editor->setCaretLineBackgroundColor(caretLineBg());
    editor->setMatchedBraceForegroundColor(matchedBraceFg());
    editor->setMatchedBraceBackgroundColor(matchedBraceBg());
    applyBaseEditorColors(editor);
    editor->viewport()->update();
}

void EditorManager::applyBaseEditorColors(CustomEditor *editor)
{
    if (!editor) return;

    const QColor foreground = ThemeColors::Editor::foreground();
    const QColor background = ThemeColors::Editor::background();
    const QColor marginForeground = ThemeColors::Editor::marginFg();
    const QColor marginBackground = ThemeColors::Editor::marginBg();
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

    using namespace ThemeColors::Lexer;

    // Set the default paper and text colour for the entire lexer.  setDefaultPaper()
    // also updates STYLE_DEFAULT, which Scintilla uses for blank space past EOL/EOF.
    lexer->setDefaultPaper(defaultPaper());
    lexer->setPaper(defaultPaper(), -1);
    lexer->setColor(defaultColor(), -1);

    // Override common syntax style colours so they are readable on the active
    // background.  Not every lexer uses every style number, but the ones listed
    // below cover the vast majority of languages that QScintilla ships with.
    //
    // Style numbers are based on the Scintilla / QScintilla conventions:
    //   0  = default
    //   1  = comment
    //   2  = comment-line  /  line comment
    //   3  = comment-doc   /  doc comment
    //   4  = number
    //   5  = keyword (primary set)
    //   6  = string
    //   7  = character
    //   8  = UUID / literal
    //   9  = preprocessor
    //  10  = operator
    //  11  = identifier
    //  12  = string-eol (unclosed string)
    //  13  = keyword (secondary set – often types / builtins)
    //  14  = comment-doc-keyword (e.g. @param in JavaDoc)
    //  15  = comment-doc-keyword-error
    //  16  = global class / keyword set 3
    //
    // We only override the styles that are present; QScintilla silently
    // ignores colour settings for style numbers it does not use.

    // Comments
    lexer->setColor(comment(), 1);
    lexer->setColor(comment(), 2);
    lexer->setColor(comment(), 3);

    // Numbers
    lexer->setColor(number(), 4);

    // Primary keywords
    lexer->setColor(keyword(), 5);

    // Strings
    lexer->setColor(string(), 6);
    lexer->setColor(string(), 7);

    // Preprocessor / directives
    lexer->setColor(preprocessor(), 9);

    // Operators
    lexer->setColor(operatorColor(), 10);

    // Identifiers
    lexer->setColor(identifier(), 11);

    // Unclosed strings
    lexer->setColor(string(), 12);

    // Secondary keywords (types, builtins)
    lexer->setColor(docKeyword(), 13);

    // Doc keyword
    lexer->setColor(docKeyword(), 14);

    // Keyword set 3 (global class)
    lexer->setColor(type(), 16);

    // Markdown-specific styles (QsciLexerMarkdown uses higher numbers)
    // These are safe to set even for non-markdown lexers (ignored).
    lexer->setColor(type(), 41);  // H1-H6 in some lexers
    lexer->setColor(type(), 42);
    lexer->setColor(type(), 43);
    lexer->setColor(type(), 44);
    lexer->setColor(type(), 45);
    lexer->setColor(type(), 46);
}
