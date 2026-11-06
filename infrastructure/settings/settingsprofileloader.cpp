#include "settingsprofileloader.h"

#include <QUrl>
#include <QUrlQuery>

ConnectionProfile SettingsProfileLoader::load(
    const QSettings &settings,
    const QString &profileId,
    const QString &username,
    const QString &password
)
{
    ConnectionProfile profile;
    profile.profileId = profileId;
    const QString easyconnectAuthType = settings.value(
        "SHOUConnect/EasyConnectAuthType",
        settings.value("Credential/CertFile", "").toString().isEmpty()
            ? "password"
            : "certificate"
    ).toString();
    const bool useCertificate =
        settings.value("SHOUConnect/Protocol").toString() == "easyconnect"
        && easyconnectAuthType == "certificate";
    profile.credentials = {
        username,
        password,
        SettingsProfileLoader::normalizeTotpSecret(
            settings.value("Credential/TOTPSecret").toString()
        ),
        useCertificate
            ? settings.value("Credential/CertFile", "").toString()
            : QString(),
        useCertificate
            ? QByteArray::fromBase64(
                  settings.value("Credential/CertPassword", "").toByteArray()
              )
            : QString(),
        settings.value("SHOUConnect/CredentialsAsArguments", false).toBool()
    };

    const QString countryCode = settings.value("SHOUConnect/PhoneCountryCode").toString();
    const QString phoneNumber = settings.value("SHOUConnect/PhoneNumber").toString();
    const QString phone = !countryCode.isEmpty() && !phoneNumber.isEmpty()
        ? countryCode + "-" + phoneNumber
        : QString();
    profile.endpoint = {
        settings.value("SHOUConnect/Protocol").toString(),
        settings.value("SHOUConnect/AuthType").toString(),
        settings.value("SHOUConnect/LoginDomain").toString(),
        phone,
        settings.value("SHOUConnect/ServerAddress").toString(),
        settings.value("SHOUConnect/ServerPort").toInt()
    };

    profile.dns = {
        settings.value("SHOUConnect/DNS").toString(),
        settings.value("SHOUConnect/DNSAuto").toBool(),
        settings.value("SHOUConnect/SecondaryDNS").toString(),
        settings.value("SHOUConnect/DNSTTL").toInt(),
        settings.value("SHOUConnect/DisableZJUDNS").toBool(),
        settings.value("SHOUConnect/CustomDNS", "").toString(),
        settings.value("SHOUConnect/LocalDNSServer", "").toString(),
        settings.value("SHOUConnect/DNSServerBind", "").toString()
    };

    const QString bindPrefix = settings.value("SHOUConnect/OutsideAccess", false).toBool()
        ? "[::]:"
        : "127.0.0.1:";
    profile.proxy = {
        bindPrefix + QString::number(settings.value("SHOUConnect/SOCKS5Port").toInt()),
        bindPrefix + QString::number(settings.value("SHOUConnect/HTTPPort").toInt()),
        settings.value("SHOUConnect/ShadowsocksURL").toString(),
        settings.value("SHOUConnect/DialDirectProxy").toString(),
        settings.value("SHOUConnect/ProxyAll").toBool(),
        settings.value("SHOUConnect/CustomProxyDomain", "").toString()
    };

    profile.tunnel = {
        settings.value("SHOUConnect/TUNMode").toBool(),
        settings.value("SHOUConnect/AddRoute").toBool(),
        settings.value("SHOUConnect/DNSHijack").toBool(),
        settings.value("SHOUConnect/FakeIP").toBool(),
        settings.value("SHOUConnect/TCPTunnelMode").toBool(),
        settings.value("SHOUConnect/TCPPortForwarding").toString(),
        settings.value("SHOUConnect/UDPPortForwarding").toString()
    };

    profile.behavior = {
        settings.value("SHOUConnect/UpdateBestNodesInterval", 300).toInt(),
        !settings.value("SHOUConnect/MultiLine").toBool(),
        !settings.value("SHOUConnect/KeepAlive").toBool(),
        settings.value("SHOUConnect/KeepAliveURL", "").toString(),
        settings.value("SHOUConnect/BindInterface", "").toString(),
        settings.value("SHOUConnect/AutoDetectInterface", false).toBool(),
        settings.value("SHOUConnect/SkipDomainResource").toBool(),
        settings.value("SHOUConnect/DisableServerConfig").toBool(),
        !settings.value("SHOUConnect/ZJUDefault").toBool()
    };

    profile.debug = {
        settings.value("SHOUConnect/Debug").toBool(),
        settings.value("SHOUConnect/DebugPCAP", false).toBool(),
        settings.value("SHOUConnect/DebugTLSLog", false).toBool()
    };

    profile.extraArguments = settings.value("SHOUConnect/ExtraArguments", "").toString();
    return profile;
}

QString SettingsProfileLoader::normalizeTotpSecret(const QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (!trimmed.startsWith(QStringLiteral("otpauth://"), Qt::CaseInsensitive))
        return trimmed;

    const QUrl url(trimmed, QUrl::TolerantMode);
    const QUrlQuery query(url);
    const QString secret = query.queryItemValue(QStringLiteral("secret")).trimmed();
    return secret;
}
