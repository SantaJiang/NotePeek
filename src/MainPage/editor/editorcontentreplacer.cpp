#include "editor/editorcontentreplacer.h"

#include "editor/customeditor.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QPointer>
#include <QTimer>

#include <cstdint>
#include <memory>

namespace {

// Insert budget per event-loop tick. Large enough for typical files in one
// pass, small enough that a 100+ MB insert keeps the UI responsive.
constexpr qsizetype ChunkedInsertBytes = 8 * 1024 * 1024;

// Scintilla delivers SCN_MODIFIED whenever (modificationType & mask) is
// non-zero, so removing only SC_MOD_INSERTTEXT/SC_MOD_DELETETEXT is not enough:
// text edits always carry SC_PERFORMED_USER and the before/check variants. All
// text-edit flags are cleared here while style/fold/marker/indicator bits are
// left intact so unrelated callers keep their notifications.
constexpr long QuietTextEditBits =
    long(QsciScintillaBase::SC_MOD_INSERTTEXT) | long(QsciScintillaBase::SC_MOD_DELETETEXT)
    | long(QsciScintillaBase::SC_MOD_BEFOREINSERT) | long(QsciScintillaBase::SC_MOD_BEFOREDELETE)
    | long(QsciScintillaBase::SC_MOD_INSERTCHECK)
    | long(QsciScintillaBase::SC_PERFORMED_USER) | long(QsciScintillaBase::SC_PERFORMED_UNDO)
    | long(QsciScintillaBase::SC_PERFORMED_REDO) | long(QsciScintillaBase::SC_STARTACTION)
    | long(QsciScintillaBase::SC_MULTISTEPUNDOREDO) | long(QsciScintillaBase::SC_LASTSTEPINUNDOREDO)
    | long(QsciScintillaBase::SC_MULTILINEUNDOREDO);

} // namespace

struct EditorContentReplacer::ReplacementState
{
    QPointer<CustomEditor> editor;
    long originalModificationMask = 0;
    bool originalUndoCollection = false;
    bool originalReadOnly = false;
    bool originalUpdatesEnabled = false;
    bool active = false;
};

EditorContentReplacer::EditorContentReplacer(QObject *parent)
    : QObject(parent)
{
}

bool EditorContentReplacer::beginReplacement(CustomEditor *editor)
{
    if (!editor || isReplacing(editor))
        return false;

    auto state = std::make_shared<ReplacementState>();
    state->editor = editor;
    state->originalModificationMask =
        editor->SendScintilla(QsciScintillaBase::SCI_GETMODEVENTMASK);
    state->originalUndoCollection =
        editor->SendScintilla(QsciScintillaBase::SCI_GETUNDOCOLLECTION) != 0;
    state->originalReadOnly = editor->isReadOnly();
    state->originalUpdatesEnabled = editor->updatesEnabled();

    // Mute text-edit notifications without disturbing any other bits the
    // caller may have configured. The quiet mask must be in effect before the
    // document is cleared so the clear does not broadcast a delete.
    const long quietMask = state->originalModificationMask & ~QuietTextEditBits;
    editor->SendScintilla(QsciScintillaBase::SCI_SETMODEVENTMASK,
                          static_cast<unsigned long>(quietMask));
    editor->SendScintilla(QsciScintillaBase::SCI_SETUNDOCOLLECTION, 0UL);
    editor->setUpdatesEnabled(false);

    // Allow the programmatic write, clear, then re-impose the user-visible
    // read-only state so the page stays non-editable between chunks.
    const bool wasReadOnly = editor->isReadOnly();
    if (wasReadOnly)
        editor->setReadOnly(false);
    editor->SendScintilla(QsciScintillaBase::SCI_CLEARALL);
    if (wasReadOnly)
        editor->setReadOnly(true);

    state->active = true;
    m_replacements.insert(editor, state);
    m_generations[editor] += 1;
    return true;
}

bool EditorContentReplacer::appendChunk(CustomEditor *editor, const char *data, qsizetype size)
{
    if (!editor || !isReplacing(editor))
        return false;
    if (size <= 0)
        return true;
    if (!data)
        return false;

    const bool wasReadOnly = editor->isReadOnly();
    if (wasReadOnly)
        editor->setReadOnly(false);
    editor->SendScintilla(QsciScintillaBase::SCI_APPENDTEXT,
                          static_cast<std::uintptr_t>(size), data);
    if (wasReadOnly)
        editor->setReadOnly(true);
    return true;
}

bool EditorContentReplacer::finishReplacement(CustomEditor *editor)
{
    if (!editor)
        return false;
    auto it = m_replacements.find(editor);
    if (it == m_replacements.end())
        return false;
    const auto state = it.value();
    m_replacements.erase(it);
    if (!state || !state->active)
        return false;
    state->active = false;

    if (CustomEditor *target = state->editor.data())
    {
        target->SendScintilla(QsciScintillaBase::SCI_SETUNDOCOLLECTION,
                              state->originalUndoCollection ? 1UL : 0UL);
        target->SendScintilla(QsciScintillaBase::SCI_EMPTYUNDOBUFFER);
        // Restore the mask before setting the save point so the save-point
        // notification is not dropped.
        target->SendScintilla(QsciScintillaBase::SCI_SETMODEVENTMASK,
                              static_cast<unsigned long>(state->originalModificationMask));
        target->SendScintilla(QsciScintillaBase::SCI_SETSAVEPOINT);
        target->setReadOnly(state->originalReadOnly);
        target->setUpdatesEnabled(state->originalUpdatesEnabled);
    }
    return true;
}

void EditorContentReplacer::abortReplacement(CustomEditor *editor)
{
    if (!editor)
        return;

    // Invalidate any queued continuation before dropping bookkeeping so an
    // in-flight chunked loop cannot write into a newer replacement.
    ++m_generations[editor];

    auto it = m_replacements.find(editor);
    if (it == m_replacements.end())
        return;
    const auto state = it.value();
    m_replacements.erase(it);
    if (!state || !state->active)
        return;
    state->active = false;

    if (CustomEditor *target = state->editor.data())
    {
        // Quietly drop the half-loaded content and leave the editor clean.
        const bool wasReadOnly = target->isReadOnly();
        if (wasReadOnly)
            target->setReadOnly(false);
        target->SendScintilla(QsciScintillaBase::SCI_CLEARALL);
        if (wasReadOnly)
            target->setReadOnly(true);
        target->SendScintilla(QsciScintillaBase::SCI_SETUNDOCOLLECTION,
                              state->originalUndoCollection ? 1UL : 0UL);
        target->SendScintilla(QsciScintillaBase::SCI_EMPTYUNDOBUFFER);
        target->SendScintilla(QsciScintillaBase::SCI_SETMODEVENTMASK,
                              static_cast<unsigned long>(state->originalModificationMask));
        target->SendScintilla(QsciScintillaBase::SCI_SETSAVEPOINT);
        target->setReadOnly(state->originalReadOnly);
        target->setUpdatesEnabled(state->originalUpdatesEnabled);
    }
}

void EditorContentReplacer::insertInChunks(CustomEditor *editor,
                                           const QByteArray &utf8Content,
                                           std::function<void()> onFinished)
{
    if (!editor)
        return;

    if (!beginReplacement(editor))
    {
        if (onFinished)
            onFinished();
        return;
    }

    if (utf8Content.size() <= ChunkedInsertBytes)
    {
        appendChunk(editor, utf8Content.constData(), utf8Content.size());
        finishReplacement(editor);
        if (onFinished)
            onFinished();
        return;
    }

    const quint64 generation = m_generations.value(editor);
    QPointer<EditorContentReplacer> self(this);
    QPointer<CustomEditor> guard(editor);
    CustomEditor *rawEditor = editor;
    auto offset = std::make_shared<qsizetype>(0);
    auto step = std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>> weakStep = step;
    *step = [self, guard, rawEditor, offset, utf8Content, onFinished, weakStep, generation]() {
        if (!self)
            return;
        if (!guard || !self->isReplacing(rawEditor)
            || self->m_generations.value(rawEditor) != generation)
        {
            return;
        }
        CustomEditor *target = guard.data();
        const qsizetype total = utf8Content.size();
        const qsizetype chunk = qMin(ChunkedInsertBytes, total - *offset);
        self->appendChunk(target, utf8Content.constData() + *offset, chunk);
        *offset += chunk;

        if (*offset < total)
        {
            if (auto strongStep = weakStep.lock())
                QTimer::singleShot(0, self.data(), [strongStep]() { (*strongStep)(); });
            return;
        }

        self->finishReplacement(target);
        if (onFinished)
            onFinished();
    };
    (*step)();
}

bool EditorContentReplacer::replaceWithEventLoopYield(CustomEditor *editor,
                                                      const QByteArray &utf8Content)
{
    if (!editor)
        return false;

    if (!beginReplacement(editor))
        return false;

    if (utf8Content.size() <= ChunkedInsertBytes)
    {
        appendChunk(editor, utf8Content.constData(), utf8Content.size());
        return finishReplacement(editor);
    }

    const quint64 generation = m_generations.value(editor);
    QPointer<CustomEditor> guard(editor);
    CustomEditor *rawEditor = editor;
    qsizetype offset = 0;
    while (offset < utf8Content.size())
    {
        if (!guard || !isReplacing(rawEditor)
            || m_generations.value(rawEditor) != generation)
        {
            return false;
        }
        const qsizetype chunk = qMin(ChunkedInsertBytes, utf8Content.size() - offset);
        appendChunk(editor, utf8Content.constData() + offset, chunk);
        offset += chunk;
        if (offset < utf8Content.size())
        {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            editor = guard.data();
            if (!editor)
                return false;
        }
    }
    return finishReplacement(editor);
}

bool EditorContentReplacer::isReplacing(CustomEditor *editor) const
{
    return editor && m_replacements.contains(editor);
}

void EditorContentReplacer::forgetEditor(CustomEditor *editor)
{
    m_replacements.remove(editor);
    m_generations.remove(editor);
}
