// JSON syntax-theme model for the NotePeek QScintilla fork.
// Copyright (c) 2026 The NotePeek contributors.
// Distributed under the GNU General Public License version 3.0.

#include "NotePeek/syntaxtheme.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

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
        QStringLiteral(":/notepeek/themes/dark-modern.json"),
        QStringLiteral(":/notepeek/themes/midnight-blue.json"),
        QStringLiteral(":/notepeek/themes/graphite.json"),
        QStringLiteral(":/notepeek/themes/forest-night.json"),
        QStringLiteral(":/notepeek/themes/light-modern.json"),
        QStringLiteral(":/notepeek/themes/quiet-paper.json"),
        QStringLiteral(":/notepeek/themes/solar-light.json"),
        QStringLiteral(":/notepeek/themes/rose-dawn.json")
    };
    return paths;
}

} // namespace

namespace NotePeek {

bool SyntaxTheme::isValid() const
{
    if (id.isEmpty() || name.isEmpty())
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
            *error = QStringLiteral("theme is missing a required editor or token color");
        return SyntaxTheme();
    }
    if (error)
        error->clear();
    return theme;
}

QVector<SyntaxTheme> SyntaxThemeCatalog::builtInThemes()
{
    // Resources are immutable for the lifetime of the process. Parsing all
    // eight JSON files for every viewport refresh adds UI-thread work without
    // changing the result, so build the catalog once and share its Qt value
    // data on subsequent calls.
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
                loadedThemes.append(theme);
        }
        return loadedThemes;
    }();
    return themes;
}

QStringList SyntaxThemeCatalog::builtInThemeIds()
{
    QStringList ids;
    for (const SyntaxTheme &theme : builtInThemes())
        ids.append(theme.id);
    return ids;
}

QString SyntaxThemeCatalog::automaticThemeId(bool systemDark)
{
    return systemDark ? QStringLiteral("notepeek-dark-modern")
                      : QStringLiteral("notepeek-light-modern");
}

SyntaxTheme SyntaxThemeCatalog::theme(const QString &id, bool systemDark)
{
    const QString resolvedId = id.isEmpty() || id == QLatin1String("auto")
        ? automaticThemeId(systemDark) : id;
    const QVector<SyntaxTheme> themes = builtInThemes();
    for (const SyntaxTheme &theme : themes)
        if (theme.id == resolvedId)
            return theme;
    for (const SyntaxTheme &theme : themes)
        if (theme.id == automaticThemeId(systemDark))
            return theme;
    return SyntaxTheme();
}

} // namespace NotePeek
