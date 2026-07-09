#ifndef DOCUMENTPERSISTENCESERVICE_H
#define DOCUMENTPERSISTENCESERVICE_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QtGlobal>

class CustomEditor;
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
    QString content;
    QString errorMessage;
    qint64 fileSize = 0;
    bool success = false;
    bool largeFile = false;
};

struct DocumentSaveRequest
{
    quint64 requestId = 0;
    QString filePath;
    QByteArray utf8Content;
};

struct DocumentSaveResult
{
    quint64 requestId = 0;
    QString filePath;
    QString errorMessage;
    bool success = false;
};

class DocumentPersistenceService : public QObject
{
    Q_OBJECT

public:
    explicit DocumentPersistenceService(QObject *parent = nullptr);

    bool loadFile(const QString &filePath, QString *content, QString *errorMessage) const;
    bool saveEditor(CustomEditor *editor, const QString &filePath, QString *errorMessage) const;
    bool saveEditorSnapshot(CustomEditor *editor, const QString &filePath, QByteArray *utf8Content, QString *errorMessage) const;
    void loadFileAsync(const DocumentLoadRequest &request);
    void saveFileAsync(const DocumentSaveRequest &request);
    QString promptSaveFileName(QWidget *parent) const;
    QString displayNameForSavePrompt(CustomEditor *editor) const;

signals:
    void fileLoaded(const DocumentLoadResult &result);
    void fileSaved(const DocumentSaveResult &result);
};

#endif // DOCUMENTPERSISTENCESERVICE_H
