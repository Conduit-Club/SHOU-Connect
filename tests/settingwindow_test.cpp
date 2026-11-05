#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>

#include "application/defaultsettings.h"
#include "presentation/dialogs/settingwindow/settingwindow.h"

namespace
{
bool editsSharedTotp(const QString &path, const QString &protocol)
{
    QSettings settings(path, QSettings::IniFormat);
    DefaultSettings::reset(settings);
    settings.setValue("SHOUConnect/Protocol", protocol);
    settings.setValue("Credential/Username", "alice");
    settings.setValue("Credential/Password", QByteArray("password").toBase64());
    settings.setValue("Credential/TOTPSecret", "legacy-secret");
    SettingWindow dialog(nullptr, &settings, "totp-settings-test");
    dialog.setAttribute(Qt::WA_DeleteOnClose, false);
    auto *totp = dialog.findChild<QLineEdit *>("totpSecretLineEdit");
    auto *show = dialog.findChild<QCheckBox *>("totpSecretVisibleCheckBox");
    auto *common = dialog.findChild<QGroupBox *>("commonGroupBox");
    auto *atrust = dialog.findChild<QRadioButton *>("atrustRadioButton");
    auto *easyconnect = dialog.findChild<QRadioButton *>("easyconnectRadioButton");
    auto *buttons = dialog.findChild<QDialogButtonBox *>("buttonBox");
    auto *tabs = dialog.findChild<QTabWidget *>("tabWidget");
    auto *loginTab = dialog.findChild<QWidget *>("loginTab");
    if (!totp || !show || !common || !atrust || !easyconnect || !buttons || !tabs || !loginTab ||
        !common->isAncestorOf(totp) || !common->isAncestorOf(show) || !totp->isEnabled() ||
        totp->text() != "legacy-secret" || totp->echoMode() != QLineEdit::Password)
    {
        qCritical() << "Shared TOTP controls unavailable for" << protocol;
        return false;
    }
    tabs->setCurrentWidget(loginTab);
    dialog.show();
    if (!totp->isVisible())
        return false;
    show->setChecked(true);
    if (totp->echoMode() != QLineEdit::Normal)
        return false;
    show->setChecked(false);
    totp->setText("  updated-secret  ");
    easyconnect->setChecked(true);
    atrust->setChecked(true);
    easyconnect->setChecked(protocol == "easyconnect");
    if (!totp->isEnabled() || totp->text() != "  updated-secret  ")
        return false;
    buttons->button(QDialogButtonBox::Apply)->click();
    QSettings reloaded(path, QSettings::IniFormat);
    if (reloaded.value("Credential/TOTPSecret") != "updated-secret"
        || totp->text() != "updated-secret")
        return false;

    totp->setText("unsaved-secret");
    buttons->button(QDialogButtonBox::Cancel)->click();
    if (settings.value("Credential/TOTPSecret") != "updated-secret")
        return false;

    SettingWindow reopened(nullptr, &settings, "totp-settings-test");
    reopened.setAttribute(Qt::WA_DeleteOnClose, false);
    auto *cleared = reopened.findChild<QLineEdit *>("totpSecretLineEdit");
    if (!cleared || cleared->text() != "updated-secret")
        return false;
    cleared->clear();
    reopened.findChild<QDialogButtonBox *>("buttonBox")
        ->button(QDialogButtonBox::Ok)->click();
    return settings.value("Credential/TOTPSecret").toString().isEmpty();
}
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir directory;
    if (!directory.isValid())
        return 1;
    if (!editsSharedTotp(directory.filePath("easyconnect.ini"), "easyconnect")
        || !editsSharedTotp(directory.filePath("atrust.ini"), "atrust"))
        return 1;
    QSettings first(directory.filePath("easyconnect.ini"), QSettings::IniFormat);
    QSettings second(directory.filePath("atrust.ini"), QSettings::IniFormat);
    second.setValue("Credential/TOTPSecret", "other-profile-secret");
    SettingWindow dialog(nullptr, &first);
    dialog.setAttribute(Qt::WA_DeleteOnClose, false);
    return dialog.findChild<QLineEdit *>("totpSecretLineEdit")->text().isEmpty()
        && second.value("Credential/TOTPSecret") == "other-profile-secret" ? 0 : 1;
}
