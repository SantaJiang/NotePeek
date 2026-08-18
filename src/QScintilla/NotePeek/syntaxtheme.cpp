// JSON syntax-theme model for the NotePeek QScintilla fork.
// Copyright (c) 2026 The NotePeek contributors.
// Distributed under the GNU General Public License version 3.0.

#include "NotePeek/syntaxtheme.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>

namespace {

using Role = NotePeek::SyntaxRole;

const QMap<QString, Role> &roleNames()
{
    static const QMap<QString, Role> names = {
        {QStringLiteral("default"), Role::Default}, {QStringLiteral("comment"), Role::Comment},
        {QStringLiteral("documentation"), Role::Documentation}, {QStringLiteral("keyword"), Role::Keyword},
        {QStringLiteral("type"), Role::Type}, {QStringLiteral("builtin"), Role::Builtin},
        {QStringLiteral("function"), Role::Function}, {QStringLiteral("variable"), Role::Variable},
        {QStringLiteral("property"), Role::Property}, {QStringLiteral("string"), Role::String},
        {QStringLiteral("escape"), Role::Escape}, {QStringLiteral("number"), Role::Number},
        {QStringLiteral("operator"), Role::Operator}, {QStringLiteral("preprocessor"), Role::Preprocessor},
        {QStringLiteral("regex"), Role::Regex}, {QStringLiteral("attribute"), Role::Attribute},
        {QStringLiteral("tag"), Role::Tag}, {QStringLiteral("link"), Role::Link},
        {QStringLiteral("added"), Role::Added}, {QStringLiteral("removed"), Role::Removed},
        {QStringLiteral("changed"), Role::Changed}, {QStringLiteral("error"), Role::Error}
    };
    return names;
}

const QStringList &requiredEditorColors()
{
    static const QStringList keys = {
        QStringLiteral("background"), QStringLiteral("foreground"),
        QStringLiteral("marginBackground"), QStringLiteral("marginForeground"),
        QStringLiteral("caret"), QStringLiteral("caretLine"),
        QStringLiteral("indentGuide"), QStringLiteral("foldMarker"),
        QStringLiteral("matchedBraceForeground"), QStringLiteral("matchedBraceBackground")
    };
    return keys;
}

QColor parseColor(const QJsonValue &value)
{
    const QColor color(value.toString());
    return color.isValid() ? color : QColor();
}

const QStringList &resourcePaths()
{
    static const QStringList paths = {
        QStringLiteral(":/notepeek/themes/vscode-dark-2026.json"),
        QStringLiteral(":/notepeek/themes/vscode-dark-plus.json"),
        QStringLiteral(":/notepeek/themes/rose-pine.json"),
        QStringLiteral(":/notepeek/themes/ayu-dark.json"),
        QStringLiteral(":/notepeek/themes/qtcreator-dark-2024.json"),
        QStringLiteral(":/notepeek/themes/vscode-light-2026.json"),
        QStringLiteral(":/notepeek/themes/vscode-light-plus.json"),
        QStringLiteral(":/notepeek/themes/vscode-quiet-light.json"),
        QStringLiteral(":/notepeek/themes/rose-pine-dawn.json"),
        QStringLiteral(":/notepeek/themes/ayu-light.json"),
    };
    return paths;
}

QVector<NotePeek::SyntaxTheme> &loadedThemes()
{
    static QVector<NotePeek::SyntaxTheme> themes;
    return themes;
}

bool &catalogLoaded()
{
    static bool loaded = false;
    return loaded;
}

QMutex &catalogMutex()
{
    static QMutex mutex;
    return mutex;
}

void mergeTheme(QVector<NotePeek::SyntaxTheme> *themes,
                const NotePeek::SyntaxTheme &candidate)
{
    for (NotePeek::SyntaxTheme &theme : *themes)
    {
        if (theme.id == candidate.id)
        {
            if (theme.dark == candidate.dark)
                theme = candidate;
            else
                qWarning() << "Ignoring syntax theme with conflicting appearance"
                           << candidate.filePath;
            return;
        }
    }
    themes->append(candidate);
}

} // namespace

namespace NotePeek {

bool SyntaxTheme::isValid() const
{
    if (id.isEmpty() || name.isEmpty() || sourceRepository.isEmpty()
        || sourceRevision.isEmpty() || sourceLicense.isEmpty())
        return false;
    for (const QString &key : requiredEditorColors())
        if (!editorColors.value(key).isValid())
            return false;
    for (const Role role : roleNames())
        if (!syntaxColors.value(role).isValid())
            return false;
    return true;
}

QColor SyntaxTheme::editorColor(const QString &key, const QColor &fallback) const
{
    return editorColors.value(key, fallback);
}

SyntaxTheme SyntaxThemeCatalog::fromJson(const QByteArray &json, QString *error)
{
    SyntaxTheme theme;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (error)
            *error = parseError.errorString();
        return theme;
    }

    const QJsonObject root = document.object();
    theme.id = root.value(QStringLiteral("id")).toString().trimmed();
    theme.name = root.value(QStringLiteral("name")).toString().trimmed();
    const QJsonObject source = root.value(QStringLiteral("source")).toObject();
    theme.sourceRepository = source.value(QStringLiteral("repository")).toString().trimmed();
    theme.sourceRevision = source.value(QStringLiteral("commit")).toString().trimmed();
    theme.sourceLicense = source.value(QStringLiteral("license")).toString().trimmed();
    const QString appearance = root.value(QStringLiteral("appearance")).toString();
    if (appearance != QLatin1String("dark") && appearance != QLatin1String("light"))
    {
        if (error)
            *error = QStringLiteral("appearance must be 'dark' or 'light'");
        return SyntaxTheme();
    }
    theme.dark = appearance == QLatin1String("dark");

    const QJsonObject editor = root.value(QStringLiteral("editor")).toObject();
    for (auto it = editor.begin(); it != editor.end(); ++it)
    {
        const QColor color = parseColor(it.value());
        if (color.isValid())
            theme.editorColors.insert(it.key(), color);
    }

    const QJsonObject tokens = root.value(QStringLiteral("tokens")).toObject();
    for (auto it = roleNames().cbegin(); it != roleNames().cend(); ++it)
    {
        const QColor color = parseColor(tokens.value(it.key()));
        if (color.isValid())
            theme.syntaxColors.insert(it.value(), color);
    }

    if (!theme.isValid())
    {
        if (error)
            *error = QStringLiteral("theme is missing provenance or a required editor or token color");
        return SyntaxTheme();
    }
    if (error)
        error->clear();
    return theme;
}

QVector<SyntaxTheme> SyntaxThemeCatalog::builtInThemes()
{
    // Resources are immutable for the lifetime of the process. Parse all ten
    // JSON files once rather than repeating that work on viewport refreshes,
    // then share the resulting Qt value data on subsequent calls.
    static const QVector<SyntaxTheme> themes = []() {
        QVector<SyntaxTheme> loadedThemes;
        for (const QString &path : resourcePaths())
        {
            QFile file(path);
            if (!file.open(QFile::ReadOnly))
                continue;
            QString error;
            SyntaxTheme theme = fromJson(file.readAll(), &error);
            if (theme.isValid())
            {
                theme.filePath = path;
                theme.builtIn = true;
                loadedThemes.append(theme);
            }
        }
        return loadedThemes;
    }();
    return themes;
}

QVector<SyntaxTheme> SyntaxThemeCatalog::loadFromDirectory(const QString &directoryPath)
{
    QVector<SyntaxTheme> loaded;
    const QDir directory(directoryPath);
    const QStringList files = directory.entryList({QStringLiteral("*.json")},
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Name | QDir::IgnoreCase);
    for (const QString &fileName : files)
    {
        const QString path = directory.filePath(fileName);
        QFile file(path);
        if (!file.open(QFile::ReadOnly))
        {
            qWarning() << "Could not read syntax theme" << path;
            continue;
        }
        QString error;
        SyntaxTheme theme = fromJson(file.readAll(), &error);
        if (!theme.isValid())
        {
            qWarning() << "Ignoring invalid syntax theme" << path << error;
            continue;
        }
        theme.filePath = path;
        loaded.append(theme);
    }
    return loaded;
}

QString SyntaxThemeCatalog::installationThemeDirectory()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("themes"));
}

QString SyntaxThemeCatalog::userThemeDirectory()
{
    const QString dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(dataDirectory).filePath(QStringLiteral("themes"));
}

void SyntaxThemeCatalog::reload()
{
    QVector<SyntaxTheme> refreshed = builtInThemes();
    const QStringList directories = {
        installationThemeDirectory(),
        userThemeDirectory()
    };
    for (const QString &directory : directories)
    {
        for (const SyntaxTheme &theme : loadFromDirectory(directory))
            mergeTheme(&refreshed, theme);
    }
    const QMutexLocker<QMutex> locker(&catalogMutex());
    loadedThemes() = refreshed;
    catalogLoaded() = true;
}

QVector<SyntaxTheme> SyntaxThemeCatalog::themes()
{
    {
        const QMutexLocker<QMutex> locker(&catalogMutex());
        if (catalogLoaded())
            return loadedThemes();
    }
    reload();
    const QMutexLocker<QMutex> locker(&catalogMutex());
    return loadedThemes();
}

QStringList SyntaxThemeCatalog::builtInThemeIds()
{
    QStringList ids;
    for (const SyntaxTheme &theme : builtInThemes())
        ids.append(theme.id);
    return ids;
}

QStringList SyntaxThemeCatalog::themeIds()
{
    QStringList ids;
    for (const SyntaxTheme &theme : themes())
        ids.append(theme.id);
    return ids;
}

QString SyntaxThemeCatalog::automaticThemeId(bool systemDark)
{
    return systemDark ? QStringLiteral("upstream-vscode-dark-2026")
                      : QStringLiteral("upstream-vscode-light-2026");
}

SyntaxTheme SyntaxThemeCatalog::theme(const QString &id, bool systemDark)
{
    const QString resolvedId = id.isEmpty() || id == QLatin1String("auto")
        ? automaticThemeId(systemDark) : id;
    const QVector<SyntaxTheme> availableThemes = themes();
    for (const SyntaxTheme &theme : availableThemes)
        if (theme.id == resolvedId)
            return theme;
    for (const SyntaxTheme &theme : availableThemes)
        if (theme.id == automaticThemeId(systemDark))
            return theme;
    return SyntaxTheme();
}

} // namespace NotePeek
