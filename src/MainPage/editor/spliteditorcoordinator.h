#ifndef SPLITEDITORCOORDINATOR_H
#define SPLITEDITORCOORDINATOR_H

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

class CustomEditor;
class MultiEditor;
class QSplitter;
class QWidget;

class SplitEditorCoordinator : public QObject
{
    Q_OBJECT

public:
    SplitEditorCoordinator(MultiEditor *leftEditor,
                           MultiEditor *rightEditor,
                           QSplitter *splitter,
                           bool singleFileWindow,
                           QObject *parent = nullptr);

    MultiEditor *activeMultiEditor() const;
    QList<MultiEditor *> editorPanes() const;
    void setActiveMultiEditor(MultiEditor *multiEditor);
    void setSplitViewEnabled(bool enabled);
    bool closeAllEditors();
    void reapplyThemeToEditorPanes();
    void refreshVisibleMarkdownPreviews();
    void updateActivePaneForFocus(QWidget *widget);

signals:
    void activeMultiEditorChanged(MultiEditor *multiEditor);
    void paneModificationChanged(MultiEditor *multiEditor, bool modified);
    void paneCopyAvailableChanged(MultiEditor *multiEditor, bool available);
    void filesDropped(MultiEditor *multiEditor, const QList<QUrl> &urls);

private:
    void connectMultiEditor(MultiEditor *multiEditor);
    void syncOpenedFileToOtherPanes(MultiEditor *source, const QString &filePath);
    void syncClosedFileToOtherPanes(MultiEditor *source, const QString &filePath);
    void syncRenamedFileToOtherPanes(MultiEditor *source,
                                     CustomEditor *sourceEditor,
                                     const QString &oldFilePath,
                                     const QString &newFilePath);
    void reloadExternallyChangedFile(MultiEditor *source, const QString &filePath);
    void showSameFileInRightPane();

    MultiEditor *m_leftEditor;
    MultiEditor *m_rightEditor;
    QSplitter *m_splitter;
    MultiEditor *m_activeMultiEditor;
    bool m_syncingEditorPanes;
    bool m_singleFileWindow;
};

#endif // SPLITEDITORCOORDINATOR_H
