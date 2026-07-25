#include "apppreferences.h"

#include <QFontDatabase>
#include <QSettings>
#include <QtGlobal>

namespace {

constexpr int kMinFontSize = 6;
constexpr int kMaxFontSize = 288;

// QFontDatabase became fully static in Qt6 but is instance-based in Qt5; wrap
// the two queries the preferences need so the code builds against both.
bool familyInstalled(const QString &family)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QFontDatabase::hasFamily(family);
#else
    return QFontDatabase().hasFamily(family);
#endif
}

bool familyFixedPitch(const QString &family)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QFontDatabase::isFixedPitch(family);
#else
    return QFontDatabase().isFixedPitch(family);
#endif
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
    : m_textFontFamily(defaultTextFontFamily())
    , m_textFontSize(defaultFontSize())
    , m_codeFontFamily(defaultCodeFontFamily())
    , m_codeFontSize(defaultFontSize())
    , m_showFileTreeOnStartup(false)
    , m_markdownDefaultPreview(false)
    , m_codeFoldingEnabled(false)
    , m_bracketAssistanceEnabled(false)
    , m_smartIndentationEnabled(true)
    , m_documentCompletionEnabled(false)
{
}

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

void AppPreferences::load()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("preferences"));

    m_textFontFamily = validatedFamily(
        settings.value(QStringLiteral("textFontFamily"), defaultTextFontFamily()).toString(),
        defaultTextFontFamily());
    m_textFontSize = clampFontSize(
        settings.value(QStringLiteral("textFontSize"), defaultFontSize()).toInt());

    QString codeFamily = validatedFamily(
        settings.value(QStringLiteral("codeFontFamily"), defaultCodeFontFamily()).toString(),
        defaultCodeFontFamily());
    // The code font must be monospaced; migrate a stale/proportional choice
    // (e.g. an earlier build's default) back to a fixed-pitch family.
    if (!familyFixedPitch(codeFamily))
    {
        codeFamily = defaultCodeFontFamily();
    }
    m_codeFontFamily = codeFamily;
    m_codeFontSize = clampFontSize(
        settings.value(QStringLiteral("codeFontSize"), defaultFontSize()).toInt());

    m_showFileTreeOnStartup =
        settings.value(QStringLiteral("showFileTreeOnStartup"), false).toBool();
    m_markdownDefaultPreview =
        settings.value(QStringLiteral("markdownDefaultPreview"), false).toBool();
    m_codeFoldingEnabled =
        settings.value(QStringLiteral("codeFoldingEnabled"), false).toBool();
    m_bracketAssistanceEnabled =
        settings.value(QStringLiteral("bracketAssistanceEnabled"), false).toBool();
    m_smartIndentationEnabled =
        settings.value(QStringLiteral("smartIndentationEnabled"), true).toBool();
    m_documentCompletionEnabled =
        settings.value(QStringLiteral("documentCompletionEnabled"), false).toBool();
    m_language = settings.value(QStringLiteral("language"), QString()).toString();

    settings.endGroup();
}

void AppPreferences::save() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("preferences"));

    settings.setValue(QStringLiteral("textFontFamily"), m_textFontFamily);
    settings.setValue(QStringLiteral("textFontSize"), m_textFontSize);
    settings.setValue(QStringLiteral("codeFontFamily"), m_codeFontFamily);
    settings.setValue(QStringLiteral("codeFontSize"), m_codeFontSize);
    settings.setValue(QStringLiteral("showFileTreeOnStartup"), m_showFileTreeOnStartup);
    settings.setValue(QStringLiteral("markdownDefaultPreview"), m_markdownDefaultPreview);
    settings.setValue(QStringLiteral("codeFoldingEnabled"), m_codeFoldingEnabled);
    settings.setValue(QStringLiteral("bracketAssistanceEnabled"), m_bracketAssistanceEnabled);
    settings.setValue(QStringLiteral("smartIndentationEnabled"), m_smartIndentationEnabled);
    settings.setValue(QStringLiteral("documentCompletionEnabled"), m_documentCompletionEnabled);
    settings.setValue(QStringLiteral("language"), m_language);

    settings.endGroup();
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
