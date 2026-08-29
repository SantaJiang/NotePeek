#include "tool/cpp/headersourcemanager.h"

#include <QDir>
#include <QFileInfo>

bool HeaderSourceManager::isCppFile(const QString &filePath) const
{
    return isHeaderFile(filePath) || isSourceFile(filePath);
}

bool HeaderSourceManager::isHeaderFile(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    static QStringList headerExtensions = {
        "h", "hh", "hpp", "hxx", "h++", "inl"
    };
    
    return headerExtensions.contains(suffix);
}

bool HeaderSourceManager::isSourceFile(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    static QStringList sourceExtensions = {
        "cpp", "cc", "cxx", "c++", "c", "mm"
    };
    
    return sourceExtensions.contains(suffix);
}

QString HeaderSourceManager::getCorrespondingFile(const QString &filePath) const
{
    if (!isCppFile(filePath)) {
        return QString();
    }
    
    QFileInfo fileInfo(filePath);
    QString baseName = getBaseName(filePath);
    QString dirPath = fileInfo.absolutePath();
    
    QStringList possibleExtensions = getPossibleExtensions(filePath);
    
    // Look for the counterpart in the same directory
    for (const QString &ext : possibleExtensions) {
        QString testPath = QDir(dirPath).filePath(baseName + "." + ext);
        if (fileExists(testPath)) {
            return testPath;
        }
    }
    
    return QString();
}

bool HeaderSourceManager::hasCorrespondingFile(const QString &filePath) const
{
    return !getCorrespondingFile(filePath).isEmpty();
}

QStringList HeaderSourceManager::getSupportedCppExtensions()
{
    return QStringList() << "h" << "hh" << "hpp" << "hxx" << "h++"
                         << "cpp" << "cc" << "cxx" << "c++" << "c"
                         << "mm" << "inl";
}

QString HeaderSourceManager::getBaseName(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    
    int dotIndex = fileName.lastIndexOf('.');
    if (dotIndex != -1) {
        return fileName.left(dotIndex);
    }
    
    return fileName;
}

QStringList HeaderSourceManager::getPossibleExtensions(const QString &filePath) const
{
    if (isHeaderFile(filePath)) {
        // A header maps to source extensions
        return QStringList{
            "cpp", "cc", "cxx", "c++", "c",
            "mm"  // Objective-C++
        };
    } else if (isSourceFile(filePath)) {
        // A source file maps to header extensions
        return QStringList{
            "h", "hh", "hpp", "hxx", "h++", "inl"
        };
    }

    return QStringList();
}

bool HeaderSourceManager::fileExists(const QString &filePath) const
{
    return QFileInfo(filePath).exists();
}
