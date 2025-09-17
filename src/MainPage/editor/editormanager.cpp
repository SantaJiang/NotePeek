#include "editormanager.h"
#include <QFileInfo>
#include <QDebug>
#include <QFontDatabase>
#include <QApplication>
#include <QScreen>
#include "../QScintilla/Qsci/qscilexer.h"
#include "../QScintilla/Qsci/qsciscintilla.h"
#include "lexerfactory.h"

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

QsciScintilla *EditorManager::createEditor(const QString &name)
{
    QsciScintilla *editor = new QsciScintilla();
    editor->setWindowTitle(name.isEmpty() ? tr("Untitled") : name);
    setupEditor(editor);
    m_editors.append(editor);
    emit editorCreated(editor);
    return editor;
}

QsciScintilla *EditorManager::createEditorFromFile(const QString &filePath, bool tryToCreate)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() && !tryToCreate)
    {
        return nullptr;
    }

    // Check if file is already open
    QsciScintilla *existingEditor = getEditorByFilePath(filePath);
    if (existingEditor)
    {
        return existingEditor;
    }

    QsciScintilla *editor = createEditor(fileInfo.fileName());

    // Load file content
    QFileInfo fileInfo2(filePath);
    bool isLargeFile = (fileInfo2.size() > 10 * 1024 * 1024);

    if (isLargeFile)
    {
        editor->setLexer(nullptr);
        editor->setEolMode(QsciScintilla::EolWindows);
        editor->setWrapMode(QsciScintilla::WrapNone);
        editor->setMarginWidth(1, 0);
        editor->setUpdatesEnabled(false);

        qint64 fileSize = fileInfo2.size();
        if (fileSize > 0 && fileSize < std::numeric_limits<unsigned long>::max() - 1000)
        {
            editor->SendScintilla(QsciScintillaBase::SCI_ALLOCATE,
                                  static_cast<unsigned long>(fileSize + 1000));
        }

        editor->setUpdatesEnabled(true);
    }
    else
    {
        setupEditorSyntax(editor, filePath);
    }

    return editor;
}

QsciScintilla *EditorManager::getEditorByFilePath(const QString &filePath)
{
    QFileInfo targetInfo(filePath);
    targetInfo.makeAbsolute();

    purgeOldEditorPointers();

    for (QsciScintilla *editor : m_editors)
    {
        if (editor && editor->property("filePath").toString() == targetInfo.filePath())
        {
            return editor;
        }
    }
    return nullptr;
}

QList<QsciScintilla *> EditorManager::getEditors() const
{
    QList<QsciScintilla *> list;
    for (const QPointer<QsciScintilla> &editor : m_editors)
    {
        if (editor)
        {
            list.append(editor);
        }
    }
    return list;
}

void EditorManager::setupEditor(QsciScintilla *editor)
{
    // Basic editor setup
    editor->setMarginType(0, QsciScintilla::NumberMargin);
    editor->setMarginWidth(0, 30);
    editor->setTabWidth(4);
    editor->setCaretLineVisible(true);
    editor->setCaretLineBackgroundColor(QColor("#E8E8FF"));
    editor->setBraceMatching(QsciScintilla::SloppyBraceMatch);
    editor->setMatchedBraceForegroundColor(Qt::blue);
    editor->setMatchedBraceBackgroundColor(Qt::yellow);

    // Set UTF-8 encoding
    editor->setUtf8(true);

    setEditorFont(editor);
    setMarginsFont(editor);

    // Connect signals
    connect(editor, SIGNAL(textChanged()), this, SLOT(handleTextChanged()));
}

void EditorManager::purgeOldEditorPointers()
{
    QMutableListIterator<QPointer<QsciScintilla>> it(m_editors);
    while (it.hasNext())
    {
        QPointer<QsciScintilla> pointer = it.next();
        if (pointer.isNull())
        {
            it.remove();
        }
    }
}

void EditorManager::handleTextChanged()
{
    // 获取发送信号的 editor 对象..
    QsciScintilla *editor = qobject_cast<QsciScintilla*>(sender());
    if (editor)
    {
        emit editor->modificationChanged(editor->isModified());
    }
}

void EditorManager::updateLineNumberMarginWidth(QsciScintilla *editor)
{
    if (!editor) return;

    int lines = editor->lines();
    int digits = 1;
    while (lines >= 10)
    {
        lines /= 10;
        digits++;
    }

    QScreen *primaryScreen = QApplication::primaryScreen();
    if(!primaryScreen)
    {
        return;
    }
    qreal dpr = primaryScreen->devicePixelRatio();
    QFont font = editor->font();
    QFontMetrics fm(font);

    int charWidth = fm.horizontalAdvance('0');
    int width = (charWidth * digits) + (10 * dpr);
    width = qMax(width, 30);

    editor->setMarginWidth(0, width);
}

void EditorManager::setupEditorSyntax(QsciScintilla *editor, const QString &filePath)
{
    if (!editor) return;

    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix();

    LexerFactory::Language lang = LexerFactory::languageFromExtension(extension);
    QsciLexer* lexer = LexerFactory::createLexer(lang);

    if(lexer)
    {
        QFont editorFont = editor->font();
        for (int i = 0; i <= QsciScintillaBase::STYLE_MAX; ++i)
        {
            lexer->setFont(editorFont, i);
        }
        editor->setLexer(lexer);
        editor->setAutoIndent(true);
    }
    else
    {
        editor->setLexer(nullptr);
        editor->setAutoIndent(false);
        setEditorFont(editor);
    }
    setMarginsFont(editor);
}

void EditorManager::setEditorFont(QsciScintilla *editor)
{
    if (!editor) return;

    QFont font("Source Code Pro", 11);
    editor->setFont(font);
}

void EditorManager::setMarginsFont(QsciScintilla *editor)
{
    if (!editor) return;
    QFont marginFont = editor->font();
    editor->setMarginsFont(marginFont);
}
