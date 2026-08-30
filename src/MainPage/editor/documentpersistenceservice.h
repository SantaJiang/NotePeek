#ifndef DOCUMENTPERSISTENCESERVICE_H
#define DOCUMENTPERSISTENCESERVICE_H

#include <QObject>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QtGlobal>

#include "editor/documentformat.h"
#include "editor/documentstreamdecoder.h"

class DocumentSaveCoordinator;
class StreamingDocumentLoader;

struct DocumentLoadRequest
{
    quint64 requestId = 0;
    QString filePath;
};

struct DocumentLoadResult
{
    quint64 requestId = 0;
    QString filePath;
    QByteArray content;
    QString errorMessage;
    qint64 fileSize = 0;
    bool success = false;
    bool largeFile = false;
    bool abnormallyLongLine = false;
    int detectedLanguage = 0;
    DocumentFormat format;
};

struct DocumentSaveRequest
{
    quint64 requestId = 0;
    QString filePath;
    QByteArray utf8Content;
    quintptr documentId = 0;
};

struct DocumentSaveResult
{
    quint64 requestId = 0;
    QString filePath;
    QString errorMessage;
    bool success = false;
    bool superseded = false;
};

class DocumentPersistenceService : public QObject
{
    Q_OBJECT

public:
    explicit DocumentPersistenceService(DocumentSaveCoordinator *saveCoordinator,
                                        QObject *parent = nullptr);

    void loadFileAsync(const DocumentLoadRequest &request);
    void saveFileAsync(const DocumentSaveRequest &request);
    void waitForPendingSavePath(const QString &filePath);
    void cancelLoad(quint64 requestId);

signals:
    void fileLoaded(const DocumentLoadResult &result);
    void fileSaved(const DocumentSaveResult &result);
    // Streaming large-file load signals. Exactly one terminal signal
    // (streamFinished or streamFailed) is emitted per successful start.
    void streamChunkReady(quint64 requestId, quint64 sequence, const QByteArray &utf8);
    void streamFinished(quint64 requestId, const StreamDocumentMetadata &metadata);
    void streamFailed(quint64 requestId, const QString &errorMessage);

private:
    void startStreamingLoad(const DocumentLoadRequest &request);
    void finishStreamingLoad(quint64 requestId);

    DocumentSaveCoordinator *m_saveCoordinator;
    QHash<quint64, StreamingDocumentLoader *> m_streamingLoads;
};

#endif // DOCUMENTPERSISTENCESERVICE_H
