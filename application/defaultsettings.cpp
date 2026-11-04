#include "application/defaultsettings.h"

#include <QSettings>

#include "application/applicationconstants.h"

void DefaultSettings::applyAuthentication(QSettings &settings)
{
    settings.setValue("SHOUConnect/Protocol", Protocol);
    settings.setValue("SHOUConnect/AuthType", AuthType);
    settings.setValue("SHOUConnect/LoginDomain", LoginDomain);
    settings.setValue("SHOUConnect/LoginURL", LoginUrl);
}

void DefaultSettings::reset(QSettings &settings)
{
    settings.setValue("Credential/Username", "");
    settings.setValue("Credential/Password", "");
    settings.setValue("Credential/TOTPSecret", "");

    settings.setValue("Common/ConnectAfterStart", false);
    settings.setValue("Common/CheckUpdateAfterStart", false);
    settings.setValue("Common/AutoSetProxy", false);
    settings.setValue("Common/ReconnectTime", 1);
    settings.setValue("Common/AutoReconnect", false);
    settings.setValue("Common/SystemProxyBypass", "");

    settings.setValue("SHOUConnect/ServerAddress", ServerAddress);
    settings.setValue("SHOUConnect/ServerPort", ServerPort);
    settings.setValue("SHOUConnect/DNS", "");
    settings.setValue("SHOUConnect/DNSAuto", true);
    settings.setValue("SHOUConnect/SecondaryDNS", "");
    settings.setValue("SHOUConnect/LocalDNSServer", "");
    settings.setValue("SHOUConnect/DNSServerBind", "");
    settings.setValue("SHOUConnect/DNSTTL", 3600);
    settings.setValue("SHOUConnect/SOCKS5Port", 11080);
    settings.setValue("SHOUConnect/HTTPPort", 11081);
    settings.setValue("SHOUConnect/ShadowsocksURL", "");
    settings.setValue("SHOUConnect/DialDirectProxy", "");
    settings.setValue("SHOUConnect/UpdateBestNodesInterval", 300);
    settings.setValue("SHOUConnect/CredentialsAsArguments", false);

    applyAuthentication(settings);
    settings.setValue("SHOUConnect/EasyConnectAuthType", "password");
    settings.setValue("SHOUConnect/PhoneCountryCode", "86");
    settings.setValue("SHOUConnect/PhoneNumber", "");

    settings.setValue("SHOUConnect/MultiLine", false);
    settings.setValue("SHOUConnect/KeepAlive", false);
    settings.setValue("SHOUConnect/KeepAliveURL", "");
    settings.setValue("SHOUConnect/BindInterface", "");
    settings.setValue("SHOUConnect/OutsideAccess", false);
    settings.setValue("SHOUConnect/SkipDomainResource", false);
    settings.setValue("SHOUConnect/DisableServerConfig", false);
    settings.setValue("SHOUConnect/ProxyAll", false);
    settings.setValue("SHOUConnect/DisableZJUDNS", false);
    settings.setValue("SHOUConnect/ZJUDefault", false);
    settings.setValue("SHOUConnect/Debug", false);
    settings.setValue("SHOUConnect/DebugPCAP", false);
    settings.setValue("SHOUConnect/DebugTLSLog", false);

    settings.setValue("SHOUConnect/TUNMode", false);
    settings.setValue("SHOUConnect/AddRoute", false);
    settings.setValue("SHOUConnect/DNSHijack", false);
    settings.setValue("SHOUConnect/FakeIP", false);
    settings.setValue("SHOUConnect/TCPTunnelMode", false);
    settings.setValue("SHOUConnect/AutoDetectInterface", false);

    settings.setValue("SHOUConnect/TCPPortForwarding", "");
    settings.setValue("SHOUConnect/UDPPortForwarding", "");
    settings.setValue("SHOUConnect/CustomDNS", "");
    settings.setValue("SHOUConnect/CustomProxyDomain", "");
    settings.setValue("SHOUConnect/ExtraArguments", "");

    settings.setValue("Common/ConfigVersion", ApplicationConstants::ConfigVersion);
}
