#ifndef NOTEPEEK_SETTINGS_SETTINGSREPOSITORY_H
#define NOTEPEEK_SETTINGS_SETTINGSREPOSITORY_H

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariant>

using SettingsValues = QHash<QString, QVariant>;

// Persistence boundary for application settings. Validation and defaults stay
// in the preferences model; adapters only load and store named values.
class SettingsRepository
{
public:
    virtual ~SettingsRepository() = default;

    virtual SettingsValues load() const = 0;
    virtual void save(const SettingsValues &values,
                      const QStringList &removedKeys = {}) const = 0;
    virtual void remove(const QStringList &keys) const = 0;
};

class QSettingsRepository final : public SettingsRepository
{
public:
    SettingsValues load() const override;
    void save(const SettingsValues &values,
              const QStringList &removedKeys = {}) const override;
    void remove(const QStringList &keys) const override;
};

#endif // NOTEPEEK_SETTINGS_SETTINGSREPOSITORY_H
