#ifndef DOCUMENTPERSISTENCESERVICE_H
#define DOCUMENTPERSISTENCESERVICE_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include "editor/documentformat.h"

class CustomEditor;
class DocumentSaveCoordinator;
class QWidget;

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

    bool saveEditorSnapshot(CustomEditor *editor,
                            QByteArray *utf8Content,
                            QString *errorMessage) const;
    void loadFileAsync(const DocumentLoadRequest &request);
    void saveFileAsync(const DocumentSaveRequest &request);
    void waitForPendingSavePath(const QString &filePath);
    QString promptSaveFileName(QWidget *parent) const;
    QString displayNameForSavePrompt(CustomEditor *editor) const;

signals:
    void fileLoaded(const DocumentLoadResult &result);
    void fileSaved(const DocumentSaveResult &result);

private:
    DocumentSaveCoordinator *m_saveCoordinator;
};

#endif // DOCUMENTPERSISTENCESERVICE_H
