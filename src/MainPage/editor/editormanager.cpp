#include "editormanager.h"
#include <QFileInfo>
#include <QDebug>
#include <QFontDatabase>
#include <QApplication>
#include <QScreen>
#include <QDir>
#include "../QScintilla/Qsci/qscilexer.h"
#include "../decorators/highlightedscrollbardecorator.h"
#include "customeditor.h"
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

CustomEditor *EditorManager::createEditor(const QString &name)
{
    CustomEditor *editor = new CustomEditor();
    editor->setWindowTitle(name.isEmpty() ? tr("Untitled") : name);
    setupEditor(editor);
    // 连接 URI 拖放信号
    connect(editor, &CustomEditor::filesDropped,  this, &EditorManager::filesDropped);

    m_editors.append(editor);
    emit editorCreated(editor);
    return editor;
}

CustomEditor *EditorManager::createEditorFromFile(const QString &filePath, bool tryToCreate)
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
        setupEditorSyntax(editor, filePath);
    }

    return editor;
}

CustomEditor *EditorManager::getEditorByFilePath(const QString &filePath)
{
    QFileInfo targetInfo(filePath);
    targetInfo.makeAbsolute();

    // 规范化路径，统一分隔符和格式..
    QString canonicalTargetPath = QDir::cleanPath(targetInfo.absoluteFilePath());

    purgeOldEditorPointers();

    for (CustomEditor *editor : m_editors)
    {
        if (editor)
        {
            QString editorPath = editor->property("filePath").toString();
            // 同样规范化编辑器中的路径..
            QString canonicalEditorPath = QDir::cleanPath(QFileInfo(editorPath).absoluteFilePath());

            if (canonicalEditorPath == canonicalTargetPath)
            {
                return editor;
            }
        }
    }
    return nullptr;
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

void EditorManager::setupEditor(CustomEditor *editor)
{
    // Basic editor setup
    editor->setMarginType(0, CustomEditor::NumberMargin);
    editor->setMarginWidth(0, 30);
    // 设置初始滚动宽度为最小值，避免空文档时显示横向滚动条
    // 启用滚动宽度跟踪后，当文档内容变宽时会自动更新
    editor->setScrollWidth(1);
    editor->setScrollWidthTracking(true);
    editor->setTabWidth(4);
    editor->setCaretLineVisible(true);
    editor->setCaretLineBackgroundColor(QColor("#E8E8FF"));
    editor->setBraceMatching(CustomEditor::SloppyBraceMatch);
    editor->setMatchedBraceForegroundColor(Qt::blue);
    editor->setMatchedBraceBackgroundColor(Qt::yellow);

    // Set UTF-8 encoding
    editor->setUtf8(true);

    setEditorFont(editor);
    setMarginsFont(editor);

    // Connect signals
    connect(editor, SIGNAL(textChanged()), this, SLOT(handleTextChanged()));

    // 设置高亮滚动条装饰器
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

void EditorManager::handleTextChanged()
{
    // 获取发送信号的 editor 对象..
    CustomEditor *editor = qobject_cast<CustomEditor*>(sender());
    if (editor)
    {
        emit editor->modificationChanged(editor->isModified());
    }
}

void EditorManager::setupScrollBarDecorator(CustomEditor *editor)
{
    HighlightedScrollBarDecorator *scrollDecorator = new HighlightedScrollBarDecorator(editor);
    scrollDecorator->setEnabled(true);

    // 存储装饰器指针以便管理
    editor->setProperty("scrollDecorator", QVariant::fromValue(scrollDecorator));
}

void EditorManager::updateLineNumberMarginWidth(CustomEditor *editor)
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

void EditorManager::setupEditorSyntax(CustomEditor *editor, const QString &filePath)
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
