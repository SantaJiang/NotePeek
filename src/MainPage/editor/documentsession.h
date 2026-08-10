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

struct DocumentSession
{
    DocumentFormat format = DocumentFormatCodec::defaultFormat();
    DocumentPersistenceState persistenceState = DocumentPersistenceState::Clean;
    bool externalConflict = false;
    bool deletedExternally = false;
    // A failed save or external deletion must remain dirty even though
    // Scintilla's internal save point may already have been moved to the
    // snapshot that failed to reach disk.
    bool forcedModified = false;
    // Large-file mode is shared by every view of the Scintilla document and is
    // intentionally sticky for the lifetime of the session.  A document that
    // crossed the threshold must not silently re-enable lexing, wrapping or
    // completion after Save As, a theme refresh, or a preferences change.
    bool largeFileMode = false;
    bool encodingConversionPending = false;
    DocumentFormat formatBeforeEncodingConversion;
    quint64 encodingReloadGeneration = 0;
};

#endif // DOCUMENTSESSION_H
