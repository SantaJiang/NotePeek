#include "apppreferences.h"
#include "features/settings/settingsrepository.h"

#include <QtTest>

#include <memory>

namespace {

class RecordingSettingsRepository final : public SettingsRepository
{
public:
    SettingsValues load() const override
    {
        return loadedValues;
    }

    void save(const SettingsValues &values,
              const QStringList &removedKeys) const override
    {
        savedValues = values;
        removedOnSave = removedKeys;
    }

    void remove(const QStringList &keys) const override
    {
        removedOnLoad.append(keys);
    }

    SettingsValues loadedValues;
    mutable SettingsValues savedValues;
    mutable QStringList removedOnLoad;
    mutable QStringList removedOnSave;
};

} // namespace

class SettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void missingValuesKeepCurrentDefaults()
    {
        auto repository = std::make_unique<RecordingSettingsRepository>();
        AppPreferences preferences(std::move(repository));

        preferences.load();

        QVERIFY(preferences.gitIntegrationEnabled());
        QVERIFY(preferences.showStatusBar());
        QVERIFY(!preferences.revealCurrentFileInTree());
        QVERIFY(preferences.semanticHighlightingEnabled());
        QVERIFY(preferences.smartIndentationEnabled());
        QCOMPARE(preferences.markdownPreviewOpenMode(),
                 AppPreferences::MarkdownPreviewOpenMode::RememberLast);
        QVERIFY(!preferences.markdownLastPreviewVisible());
        QVERIFY(!preferences.markdownLastPreviewOnly());
        QVERIFY(!preferences.shouldOpenMarkdownPreview());
        QVERIFY(!preferences.shouldOpenMarkdownPreviewOnly());
    }

    void markdownPreviewOpenModeMigratesLegacyCheckboxAndRemembersLayout()
    {
        auto repository = std::make_unique<RecordingSettingsRepository>();
        repository->loadedValues.insert(QStringLiteral("markdownDefaultPreview"), true);
        AppPreferences preferences(std::move(repository));

        preferences.load();

        QCOMPARE(preferences.markdownPreviewOpenMode(),
                 AppPreferences::MarkdownPreviewOpenMode::AlwaysOn);
        QVERIFY(preferences.markdownLastPreviewVisible());
        QVERIFY(preferences.shouldOpenMarkdownPreview());
        QVERIFY(!preferences.shouldOpenMarkdownPreviewOnly());

        preferences.rememberMarkdownPreviewState(true, true);
        QVERIFY(preferences.shouldOpenMarkdownPreviewOnly());

        preferences.setMarkdownPreviewOpenMode(AppPreferences::MarkdownPreviewOpenMode::RememberLast);
        preferences.rememberMarkdownPreviewState(false, true);
        QVERIFY(!preferences.shouldOpenMarkdownPreview());
        QVERIFY(!preferences.shouldOpenMarkdownPreviewOnly());

        preferences.rememberMarkdownPreviewState(true, false);
        QVERIFY(preferences.shouldOpenMarkdownPreview());
        QVERIFY(!preferences.shouldOpenMarkdownPreviewOnly());
    }

    void markdownPreviewOpenModePersistsCanonicalValuesAndDropsLegacyKey()
    {
        auto repository = std::make_unique<RecordingSettingsRepository>();
        RecordingSettingsRepository *recording = repository.get();
        AppPreferences preferences(std::move(repository));

        preferences.setMarkdownPreviewOpenMode(AppPreferences::MarkdownPreviewOpenMode::AlwaysOff);
        preferences.rememberMarkdownPreviewState(true, true);
        QCOMPARE(recording->savedValues.value(QStringLiteral("markdownPreviewOpenMode")).toString(),
                 QStringLiteral("off"));
        QCOMPARE(recording->savedValues.value(QStringLiteral("markdownLastPreviewVisible")).toBool(),
                 true);
        QCOMPARE(recording->savedValues.value(QStringLiteral("markdownLastPreviewOnly")).toBool(),
                 true);
        QVERIFY(recording->removedOnSave.contains(QStringLiteral("markdownDefaultPreview")));

        auto loadedRepository = std::make_unique<RecordingSettingsRepository>();
        loadedRepository->loadedValues = recording->savedValues;
        AppPreferences loaded(std::move(loadedRepository));
        loaded.load();
        QCOMPARE(loaded.markdownPreviewOpenMode(),
                 AppPreferences::MarkdownPreviewOpenMode::AlwaysOff);
        QVERIFY(!loaded.shouldOpenMarkdownPreview());
        QVERIFY(!loaded.shouldOpenMarkdownPreviewOnly());
    }

    void savesCanonicalValuesAndRemovesLegacyThemeKeys()
    {
        auto repository = std::make_unique<RecordingSettingsRepository>();
        RecordingSettingsRepository *recording = repository.get();
        AppPreferences preferences(std::move(repository));

        preferences.setGitIntegrationEnabled(false);
        preferences.setShowStatusBar(false);
        preferences.setRevealCurrentFileInTree(true);
        preferences.setSemanticHighlightingEnabled(false);
        preferences.save();

        QCOMPARE(recording->savedValues.value(QStringLiteral("gitIntegrationEnabled")).toBool(),
                 false);
        QCOMPARE(recording->savedValues.value(QStringLiteral("showStatusBar")).toBool(), false);
        QCOMPARE(recording->savedValues.value(
                     QStringLiteral("revealCurrentFileInTree")).toBool(), true);
        QCOMPARE(recording->savedValues.value(QStringLiteral("semanticHighlightingEnabled")).toBool(),
                 false);
        QVERIFY(recording->removedOnSave.contains(QStringLiteral("syntaxTheme")));
        QVERIFY(recording->removedOnSave.contains(QStringLiteral("fixedSyntaxTheme")));
        QVERIFY(recording->removedOnSave.contains(QStringLiteral("syntaxThemeFollowsSystem")));
    }

    void loadRemovesObsoleteFixedThemeKeys()
    {
        auto repository = std::make_unique<RecordingSettingsRepository>();
        RecordingSettingsRepository *recording = repository.get();
        repository->loadedValues.insert(QStringLiteral("fixedSyntaxTheme"),
                                        QStringLiteral("missing-theme"));
        repository->loadedValues.insert(QStringLiteral("syntaxThemeFollowsSystem"), false);
        AppPreferences preferences(std::move(repository));

        preferences.load();

        QVERIFY(recording->removedOnLoad.contains(QStringLiteral("fixedSyntaxTheme")));
        QVERIFY(recording->removedOnLoad.contains(QStringLiteral("syntaxThemeFollowsSystem")));
    }
};

QTEST_MAIN(SettingsTest)

#include "settings_test.moc"
