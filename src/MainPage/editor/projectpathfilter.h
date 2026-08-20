#ifndef PROJECTPATHFILTER_H
#define PROJECTPATHFILTER_H

#include <QList>
#include <QRegularExpression>
#include <QString>

// Shared project-tree exclusion policy. Built-in heavy directories are always
// ignored, and root .gitignore rules are applied in order.
class ProjectPathFilter
{
public:
    explicit ProjectPathFilter(const QString &rootDirectory = QString());

    bool ignores(const QString &absolutePath, bool directory) const;
    static bool isStandardIgnoredDirectoryName(const QString &name);

private:
    struct Rule
    {
        QRegularExpression expression;
        bool negated = false;
        bool directoryOnly = false;
    };

    void appendRule(const QString &pattern);
    QString m_rootDirectory;
    QList<Rule> m_rules;
};

#endif // PROJECTPATHFILTER_H
