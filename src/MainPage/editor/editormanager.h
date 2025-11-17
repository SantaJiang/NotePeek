#ifndef EDITORMANAGER_H
#define EDITORMANAGER_H

#include <QObject>
#include <QObject>
#include <QPointer>
#include <QList>
#include <QUrl>

class CustomEditor;
class EditorManager : public QObject
{
    Q_OBJECT
public:
    explicit EditorManager(QObject *parent = nullptr);
    ~EditorManager();

    CustomEditor *createEditor(const QString &name = QString());
    CustomEditor *createEditorFromFile(const QString &filePath, bool tryToCreate = false);
    CustomEditor *getEditorByFilePath(const QString &filePath);
    QList<CustomEditor *> getEditors() const;

    void setupEditor(CustomEditor *editor);
    void updateLineNumberMarginWidth(CustomEditor *editor);

signals:
    void fileDraged(QString filePath);
    void editorCreated(CustomEditor *editor);
    void editorClosed(CustomEditor *editor);
    void filesDropped(const QList<QUrl> &urls);

private slots:
    void handleTextChanged();

private:
    void setupScrollBarDecorator(CustomEditor *editor);
    void setupEditorSyntax(CustomEditor *editor, const QString &filePath);
    void setEditorFont(CustomEditor *editor);
    void setMarginsFont(CustomEditor *editor);
    void purgeOldEditorPointers();

    QList<QPointer<CustomEditor>> m_editors;
};

#endif // EDITORMANAGER_H
