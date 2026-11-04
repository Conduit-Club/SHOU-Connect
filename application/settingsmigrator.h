#ifndef SETTINGSMIGRATOR_H
#define SETTINGSMIGRATOR_H

class QSettings;

enum class SettingsMigrationAction
{
    None,
    MigrateAutoStart,
    RecommendReset,
    Failed
};

class SettingsMigrator
{
public:
    static SettingsMigrationAction prepare(QSettings &settings);
    static bool finish(QSettings &settings, bool resetToDefaults);
};

#endif // SETTINGSMIGRATOR_H
