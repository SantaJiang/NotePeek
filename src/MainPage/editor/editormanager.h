#ifndef EDITORMANAGER_H
#define EDITORMANAGER_H

#include <QObject>
#include <QObject>
#include <QPointer>
#include <QList>

class QsciScintilla;
class EditorManager : public QObject
{
    Q_OBJECT
public:
    explicit EditorManager(QObject *parent = nullptr);
    ~EditorManager();

    QsciScintilla *createEditor(const QString &name = QString());
    QsciScintilla *createEditorFromFile(const QString &filePath, bool tryToCreate = false);
    QsciScintilla *getEditorByFilePath(const QString &filePath);
    QList<QsciScintilla *> getEditors() const;

    void setupEditor(QsciScintilla *editor);
    void updateLineNumberMarginWidth(QsciScintilla *editor);

signals:
    void editorCreated(QsciScintilla *editor);
    void editorClosed(QsciScintilla *editor);

private slots:
    void handleTextChanged();

private:
    void setupEditorSyntax(QsciScintilla *editor, const QString &filePath);
    void setEditorFont(QsciScintilla *editor);
    void setMarginsFont(QsciScintilla *editor);
    void purgeOldEditorPointers();

    QList<QPointer<QsciScintilla>> m_editors;
};

#endif // EDITORMANAGER_H
