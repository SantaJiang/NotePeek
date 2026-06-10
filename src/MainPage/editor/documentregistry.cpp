#include "documentregistry.h"

#include "customeditor.h"

#include <QDir>
#include <QFileInfo>

QString DocumentRegistry::normalizedFilePath(const QString &filePath)
{
    if (filePath.isEmpty())
    {
        return QString();
    }

    return QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
}

CustomEditor *DocumentRegistry::findEditorByFilePath(const QList<CustomEditor *> &editors, const QString &filePath)
{
    const QString normalizedPath = normalizedFilePath(filePath);
    if (normalizedPath.isEmpty())
    {
        return nullptr;
    }

    for (CustomEditor *editor : editors)
    {
        if (editor && normalizedFilePath(editor->filePath()) == normalizedPath)
        {
            return editor;
        }
    }

    return nullptr;
}

QSet<QString> DocumentRegistry::trackedFilePaths(const QList<CustomEditor *> &editors)
{
    QSet<QString> filePaths;
    for (CustomEditor *editor : editors)
    {
        const QString filePath = editor ? normalizedFilePath(editor->filePath()) : QString();
        if (!filePath.isEmpty())
        {
            filePaths.insert(filePath);
        }
    }
    return filePaths;
}

bool DocumentRegistry::hasModifiedEditorForFile(const QList<CustomEditor *> &editors, const QString &filePath)
{
    const QString normalizedPath = normalizedFilePath(filePath);
    if (normalizedPath.isEmpty())
    {
        return false;
    }

    for (CustomEditor *editor : editors)
    {
        if (editor && normalizedFilePath(editor->filePath()) == normalizedPath && editor->isModified())
        {
            return true;
        }
    }

    return false;
}
