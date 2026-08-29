#include "features/settings/settingsrepository.h"

#include <QSettings>

namespace {

constexpr auto PreferencesGroup = "preferences";

} // namespace

SettingsValues QSettingsRepository::load() const
{
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(PreferencesGroup));

    SettingsValues values;
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys)
        values.insert(key, settings.value(key));

    settings.endGroup();
    return values;
}

void QSettingsRepository::save(const SettingsValues &values,
                               const QStringList &removedKeys) const
{
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(PreferencesGroup));

    for (auto it = values.cbegin(); it != values.cend(); ++it)
        settings.setValue(it.key(), it.value());
    for (const QString &key : removedKeys)
        settings.remove(key);

    settings.endGroup();
}

void QSettingsRepository::remove(const QStringList &keys) const
{
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(PreferencesGroup));
    for (const QString &key : keys)
        settings.remove(key);
    settings.endGroup();
}
