#ifndef MARKDOWNTOOLCOORDINATOR_H
#define MARKDOWNTOOLCOORDINATOR_H

#include <QObject>
#include <QString>

class CustomEditor;
class DocumentController;
class MarkdownPreviewer;
class QWidget;

// Coordinates Markdown presentation commands while the previewer remains the
// rendering service and EditorPane remains the view owner.
class MarkdownToolCoordinator final : public QObject
{
    Q_OBJECT

public:
    explicit MarkdownToolCoordinator(QWidget *dialogParent, QObject *parent = nullptr);

    MarkdownPreviewer *previewer() const { return m_previewer; }
    bool isMarkdownFile(const QString &filePath) const;
    void togglePreview(DocumentController *controller);
    void exportToPdf(CustomEditor *editor);

signals:
    void previewStateChanged(CustomEditor *editor, bool visible);

private:
    QWidget *m_dialogParent;
    MarkdownPreviewer *m_previewer;
};

#endif // MARKDOWNTOOLCOORDINATOR_H
