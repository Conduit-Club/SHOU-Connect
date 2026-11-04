#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QList>
#include <QPair>
#include <QSettings>
#include <QTemporaryDir>

#include "application/settingsmigrator.h"
#include "application/defaultsettings.h"
#include "application/applicationconstants.h"

namespace
{
bool migratesKnownVersions()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("profile.ini"), QSettings::IniFormat);
    settings.setValue("Common/ConfigVersion", 4);

    const auto action = SettingsMigrator::prepare(settings);
    const bool passed =
        action == SettingsMigrationAction::None
        && settings.value("SHOUConnect/Protocol").toString() == "easyconnect";
    if (!passed)
    {
        qCritical() << "migratesKnownVersions failed";
    }
    return passed;
}

bool migratesLegacySettingsNamespace()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("profile.ini"), QSettings::IniFormat);
    settings.setValue("Common/ConfigVersion", 8);
    settings.setValue("ZJUConnect/ServerAddress", "legacy.example.edu");

    const auto action = SettingsMigrator::prepare(settings);
    const bool passed =
        action == SettingsMigrationAction::None
        && settings.value("SHOUConnect/ServerAddress").toString() == "legacy.example.edu";
    if (!passed)
    {
        qCritical() << "migratesLegacySettingsNamespace failed";
    }
    return passed;
}

bool recommendsResetForUnsupportedVersions()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("profile.ini"), QSettings::IniFormat);
    settings.setValue("Common/ConfigVersion", 5);
    const bool passed =
        SettingsMigrator::prepare(settings) ==
        SettingsMigrationAction::RecommendReset;
    if (!passed)
    {
        qCritical() << "recommendsResetForUnsupportedVersions failed";
    }
    return passed;
}

void oldSchoolProfile(QSettings &settings, int version = 9)
{
    settings.setValue("Common/ConfigVersion", version);
    settings.setValue("SHOUConnect/ServerAddress", "vpn.shou.edu.cn");
    settings.setValue("SHOUConnect/ServerPort", 443);
    settings.setValue("SHOUConnect/Protocol", "easyconnect");
    settings.setValue("SHOUConnect/EasyConnectAuthType", "password");
    settings.setValue("Credential/Username", "alice");
    settings.setValue("Credential/Password", "encoded-password");
    settings.setValue("Credential/TOTPSecret", "totp-secret");
    settings.setValue("SHOUConnect/DNS", "custom-dns");
}

bool defaultsUseSchoolCas()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("new.ini"), QSettings::IniFormat);
    return SettingsMigrator::prepare(settings) == SettingsMigrationAction::None
        && settings.value("SHOUConnect/Protocol") == "atrust"
        && settings.value("SHOUConnect/AuthType") == "cas"
        && settings.value("SHOUConnect/LoginDomain") == "cas07533"
        && settings.value("SHOUConnect/LoginURL") == DefaultSettings::LoginUrl
        && settings.value("SHOUConnect/ServerAddress") == DefaultSettings::ServerAddress
        && settings.value("SHOUConnect/ServerPort") == 443;
}

bool migratesSchoolWithBackupAndPreservesOtherSettings()
{
    for (const int version : {4, 6, 8, 9})
    {
        QTemporaryDir directory;
        QSettings settings(directory.filePath("old.ini"), QSettings::IniFormat);
        oldSchoolProfile(settings, version);
        if (version == 8)
        {
            // Exercise namespace migration before matching the old defaults.
            for (const QString &key : settings.allKeys())
            {
                if (key.startsWith("SHOUConnect/"))
                {
                    settings.setValue("ZJUConnect/" + key.mid(12), settings.value(key));
                    settings.remove(key);
                }
            }
        }
        const auto action = SettingsMigrator::prepare(settings);
        if (action != (version == 6 ? SettingsMigrationAction::MigrateAutoStart : SettingsMigrationAction::None)
            || !SettingsMigrator::finish(settings, false))
            return false;
        QSettings backup(settings.fileName() + ".pre-v10.bak", QSettings::IniFormat);
        QSettings reloaded(settings.fileName(), QSettings::IniFormat);
        if (backup.value("Common/ConfigVersion").toInt() != version
            || reloaded.value("Common/ConfigVersion").toInt() != ApplicationConstants::ConfigVersion
            || reloaded.value("SHOUConnect/Protocol") != "atrust"
            || reloaded.value("SHOUConnect/AuthType") != "cas"
            || reloaded.value("SHOUConnect/LoginDomain") != "cas07533"
            || reloaded.value("Credential/Username") != "alice"
            || reloaded.value("Credential/Password") != "encoded-password"
            || reloaded.value("Credential/TOTPSecret") != "totp-secret"
            || reloaded.value("SHOUConnect/DNS") != "custom-dns")
            return false;
        if (SettingsMigrator::prepare(reloaded) != SettingsMigrationAction::None)
            return false;
    }
    return true;
}

bool leavesCustomProfilesUntouched()
{
    const QList<QPair<QString, QVariant>> changes{
        {"SHOUConnect/ServerAddress", "other.example.edu"},
        {"SHOUConnect/ServerPort", 8443}, {"SHOUConnect/Protocol", "atrust"},
        {"SHOUConnect/EasyConnectAuthType", "certificate"},
        {"Credential/CertFile", "/tmp/cert.p12"},
        {"SHOUConnect/AuthType", "psw"}, {"SHOUConnect/LoginDomain", "custom"},
        {"SHOUConnect/LoginURL", "https://login.example.edu"}
    };
    for (const auto &change : changes)
    {
        QTemporaryDir directory;
        QSettings settings(directory.filePath("custom.ini"), QSettings::IniFormat);
        oldSchoolProfile(settings);
        settings.setValue(change.first, change.second);
        if (SettingsMigrator::prepare(settings) != SettingsMigrationAction::None
            || settings.value(change.first) != change.second
            || QFile::exists(settings.fileName() + ".pre-v10.bak"))
            return false;
    }
    return true;
}

bool preservesOriginalWhenBackupFails()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("blocked.ini"), QSettings::IniFormat);
    oldSchoolProfile(settings);
    settings.sync();
    if (!QDir().mkdir(settings.fileName() + ".pre-v10.bak"))
        return false;
    return SettingsMigrator::prepare(settings) == SettingsMigrationAction::Failed
        && settings.value("SHOUConnect/Protocol") == "easyconnect"
        && settings.value("Common/ConfigVersion").toInt() == 9;
}

bool doesNotStampFailedWrites()
{
    QTemporaryDir directory;
    // A directory cannot be replaced by a QSettings INI file.
    const QString path = directory.filePath("not-a-file");
    QDir().mkdir(path);
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue("Common/ConfigVersion", 9);
    return !SettingsMigrator::finish(settings, false)
        && settings.value("Common/ConfigVersion").toInt() == 9;
}

bool protectsExistingBackupAndFutureVersions()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("profile.ini"), QSettings::IniFormat);
    oldSchoolProfile(settings);
    QFile backup(settings.fileName() + ".pre-v10.bak");
    if (!backup.open(QIODevice::WriteOnly) || backup.write("keep-original-backup") < 0)
        return false;
    backup.close();
    if (SettingsMigrator::prepare(settings) != SettingsMigrationAction::Failed
        || settings.value("SHOUConnect/Protocol") != "easyconnect")
        return false;
    settings.setValue("Common/ConfigVersion", 11);
    return SettingsMigrator::prepare(settings) == SettingsMigrationAction::None
        && SettingsMigrator::finish(settings, false)
        && settings.value("Common/ConfigVersion").toInt() == 11;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    return migratesKnownVersions() && migratesLegacySettingsNamespace()
        && recommendsResetForUnsupportedVersions() && defaultsUseSchoolCas()
        && migratesSchoolWithBackupAndPreservesOtherSettings()
        && leavesCustomProfilesUntouched() && preservesOriginalWhenBackupFails()
        && doesNotStampFailedWrites() && protectsExistingBackupAndFutureVersions() ? 0 : 1;
}
