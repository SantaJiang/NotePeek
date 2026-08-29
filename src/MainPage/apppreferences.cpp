#include "apppreferences.h"
#include "features/settings/settingsrepository.h"

#include <QFontDatabase>
#include <QFontMetrics>
#include <QtGlobal>
#include <NotePeek/syntaxtheme.h>

#include <utility>

namespace {

constexpr int kMinFontSize = 6;
constexpr int kMaxFontSize = 288;

bool familyInstalled(const QString &family)
{
    return QFontDatabase::hasFamily(family);
}

// The first font family from candidates that is actually installed.  The
// bundled families (loaded in main.cpp) are preferred so the application looks
// the same everywhere before the user picks anything.
QString firstAvailableFamily(const QStringList &candidates, const QString &fallback)
{
    for (const QString &family : candidates)
    {
        if (familyInstalled(family))
        {
            return family;
        }
    }
    return fallback;
}

int clampFontSize(int size)
{
    if (size < kMinFontSize || size > kMaxFontSize)
    {
        return AppPreferences::defaultFontSize();
    }
    return size;
}

QString validatedThemeId(const QString &themeId, bool dark, const QString &fallback)
{
    const NotePeek::SyntaxTheme theme = NotePeek::SyntaxThemeCatalog::theme(themeId, dark);
    return theme.id == themeId && theme.dark == dark ? themeId : fallback;
}

// Validate a persisted family: fall back to the default when the stored family
// is empty or no longer installed on this machine.
QString validatedFamily(const QString &family, const QString &fallback)
{
    if (family.isEmpty() || !familyInstalled(family))
    {
        return fallback;
    }
    return family;
}

} // namespace

AppPreferences &AppPreferences::instance()
{
    static AppPreferences preferences;
    return preferences;
}

AppPreferences::AppPreferences()
    : AppPreferences(std::make_unique<QSettingsRepository>())
{
}

AppPreferences::AppPreferences(std::unique_ptr<SettingsRepository> repository)
    : m_textFontFamily(defaultTextFontFamily())
    , m_textFontSize(defaultFontSize())
    , m_codeFontFamily(defaultCodeFontFamily())
    , m_codeFontSize(defaultFontSize())
    , m_darkSyntaxThemeId(QStringLiteral("upstream-vscode-dark-2026"))
    , m_lightSyntaxThemeId(QStringLiteral("upstream-vscode-light-2026"))
    , m_showFileTreeOnStartup(false)
    , m_showStatusBar(true)
    , m_revealCurrentFileInTree(false)
    , m_gitIntegrationEnabled(true)
    , m_markdownDefaultPreview(false)
    , m_untitledUsesCodeMode(false)
    , m_overwriteMode(false)
    , m_codeFoldingEnabled(false)
    , m_bracketAssistanceEnabled(false)
    , m_smartIndentationEnabled(true)
    , m_documentCompletionEnabled(false)
    , m_semanticHighlightingEnabled(true)
    , m_repository(std::move(repository))
{
    if (!m_repository)
        m_repository = std::make_unique<QSettingsRepository>();
}

AppPreferences::~AppPreferences() = default;

QString AppPreferences::defaultCodeFontFamily()
{
#if defined(Q_OS_WIN)
    const QString platformFallback = QStringLiteral("Consolas");
#elif defined(Q_OS_MAC)
    const QString platformFallback = QStringLiteral("Menlo");
#else
    const QString platformFallback = QStringLiteral("Noto Sans Mono");
#endif
    return firstAvailableFamily({QStringLiteral("Source Code Pro"),
                                 QStringLiteral("Fira Code")},
                                platformFallback);
}

QString AppPreferences::defaultTextFontFamily()
{
    // Prose previously shared the monospace editor font; keep that as the
    // default so nothing changes until the user chooses a different text font.
    return defaultCodeFontFamily();
}

int AppPreferences::defaultFontSize()
{
#if defined(Q_OS_MAC)
    return 12;
#else
    return 10;
#endif
}

bool AppPreferences::isMonospaceFamily(const QString &family)
{
    if (family.isEmpty() || !familyInstalled(family))
    {
        return false;
    }
    // Trust the font's own fixed-pitch metadata when present — fast and
    // unambiguous (Menlo, Consolas, Source Code Pro, Fira Code, ...).
    if (QFontDatabase::isFixedPitch(family))
    {
        return true;
    }
    // Some genuinely-monospace fonts ship an incorrect post.isFixedPitch
    // bit (proportional), e.g. certain Nerd Font builds.  Detect them by
    // confirming that glyphs which diverge widely in proportional faces
    // all share the same advance width.  'M'/'W' are tested against 'i'
    // first so proportional families fail after a single comparison.
    const QFont font(family);
    const QFontMetrics metrics(font);
    const QChar probes[] = {QChar('i'), QChar('M'), QChar('W'),
                            QChar('l'), QChar('.'), QChar(' '), QChar('0')};
    const int width = metrics.horizontalAdvance(probes[0]);
    if (width <= 0)
    {
        return false;
    }
    for (QChar ch : probes)
    {
        if (metrics.horizontalAdvance(ch) != width)
        {
            return false;
        }
    }
    return true;
}

void AppPreferences::load()
{
    const SettingsValues settings = m_repository->load();

    m_textFontFamily = validatedFamily(
        settings.value(QStringLiteral("textFontFamily"), defaultTextFontFamily()).toString(),
        defaultTextFontFamily());
    m_textFontSize = clampFontSize(
        settings.value(QStringLiteral("textFontSize"), defaultFontSize()).toInt());

    QString codeFamily = validatedFamily(
        settings.value(QStringLiteral("codeFontFamily"), defaultCodeFontFamily()).toString(),
        defaultCodeFontFamily());
    // The code font must be monospaced; migrate a stale/proportional choice
    // (e.g. an earlier build's default, or a family that is no longer
    // recognised as monospaced) back to a fixed-pitch family.
    if (!isMonospaceFamily(codeFamily))
    {
        codeFamily = defaultCodeFontFamily();
    }
    m_codeFontFamily = codeFamily;
    m_codeFontSize = clampFontSize(
        settings.value(QStringLiteral("codeFontSize"), defaultFontSize()).toInt());
    const QString defaultDark = QStringLiteral("upstream-vscode-dark-2026");
    const QString defaultLight = QStringLiteral("upstream-vscode-light-2026");
    if (settings.contains(QStringLiteral("darkSyntaxTheme"))
        || settings.contains(QStringLiteral("lightSyntaxTheme")))
    {
        setDarkSyntaxThemeId(settings.value(QStringLiteral("darkSyntaxTheme"), defaultDark).toString());
        setLightSyntaxThemeId(settings.value(QStringLiteral("lightSyntaxTheme"), defaultLight).toString());
    }
    else
    {
        // Migrate the original single selector. "auto" maps to the new paired
        // defaults. An explicit legacy theme is folded into the matching
        // dark/light slot based on its own appearance, since syntax
        // highlighting now always tracks the system theme.
        const QString legacyTheme =
            settings.value(QStringLiteral("syntaxTheme"), QStringLiteral("auto")).toString();
        setDarkSyntaxThemeId(defaultDark);
        setLightSyntaxThemeId(defaultLight);
        if (legacyTheme != QLatin1String("auto")
            && NotePeek::SyntaxThemeCatalog::themeIds().contains(legacyTheme))
        {
            const NotePeek::SyntaxTheme theme =
                NotePeek::SyntaxThemeCatalog::theme(legacyTheme, true);
            if (theme.dark)
                setDarkSyntaxThemeId(legacyTheme);
            else
                setLightSyntaxThemeId(legacyTheme);
        }
    }
    // Drop the obsolete fixed-theme / follows-system keys: the active theme
    // is now always chosen from the dark/light pair by system appearance.
    // When a user had explicitly pinned a fixed theme (follows=false),
    // preserve it by folding it into the matching dark/light slot based on
    // its own appearance; otherwise the pair loaded above is authoritative.
    if (settings.contains(QStringLiteral("syntaxThemeFollowsSystem"))
        && !settings.value(QStringLiteral("syntaxThemeFollowsSystem"), true).toBool()
        && settings.contains(QStringLiteral("fixedSyntaxTheme")))
    {
        const QString fixedTheme = settings.value(QStringLiteral("fixedSyntaxTheme")).toString();
        if (!fixedTheme.isEmpty()
            && NotePeek::SyntaxThemeCatalog::themeIds().contains(fixedTheme))
        {
            const NotePeek::SyntaxTheme theme =
                NotePeek::SyntaxThemeCatalog::theme(fixedTheme, true);
            if (theme.dark)
                setDarkSyntaxThemeId(fixedTheme);
            else
                setLightSyntaxThemeId(fixedTheme);
        }
    }
    if (settings.contains(QStringLiteral("fixedSyntaxTheme")))
        m_repository->remove({QStringLiteral("fixedSyntaxTheme")});
    if (settings.contains(QStringLiteral("syntaxThemeFollowsSystem")))
        m_repository->remove({QStringLiteral("syntaxThemeFollowsSystem")});

    m_showFileTreeOnStartup =
        settings.value(QStringLiteral("showFileTreeOnStartup"), false).toBool();
    m_showStatusBar =
        settings.value(QStringLiteral("showStatusBar"), true).toBool();
    m_revealCurrentFileInTree =
        settings.value(QStringLiteral("revealCurrentFileInTree"), false).toBool();
    m_gitIntegrationEnabled =
        settings.value(QStringLiteral("gitIntegrationEnabled"), true).toBool();
    m_markdownDefaultPreview =
        settings.value(QStringLiteral("markdownDefaultPreview"), false).toBool();
    // Stored as a readable mode name; anything other than "code" (including a
    // missing value) keeps the plain-text default.
    m_untitledUsesCodeMode =
        settings.value(QStringLiteral("untitledDocumentMode"), QStringLiteral("text")).toString()
        == QLatin1String("code");
    m_overwriteMode =
        settings.value(QStringLiteral("overwriteMode"), false).toBool();
    m_codeFoldingEnabled =
        settings.value(QStringLiteral("codeFoldingEnabled"), false).toBool();
    m_bracketAssistanceEnabled =
        settings.value(QStringLiteral("bracketAssistanceEnabled"), false).toBool();
    m_smartIndentationEnabled =
        settings.value(QStringLiteral("smartIndentationEnabled"), true).toBool();
    m_documentCompletionEnabled =
        settings.value(QStringLiteral("documentCompletionEnabled"), false).toBool();
    m_semanticHighlightingEnabled =
        settings.value(QStringLiteral("semanticHighlightingEnabled"), true).toBool();
    m_language = settings.value(QStringLiteral("language"), QString()).toString();

}

void AppPreferences::save() const
{
    SettingsValues settings;

    settings.insert(QStringLiteral("textFontFamily"), m_textFontFamily);
    settings.insert(QStringLiteral("textFontSize"), m_textFontSize);
    settings.insert(QStringLiteral("codeFontFamily"), m_codeFontFamily);
    settings.insert(QStringLiteral("codeFontSize"), m_codeFontSize);
    settings.insert(QStringLiteral("darkSyntaxTheme"), m_darkSyntaxThemeId);
    settings.insert(QStringLiteral("lightSyntaxTheme"), m_lightSyntaxThemeId);
    settings.insert(QStringLiteral("showFileTreeOnStartup"), m_showFileTreeOnStartup);
    settings.insert(QStringLiteral("showStatusBar"), m_showStatusBar);
    settings.insert(QStringLiteral("revealCurrentFileInTree"), m_revealCurrentFileInTree);
    settings.insert(QStringLiteral("gitIntegrationEnabled"), m_gitIntegrationEnabled);
    settings.insert(QStringLiteral("markdownDefaultPreview"), m_markdownDefaultPreview);
    settings.insert(QStringLiteral("untitledDocumentMode"),
                    m_untitledUsesCodeMode ? QStringLiteral("code") : QStringLiteral("text"));
    settings.insert(QStringLiteral("overwriteMode"), m_overwriteMode);
    settings.insert(QStringLiteral("codeFoldingEnabled"), m_codeFoldingEnabled);
    settings.insert(QStringLiteral("bracketAssistanceEnabled"), m_bracketAssistanceEnabled);
    settings.insert(QStringLiteral("smartIndentationEnabled"), m_smartIndentationEnabled);
    settings.insert(QStringLiteral("documentCompletionEnabled"), m_documentCompletionEnabled);
    settings.insert(QStringLiteral("semanticHighlightingEnabled"), m_semanticHighlightingEnabled);
    settings.insert(QStringLiteral("language"), m_language);

    m_repository->save(settings,
                       {QStringLiteral("syntaxTheme"),
                        QStringLiteral("fixedSyntaxTheme"),
                        QStringLiteral("syntaxThemeFollowsSystem")});
}

QFont AppPreferences::textFont() const
{
    return QFont(m_textFontFamily, m_textFontSize);
}

QFont AppPreferences::codeFont() const
{
    return QFont(m_codeFontFamily, m_codeFontSize);
}

void AppPreferences::setTextFont(const QString &family, int pointSize)
{
    m_textFontFamily = validatedFamily(family, defaultTextFontFamily());
    m_textFontSize = clampFontSize(pointSize);
}

void AppPreferences::setCodeFont(const QString &family, int pointSize)
{
    m_codeFontFamily = validatedFamily(family, defaultCodeFontFamily());
    m_codeFontSize = clampFontSize(pointSize);
}

void AppPreferences::setDarkSyntaxThemeId(const QString &themeId)
{
    m_darkSyntaxThemeId = validatedThemeId(themeId, true,
        QStringLiteral("upstream-vscode-dark-2026"));
}

void AppPreferences::setLightSyntaxThemeId(const QString &themeId)
{
    m_lightSyntaxThemeId = validatedThemeId(themeId, false,
        QStringLiteral("upstream-vscode-light-2026"));
}

QString AppPreferences::currentSyntaxThemeId(bool systemDark) const
{
    return systemDark ? m_darkSyntaxThemeId : m_lightSyntaxThemeId;
}
