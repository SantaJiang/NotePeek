#include "editor/customeditor.h"
#include <QMimeData>
#include <QFrame>

CustomEditor::CustomEditor(QWidget *parent)
    : QsciScintilla{parent}
{
    setAcceptDrops(true);
    setFrameShape(QFrame::NoFrame);
}

void CustomEditor::setFilePath(const QString &path)
{
    m_filePath = path;
}

QString CustomEditor::filePath() const
{
    return m_filePath;
}

void CustomEditor::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
    {
        e->acceptProposedAction();
    }
    else
    {
        QsciScintilla::dragEnterEvent(e);
    }
}

void CustomEditor::dropEvent(QDropEvent *e)
{
    const QMimeData *mimeData = e->mimeData();

    if (mimeData->hasUrls())
    {
        QList<QUrl> urlList = mimeData->urls();
        emit filesDropped(urlList);
        e->acceptProposedAction();
    }
    else
    {
        // Not a file drop; let the base class handle text drops etc.
        QsciScintilla::dropEvent(e);
    }
}
