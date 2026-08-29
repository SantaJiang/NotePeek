#include "editor/documentsession.h"

void DocumentSession::setExternalConflict(bool conflict, bool fileDeleted)
{
    m_externalConflict = conflict;
    m_deletedExternally = conflict && fileDeleted;
    if (conflict)
        m_persistenceState = DocumentPersistenceState::ExternalConflict;
}

void DocumentSession::promoteToLargeFileMode(bool enabled)
{
    // Promotion is sticky. Re-enabling expensive editor services after a
    // temporary shrink can stall on the next undo or external reload.
    m_largeFileMode = m_largeFileMode || enabled;
}

void DocumentSession::beginSave()
{
    m_persistenceState = DocumentPersistenceState::Saving;
}

void DocumentSession::completeSave(bool modified)
{
    m_persistenceState = modified ? DocumentPersistenceState::Dirty
                                  : DocumentPersistenceState::Clean;
}

void DocumentSession::failSave()
{
    m_persistenceState = DocumentPersistenceState::SaveFailed;
}

void DocumentSession::markClean()
{
    m_persistenceState = DocumentPersistenceState::Clean;
}

void DocumentSession::markDirty()
{
    m_persistenceState = DocumentPersistenceState::Dirty;
}

void DocumentSession::updateForModification(bool modified)
{
    if (modified)
    {
        if (!m_externalConflict && m_persistenceState != DocumentPersistenceState::SaveFailed)
            markDirty();
    }
    else if (m_externalConflict)
    {
        // Reaching Scintilla's save point only means the in-memory buffer is
        // back at its last local snapshot. It does not resolve a disk change
        // that was detected afterwards.
        if (m_persistenceState != DocumentPersistenceState::Saving
            && m_persistenceState != DocumentPersistenceState::SaveFailed)
        {
            m_persistenceState = DocumentPersistenceState::ExternalConflict;
        }
    }
    else if (m_persistenceState != DocumentPersistenceState::Saving)
    {
        markClean();
    }
}

bool DocumentSession::isModified(bool editorModified) const
{
    return m_forcedModified || editorModified;
}

void DocumentSession::clearForcedModification()
{
    m_forcedModified = false;
}

bool DocumentSession::forceModified(bool editorModified)
{
    if (m_forcedModified)
        return false;
    m_forcedModified = true;
    return !editorModified;
}

void DocumentSession::beginEncodingConversion(DocumentEncoding encoding)
{
    if (!m_encodingConversionPending)
    {
        m_formatBeforeEncodingConversion = m_format;
        m_encodingConversionPending = true;
    }
    m_format.encoding = encoding;
}

void DocumentSession::finishEncodingConversion(bool success)
{
    if (!m_encodingConversionPending)
        return;
    if (!success)
        m_format = m_formatBeforeEncodingConversion;
    m_encodingConversionPending = false;
}

quint64 DocumentSession::beginEncodingReload()
{
    return ++m_encodingReloadGeneration;
}

bool DocumentSession::isCurrentEncodingReload(quint64 generation) const
{
    return m_encodingReloadGeneration == generation;
}
