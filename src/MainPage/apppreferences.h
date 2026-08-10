#ifndef APPPREFERENCES_H
#define APPPREFERENCES_H

#include <QFont>
#include <QString>

// Application-wide user preferences, persisted across restarts via QSettings.
//
// Stores editor fonts, startup/view preferences and code-editing feature
// switches.  A single shared instance is used everywhere; call load() once at
// startup and save() after mutating.
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

    // Syntax colour theme id exported by NotePeekScintilla. "auto" follows
    // the operating-system light/dark appearance.
    QString syntaxThemeId() const { return m_syntaxThemeId; }
    void setSyntaxThemeId(const QString &themeId);

    // Show the left file-tree dock when the application starts.  This only
    // seeds the initial state; the user's manual toggles during a session are
    // never overwritten by applying preferences.
    bool showFileTreeOnStartup() const { return m_showFileTreeOnStartup; }
    void setShowFileTreeOnStartup(bool show) { m_showFileTreeOnStartup = show; }

    // Show the editor information bar along the bottom of the main window.
    // Unlike the file-tree startup preference, this is applied immediately.
    bool showStatusBar() const { return m_showStatusBar; }
    void setShowStatusBar(bool show) { m_showStatusBar = show; }

    // When true, freshly opened Markdown files start in preview mode instead
    // of source mode.
    bool markdownDefaultPreview() const { return m_markdownDefaultPreview; }
    void setMarkdownDefaultPreview(bool preview) { m_markdownDefaultPreview = preview; }

    // Presentation used by the "untitled" document that has no file path yet.
    // Text mode wraps long lines and hides the line-number margin (the plain
    // text default); code mode keeps line numbers and horizontal scrolling.
    bool untitledUsesCodeMode() const { return m_untitledUsesCodeMode; }
    void setUntitledUsesCodeMode(bool useCodeMode) { m_untitledUsesCodeMode = useCodeMode; }

    // When true, typing replaces the character ahead of the caret instead of
    // inserting before it.  Applied to every open editor and to new ones.
    bool overwriteMode() const { return m_overwriteMode; }
    void setOverwriteMode(bool overwrite) { m_overwriteMode = overwrite; }

    bool codeFoldingEnabled() const { return m_codeFoldingEnabled; }
    void setCodeFoldingEnabled(bool enabled) { m_codeFoldingEnabled = enabled; }
    bool bracketAssistanceEnabled() const { return m_bracketAssistanceEnabled; }
    void setBracketAssistanceEnabled(bool enabled) { m_bracketAssistanceEnabled = enabled; }
    bool smartIndentationEnabled() const { return m_smartIndentationEnabled; }
    void setSmartIndentationEnabled(bool enabled) { m_smartIndentationEnabled = enabled; }
    bool documentCompletionEnabled() const { return m_documentCompletionEnabled; }
    void setDocumentCompletionEnabled(bool enabled) { m_documentCompletionEnabled = enabled; }
    bool semanticHighlightingEnabled() const { return m_semanticHighlightingEnabled; }
    void setSemanticHighlightingEnabled(bool enabled) { m_semanticHighlightingEnabled = enabled; }

    // UI language as a locale name ("en", "zh_CN"); empty follows the system
    // locale. Applied at the next launch.
    QString language() const { return m_language; }
    void setLanguage(const QString &language) { m_language = language; }

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
    QString m_syntaxThemeId;
    bool m_showFileTreeOnStartup;
    bool m_showStatusBar;
    bool m_markdownDefaultPreview;
    bool m_untitledUsesCodeMode;
    bool m_overwriteMode;
    bool m_codeFoldingEnabled;
    bool m_bracketAssistanceEnabled;
    bool m_smartIndentationEnabled;
    bool m_documentCompletionEnabled;
    bool m_semanticHighlightingEnabled;
    QString m_language;
};

#endif // APPPREFERENCES_H
