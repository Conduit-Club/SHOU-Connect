#include "settingsmigrator.h"

#include <QSettings>
#include <QDebug>
#include <QFile>
#include <QFileInfo>

#include "application/applicationconstants.h"
#include "application/defaultsettings.h"

namespace
{
QVariant effectiveValue(const QSettings &settings, const QString &name)
{
    const QString currentKey = "SHOUConnect/" + name;
    return settings.contains(currentKey) ? settings.value(currentKey)
        : settings.value("ZJUConnect/" + name);
}

bool hasOldSchoolDefaults(const QSettings &settings, int version)
{
    const QString protocol = version == 4 ? "easyconnect"
        : effectiveValue(settings, "Protocol").toString();
    const QString easyconnectAuth = effectiveValue(settings, "EasyConnectAuthType").toString();
    return effectiveValue(settings, "ServerAddress").toString().trimmed().compare(
               DefaultSettings::ServerAddress, Qt::CaseInsensitive) == 0
        && effectiveValue(settings, "ServerPort").toInt() == DefaultSettings::ServerPort
        && protocol == "easyconnect"
        && (easyconnectAuth.isEmpty() || easyconnectAuth == "password")
        && settings.value("Credential/CertFile").toString().isEmpty()
        && effectiveValue(settings, "AuthType").toString().isEmpty()
        && effectiveValue(settings, "LoginDomain").toString().isEmpty()
        && effectiveValue(settings, "LoginURL").toString().isEmpty();
}

bool backupProfile(QSettings &settings)
{
    settings.sync();
    if (settings.status() != QSettings::NoError || !QFileInfo::exists(settings.fileName()))
        return false;
    const QString backupPath = settings.fileName() + ".pre-v10.bak";
    if (QFileInfo::exists(backupPath))
    {
        QFile original(settings.fileName());
        QFile backup(backupPath);
        return original.open(QIODevice::ReadOnly) && backup.open(QIODevice::ReadOnly)
            && original.readAll() == backup.readAll();
    }
    return QFile::copy(settings.fileName(), backupPath);
}
}

SettingsMigrationAction SettingsMigrator::prepare(QSettings &settings)
{
    const int configVersion = settings.value("Common/ConfigVersion", -1).toInt();
    if (configVersion == -1)
    {
        DefaultSettings::reset(settings);
        return SettingsMigrationAction::None;
    }
    // Inspect legacy keys without changing the original before its backup.
    const bool knownVersion = configVersion == 4 || configVersion == 6
        || configVersion == 8 || configVersion == 9;
    const bool migrateSchool = knownVersion && hasOldSchoolDefaults(settings, configVersion);
    if (migrateSchool && !backupProfile(settings))
    {
        qWarning() << "Cannot back up profile for CAS migration:" << settings.fileName();
        return SettingsMigrationAction::Failed;
    }
    // Rename the application settings group while preserving existing profiles.
    // Copy only when the new key is absent so an explicitly migrated value wins.
    if (configVersion < ApplicationConstants::ConfigVersion)
    {
        const QString legacyPrefix = "ZJUConnect/";
        const QString currentPrefix = "SHOUConnect/";
        for (const QString &key : settings.allKeys())
        {
            if (key.startsWith(legacyPrefix))
            {
                const QString migratedKey = currentPrefix + key.mid(legacyPrefix.size());
                if (!settings.contains(migratedKey))
                {
                    settings.setValue(migratedKey, settings.value(key));
                }
            }
        }
    }
    if (configVersion == 4)
    {
        settings.setValue("SHOUConnect/Protocol", "easyconnect");
    }
    if (migrateSchool)
    {
        DefaultSettings::applyAuthentication(settings);
        qInfo() << "Prepared school profile migration to aTrust/CAS; backup:" << settings.fileName() + ".pre-v10.bak";
    }
    if (configVersion == 6)
    {
        return SettingsMigrationAction::MigrateAutoStart;
    }
    if (configVersion == 4 || configVersion == 8 || configVersion == 9)
    {
        return SettingsMigrationAction::None;
    }
    if (configVersion < ApplicationConstants::ConfigVersion)
    {
        return SettingsMigrationAction::RecommendReset;
    }
    return SettingsMigrationAction::None;
}

bool SettingsMigrator::finish(QSettings &settings, bool resetToDefaults)
{
    const QVariant oldVersion = settings.value("Common/ConfigVersion");
    if (!resetToDefaults && oldVersion.toInt() > ApplicationConstants::ConfigVersion)
        return true;
    if (resetToDefaults)
    {
        settings.clear();
        DefaultSettings::reset(settings);
    }
    settings.setValue(
        "Common/ConfigVersion",
        ApplicationConstants::ConfigVersion
    );
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        if (oldVersion.isValid())
            settings.setValue("Common/ConfigVersion", oldVersion);
        else
            settings.remove("Common/ConfigVersion");
        settings.sync();
        qWarning() << "Could not save configuration upgrade:" << settings.fileName();
        return false;
    }
    return true;
}
