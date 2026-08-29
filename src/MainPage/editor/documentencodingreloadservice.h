#ifndef DOCUMENTENCODINGRELOADSERVICE_H
#define DOCUMENTENCODINGRELOADSERVICE_H

#include "editor/documentformat.h"

#include <QByteArray>
#include <QObject>
#include <QString>

struct DocumentEncodingReloadRequest
{
    quint64 requestId = 0;
    QString filePath;
    DocumentEncoding encoding = DocumentEncoding::Utf8;
};

struct DocumentEncodingReloadResult
{
    quint64 requestId = 0;
    QString filePath;
    bool success = false;
    bool performanceDegraded = false;
    int detectedLanguage = 0;
    QByteArray content;
    DocumentFormat format;
    QString errorMessage;
};

// Performs encoding-specific disk reads and content classification without
// knowing about editor widgets or presentation policy.
class DocumentEncodingReloadService : public QObject
{
    Q_OBJECT

public:
    explicit DocumentEncodingReloadService(QObject *parent = nullptr);

    void reloadFileAsync(const DocumentEncodingReloadRequest &request);

signals:
    void fileReloaded(const DocumentEncodingReloadResult &result);

};

#endif // DOCUMENTENCODINGRELOADSERVICE_H
