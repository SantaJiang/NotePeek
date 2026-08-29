#ifndef DOCUMENTSESSION_H
#define DOCUMENTSESSION_H

#include "editor/documentformat.h"

enum class DocumentPersistenceState
{
    Clean,
    Dirty,
    Saving,
    SaveFailed,
    ExternalConflict
};

// UI-independent lifecycle state shared by every view of one Scintilla
// document. State transitions live here so split panes cannot independently
// interpret save, conflict, or encoding outcomes.
class DocumentSession
{
public:
    void setFormat(const DocumentFormat &format) { m_format = format; }
    DocumentFormat format() const { return m_format; }

    void setExternalConflict(bool conflict, bool fileDeleted = false);
    bool hasExternalConflict() const { return m_externalConflict; }
    bool wasDeletedExternally() const { return m_deletedExternally; }

    void promoteToLargeFileMode(bool enabled);
    bool isLargeFileMode() const { return m_largeFileMode; }

    void beginSave();
    void completeSave(bool modified);
    void failSave();
    void markClean();
    void markDirty();
    void updateForModification(bool modified);
    DocumentPersistenceState persistenceState() const { return m_persistenceState; }

    bool isModified(bool editorModified) const;
    void clearForcedModification();
    // Returns true when the editor must emit modificationChanged(true).
    bool forceModified(bool editorModified);

    void beginEncodingConversion(DocumentEncoding encoding);
    bool hasPendingEncodingConversion() const { return m_encodingConversionPending; }
    void finishEncodingConversion(bool success);
    quint64 beginEncodingReload();
    bool isCurrentEncodingReload(quint64 generation) const;

private:
    DocumentFormat m_format = DocumentFormatCodec::defaultFormat();
    DocumentPersistenceState m_persistenceState = DocumentPersistenceState::Clean;
    bool m_externalConflict = false;
    bool m_deletedExternally = false;
    // A failed save or external deletion must remain dirty even though
    // Scintilla's internal save point may already have been moved to the
    // snapshot that failed to reach disk.
    bool m_forcedModified = false;
    // Large-file mode is shared by every view of the Scintilla document and is
    // intentionally sticky for the lifetime of the session.  A document that
    // crossed the threshold must not silently re-enable lexing, wrapping or
    // completion after Save As, a theme refresh, or a preferences change.
    bool m_largeFileMode = false;
    bool m_encodingConversionPending = false;
    DocumentFormat m_formatBeforeEncodingConversion;
    quint64 m_encodingReloadGeneration = 0;
};

#endif // DOCUMENTSESSION_H
