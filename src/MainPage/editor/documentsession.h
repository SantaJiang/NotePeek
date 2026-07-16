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
};

#endif // DOCUMENTSESSION_H
