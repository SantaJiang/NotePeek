#ifndef EDITORCONTENTREPLACER_H
#define EDITORCONTENTREPLACER_H

#include <QByteArray>
#include <QHash>
#include <QObject>

#include <functional>
#include <memory>

class CustomEditor;

// Applies loaded bytes to Scintilla while preserving responsiveness, undo
// invariants, read-only state, and editor lifetime safety. A bulk replacement
// suppresses per-chunk insert/delete notifications by removing
// SC_MOD_INSERTTEXT/SC_MOD_DELETETEXT from the modification event mask; the
// caller aggregates a single textChanged()/linesChanged() via
// CustomEditor::notifyDocumentReplaced() after finishReplacement().
class EditorContentReplacer final : public QObject
{
    Q_OBJECT

public:
    explicit EditorContentReplacer(QObject *parent = nullptr);

    // Begin a bulk replacement for `editor`: clear the document, disable undo
    // collection and mute insert/delete notifications for the duration.
    // Returns false if a replacement is already active for this editor.
    bool beginReplacement(CustomEditor *editor);
    // Append a chunk on the GUI thread without broadcasting a modification
    // signal. Only valid while a replacement is active for `editor`.
    bool appendChunk(CustomEditor *editor, const char *data, qsizetype size);
    // Finish the replacement: restore undo collection, empty the undo buffer,
    // restore the modification mask, set the save point and restore read-only
    // and updates-enabled state. Idempotent; returns false if not active.
    bool finishReplacement(CustomEditor *editor);
    // Abort the replacement: invalidate any queued continuation, drop the
    // half-loaded content and restore the editor's original state. Idempotent.
    void abortReplacement(CustomEditor *editor);

    // Convenience wrapper for loading a whole buffer, yielding to the event
    // loop between chunks for large content. `onFinished` runs once on
    // success; it is never called after cancellation or editor destruction.
    void insertInChunks(CustomEditor *editor,
                        const QByteArray &utf8Content,
                        std::function<void()> onFinished);
    // Synchronous variant used by reload paths; returns true on success.
    bool replaceWithEventLoopYield(CustomEditor *editor, const QByteArray &utf8Content);

    bool isReplacing(CustomEditor *editor) const;
    void forgetEditor(CustomEditor *editor);

private:
    struct ReplacementState;

    QHash<CustomEditor *, std::shared_ptr<ReplacementState>> m_replacements;
    QHash<CustomEditor *, quint64> m_generations;
};

#endif // EDITORCONTENTREPLACER_H
