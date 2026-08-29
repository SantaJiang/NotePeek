#ifndef DOCUMENTSAVETRANSACTIONSTORE_H
#define DOCUMENTSAVETRANSACTIONSTORE_H

#include "editor/documentformat.h"

#include <QHash>
#include <QPointer>
#include <QString>

class CustomEditor;
class DocumentSaveCoordinator;

// Owns save request identity and generation bookkeeping. UI reactions to a
// completed transaction deliberately remain in DocumentController.
class DocumentSaveTransactionStore
{
public:
    struct PendingSave
    {
        QPointer<CustomEditor> editor;
        QString oldFilePath;
        QString newFilePath;
        void *documentKey = nullptr;
        quint64 documentGeneration = 0;
        bool restoreFormatOnFailure = false;
        DocumentFormat formatAtDispatch;
    };

    struct AsyncRequest
    {
        quint64 requestId = 0;
        quintptr documentId = 0;
    };

    struct Completion
    {
        PendingSave pending;
        bool found = false;
        bool latestForEditor = false;
    };

    struct SynchronousTransaction
    {
        void *documentKey = nullptr;
        quint64 documentGeneration = 0;
        QString filePath;
    };

    explicit DocumentSaveTransactionStore(DocumentSaveCoordinator *coordinator);
    ~DocumentSaveTransactionStore();

    AsyncRequest beginAsync(CustomEditor *editor,
                            const QString &filePath,
                            const QString &oldFilePath,
                            const QString &newFilePath);
    Completion completeAsync(quint64 requestId);

    SynchronousTransaction beginSynchronous(CustomEditor *editor, const QString &filePath);
    void refreshAuthority(SynchronousTransaction *transaction);
    void finish(const SynchronousTransaction &transaction);

    QString effectivePath(CustomEditor *editor) const;
    bool hasPending(CustomEditor *editor) const;
    bool isEmpty() const { return m_pendingSaves.isEmpty(); }

private:
    static void *documentKey(CustomEditor *editor);

    DocumentSaveCoordinator *m_coordinator;
    quint64 m_nextRequestId = 1;
    QHash<quint64, PendingSave> m_pendingSaves;
};

#endif // DOCUMENTSAVETRANSACTIONSTORE_H
