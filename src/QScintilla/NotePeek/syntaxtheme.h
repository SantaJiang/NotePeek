// JSON syntax-theme model for the NotePeek QScintilla fork.
// Copyright (c) 2026 The NotePeek contributors.
// Distributed under the GNU General Public License version 3.0.

#ifndef NOTEPEEK_SYNTAXTHEME_H
#define NOTEPEEK_SYNTAXTHEME_H

#include <Qsci/qsciglobal.h>
#include <NotePeek/syntaxstyler.h>

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace NotePeek {

class SyntaxTheme
{
public:
    QString id;
    QString name;
    QString sourceRepository;
    QString sourceRevision;
    QString sourceLicense;
    QString filePath;
    bool builtIn = false;
    bool dark = false;
    QMap<QString, QColor> editorColors;
    SyntaxPalette syntaxColors;

    bool isValid() const;
    QColor editorColor(const QString &key, const QColor &fallback = QColor()) const;
};

class SyntaxThemeCatalog
{
public:
    static SyntaxTheme fromJson(const QByteArray &json, QString *error = nullptr);
    static QVector<SyntaxTheme> builtInThemes();
    static QVector<SyntaxTheme> themes();
    static QVector<SyntaxTheme> loadFromDirectory(const QString &directoryPath);
    static QStringList builtInThemeIds();
    static QStringList themeIds();
    static QString installationThemeDirectory();
    static QString userThemeDirectory();
    static void reload();
    static SyntaxTheme theme(const QString &id, bool systemDark);
    static QString automaticThemeId(bool systemDark);
};

} // namespace NotePeek

#endif // NOTEPEEK_SYNTAXTHEME_H
