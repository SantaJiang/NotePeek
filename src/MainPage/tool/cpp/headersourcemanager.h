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

    // 检查文件是否是C++文件
    bool isCppFile(const QString &filePath) const;
    
    // 检查文件是否是头文件
    bool isHeaderFile(const QString &filePath) const;
    
    // 检查文件是否是源文件
    bool isSourceFile(const QString &filePath) const;
    
    // 获取对应的头文件/源文件路径
    QString getCorrespondingFile(const QString &filePath) const;
    
    // 检查对应的文件是否存在
    bool hasCorrespondingFile(const QString &filePath) const;
    
    // 获取支持的C++文件扩展名
    static QStringList getSupportedCppExtensions();

private:
    // 获取文件的基名（不带扩展名）
    QString getBaseName(const QString &filePath) const;
    
    // 获取可能的对应文件扩展名列表
    QStringList getPossibleExtensions(const QString &filePath) const;
    
    // 检查文件是否存在
    bool fileExists(const QString &filePath) const;
};

#endif // HEADERSOURCEMANAGER_H