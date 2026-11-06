#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QTemporaryDir>

#include "infrastructure/settings/settingsprofileloader.h"

namespace
{
bool loadsSettingsIntoTypedProfile()
{
    QTemporaryDir directory;
    if (!directory.isValid())
    {
        qCritical() << "Unable to create temporary directory";
        return false;
    }

    QSettings settings(directory.filePath("profile.ini"), QSettings::IniFormat);
    settings.setValue("Credential/TOTPSecret", "totp");
    settings.setValue("Credential/CertFile", "/tmp/client.p12");
    settings.setValue(
        "Credential/CertPassword",
        QString(QStringLiteral("cert-password").toUtf8().toBase64())
    );
    settings.setValue("SHOUConnect/Protocol", "atrust");
    settings.setValue("SHOUConnect/AuthType", "cas");
    settings.setValue("SHOUConnect/LoginDomain", "domain");
    settings.setValue("SHOUConnect/PhoneCountryCode", "86");
    settings.setValue("SHOUConnect/PhoneNumber", "123456");
    settings.setValue("SHOUConnect/ServerAddress", "vpn.example.edu");
    settings.setValue("SHOUConnect/ServerPort", 8443);
    settings.setValue("SHOUConnect/DNS", "10.0.0.1");
    settings.setValue("SHOUConnect/DNSAuto", false);
    settings.setValue("SHOUConnect/SecondaryDNS", "10.0.0.2");
    settings.setValue("SHOUConnect/LocalDNSServer", "223.5.5.5:53");
    settings.setValue("SHOUConnect/DNSServerBind", "127.0.0.1:5353");
    settings.setValue("SHOUConnect/DNSTTL", 60);
    settings.setValue("SHOUConnect/DisableZJUDNS", true);
    settings.setValue("SHOUConnect/CustomDNS", "example.org=1.1.1.1");
    settings.setValue("SHOUConnect/OutsideAccess", true);
    settings.setValue("SHOUConnect/SOCKS5Port", 1080);
    settings.setValue("SHOUConnect/HTTPPort", 1081);
    settings.setValue("SHOUConnect/ShadowsocksURL", "ss://url");
    settings.setValue("SHOUConnect/DialDirectProxy", "http://direct");
    settings.setValue("SHOUConnect/ProxyAll", true);
    settings.setValue("SHOUConnect/CustomProxyDomain", "example.org");
    settings.setValue("SHOUConnect/TUNMode", true);
    settings.setValue("SHOUConnect/AddRoute", true);
    settings.setValue("SHOUConnect/DNSHijack", true);
    settings.setValue("SHOUConnect/FakeIP", true);
    settings.setValue("SHOUConnect/TCPTunnelMode", true);
    settings.setValue("SHOUConnect/TCPPortForwarding", "tcp-forward");
    settings.setValue("SHOUConnect/UDPPortForwarding", "udp-forward");
    settings.setValue("SHOUConnect/UpdateBestNodesInterval", 30);
    settings.setValue("SHOUConnect/CredentialsAsArguments", true);
    settings.setValue("SHOUConnect/MultiLine", false);
    settings.setValue("SHOUConnect/KeepAlive", false);
    settings.setValue("SHOUConnect/KeepAliveURL", "https://keepalive");
    settings.setValue("SHOUConnect/BindInterface", "en0");
    settings.setValue("SHOUConnect/AutoDetectInterface", true);
    settings.setValue("SHOUConnect/SkipDomainResource", true);
    settings.setValue("SHOUConnect/DisableServerConfig", true);
    settings.setValue("SHOUConnect/ZJUDefault", false);
    settings.setValue("SHOUConnect/Debug", true);
    settings.setValue("SHOUConnect/DebugPCAP", true);
    settings.setValue("SHOUConnect/DebugTLSLog", true);
    settings.setValue("SHOUConnect/ExtraArguments", "-foo bar");

    const ConnectionProfile profile =
        SettingsProfileLoader::load(settings, "campus", "alice", "secret");

    const bool passed =
        profile.profileId == "campus"
        && profile.credentials.username == "alice"
        && profile.credentials.password == "secret"
        && profile.credentials.totpSecret == "totp"
        && profile.credentials.certFile.isEmpty()
        && profile.credentials.certPassword.isEmpty()
        && profile.credentials.passAsArguments
        && profile.endpoint.protocol == "atrust"
        && profile.endpoint.authType == "cas"
        && profile.endpoint.loginDomain == "domain"
        && profile.endpoint.phone == "86-123456"
        && profile.endpoint.server == "vpn.example.edu"
        && profile.endpoint.port == 8443
        && profile.dns.primary == "10.0.0.1"
        && !profile.dns.automatic
        && profile.dns.secondary == "10.0.0.2"
        && profile.dns.ttl == 60
        && profile.dns.disableZjuDns
        && profile.dns.custom == "example.org=1.1.1.1"
        && profile.dns.localDnsServer == "223.5.5.5:53"
        && profile.dns.dnsServerBind == "127.0.0.1:5353"
        && profile.proxy.socksBind == "[::]:1080"
        && profile.proxy.httpBind == "[::]:1081"
        && profile.proxy.shadowsocksUrl == "ss://url"
        && profile.proxy.dialDirectProxy == "http://direct"
        && profile.proxy.proxyAll
        && profile.proxy.customDomains == "example.org"
        && profile.tunnel.tunMode
        && profile.tunnel.addRoute
        && profile.tunnel.dnsHijack
        && profile.tunnel.fakeIp
        && profile.tunnel.tcpTunnelMode
        && profile.tunnel.tcpPortForwarding == "tcp-forward"
        && profile.tunnel.udpPortForwarding == "udp-forward"
        && profile.behavior.updateBestNodesInterval == 30
        && profile.behavior.disableMultiLine
        && profile.behavior.disableKeepAlive
        && profile.behavior.keepAliveUrl == "https://keepalive"
        && profile.behavior.bindInterface == "en0"
        && profile.behavior.autoDetectInterface
        && profile.behavior.skipDomainResource
        && profile.behavior.disableServerConfig
        && profile.behavior.disableZjuConfig
        && profile.debug.detailedOutput
        && profile.debug.capturePcap
        && profile.debug.exportTlsKeys
        && profile.extraArguments == "-foo bar";

    if (!passed)
    {
        qCritical() << "loadsSettingsIntoTypedProfile failed";
    }
    return passed;
}

bool usesCompatibleDefaults()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("empty.ini"), QSettings::IniFormat);

    const ConnectionProfile profile = SettingsProfileLoader::load(settings, "", "", "");
    const bool passed =
        profile.behavior.updateBestNodesInterval == 300
        && !profile.credentials.passAsArguments
        && profile.behavior.disableMultiLine
        && profile.behavior.disableKeepAlive
        && profile.behavior.disableZjuConfig
        && profile.dns.localDnsServer.isEmpty()
        && profile.dns.dnsServerBind.isEmpty()
        && !profile.debug.detailedOutput
        && !profile.debug.capturePcap
        && !profile.debug.exportTlsKeys
        && profile.proxy.socksBind == "127.0.0.1:0"
        && profile.proxy.httpBind == "127.0.0.1:0";
    if (!passed)
    {
        qCritical() << "usesCompatibleDefaults failed";
    }
    return passed;
}

bool respectsEasyConnectAuthenticationMode()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("easyconnect.ini"), QSettings::IniFormat);
    settings.setValue("SHOUConnect/Protocol", "easyconnect");
    settings.setValue("Credential/CertFile", "/tmp/client.p12");
    settings.setValue(
        "Credential/CertPassword",
        QString(QStringLiteral("cert-password").toUtf8().toBase64())
    );

    settings.setValue("SHOUConnect/EasyConnectAuthType", "password");
    const ConnectionProfile passwordProfile =
        SettingsProfileLoader::load(settings, "", "alice", "secret");
    if (!passwordProfile.credentials.certFile.isEmpty()
        || !passwordProfile.credentials.certPassword.isEmpty())
    {
        qCritical() << "password mode kept certificate credentials";
        return false;
    }

    settings.setValue("SHOUConnect/EasyConnectAuthType", "certificate");
    const ConnectionProfile certificateProfile =
        SettingsProfileLoader::load(settings, "", "", "");
    const bool passed =
        certificateProfile.credentials.certFile == "/tmp/client.p12"
        && certificateProfile.credentials.certPassword == "cert-password";
    if (!passed)
    {
        qCritical() << "certificate mode did not load certificate credentials";
    }
    return passed;
}

bool normalizesOtpauthTotpSecret()
{
    // Raw secret passes through untouched.
    if (SettingsProfileLoader::normalizeTotpSecret("  RAWSECRET  ") != "RAWSECRET")
    {
        qCritical() << "raw secret not trimmed";
        return false;
    }

    // otpauth URL yields only the secret query parameter.
    const QString url = QString::fromUtf8(
        "otpauth://totp/cas%20user-2521213?secret=EXAMPLESECRET123&issuer=cas"
    );
    if (SettingsProfileLoader::normalizeTotpSecret(url) != "EXAMPLESECRET123")
    {
        qCritical() << "otpauth secret not extracted";
        return false;
    }

    // Case-insensitive scheme and surrounding whitespace are tolerated.
    if (SettingsProfileLoader::normalizeTotpSecret(
            "  OTPAUTH://totp/label?secret=ABCDEF  ") != "ABCDEF")
    {
        qCritical() << "case-insensitive otpauth not handled";
        return false;
    }

    // Non-otpauth input is returned unchanged.
    if (SettingsProfileLoader::normalizeTotpSecret("web+otpauth://x?secret=Z")
        != "web+otpauth://x?secret=Z")
    {
        qCritical() << "non-otpauth input was modified";
        return false;
    }

    return true;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    return loadsSettingsIntoTypedProfile()
        && usesCompatibleDefaults()
        && respectsEasyConnectAuthenticationMode()
        && normalizesOtpauthTotpSecret() ? 0 : 1;
}
