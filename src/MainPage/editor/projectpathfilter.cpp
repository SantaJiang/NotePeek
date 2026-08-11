#include "editor/projectpathfilter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

ProjectPathFilter::ProjectPathFilter(const QString &rootDirectory,
                                     const QStringList &extraExcludeGlobs)
    : m_rootDirectory(QDir::cleanPath(QFileInfo(rootDirectory).absoluteFilePath()))
{
    QFile ignoreFile(QDir(m_rootDirectory).filePath(QStringLiteral(".gitignore")));
    if (ignoreFile.open(QFile::ReadOnly | QFile::Text))
    {
        while (!ignoreFile.atEnd())
            appendRule(QString::fromUtf8(ignoreFile.readLine()).trimmed(), true);
    }
    for (const QString &pattern : extraExcludeGlobs)
        appendRule(pattern.trimmed(), false);
}

bool ProjectPathFilter::isStandardIgnoredDirectoryName(const QString &name)
{
    static const QSet<QString> names = {
        QStringLiteral(".git"), QStringLiteral(".svn"), QStringLiteral(".hg"),
        QStringLiteral("build"), QStringLiteral("node_modules"),
        QStringLiteral("DerivedData"), QStringLiteral("__pycache__")
    };
    return names.contains(name) || name.startsWith(QStringLiteral("cmake-build-"));
}

bool ProjectPathFilter::ignores(const QString &absolutePath, bool directory) const
{
    const QFileInfo info(absolutePath);
    if (directory && isStandardIgnoredDirectoryName(info.fileName()))
        return true;

    QString relative = QDir(m_rootDirectory).relativeFilePath(info.absoluteFilePath());
    relative = QDir::fromNativeSeparators(QDir::cleanPath(relative));
    if (relative == QLatin1String(".") || relative.startsWith(QStringLiteral("../")))
        return false;

    bool ignored = false;
    const QString matchPath = directory ? relative + QLatin1Char('/') : relative;
    for (const Rule &rule : m_rules)
    {
        if ((!rule.directoryOnly || directory)
            && rule.expression.match(matchPath).hasMatch())
        {
            ignored = !rule.negated;
        }
    }
    return ignored;
}

void ProjectPathFilter::appendRule(const QString &rawPattern, bool allowNegation)
{
    QString pattern = QDir::fromNativeSeparators(rawPattern);
    if (pattern.isEmpty() || pattern.startsWith(QLatin1Char('#')))
        return;

    Rule rule;
    if (allowNegation && pattern.startsWith(QLatin1Char('!')))
    {
        rule.negated = true;
        pattern.remove(0, 1);
    }
    if (pattern.isEmpty())
        return;
    rule.directoryOnly = pattern.endsWith(QLatin1Char('/'));
    if (rule.directoryOnly)
        pattern.chop(1);

    const bool anchored = pattern.startsWith(QLatin1Char('/'));
    if (anchored)
        pattern.remove(0, 1);
    QString expression = QRegularExpression::wildcardToRegularExpression(pattern);
    expression.remove(0, 2); // remove \A
    expression.chop(2);      // remove \z
    const QString prefix = anchored || pattern.contains(QLatin1Char('/'))
                               ? QStringLiteral("^")
                               : QStringLiteral("^(?:.*/)?");
    // Git patterns without a trailing slash still match directories. The
    // optional descendant suffix makes both `vendor` and `vendor/` prune the
    // directory during traversal, while files named `vendor` continue to
    // match normally.
    const QString suffix = QStringLiteral("(?:/.*)?$");
    rule.expression = QRegularExpression(prefix + expression + suffix);
    if (rule.expression.isValid())
        m_rules.append(rule);
}
