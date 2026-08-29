#include "editor/customeditor.h"
#include <QFrame>
#include <QMimeData>

namespace {
constexpr int EditorContentPadding = 5;
}

CustomEditor::CustomEditor(QWidget *parent)
    : NotePeek::Editor{parent}
{
    setAcceptDrops(true);
    setFrameShape(QFrame::NoFrame);
    SendScintilla(QsciScintillaBase::SCI_SETMARGINLEFT, 0UL, EditorContentPadding);
}

void CustomEditor::setFilePath(const QString &path)
{
    m_filePath = path;
}

QString CustomEditor::filePath() const
{
    return m_filePath;
}

void CustomEditor::setDocumentFormat(const DocumentFormat &format)
{
    m_documentSession->setFormat(format);
}

DocumentFormat CustomEditor::documentFormat() const
{
    return m_documentSession->format();
}

void CustomEditor::setExternalConflict(bool conflict, bool fileDeleted)
{
    m_documentSession->setExternalConflict(conflict, fileDeleted);
}

bool CustomEditor::hasExternalConflict() const
{
    return m_documentSession->hasExternalConflict();
}

bool CustomEditor::wasDeletedExternally() const
{
    return m_documentSession->wasDeletedExternally();
}

void CustomEditor::shareDocumentSessionFrom(const CustomEditor *source)
{
    if (source)
        m_documentSession = source->m_documentSession;
}

bool CustomEditor::sharesDocumentSessionWith(const CustomEditor *other) const
{
    return other && m_documentSession == other->m_documentSession;
}

void CustomEditor::setLargeFileMode(bool enabled)
{
    m_documentSession->promoteToLargeFileMode(enabled);
}

bool CustomEditor::isLargeFileMode() const
{
    return m_documentSession->isLargeFileMode();
}

void CustomEditor::beginSave()
{
    m_documentSession->beginSave();
}

void CustomEditor::completeSave()
{
    m_documentSession->completeSave(isModified());
}

void CustomEditor::failSave()
{
    m_documentSession->failSave();
}

void CustomEditor::markClean()
{
    m_documentSession->markClean();
}

void CustomEditor::markDirty()
{
    m_documentSession->markDirty();
}

void CustomEditor::updatePersistenceForModification()
{
    m_documentSession->updateForModification(isModified());
}

DocumentPersistenceState CustomEditor::persistenceState() const
{
    return m_documentSession->persistenceState();
}

bool CustomEditor::isModified() const
{
    return m_documentSession->isModified(QsciScintilla::isModified());
}

void CustomEditor::setModified(bool modified)
{
    if (modified)
    {
        markModified();
        return;
    }

    m_documentSession->clearForcedModification();
    QsciScintilla::setModified(false);
}

void CustomEditor::markModified()
{
    const bool wasModified = QsciScintilla::isModified();
    if (m_documentSession->forceModified(wasModified))
        emit modificationChanged(true);
}

void CustomEditor::beginEncodingConversion(DocumentEncoding encoding)
{
    m_documentSession->beginEncodingConversion(encoding);
}

bool CustomEditor::hasPendingEncodingConversion() const
{
    return m_documentSession->hasPendingEncodingConversion();
}

void CustomEditor::finishEncodingConversion(bool success)
{
    m_documentSession->finishEncodingConversion(success);
}

quint64 CustomEditor::beginEncodingReload()
{
    return m_documentSession->beginEncodingReload();
}

bool CustomEditor::isCurrentEncodingReload(quint64 generation) const
{
    return m_documentSession->isCurrentEncodingReload(generation);
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
