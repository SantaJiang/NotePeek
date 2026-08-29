#ifndef EDITORSNAPSHOTENCODER_H
#define EDITORSNAPSHOTENCODER_H

#include <QByteArray>
#include <QString>

class CustomEditor;

// Adapter from Scintilla's UTF-8 document buffer to persistence bytes. Keeping
// this at the widget boundary prevents the core persistence service from
// depending on CustomEditor while preserving the zero-copy fast path.
class EditorSnapshotEncoder final
{
public:
    static bool encode(CustomEditor *editor,
                       QByteArray *encodedContent,
                       QString *errorMessage);
};

#endif // EDITORSNAPSHOTENCODER_H
