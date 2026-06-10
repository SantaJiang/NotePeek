#ifndef DOCUMENTREGISTRY_H
#define DOCUMENTREGISTRY_H

#include <QList>
#include <QSet>
#include <QString>

class CustomEditor;

class DocumentRegistry
{
public:
    static QString normalizedFilePath(const QString &filePath);
    static CustomEditor *findEditorByFilePath(const QList<CustomEditor *> &editors, const QString &filePath);
    static QSet<QString> trackedFilePaths(const QList<CustomEditor *> &editors);
    static bool hasModifiedEditorForFile(const QList<CustomEditor *> &editors, const QString &filePath);
};

#endif // DOCUMENTREGISTRY_H
