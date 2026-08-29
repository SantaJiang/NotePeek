#include "editor/documentsavetransactionstore.h"

#include "editor/customeditor.h"
#include "editor/documentsavecoordinator.h"

DocumentSaveTransactionStore::DocumentSaveTransactionStore(DocumentSaveCoordinator *coordinator)
    : m_coordinator(coordinator)
{
    Q_ASSERT(m_coordinator);
}

DocumentSaveTransactionStore::~DocumentSaveTransactionStore() = default;

DocumentSaveTransactionStore::AsyncRequest DocumentSaveTransactionStore::beginAsync(
    CustomEditor *editor,
    const QString &filePath,
    const QString &oldFilePath,
    const QString &newFilePath)
{
    const quint64 requestId = m_nextRequestId++;
    void *key = documentKey(editor);

    PendingSave pending;
    pending.editor = editor;
    pending.oldFilePath = oldFilePath;
    pending.newFilePath = newFilePath;
    pending.documentKey = key;
    pending.documentGeneration = m_coordinator->beginDocumentSave(key, filePath);
    pending.restoreFormatOnFailure = editor->hasPendingEncodingConversion();
    if (pending.restoreFormatOnFailure)
        pending.formatAtDispatch = editor->documentFormat();

    m_pendingSaves.insert(requestId, pending);
    return { requestId, reinterpret_cast<quintptr>(key) };
}

DocumentSaveTransactionStore::Completion DocumentSaveTransactionStore::completeAsync(
    quint64 requestId)
{
    auto it = m_pendingSaves.find(requestId);
    if (it == m_pendingSaves.end())
        return {};

    Completion completion;
    completion.pending = it.value();
    completion.found = true;
    completion.latestForEditor = completion.pending.editor
                                 && m_coordinator->isLatestDocumentSave(
                                     completion.pending.documentKey,
                                     completion.pending.documentGeneration);
    m_pendingSaves.erase(it);
    m_coordinator->finishDocumentSave(completion.pending.documentKey,
                                      completion.pending.documentGeneration);
    return completion;
}

DocumentSaveTransactionStore::SynchronousTransaction
DocumentSaveTransactionStore::beginSynchronous(CustomEditor *editor, const QString &filePath)
{
    SynchronousTransaction transaction;
    transaction.documentKey = documentKey(editor);
    transaction.filePath = filePath;
    transaction.documentGeneration = m_coordinator->beginDocumentSave(transaction.documentKey,
                                                                       filePath);
    return transaction;
}

void DocumentSaveTransactionStore::refreshAuthority(SynchronousTransaction *transaction)
{
    if (!transaction
        || m_coordinator->isLatestDocumentSave(transaction->documentKey,
                                               transaction->documentGeneration))
    {
        return;
    }

    transaction->documentGeneration = m_coordinator->beginDocumentSave(transaction->documentKey,
                                                                        transaction->filePath);
}

void DocumentSaveTransactionStore::finish(const SynchronousTransaction &transaction)
{
    m_coordinator->finishDocumentSave(transaction.documentKey,
                                      transaction.documentGeneration);
}

QString DocumentSaveTransactionStore::effectivePath(CustomEditor *editor) const
{
    if (!editor)
        return QString();

    const QString pendingPath = m_coordinator->latestDocumentSavePath(documentKey(editor));
    return pendingPath.isEmpty() ? editor->filePath() : pendingPath;
}

bool DocumentSaveTransactionStore::hasPending(CustomEditor *editor) const
{
    if (!editor)
        return false;

    for (auto it = m_pendingSaves.cbegin(); it != m_pendingSaves.cend(); ++it)
    {
        if (it.value().editor == editor)
            return true;
    }
    return false;
}

void *DocumentSaveTransactionStore::documentKey(CustomEditor *editor)
{
    if (!editor)
        return nullptr;

    void *key = editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER);
    return key ? key : editor;
}
