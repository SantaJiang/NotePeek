#include "editor/editormanager.h"
#include "apppreferences.h"
#include "themecolors.h"
#include <QFileInfo>
#include <QFontDatabase>
#include <QSet>
#include <QTimer>
#include <Qsci/qscilexer.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerdiff.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexerrust.h>
#include "decorators/highlightedscrollbardecorator.h"
#include "editor/customeditor.h"
#include "editor/documentregistry.h"
#include "editor/lexerfactory.h"
#include "editor/performanceconstants.h"

namespace {

using SyntaxRole = ThemeColors::Lexer::Role;

SyntaxRole roleFromDescription(QString description)
{
    description = description.toLower();

    if (description.contains(QStringLiteral("error")) || description.contains(QStringLiteral("unknown"))
        || description.contains(QStringLiteral("unclosed")) || description.contains(QStringLiteral("bad")))
        return SyntaxRole::Error;
    if (description.contains(QStringLiteral("comment")) || description.contains(QStringLiteral("remark")))
        return description.contains(QStringLiteral("doc")) ? SyntaxRole::Documentation : SyntaxRole::Comment;
    if (description.contains(QStringLiteral("escape")) || description.contains(QStringLiteral("entity")))
        return SyntaxRole::Escape;
    if (description.contains(QStringLiteral("regex")) || description.contains(QStringLiteral("regular expression")))
        return SyntaxRole::Regex;
    if (description.contains(QStringLiteral("string")) || description.contains(QStringLiteral("character"))
        || description.contains(QStringLiteral("quoted")))
        return SyntaxRole::String;
    if (description.contains(QStringLiteral("number")) || description.contains(QStringLiteral("numeric")))
        return SyntaxRole::Number;
    if (description.contains(QStringLiteral("preprocessor")) || description.contains(QStringLiteral("directive"))
        || description.contains(QStringLiteral("@-rule")))
        return SyntaxRole::Preprocessor;
    if (description.contains(QStringLiteral("operator")) || description.contains(QStringLiteral("delimiter")))
        return SyntaxRole::Operator;
    if (description.contains(QStringLiteral("function")) || description.contains(QStringLiteral("method"))
        || description.contains(QStringLiteral("command")) || description.contains(QStringLiteral("target"))
        || description.contains(QStringLiteral("label")))
        return SyntaxRole::Function;
    if (description.contains(QStringLiteral("class name")) || description.contains(QStringLiteral("type"))
        || description.contains(QStringLiteral("lifetime")))
        return SyntaxRole::Type;
    if (description.contains(QStringLiteral("builtin")) || description.contains(QStringLiteral("built-in"))
        || description.contains(QStringLiteral("macro")))
        return SyntaxRole::Builtin;
    if (description.contains(QStringLiteral("property")) || description.contains(QStringLiteral("key"))
        || description.contains(QStringLiteral("section")))
        return SyntaxRole::Property;
    if (description.contains(QStringLiteral("attribute")) || description.contains(QStringLiteral("decorator")))
        return SyntaxRole::Attribute;
    if (description.contains(QStringLiteral("tag")) || description.contains(QStringLiteral("heading")))
        return SyntaxRole::Tag;
    if (description.contains(QStringLiteral("link")) || description.contains(QStringLiteral("url"))
        || description.contains(QStringLiteral("iri")))
        return SyntaxRole::Link;
    if (description.contains(QStringLiteral("keyword")) || description.contains(QStringLiteral("reserved")))
        return SyntaxRole::Keyword;
    if (description.contains(QStringLiteral("variable")) || description.contains(QStringLiteral("parameter"))
        || description.contains(QStringLiteral("reference")))
        return SyntaxRole::Variable;
    // Most lexical scanners cannot distinguish a function/member/type use
    // from a plain identifier.  VS Code's TextMate fallback deliberately keeps
    // these at the editor foreground colour until semantic tokens are present.
    if (description.contains(QStringLiteral("identifier")) || description.contains(QStringLiteral("word")))
        return SyntaxRole::Default;
    return SyntaxRole::Default;
}

SyntaxRole roleForStyle(QsciLexer *lexer, int style)
{
    if (dynamic_cast<QsciLexerDiff *>(lexer)) {
        switch (style) {
        case QsciLexerDiff::Comment: return SyntaxRole::Comment;
        case QsciLexerDiff::Command: case QsciLexerDiff::Header: return SyntaxRole::Keyword;
        case QsciLexerDiff::Position: return SyntaxRole::Link;
        case QsciLexerDiff::LineAdded: case QsciLexerDiff::AddingPatchAdded:
        case QsciLexerDiff::RemovingPatchAdded: return SyntaxRole::Added;
        case QsciLexerDiff::LineRemoved: case QsciLexerDiff::AddingPatchRemoved:
        case QsciLexerDiff::RemovingPatchRemoved: return SyntaxRole::Removed;
        case QsciLexerDiff::LineChanged: return SyntaxRole::Changed;
        default: break;
        }
    }
    if (dynamic_cast<QsciLexerMarkdown *>(lexer)) {
        switch (style) {
        case QsciLexerMarkdown::StrongEmphasisAsterisks:
        case QsciLexerMarkdown::StrongEmphasisUnderscores: return SyntaxRole::Keyword;
        case QsciLexerMarkdown::EmphasisAsterisks:
        case QsciLexerMarkdown::EmphasisUnderscores:
        case QsciLexerMarkdown::StrikeOut: return SyntaxRole::Attribute;
        case QsciLexerMarkdown::Header1: case QsciLexerMarkdown::Header2:
        case QsciLexerMarkdown::Header3: case QsciLexerMarkdown::Header4:
        case QsciLexerMarkdown::Header5: case QsciLexerMarkdown::Header6: return SyntaxRole::Tag;
        case QsciLexerMarkdown::UnorderedListItem: case QsciLexerMarkdown::OrderedListItem:
        case QsciLexerMarkdown::HorizontalRule: return SyntaxRole::Operator;
        case QsciLexerMarkdown::BlockQuote: return SyntaxRole::Documentation;
        case QsciLexerMarkdown::Link: return SyntaxRole::Link;
        case QsciLexerMarkdown::CodeBackticks: case QsciLexerMarkdown::CodeDoubleBackticks:
        case QsciLexerMarkdown::CodeBlock: return SyntaxRole::String;
        default: break;
        }
    }
    if (dynamic_cast<QsciLexerPython *>(lexer)) {
        switch (style) {
        case QsciLexerPython::Comment: case QsciLexerPython::CommentBlock: return SyntaxRole::Comment;
        case QsciLexerPython::Number: return SyntaxRole::Number;
        case QsciLexerPython::DoubleQuotedString: case QsciLexerPython::SingleQuotedString:
        case QsciLexerPython::TripleSingleQuotedString: case QsciLexerPython::TripleDoubleQuotedString:
        case QsciLexerPython::DoubleQuotedFString: case QsciLexerPython::SingleQuotedFString:
        case QsciLexerPython::TripleSingleQuotedFString: case QsciLexerPython::TripleDoubleQuotedFString:
            return SyntaxRole::String;
        case QsciLexerPython::Keyword: return SyntaxRole::Keyword;
        case QsciLexerPython::ClassName: return SyntaxRole::Type;
        case QsciLexerPython::FunctionMethodName: return SyntaxRole::Function;
        case QsciLexerPython::Operator: return SyntaxRole::Operator;
        case QsciLexerPython::Identifier: return SyntaxRole::Variable;
        case QsciLexerPython::HighlightedIdentifier: return SyntaxRole::Builtin;
        case QsciLexerPython::Decorator: return SyntaxRole::Attribute;
        case QsciLexerPython::UnclosedString: return SyntaxRole::Error;
        default: break;
        }
    }
    if (dynamic_cast<QsciLexerRust *>(lexer)) {
        switch (style) {
        case QsciLexerRust::CommentBlock: case QsciLexerRust::CommentLine: return SyntaxRole::Comment;
        case QsciLexerRust::CommentBlockDoc: case QsciLexerRust::CommentLineDoc: return SyntaxRole::Documentation;
        case QsciLexerRust::Number: return SyntaxRole::Number;
        case QsciLexerRust::Keyword: case QsciLexerRust::KeywordSet3: case QsciLexerRust::KeywordSet4:
        case QsciLexerRust::KeywordSet5: case QsciLexerRust::KeywordSet6: case QsciLexerRust::KeywordSet7:
            return SyntaxRole::Keyword;
        case QsciLexerRust::BuiltInType: return SyntaxRole::Type;
        case QsciLexerRust::String: case QsciLexerRust::RawString: case QsciLexerRust::Character:
        case QsciLexerRust::ByteString: case QsciLexerRust::RawByteString: case QsciLexerRust::ByteCharacter:
            return SyntaxRole::String;
        case QsciLexerRust::Operator: return SyntaxRole::Operator;
        case QsciLexerRust::Identifier: return SyntaxRole::Variable;
        case QsciLexerRust::Lifetime: return SyntaxRole::Type;
        case QsciLexerRust::Macro: return SyntaxRole::Builtin;
        case QsciLexerRust::LexingError: return SyntaxRole::Error;
        default: break;
        }
    }
    if (dynamic_cast<QsciLexerJSON *>(lexer)) {
        switch (style) {
        case QsciLexerJSON::Number: return SyntaxRole::Number;
        case QsciLexerJSON::String: return SyntaxRole::String;
        case QsciLexerJSON::Property: return SyntaxRole::Property;
        case QsciLexerJSON::EscapeSequence: return SyntaxRole::Escape;
        case QsciLexerJSON::CommentLine: case QsciLexerJSON::CommentBlock: return SyntaxRole::Comment;
        case QsciLexerJSON::Operator: return SyntaxRole::Operator;
        case QsciLexerJSON::IRI: case QsciLexerJSON::IRICompact: return SyntaxRole::Link;
        case QsciLexerJSON::Keyword: case QsciLexerJSON::KeywordLD: return SyntaxRole::Keyword;
        case QsciLexerJSON::UnclosedString: case QsciLexerJSON::Error: return SyntaxRole::Error;
        default: break;
        }
    }
    if (dynamic_cast<QsciLexerCPP *>(lexer)) {
        const int activeStyle = style >= 64 ? style - 64 : style;
        switch (activeStyle) {
        case QsciLexerCPP::Comment: case QsciLexerCPP::CommentLine: return SyntaxRole::Comment;
        case QsciLexerCPP::CommentDoc: case QsciLexerCPP::CommentLineDoc:
        case QsciLexerCPP::CommentDocKeyword: case QsciLexerCPP::TaskMarker: return SyntaxRole::Documentation;
        case QsciLexerCPP::CommentDocKeywordError: return SyntaxRole::Error;
        case QsciLexerCPP::Number: return SyntaxRole::Number;
        case QsciLexerCPP::Keyword: return SyntaxRole::Keyword;
        case QsciLexerCPP::KeywordSet2: case QsciLexerCPP::GlobalClass: return SyntaxRole::Type;
        case QsciLexerCPP::DoubleQuotedString: case QsciLexerCPP::SingleQuotedString:
        case QsciLexerCPP::VerbatimString: case QsciLexerCPP::RawString:
        case QsciLexerCPP::TripleQuotedVerbatimString: case QsciLexerCPP::HashQuotedString:
        case QsciLexerCPP::UserLiteral: return SyntaxRole::String;
        case QsciLexerCPP::EscapeSequence: return SyntaxRole::Escape;
        case QsciLexerCPP::Regex: return SyntaxRole::Regex;
        case QsciLexerCPP::PreProcessor: return SyntaxRole::Preprocessor;
        case QsciLexerCPP::PreProcessorComment: return SyntaxRole::Comment;
        case QsciLexerCPP::PreProcessorCommentLineDoc: return SyntaxRole::Documentation;
        case QsciLexerCPP::Operator: return SyntaxRole::Operator;
        // LexCPP uses Identifier for virtually every non-keyword name.  Giving
        // it a dedicated variable colour paints functions, members and local
        // variables alike, producing large monochrome regions.  Dark+ keeps
        // lexical-only identifiers at the normal foreground colour; richer
        // distinctions require semantic tokens rather than a lexer palette.
        case QsciLexerCPP::Identifier: return SyntaxRole::Default;
        case QsciLexerCPP::UnclosedString: return SyntaxRole::Error;
        default: break;
        }
    }

    return roleFromDescription(lexer->description(style));
}

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
    editor->shareDocumentSessionFrom(sourceEditor);
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
    bool isLargeFile = (fileInfo.size() > PerformanceConstants::LargeFileSize);

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

    // Set UTF-8 encoding
    editor->setUtf8(true);

    // No file path yet: seed with the code font.  updateEditorSyntax() applies
    // the correct text/code font once the path (and therefore the lexer) is known.
    const QFont initialFont = fontForPath(QString());
    editor->setFont(initialFont);
    setMarginsFont(editor, initialFont);

    // Apply theme-aware colors (margin background/foreground, caret line, etc.)
    applyEditorColors(editor);

    // Connect signals
    connect(editor, &CustomEditor::textChanged, this, [editor]() {
        emit editor->modificationChanged(editor->isModified());
    });
    connect(editor, &CustomEditor::linesChanged, this, [this, editor]() {
        updateLineNumberMarginWidth(editor);
    });

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

void EditorManager::setMarginsFont(CustomEditor *editor, const QFont &font)
{
    if (!editor) return;

    editor->setMarginsFont(font);
}

void EditorManager::updateEditorSyntax(CustomEditor *editor, const QString &filePath)
{
    if (!editor) return;

    const QFont font = fontForPath(filePath);

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
    editor->setMarginWidth(0, softWrap ? 0 : 30);
    if (!softWrap)
    {
        updateLineNumberMarginWidth(editor);
    }
}

void EditorManager::applyCodeFeaturePreferences()
{
    for (auto &editorPtr : m_editors)
    {
        if (!editorPtr)
            continue;

        CustomEditor *editor = editorPtr;
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
        const QFont font = fontForPath(editor->filePath());

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
        applyUniformStyleFont(lexer, fontForPath(editor->filePath()));
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
    editor->setFoldMarginColors(marginBg(), marginBg());
    editor->setIndentationGuidesBackgroundColor(background());
    editor->setIndentationGuidesForegroundColor(indentGuide());
    for (int marker = QsciScintillaBase::SC_MARKNUM_FOLDEREND;
         marker <= static_cast<int>(QsciScintillaBase::SC_MARKNUM_FOLDEROPEN);
         ++marker)
    {
        editor->setMarkerForegroundColor(marginBg(), marker);
        editor->setMarkerBackgroundColor(foldMarker(), marker);
    }
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

    // Style numbers are lexer-specific.  Apply colours only after translating
    // each style into a semantic role; treating every lexer like C++ causes,
    // for example, Python strings and JSON properties to receive number colours.
    lexer->setDefaultPaper(defaultPaper());
    lexer->setPaper(defaultPaper(), -1);
    for (int style = 0; style <= QsciScintillaBase::STYLE_MAX; ++style) {
        if (lexer->description(style).isEmpty())
            continue;
        lexer->setColor(color(roleForStyle(lexer, style)), style);
    }
}
