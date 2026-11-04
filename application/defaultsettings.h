#ifndef DEFAULTSETTINGS_H
#define DEFAULTSETTINGS_H

class QSettings;

namespace DefaultSettings
{
inline constexpr char ServerAddress[] = "vpn.shou.edu.cn";
inline constexpr int ServerPort = 443;
inline constexpr char Protocol[] = "atrust";
inline constexpr char AuthType[] = "cas";
inline constexpr char LoginDomain[] = "cas07533";
inline constexpr char LoginUrl[] = "/passport/v1/public/casLogin?sfDomain=cas07533";

void applyAuthentication(QSettings &settings);
void reset(QSettings &settings);
}

#endif // DEFAULTSETTINGS_H
