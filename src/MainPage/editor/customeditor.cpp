#include "editor/customeditor.h"
#include <QMimeData>
#include <QFrame>

CustomEditor::CustomEditor(QWidget *parent)
    : NotePeek::Editor{parent}
{
    setAcceptDrops(true);
    setFrameShape(QFrame::NoFrame);
    SendScintilla(QsciScintillaBase::SCI_SETMARGINLEFT, 0UL, 5L);
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
    m_documentSession->format = format;
}

DocumentFormat CustomEditor::documentFormat() const
{
    return m_documentSession->format;
}

void CustomEditor::setExternalConflict(bool conflict, bool fileDeleted)
{
    m_documentSession->externalConflict = conflict;
    m_documentSession->deletedExternally = conflict && fileDeleted;
    if (conflict)
        m_documentSession->persistenceState = DocumentPersistenceState::ExternalConflict;
}

bool CustomEditor::hasExternalConflict() const
{
    return m_documentSession->externalConflict;
}

bool CustomEditor::wasDeletedExternally() const
{
    return m_documentSession->deletedExternally;
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
    // Promotion is sticky.  Re-enabling expensive editor services after a
    // temporary shrink can immediately stall on the next undo or external
    // reload that restores the large buffer.
    m_documentSession->largeFileMode = m_documentSession->largeFileMode || enabled;
}

bool CustomEditor::isLargeFileMode() const
{
    return m_documentSession->largeFileMode;
}

void CustomEditor::setPersistenceState(DocumentPersistenceState state)
{
    m_documentSession->persistenceState = state;
}

DocumentPersistenceState CustomEditor::persistenceState() const
{
    return m_documentSession->persistenceState;
}

bool CustomEditor::isModified() const
{
    return m_documentSession->forcedModified || QsciScintilla::isModified();
}

void CustomEditor::setModified(bool modified)
{
    if (modified)
    {
        markModified();
        return;
    }

    m_documentSession->forcedModified = false;
    QsciScintilla::setModified(false);
}

void CustomEditor::markModified()
{
    if (m_documentSession->forcedModified)
        return;

    const bool wasModified = QsciScintilla::isModified();
    m_documentSession->forcedModified = true;
    if (!wasModified)
        emit modificationChanged(true);
}

void CustomEditor::beginEncodingConversion(DocumentEncoding encoding)
{
    if (!m_documentSession->encodingConversionPending)
    {
        m_documentSession->formatBeforeEncodingConversion = m_documentSession->format;
        m_documentSession->encodingConversionPending = true;
    }
    m_documentSession->format.encoding = encoding;
}

bool CustomEditor::hasPendingEncodingConversion() const
{
    return m_documentSession->encodingConversionPending;
}

void CustomEditor::finishEncodingConversion(bool success)
{
    if (!m_documentSession->encodingConversionPending)
        return;
    if (!success)
        m_documentSession->format = m_documentSession->formatBeforeEncodingConversion;
    m_documentSession->encodingConversionPending = false;
}

quint64 CustomEditor::beginEncodingReload()
{
    return ++m_documentSession->encodingReloadGeneration;
}

bool CustomEditor::isCurrentEncodingReload(quint64 generation) const
{
    return m_documentSession->encodingReloadGeneration == generation;
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
