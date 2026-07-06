#ifndef APPPREFERENCES_H
#define APPPREFERENCES_H

#include <QFont>
#include <QString>

// Application-wide user preferences, persisted across restarts via QSettings.
//
// Exposes two editor fonts (a "text" font used for prose files and a "code"
// font used for source files), the startup visibility of the file-tree dock,
// and the default view mode for Markdown files.  A single shared instance is
// used everywhere; call load() once at startup and save() after mutating.
class AppPreferences
{
public:
    static AppPreferences &instance();

    void load();
    void save() const;

    // Editor fonts.
    QFont textFont() const;
    QFont codeFont() const;
    QString textFontFamily() const { return m_textFontFamily; }
    int textFontSize() const { return m_textFontSize; }
    QString codeFontFamily() const { return m_codeFontFamily; }
    int codeFontSize() const { return m_codeFontSize; }

    void setTextFont(const QString &family, int pointSize);
    void setCodeFont(const QString &family, int pointSize);

    // Show the left file-tree dock when the application starts.  This only
    // seeds the initial state; the user's manual toggles during a session are
    // never overwritten by applying preferences.
    bool showFileTreeOnStartup() const { return m_showFileTreeOnStartup; }
    void setShowFileTreeOnStartup(bool show) { m_showFileTreeOnStartup = show; }

    // When true, freshly opened Markdown files start in preview mode instead
    // of source mode.
    bool markdownDefaultPreview() const { return m_markdownDefaultPreview; }
    void setMarkdownDefaultPreview(bool preview) { m_markdownDefaultPreview = preview; }

    // Sensible cross-platform defaults, honouring the fonts bundled with the
    // application (Source Code Pro, Fira Code) before falling back to a
    // platform monospace family.
    static QString defaultCodeFontFamily();
    static QString defaultTextFontFamily();
    static int defaultFontSize();

private:
    AppPreferences();
    AppPreferences(const AppPreferences &) = delete;
    AppPreferences &operator=(const AppPreferences &) = delete;

    QString m_textFontFamily;
    int m_textFontSize;
    QString m_codeFontFamily;
    int m_codeFontSize;
    bool m_showFileTreeOnStartup;
    bool m_markdownDefaultPreview;
};

#endif // APPPREFERENCES_H
