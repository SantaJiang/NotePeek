#include "editordecorator.h"
#include <Qsci/qsciscintilla.h>

EditorDecorator::EditorDecorator(QsciScintilla *editor)
    : QObject(editor), editor(editor), enabled(false)
{
}

EditorDecorator::~EditorDecorator()
{
}

void EditorDecorator::setEnabled(bool b)
{
    if (enabled == b)
        return;

    enabled = b;

    if (enabled) {
        // 这里可以连接编辑器的信号
    } else {
        // 断开信号连接
    }

    emit stateChanged(enabled);
}
