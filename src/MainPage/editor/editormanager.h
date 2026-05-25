#ifndef EDITORMANAGER_H
#define EDITORMANAGER_H

#include <QObject>
#include <QPointer>
#include <QList>
#include <QUrl>
#include <QtGlobal>

class CustomEditor;
class EditorManager : public QObject
{
    Q_OBJECT
public:
    explicit EditorManager(QObject *parent = nullptr);
    ~EditorManager();

    CustomEditor *createEditor(const QString &name = QString());
    CustomEditor *createEditorForFile(const QString &filePath, bool tryToCreate = false);
    CustomEditor *getEditorByFilePath(const QString &filePath);
    QList<CustomEditor *> getEditors() const;
    void forgetEditor(CustomEditor *editor);

    void setupEditor(CustomEditor *editor);
    void updateLineNumberMarginWidth(CustomEditor *editor);
    void updateEditorSyntax(CustomEditor *editor, const QString &filePath);

    // Re-apply theme colours to every open editor (call on palette change)
    void reapplyThemeToAllEditors();

signals:
    void editorCreated(CustomEditor *editor);
    void editorClosed(CustomEditor *editor);
    void filesDropped(const QList<QUrl> &urls);

private:
    void setupScrollBarDecorator(CustomEditor *editor);
    void setEditorFont(CustomEditor *editor);
    void setMarginsFont(CustomEditor *editor);
    void purgeOldEditorPointers();
    void applyEditorColors(CustomEditor *editor);
    void applyLexerTheme(class QsciLexer *lexer);
    void reapplyEditorSyntax(CustomEditor *editor);
    void applyBaseEditorColors(CustomEditor *editor);

    static constexpr qint64 LARGE_FILE_SIZE = 10 * 1024 * 1024; // 10 MB

    QList<QPointer<CustomEditor>> m_editors;
};

#endif // EDITORMANAGER_H
