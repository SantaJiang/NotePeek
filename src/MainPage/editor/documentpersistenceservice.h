#ifndef DOCUMENTPERSISTENCESERVICE_H
#define DOCUMENTPERSISTENCESERVICE_H

#include <QObject>
#include <QString>

class CustomEditor;
class QWidget;

class DocumentPersistenceService : public QObject
{
    Q_OBJECT

public:
    explicit DocumentPersistenceService(QObject *parent = nullptr);

    bool loadFile(const QString &filePath, QString *content, QString *errorMessage) const;
    bool saveEditor(CustomEditor *editor, const QString &filePath, QString *errorMessage) const;
    QString promptSaveFileName(QWidget *parent) const;
    QString displayNameForSavePrompt(CustomEditor *editor) const;
};

#endif // DOCUMENTPERSISTENCESERVICE_H
