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

} // namespace

EditorContentReplacer::EditorContentReplacer(QObject *parent)
    : QObject(parent)
{
}

void EditorContentReplacer::replaceImmediately(CustomEditor *editor,
                                               const QByteArray &utf8Content)
{
    if (!editor)
        return;

    const bool wasReadOnly = editor->isReadOnly();
    if (wasReadOnly)
        editor->setReadOnly(false);
    editor->SendScintilla(QsciScintillaBase::SCI_SETUNDOCOLLECTION, 0UL);
    editor->SendScintilla(QsciScintillaBase::SCI_CLEARALL);
    if (!utf8Content.isEmpty())
    {
        editor->SendScintilla(QsciScintillaBase::SCI_ADDTEXT,
                              static_cast<std::uintptr_t>(utf8Content.size()),
                              utf8Content.constData());
    }
    editor->SendScintilla(QsciScintillaBase::SCI_SETUNDOCOLLECTION, 1UL);
    editor->SendScintilla(QsciScintillaBase::SCI_EMPTYUNDOBUFFER);
    editor->SendScintilla(QsciScintillaBase::SCI_SETSAVEPOINT);
    if (wasReadOnly)
        editor->setReadOnly(true);
}

void EditorContentReplacer::insertInChunks(CustomEditor *editor,
                                           const QByteArray &utf8Content,
                                           std::function<void()> onFinished)
{
    if (!editor)
        return;

    if (utf8Content.size() <= ChunkedInsertBytes)
    {
        replaceImmediately(editor, utf8Content);
        if (onFinished)
            onFinished();
        return;
    }

    const bool wasReadOnly = editor->isReadOnly();
    editor->setReadOnly(false);
    editor->SendScintilla(QsciScintillaBase::SCI_SETUNDOCOLLECTION, 0UL);
    editor->SendScintilla(QsciScintillaBase::SCI_CLEARALL);
    editor->setReadOnly(wasReadOnly);
    m_editorsBeingReplaced.insert(editor);

    QPointer<EditorContentReplacer> self(this);
    QPointer<CustomEditor> guard(editor);
    CustomEditor *rawEditor = editor;
    auto offset = std::make_shared<qsizetype>(0);
    auto step = std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>> weakStep = step;
    *step = [self, guard, rawEditor, offset, utf8Content, onFinished, weakStep]() {
        if (!self)
            return;
        if (!guard)
        {
            self->m_editorsBeingReplaced.remove(rawEditor);
            return;
        }

        CustomEditor *target = guard.data();
        const qsizetype total = utf8Content.size();
        const qsizetype chunk = qMin(ChunkedInsertBytes, total - *offset);
        const bool readOnly = target->isReadOnly();
        target->setReadOnly(false);
        target->SendScintilla(QsciScintillaBase::SCI_APPENDTEXT,
                              static_cast<std::uintptr_t>(chunk),
                              utf8Content.constData() + *offset);
        target->setReadOnly(readOnly);
        *offset += chunk;

        if (*offset < total)
        {
            if (auto strongStep = weakStep.lock())
                QTimer::singleShot(0, self.data(), [strongStep]() { (*strongStep)(); });
            return;
        }

        self->m_editorsBeingReplaced.remove(rawEditor);
        target->SendScintilla(QsciScintillaBase::SCI_SETUNDOCOLLECTION, 1UL);
        target->SendScintilla(QsciScintillaBase::SCI_EMPTYUNDOBUFFER);
        target->SendScintilla(QsciScintillaBase::SCI_SETSAVEPOINT);
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

    if (utf8Content.size() <= ChunkedInsertBytes)
    {
        replaceImmediately(editor, utf8Content);
        return true;
    }

    const bool wasReadOnly = editor->isReadOnly();
    const bool updatesWereEnabled = editor->updatesEnabled();
    QPointer<CustomEditor> guard(editor);
    CustomEditor *rawEditor = editor;

    m_editorsBeingReplaced.insert(editor);
    editor->setUpdatesEnabled(false);
    editor->setReadOnly(false);
    editor->SendScintilla(QsciScintillaBase::SCI_SETUNDOCOLLECTION, 0UL);
    editor->SendScintilla(QsciScintillaBase::SCI_CLEARALL);

    qsizetype offset = 0;
    while (offset < utf8Content.size())
    {
        const qsizetype chunk = qMin(ChunkedInsertBytes, utf8Content.size() - offset);
        editor->SendScintilla(QsciScintillaBase::SCI_APPENDTEXT,
                              static_cast<std::uintptr_t>(chunk),
                              utf8Content.constData() + offset);
        offset += chunk;
        if (offset < utf8Content.size())
        {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            if (!guard)
            {
                m_editorsBeingReplaced.remove(rawEditor);
                return false;
            }
            editor = guard.data();
        }
    }

    editor->SendScintilla(QsciScintillaBase::SCI_SETUNDOCOLLECTION, 1UL);
    editor->SendScintilla(QsciScintillaBase::SCI_EMPTYUNDOBUFFER);
    editor->SendScintilla(QsciScintillaBase::SCI_SETSAVEPOINT);
    editor->setReadOnly(wasReadOnly);
    editor->setUpdatesEnabled(updatesWereEnabled);
    m_editorsBeingReplaced.remove(rawEditor);
    return true;
}

bool EditorContentReplacer::isReplacing(CustomEditor *editor) const
{
    return editor && m_editorsBeingReplaced.contains(editor);
}

void EditorContentReplacer::forgetEditor(CustomEditor *editor)
{
    m_editorsBeingReplaced.remove(editor);
}
