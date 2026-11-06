#ifndef SETTINGSPROFILELOADER_H
#define SETTINGSPROFILELOADER_H

#include <QSettings>

#include "core/connectionprofile.h"

class SettingsProfileLoader
{
public:
    static ConnectionProfile load(
        const QSettings &settings,
        const QString &profileId,
        const QString &username,
        const QString &password
    );

    // Extracts a bare TOTP secret from user input that may be either a raw
    // secret or an otpauth:// URL (e.g. the export link from an authenticator
    // app). Returns the trimmed input unchanged when it is not an otpauth URL.
    static QString normalizeTotpSecret(const QString &raw);
};

#endif // SETTINGSPROFILELOADER_H
