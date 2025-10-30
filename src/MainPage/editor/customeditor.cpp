#include "customeditor.h"
#include <QMimeData>

CustomEditor::CustomEditor(QWidget *parent)
    : QsciScintilla{parent}
{
    setAcceptDrops(true);
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
        emit filesDropped(urlList);  // 发射所有文件
        e->acceptProposedAction();
    }
    else
    {
        // 不是文件拖放，交给父类处理文本拖放等
        QsciScintilla::dropEvent(e);
    }
}
