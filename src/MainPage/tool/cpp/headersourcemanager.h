#ifndef HEADERSOURCEMANAGER_H
#define HEADERSOURCEMANAGER_H

#include <QObject>
#include <QString>
#include <QFileInfo>
#include <QDir>

class HeaderSourceManager : public QObject
{
    Q_OBJECT

public:
    explicit HeaderSourceManager(QObject *parent = nullptr);

    // True if the file is a C++ header or source file
    bool isCppFile(const QString &filePath) const;

    bool isHeaderFile(const QString &filePath) const;

    bool isSourceFile(const QString &filePath) const;

    // Path of the matching header/source counterpart, or empty if none exists
    QString getCorrespondingFile(const QString &filePath) const;

    bool hasCorrespondingFile(const QString &filePath) const;

    static QStringList getSupportedCppExtensions();

private:
    // File name without its extension
    QString getBaseName(const QString &filePath) const;

    // Candidate extensions of the counterpart file (source for headers, header for sources)
    QStringList getPossibleExtensions(const QString &filePath) const;

    bool fileExists(const QString &filePath) const;
};

#endif // HEADERSOURCEMANAGER_H