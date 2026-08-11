#ifndef DOCUMENTSAVECOORDINATOR_H
#define DOCUMENTSAVECOORDINATOR_H

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QSet>

#include "editor/documentpersistenceservice.h"

class QEventLoop;

// Application-scoped owner of serialized file writes and shared-document save
// generations. Keeping this state explicit avoids process-lifetime singletons
// and lets tests create an isolated scheduler.
class DocumentSaveCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit DocumentSaveCoordinator(QObject *parent = nullptr);

    void enqueue(DocumentPersistenceService *receiver, const DocumentSaveRequest &request);
    void waitForPath(const QString &filePath);

    quint64 beginDocumentSave(void *documentKey);
    bool isLatestDocumentSave(void *documentKey, quint64 generation) const;
    bool hasOutstandingDocumentSave(void *documentKey) const;
    void finishDocumentSave(void *documentKey, quint64 generation);

private:
    struct Job
    {
        QPointer<DocumentPersistenceService> receiver;
        DocumentSaveRequest request;
    };

    void startNext(const QString &path);

    QHash<QString, QQueue<Job>> m_queues;
    QSet<QString> m_activePaths;
    QHash<QString, QList<QPointer<QEventLoop>>> m_waiters;
    QHash<void *, quint64> m_latestDocumentGenerations;
    quint64 m_nextGeneration = 1;
};

#endif // DOCUMENTSAVECOORDINATOR_H
