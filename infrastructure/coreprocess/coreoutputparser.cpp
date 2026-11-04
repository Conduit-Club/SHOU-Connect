#include "coreoutputparser.h"

namespace
{
struct OutputRule
{
    const char *text;
    CoreOutputEvent event;
    bool interactive;
    Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive;
};

// Keep prompt recognition and no-newline buffer flushing in sync. Order preserves
// the existing first-match priority, including prompts before error messages.
const OutputRule rules[]{
    {"SUDO_ASK_PASS", CoreOutputEvent::AskSudoPassword, true},
    {"Graph check code saved to ", CoreOutputEvent::GraphCaptcha, false},
    {"Please enter the graph check code JSON:", CoreOutputEvent::GraphCaptcha, true},
    {"Please enter the SMS verification code:", CoreOutputEvent::SmsCodeWithSkipOption, true},
    {"Please enter your SMS code:", CoreOutputEvent::SmsCode, true},
    {"Please enter your TOTP code:", CoreOutputEvent::TotpCode, true},
    {"Please enter the TOTP token:", CoreOutputEvent::TotpCode, true},
    {"Please enter rand code:", CoreOutputEvent::RandCode, true},
    {"Please enter the RADIUS token:", CoreOutputEvent::RadiusCodeWithSkipOption, true},
    {"Please enter the callback url:", CoreOutputEvent::SsoCallback, true, Qt::CaseInsensitive},
    {"VPN client started", CoreOutputEvent::ClientStarted, false},
    {"graph check code still required after second login attempt", CoreOutputEvent::CaptchaFailed, false},
    {"The characters are incorrect or has expired!", CoreOutputEvent::CaptchaFailed, false},
    {"Access is denied.", CoreOutputEvent::AccessDenied, false},
    {"listen failed", CoreOutputEvent::ListenFailed, false},
    {"Invalid username or password!", CoreOutputEvent::InvalidCredentials, false},
    {"ticket is empty", CoreOutputEvent::InvalidCredentials, false},
    {"You are trying brute-force login on this IP address.", CoreOutputEvent::BruteForceBlocked, false},
    {"Login failed", CoreOutputEvent::LoginFailed, false},
    {"too many login failures", CoreOutputEvent::LoginFailed, false},
    {"unexpected newline", CoreOutputEvent::InteractiveError, false},
    {"auth type/login domain combination not found", CoreOutputEvent::AuthNotAvailable, false},
    {"invalid SID", CoreOutputEvent::AuthExpired, false},
    {"l3-tunnel tunnel auth failed:", CoreOutputEvent::AuthExpired, false},
    {"client setup error", CoreOutputEvent::ClientFailed, false},
    {"panic", CoreOutputEvent::CorePanic, false},
};
}

CoreOutputEvent CoreOutputParser::parse(const QString &output)
{
    for (const OutputRule &rule : rules)
    {
        if (output.contains(QLatin1String(rule.text), rule.caseSensitivity))
        {
            return rule.event;
        }
    }
    return CoreOutputEvent::None;
}

QString CoreOutputParser::graphCaptchaFile(const QString &output)
{
    const QString prefix = "Graph check code saved to ";
    const qsizetype markerIndex = output.indexOf(prefix);
    if (markerIndex < 0)
    {
        return {};
    }

    return output.mid(markerIndex + prefix.size()).trimmed();
}

bool CoreOutputParser::hasInteractivePrompt(const QByteArray &output)
{
    const QString text = QString::fromUtf8(output);
    for (const OutputRule &rule : rules)
    {
        if (rule.interactive && text.contains(QLatin1String(rule.text), rule.caseSensitivity))
        {
            return true;
        }
    }
    return false;
}
