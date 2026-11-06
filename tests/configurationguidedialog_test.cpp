#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QGroupBox>
#include <QLineEdit>
#include <QList>
#include <QPair>
#include <QRadioButton>
#include <QSpinBox>
#include <QSettings>
#include <QStackedWidget>
#include <QTemporaryDir>

#include "presentation/dialogs/configurationguidedialog/configurationguidedialog.h"

namespace
{
QLineEdit *lineEdit(ConfigurationGuideDialog &dialog, const char *name)
{
    QLineEdit *field = dialog.findChild<QLineEdit *>(name);
    if (field == nullptr)
    {
        qCritical() << "Missing guide field" << name;
    }
    return field;
}

QStackedWidget *credentialPages(ConfigurationGuideDialog &dialog)
{
    QStackedWidget *pages = dialog.findChild<QStackedWidget *>(
        "credentialPages"
    );
    if (pages == nullptr)
    {
        qCritical() << "Missing credential pages";
    }
    return pages;
}

bool savesPasswordCredentials(const QString &settingsPath)
{
    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.setValue("SHOUConnect/Protocol", "easyconnect");
    settings.setValue("SHOUConnect/EasyConnectAuthType", "password");
    settings.setValue("Credential/Username", "old-user");
    settings.setValue(
        "Credential/Password",
        QString(QStringLiteral("old-password").toUtf8().toBase64())
    );
    settings.setValue("Credential/TOTPSecret", "old-totp");

    ConfigurationGuideDialog dialog(nullptr, &settings);
    QStackedWidget *pages = credentialPages(dialog);
    QLineEdit *username = lineEdit(dialog, "guideUsernameLineEdit");
    QLineEdit *password = lineEdit(dialog, "guidePasswordLineEdit");
    QLineEdit *totp = lineEdit(dialog, "guideTotpSecretLineEdit");
    if (pages == nullptr || username == nullptr || password == nullptr
        || totp == nullptr)
    {
        return false;
    }
    if (pages->currentIndex() != 0 || username->text() != "old-user"
        || password->text() != "old-password" || totp->text() != "old-totp")
    {
        qCritical() << "Password credentials were not loaded into the guide";
        return false;
    }

    username->setText("alice");
    password->setText("secret");
    totp->setText("totp-key");
    dialog.applyTo(settings);

    const QString savedPassword = QByteArray::fromBase64(
        settings.value("Credential/Password").toByteArray()
    );
    const bool passed = settings.value("Credential/Username") == "alice"
        && savedPassword == "secret"
        && settings.value("Credential/TOTPSecret") == "totp-key";
    if (!passed)
    {
        qCritical() << "Password credentials were not saved by the guide";
    }
    return passed;
}

bool savesSmsCredentials(const QString &settingsPath)
{
    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.setValue("SHOUConnect/Protocol", "atrust");
    settings.setValue("SHOUConnect/AuthType", "smsCheckCode");
    settings.setValue("SHOUConnect/PhoneCountryCode", "86");
    settings.setValue("SHOUConnect/PhoneNumber", "123456");

    ConfigurationGuideDialog dialog(nullptr, &settings);
    QStackedWidget *pages = credentialPages(dialog);
    QLineEdit *countryCode = lineEdit(dialog, "guideCountryCodeLineEdit");
    QLineEdit *phoneNumber = lineEdit(dialog, "guidePhoneNumberLineEdit");
    if (pages == nullptr || countryCode == nullptr || phoneNumber == nullptr)
    {
        return false;
    }
    if (pages->currentIndex() != 1 || countryCode->text() != "86"
        || phoneNumber->text() != "123456")
    {
        qCritical() << "SMS credentials were not loaded into the guide";
        return false;
    }

    countryCode->setText("852");
    phoneNumber->setText("98765432");
    dialog.applyTo(settings);

    const bool passed = settings.value("SHOUConnect/PhoneCountryCode") == "852"
        && settings.value("SHOUConnect/PhoneNumber") == "98765432";
    if (!passed)
    {
        qCritical() << "SMS credentials were not saved by the guide";
    }
    return passed;
}

bool savesCertificateCredentials(const QString &settingsPath)
{
    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.setValue("SHOUConnect/Protocol", "easyconnect");
    settings.setValue("SHOUConnect/EasyConnectAuthType", "certificate");
    settings.setValue("Credential/CertFile", "/tmp/old.p12");
    settings.setValue(
        "Credential/CertPassword",
        QString(QStringLiteral("old-cert-password").toUtf8().toBase64())
    );
    settings.setValue("Credential/TOTPSecret", "old-cert-totp");

    ConfigurationGuideDialog dialog(nullptr, &settings);
    QStackedWidget *pages = credentialPages(dialog);
    QLineEdit *certificateFile = lineEdit(
        dialog,
        "guideCertificateFileLineEdit"
    );
    QLineEdit *certificatePassword = lineEdit(
        dialog,
        "guideCertificatePasswordLineEdit"
    );
    QLineEdit *certificateTotp = lineEdit(
        dialog,
        "guideTotpSecretLineEdit"
    );
    if (pages == nullptr || certificateFile == nullptr
        || certificatePassword == nullptr || certificateTotp == nullptr)
    {
        return false;
    }
    if (pages->currentIndex() != 2 || certificateFile->text() != "/tmp/old.p12"
        || certificatePassword->text() != "old-cert-password"
        || certificateTotp->text() != "old-cert-totp")
    {
        qCritical() << "Certificate credentials were not loaded into the guide";
        return false;
    }

    certificateFile->setText("/tmp/client.pfx");
    certificatePassword->setText("cert-secret");
    certificateTotp->setText("new-cert-totp");
    dialog.applyTo(settings);

    const QString savedPassword = QByteArray::fromBase64(
        settings.value("Credential/CertPassword").toByteArray()
    );
    const bool passed =
        settings.value("Credential/CertFile") == "/tmp/client.pfx"
        && savedPassword == "cert-secret"
        && settings.value("Credential/TOTPSecret") == "new-cert-totp";
    if (!passed)
    {
        qCritical() << "Certificate credentials were not saved by the guide";
    }
    return passed;
}

bool showsCasCredentialFields(const QString &settingsPath)
{
    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.setValue("SHOUConnect/Protocol", "atrust");
    settings.setValue("SHOUConnect/AuthType", "cas");
    settings.setValue("Credential/Username", "20260001");
    settings.setValue(
        "Credential/Password",
        QString(QStringLiteral("test-password").toUtf8().toBase64())
    );

    ConfigurationGuideDialog dialog(nullptr, &settings);
    QStackedWidget *pages = credentialPages(dialog);
    QLineEdit *username = lineEdit(dialog, "guideUsernameLineEdit");
    QLineEdit *password = lineEdit(dialog, "guidePasswordLineEdit");
    auto *group = qobject_cast<QGroupBox *>(
        dialog.findChild<QWidget *>("guideCredentialsGroupBox")
    );
    const bool passed = pages != nullptr && pages->currentIndex() == 0
        && username != nullptr && username->text() == "20260001"
        && password != nullptr && password->text() == QStringLiteral("test-password")
        && group != nullptr && group->title().contains("CAS");
    if (!passed)
    {
        qCritical() << "CAS did not select the credential guide page";
    }
    return passed;
}

bool sharesTotpAcrossAuthenticationMethods(const QString &directory)
{
    const QList<QPair<QString, QString>> methods{
        {"easyconnect", "password"}, {"easyconnect", "certificate"},
        {"atrust", "psw"}, {"atrust", "smsCheckCode"},
        {"atrust", "cas"}, {"atrust", "httpsOauth2"}
    };
    for (const auto &method : methods)
    {
        const QString path = directory + "/" + method.first + "-" + method.second + ".ini";
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue("SHOUConnect/Protocol", method.first);
        settings.setValue("SHOUConnect/AuthType", method.second);
        settings.setValue("SHOUConnect/EasyConnectAuthType", method.second);
        settings.setValue("Credential/TOTPSecret", "legacy-secret");
        ConfigurationGuideDialog dialog(nullptr, &settings);
        auto *totp = lineEdit(dialog, "guideTotpSecretLineEdit");
        auto *credentials = credentialPages(dialog);
        auto *show = dialog.findChild<QCheckBox *>("guideTotpSecretVisibleCheckBox");
        auto *atrust = dialog.findChild<QRadioButton *>("guideAtrustRadioButton");
        auto *easyconnect = dialog.findChild<QRadioButton *>("guideEasyconnectRadioButton");
        if (!totp || !credentials || !show || !atrust || !easyconnect
            || credentials->isAncestorOf(totp) || !totp->isEnabled()
            || totp->text() != "legacy-secret" || totp->echoMode() != QLineEdit::Password
            || dialog.findChild<QLineEdit *>("guideCertificateTotpSecretLineEdit"))
        {
            qCritical() << "TOTP is not shared for" << method;
            return false;
        }
        // Show the credentials step and check actual visibility for SSO/SMS too.
        auto *steps = qobject_cast<QStackedWidget *>(credentials->parentWidget()->parentWidget());
        if (!steps)
            return false;
        steps->setCurrentWidget(credentials->parentWidget());
        dialog.show();
        if (!totp->isVisible())
            return false;
        show->setChecked(true);
        if (totp->echoMode() != QLineEdit::Normal)
            return false;
        show->setChecked(false);
        totp->setText("  shared-secret  ");
        easyconnect->setChecked(true);
        atrust->setChecked(true);
        easyconnect->setChecked(method.first == "easyconnect");
        if (totp->text() != "  shared-secret  " || !totp->isEnabled()
            || totp->echoMode() != QLineEdit::Password)
            return false;
        dialog.applyTo(settings);
        QSettings reloaded(path, QSettings::IniFormat);
        if (reloaded.value("Credential/TOTPSecret") != "shared-secret")
            return false;

        totp->clear();
        dialog.applyTo(settings);
        if (!settings.value("Credential/TOTPSecret").toString().isEmpty())
            return false;
        settings.setValue("Credential/TOTPSecret", "keep-on-cancel");
        totp->setText("unsaved");
        dialog.reject();
        if (settings.value("Credential/TOTPSecret") != "keep-on-cancel")
            return false;
    }
    return true;
}

bool keepsProfileSecretsSeparate(const QString &directory)
{
    QSettings first(directory + "/first.ini", QSettings::IniFormat);
    QSettings second(directory + "/second.ini", QSettings::IniFormat);
    first.setValue("Credential/TOTPSecret", "first-secret");
    second.setValue("Credential/TOTPSecret", "second-secret");
    ConfigurationGuideDialog dialog(nullptr, &first);
    lineEdit(dialog, "guideTotpSecretLineEdit")->setText("updated-first");
    dialog.applyTo(first);
    ConfigurationGuideDialog other(nullptr, &second);
    return first.value("Credential/TOTPSecret") == "updated-first"
        && lineEdit(other, "guideTotpSecretLineEdit")->text() == "second-secret"
        && second.value("Credential/TOTPSecret") == "second-secret";
}

bool defaultsToSchoolCasAndInvalidatesOnServerChange(const QString &directory)
{
    QSettings settings(directory + "/school.ini", QSettings::IniFormat);
    ConfigurationGuideDialog dialog(nullptr, &settings);
    auto *server = lineEdit(dialog, "guideServerAddressLineEdit");
    auto *port = dialog.findChild<QSpinBox *>("guideServerPortSpinBox");
    auto *atrust = dialog.findChild<QRadioButton *>("guideAtrustRadioButton");
    if (!server || !port || !atrust || !atrust->isChecked()
        || server->text() != "vpn.shou.edu.cn" || port->value() != 443)
        return false;
    dialog.applyTo(settings);
    if (settings.value("SHOUConnect/Protocol") != "atrust"
        || settings.value("SHOUConnect/AuthType") != "cas"
        || settings.value("SHOUConnect/LoginDomain") != "cas07533"
        || !settings.value("SHOUConnect/LoginURL").toString().contains("cas07533"))
        return false;
    server->setText("another.example.edu");
    QMetaObject::invokeMethod(server, "editingFinished");
    dialog.applyTo(settings);
    return settings.value("SHOUConnect/AuthType").toString().isEmpty()
        && settings.value("SHOUConnect/LoginDomain").toString().isEmpty()
        && settings.value("SHOUConnect/LoginURL").toString().isEmpty();
}
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid())
    {
        qCritical() << "Unable to create temporary directory";
        return 1;
    }

    const bool passed =
        savesPasswordCredentials(directory.filePath("password.ini"))
        && savesSmsCredentials(directory.filePath("sms.ini"))
        && savesCertificateCredentials(directory.filePath("certificate.ini"))
        && showsCasCredentialFields(directory.filePath("sso.ini"))
        && sharesTotpAcrossAuthenticationMethods(directory.path())
        && keepsProfileSecretsSeparate(directory.path())
        && defaultsToSchoolCasAndInvalidatesOnServerChange(directory.path());
    return passed ? 0 : 1;
}
