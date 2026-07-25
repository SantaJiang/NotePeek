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
    bool encodingConversionPending = false;
    DocumentFormat formatBeforeEncodingConversion;
    quint64 encodingReloadGeneration = 0;
};

#endif // DOCUMENTSESSION_H
