#ifndef EDITORCONTENTREPLACER_H
#define EDITORCONTENTREPLACER_H

#include <QByteArray>
#include <QObject>
#include <QSet>

#include <functional>

class CustomEditor;

// Applies loaded bytes to Scintilla while preserving responsiveness, undo
// invariants, read-only state, and editor lifetime safety.
class EditorContentReplacer final : public QObject
{
    Q_OBJECT

public:
    explicit EditorContentReplacer(QObject *parent = nullptr);

    void insertInChunks(CustomEditor *editor,
                        const QByteArray &utf8Content,
                        std::function<void()> onFinished);
    bool replaceWithEventLoopYield(CustomEditor *editor, const QByteArray &utf8Content);
    bool isReplacing(CustomEditor *editor) const;
    void forgetEditor(CustomEditor *editor);

private:
    static void replaceImmediately(CustomEditor *editor, const QByteArray &utf8Content);

    QSet<CustomEditor *> m_editorsBeingReplaced;
};

#endif // EDITORCONTENTREPLACER_H
