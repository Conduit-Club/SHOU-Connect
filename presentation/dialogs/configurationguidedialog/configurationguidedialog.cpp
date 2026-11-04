#include "configurationguidedialog.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStringList>
#include <QVBoxLayout>

#include "presentation/dialogs/authinfowindow/authinfowindow.h"
#include "application/defaultsettings.h"

namespace
{
QString normalizedAuthType(const QString &authType)
{
    QString normalized = authType;
    if (normalized.startsWith("auth/"))
    {
        normalized.remove(0, 5);
    }
    return normalized;
}
}

ConfigurationGuideDialog::ConfigurationGuideDialog(
    QWidget *parent,
    const QSettings *settings
)
    : QDialog(parent),
      sourceSettings(settings)
{
    setWindowTitle("配置引导");
    setWindowModality(Qt::WindowModal);
    setMinimumSize(520, 340);

    auto *layout = new QVBoxLayout(this);

    stepLabel = new QLabel(this);
    titleLabel = new QLabel(this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    descriptionLabel = new QLabel(this);
    descriptionLabel->setWordWrap(true);

    layout->addWidget(stepLabel);
    layout->addWidget(titleLabel);
    layout->addWidget(descriptionLabel);

    pages = new QStackedWidget(this);
    pages->addWidget(createProtocolPage());
    pages->addWidget(createServerPage());
    pages->addWidget(createAuthenticationPage());
    pages->addWidget(createCredentialsPage());
    layout->addWidget(pages, 1);

    auto *navigationLayout = new QHBoxLayout;
    auto *cancelButton = new QPushButton("取消", this);
    backButton = new QPushButton("上一步", this);
    nextButton = new QPushButton("下一步", this);
    nextButton->setDefault(true);
    navigationLayout->addWidget(cancelButton);
    navigationLayout->addStretch();
    navigationLayout->addWidget(backButton);
    navigationLayout->addWidget(nextButton);
    layout->addLayout(navigationLayout);

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(backButton, &QPushButton::clicked, this, &ConfigurationGuideDialog::goBack);
    connect(nextButton, &QPushButton::clicked, this, &ConfigurationGuideDialog::goNext);
    connect(
        pages,
        &QStackedWidget::currentChanged,
        this,
        &ConfigurationGuideDialog::updateNavigation
    );

    const QString protocol = sourceSettings->value(
        "SHOUConnect/Protocol",
        "atrust"
    ).toString();
    if (protocol == "easyconnect")
    {
        easyconnectRadioButton->setChecked(true);
    }
    else
    {
        atrustRadioButton->setChecked(true);
    }

    const bool schoolServer = sourceSettings->value(
        "SHOUConnect/ServerAddress", DefaultSettings::ServerAddress
    ).toString().trimmed().compare(DefaultSettings::ServerAddress, Qt::CaseInsensitive) == 0
        && sourceSettings->value("SHOUConnect/ServerPort", DefaultSettings::ServerPort).toInt()
            == DefaultSettings::ServerPort;
    selectAuthenticationMethod(
        sourceSettings->value("SHOUConnect/AuthType", schoolServer ? DefaultSettings::AuthType : "psw").toString(),
        sourceSettings->value("SHOUConnect/LoginDomain", schoolServer ? DefaultSettings::LoginDomain : "").toString(),
        sourceSettings->value("SHOUConnect/LoginURL", schoolServer ? DefaultSettings::LoginUrl : "").toString()
    );

    const QString easyconnectAuthType = sourceSettings->value(
        "SHOUConnect/EasyConnectAuthType",
        sourceSettings->value("Credential/CertFile").toString().isEmpty()
            ? "password"
            : "certificate"
    ).toString();
    certificateAuthenticationRadioButton->setChecked(
        easyconnectAuthType == "certificate"
    );
    passwordAuthenticationRadioButton->setChecked(
        easyconnectAuthType != "certificate"
    );

    usernameLineEdit->setText(
        sourceSettings->value("Credential/Username").toString()
    );
    passwordLineEdit->setText(QByteArray::fromBase64(
        sourceSettings->value("Credential/Password").toByteArray()
    ));
    totpSecretLineEdit->setText(
        sourceSettings->value("Credential/TOTPSecret").toString()
    );
    countryCodeLineEdit->setText(
        sourceSettings->value("SHOUConnect/PhoneCountryCode", "86").toString()
    );
    phoneNumberLineEdit->setText(
        sourceSettings->value("SHOUConnect/PhoneNumber").toString()
    );
    certificateFileLineEdit->setText(
        sourceSettings->value("Credential/CertFile").toString()
    );
    certificatePasswordLineEdit->setText(QByteArray::fromBase64(
        sourceSettings->value("Credential/CertPassword").toByteArray()
    ));

    updateProtocolPage();
    updateNavigation();
}

void ConfigurationGuideDialog::applyTo(QSettings &settings) const
{
    settings.setValue(
        "SHOUConnect/ServerAddress",
        serverAddressLineEdit->text().trimmed()
    );
    settings.setValue("SHOUConnect/ServerPort", serverPortSpinBox->value());
    settings.setValue(
        "Credential/Username",
        usernameLineEdit->text().trimmed()
    );
    settings.setValue(
        "Credential/Password",
        QString(passwordLineEdit->text().toUtf8().toBase64())
    );
    settings.setValue(
        "Credential/TOTPSecret",
        totpSecretLineEdit->text().trimmed()
    );
    settings.setValue(
        "Credential/CertFile",
        certificateFileLineEdit->text().trimmed()
    );
    settings.setValue(
        "Credential/CertPassword",
        QString(certificatePasswordLineEdit->text().toUtf8().toBase64())
    );
    settings.setValue(
        "SHOUConnect/PhoneCountryCode",
        countryCodeLineEdit->text().trimmed()
    );
    settings.setValue(
        "SHOUConnect/PhoneNumber",
        phoneNumberLineEdit->text().trimmed()
    );

    if (atrustRadioButton->isChecked())
    {
        settings.setValue("SHOUConnect/Protocol", "atrust");
        settings.setValue("SHOUConnect/AuthType", selectedAuthType);
        settings.setValue("SHOUConnect/LoginDomain", selectedLoginDomain);
        settings.setValue("SHOUConnect/LoginURL", selectedLoginUrl);
    }
    else
    {
        settings.setValue("SHOUConnect/Protocol", "easyconnect");
        settings.setValue(
            "SHOUConnect/EasyConnectAuthType",
            certificateAuthenticationRadioButton->isChecked()
                ? "certificate"
                : "password"
        );
    }
    settings.sync();
}

QWidget *ConfigurationGuideDialog::createServerPage()
{
    auto *page = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(page);
    auto *formLayout = new QFormLayout();
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    serverAddressLineEdit = new QLineEdit(page);
    serverAddressLineEdit->setObjectName("guideServerAddressLineEdit");
    serverAddressLineEdit->setPlaceholderText("例如：vpn.example.edu.cn");
    serverAddressLineEdit->setText(
        sourceSettings->value("SHOUConnect/ServerAddress", DefaultSettings::ServerAddress).toString()
    );
    formLayout->addRow("服务器地址", serverAddressLineEdit);

    serverPortSpinBox = new QSpinBox(page);
    serverPortSpinBox->setObjectName("guideServerPortSpinBox");
    serverPortSpinBox->setRange(1, 65535);
    serverPortSpinBox->setValue(
        sourceSettings->value("SHOUConnect/ServerPort", DefaultSettings::ServerPort).toInt()
    );
    formLayout->addRow("服务器端口", serverPortSpinBox);

    const auto invalidateAuthentication = [this]() {
        if (serverAddressLineEdit->text().trimmed().compare(authenticationServer, Qt::CaseInsensitive) != 0
            || serverPortSpinBox->value() != authenticationPort)
        {
            selectAuthenticationMethod({}, {}, {});
            fetchAuthenticationButton->setText("获取认证方式");
        }
    };
    connect(serverAddressLineEdit, &QLineEdit::editingFinished, this, invalidateAuthentication);
    connect(serverPortSpinBox, &QSpinBox::editingFinished, this, invalidateAuthentication);

    pageLayout->addLayout(formLayout);
    pageLayout->addStretch();
    return page;
}

QWidget *ConfigurationGuideDialog::createProtocolPage()
{
    auto *page = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(page);

    auto *protocolGroup = new QGroupBox("服务器协议", page);
    auto *protocolLayout = new QVBoxLayout(protocolGroup);

    atrustRadioButton = new QRadioButton("aTrust", protocolGroup);
    atrustRadioButton->setObjectName("guideAtrustRadioButton");
    auto *atrustDescription = new QLabel(
        "适用于新版深信服 aTrust 服务器，可从服务器获取认证方式。",
        protocolGroup
    );
    atrustDescription->setWordWrap(true);

    easyconnectRadioButton = new QRadioButton("EasyConnect", protocolGroup);
    easyconnectRadioButton->setObjectName("guideEasyconnectRadioButton");
    auto *easyconnectDescription = new QLabel(
        "适用于传统 EasyConnect 服务器。",
        protocolGroup
    );
    easyconnectDescription->setWordWrap(true);

    protocolLayout->addWidget(atrustRadioButton);
    protocolLayout->addWidget(atrustDescription);
    protocolLayout->addSpacing(12);
    protocolLayout->addWidget(easyconnectRadioButton);
    protocolLayout->addWidget(easyconnectDescription);

    pageLayout->addWidget(protocolGroup);
    pageLayout->addStretch();

    connect(
        atrustRadioButton,
        &QRadioButton::toggled,
        this,
        &ConfigurationGuideDialog::updateProtocolPage
    );
    return page;
}

QWidget *ConfigurationGuideDialog::createAuthenticationPage()
{
    auto *page = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(page);

    authenticationPages = new QStackedWidget(page);

    auto *atrustPage = new QWidget(authenticationPages);
    auto *atrustLayout = new QVBoxLayout(atrustPage);
    auto *atrustInfo = new QLabel(
        "从服务器读取可用的认证方式，然后选择与你的账号相符的一项。",
        atrustPage
    );
    atrustInfo->setWordWrap(true);
    selectedAuthenticationLabel = new QLabel(atrustPage);
    selectedAuthenticationLabel->setWordWrap(true);
    fetchAuthenticationButton = new QPushButton("获取认证方式", atrustPage);
    atrustLayout->addWidget(atrustInfo);
    atrustLayout->addWidget(selectedAuthenticationLabel);
    atrustLayout->addWidget(fetchAuthenticationButton, 0, Qt::AlignLeft);
    atrustLayout->addStretch();
    authenticationPages->addWidget(atrustPage);

    auto *easyconnectPage = new QWidget(authenticationPages);
    auto *easyconnectLayout = new QVBoxLayout(easyconnectPage);
    passwordAuthenticationRadioButton = new QRadioButton(
        "用户名和密码",
        easyconnectPage
    );
    certificateAuthenticationRadioButton = new QRadioButton(
        "证书",
        easyconnectPage
    );
    auto *certificateHint = new QLabel(
        "选择证书认证后，下一步填写证书文件和密码。",
        easyconnectPage
    );
    certificateHint->setWordWrap(true);
    easyconnectLayout->addWidget(passwordAuthenticationRadioButton);
    easyconnectLayout->addWidget(certificateAuthenticationRadioButton);
    easyconnectLayout->addWidget(certificateHint);
    easyconnectLayout->addStretch();
    authenticationPages->addWidget(easyconnectPage);

    pageLayout->addWidget(authenticationPages);

    connect(
        fetchAuthenticationButton,
        &QPushButton::clicked,
        this,
        &ConfigurationGuideDialog::fetchAuthenticationMethods
    );
    connect(
        passwordAuthenticationRadioButton,
        &QRadioButton::toggled,
        this,
        &ConfigurationGuideDialog::updateCredentialsPage
    );
    return page;
}

QWidget *ConfigurationGuideDialog::createCredentialsPage()
{
    auto *page = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(page);
    credentialPages = new QStackedWidget(page);
    credentialPages->setObjectName("credentialPages");

    auto *passwordPage = new QWidget(credentialPages);
    auto *passwordPageLayout = new QVBoxLayout(passwordPage);
    auto *passwordGroup = new QGroupBox("账号凭据", passwordPage);
    auto *passwordForm = new QFormLayout(passwordGroup);
    passwordForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    usernameLineEdit = new QLineEdit(passwordGroup);
    usernameLineEdit->setObjectName("guideUsernameLineEdit");
    usernameLineEdit->setPlaceholderText("VPN 账号");
    passwordForm->addRow("账号", usernameLineEdit);

    auto *passwordRow = new QWidget(passwordGroup);
    auto *passwordRowLayout = new QHBoxLayout(passwordRow);
    passwordRowLayout->setContentsMargins(0, 0, 0, 0);
    passwordLineEdit = new QLineEdit(passwordRow);
    passwordLineEdit->setObjectName("guidePasswordLineEdit");
    passwordLineEdit->setEchoMode(QLineEdit::Password);
    passwordLineEdit->setPlaceholderText("VPN 密码");
    auto *showPasswordCheckBox = new QCheckBox("显示", passwordRow);
    passwordRowLayout->addWidget(passwordLineEdit, 1);
    passwordRowLayout->addWidget(showPasswordCheckBox);
    passwordForm->addRow("密码", passwordRow);

    passwordPageLayout->addWidget(passwordGroup);
    passwordPageLayout->addStretch();
    credentialPages->addWidget(passwordPage);

    auto *phonePage = new QWidget(credentialPages);
    auto *phonePageLayout = new QVBoxLayout(phonePage);
    auto *phoneGroup = new QGroupBox("短信验证手机号", phonePage);
    auto *phoneForm = new QFormLayout(phoneGroup);
    phoneForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *phoneRow = new QWidget(phoneGroup);
    auto *phoneRowLayout = new QHBoxLayout(phoneRow);
    phoneRowLayout->setContentsMargins(0, 0, 0, 0);
    countryCodeLineEdit = new QLineEdit(phoneRow);
    countryCodeLineEdit->setObjectName("guideCountryCodeLineEdit");
    countryCodeLineEdit->setMaximumWidth(64);
    countryCodeLineEdit->setPlaceholderText("86");
    countryCodeLineEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[0-9]{1,4}"),
        countryCodeLineEdit
    ));
    phoneNumberLineEdit = new QLineEdit(phoneRow);
    phoneNumberLineEdit->setObjectName("guidePhoneNumberLineEdit");
    phoneNumberLineEdit->setPlaceholderText("手机号码");
    phoneNumberLineEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[0-9]{1,20}"),
        phoneNumberLineEdit
    ));
    phoneRowLayout->addWidget(new QLabel("+", phoneRow));
    phoneRowLayout->addWidget(countryCodeLineEdit);
    phoneRowLayout->addWidget(phoneNumberLineEdit, 1);
    phoneForm->addRow("手机号", phoneRow);
    phonePageLayout->addWidget(phoneGroup);
    phonePageLayout->addStretch();
    credentialPages->addWidget(phonePage);

    auto *certificatePage = new QWidget(credentialPages);
    auto *certificatePageLayout = new QVBoxLayout(certificatePage);
    auto *certificateGroup = new QGroupBox("证书凭据", certificatePage);
    auto *certificateForm = new QFormLayout(certificateGroup);
    certificateForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto *certificateFileRow = new QWidget(certificateGroup);
    auto *certificateFileRowLayout = new QHBoxLayout(certificateFileRow);
    certificateFileRowLayout->setContentsMargins(0, 0, 0, 0);
    certificateFileLineEdit = new QLineEdit(certificateFileRow);
    certificateFileLineEdit->setObjectName("guideCertificateFileLineEdit");
    certificateFileLineEdit->setPlaceholderText("P12 或 PFX 证书文件");
    auto *browseCertificateButton = new QPushButton(
        "浏览...",
        certificateFileRow
    );
    certificateFileRowLayout->addWidget(certificateFileLineEdit, 1);
    certificateFileRowLayout->addWidget(browseCertificateButton);
    certificateForm->addRow("证书文件", certificateFileRow);

    auto *certificatePasswordRow = new QWidget(certificateGroup);
    auto *certificatePasswordRowLayout = new QHBoxLayout(
        certificatePasswordRow
    );
    certificatePasswordRowLayout->setContentsMargins(0, 0, 0, 0);
    certificatePasswordLineEdit = new QLineEdit(certificatePasswordRow);
    certificatePasswordLineEdit->setObjectName(
        "guideCertificatePasswordLineEdit"
    );
    certificatePasswordLineEdit->setEchoMode(QLineEdit::Password);
    certificatePasswordLineEdit->setPlaceholderText("可选，证书密码");
    auto *showCertificatePasswordCheckBox = new QCheckBox(
        "显示",
        certificatePasswordRow
    );
    certificatePasswordRowLayout->addWidget(certificatePasswordLineEdit, 1);
    certificatePasswordRowLayout->addWidget(showCertificatePasswordCheckBox);
    certificateForm->addRow("证书密码", certificatePasswordRow);

    certificatePageLayout->addWidget(certificateGroup);
    certificatePageLayout->addStretch();
    credentialPages->addWidget(certificatePage);

    auto *ssoPage = new QWidget(credentialPages);
    auto *ssoPageLayout = new QVBoxLayout(ssoPage);
    auto *ssoLabel = new QLabel(
        "此认证方式将在连接时打开登录页面，无需提前填写账号和密码；如需二次认证，可在下方配置 TOTP 密钥。",
        ssoPage
    );
    ssoLabel->setWordWrap(true);
    ssoPageLayout->addWidget(ssoLabel);
    ssoPageLayout->addStretch();
    credentialPages->addWidget(ssoPage);

    pageLayout->addWidget(credentialPages);

    auto *totpGroup = new QGroupBox("二次认证（TOTP）", page);
    totpGroup->setObjectName("guideTotpGroupBox");
    auto *totpForm = new QFormLayout(totpGroup);
    totpForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *totpRow = new QWidget(totpGroup);
    auto *totpRowLayout = new QHBoxLayout(totpRow);
    totpRowLayout->setContentsMargins(0, 0, 0, 0);
    totpSecretLineEdit = new QLineEdit(totpRow);
    totpSecretLineEdit->setObjectName("guideTotpSecretLineEdit");
    totpSecretLineEdit->setEchoMode(QLineEdit::Password);
    totpSecretLineEdit->setPlaceholderText("可选，TOTP 验证器密钥");
    auto *showTotpCheckBox = new QCheckBox("显示", totpRow);
    showTotpCheckBox->setObjectName("guideTotpSecretVisibleCheckBox");
    totpRowLayout->addWidget(totpSecretLineEdit, 1);
    totpRowLayout->addWidget(showTotpCheckBox);
    totpForm->addRow("TOTP 密钥", totpRow);

    auto *totpHelp = new QLabel(
        "可选，填写验证器密钥可自动生成验证码；留空则在服务器要求时手动输入。",
        totpGroup
    );
    totpHelp->setWordWrap(true);
    totpForm->addRow(totpHelp);
    pageLayout->addWidget(totpGroup);

    connect(showPasswordCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        passwordLineEdit->setEchoMode(
            checked ? QLineEdit::Normal : QLineEdit::Password
        );
    });
    connect(showTotpCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        totpSecretLineEdit->setEchoMode(
            checked ? QLineEdit::Normal : QLineEdit::Password
        );
    });
    connect(
        showCertificatePasswordCheckBox,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            certificatePasswordLineEdit->setEchoMode(
                checked ? QLineEdit::Normal : QLineEdit::Password
            );
        }
    );
    connect(
        browseCertificateButton,
        &QPushButton::clicked,
        this,
        &ConfigurationGuideDialog::browseCertificateFile
    );
    return page;
}

void ConfigurationGuideDialog::goBack()
{
    if (pages->currentIndex() > 0)
    {
        pages->setCurrentIndex(pages->currentIndex() - 1);
    }
}

void ConfigurationGuideDialog::goNext()
{
    if (!validateCurrentPage())
    {
        return;
    }

    if (pages->currentIndex() == pages->count() - 1)
    {
        accept();
        return;
    }
    pages->setCurrentIndex(pages->currentIndex() + 1);
}

void ConfigurationGuideDialog::updateProtocolPage()
{
    if (authenticationPages != nullptr)
    {
        authenticationPages->setCurrentIndex(
            atrustRadioButton->isChecked() ? 0 : 1
        );
    }
    updateCredentialsPage();
}

void ConfigurationGuideDialog::updateCredentialsPage()
{
    if (credentialPages == nullptr)
    {
        return;
    }

    if (!atrustRadioButton->isChecked())
    {
        credentialPages->setCurrentIndex(
            certificateAuthenticationRadioButton->isChecked() ? 2 : 0
        );
        return;
    }

    if (selectedAuthType == "smsCheckCode")
    {
        credentialPages->setCurrentIndex(1);
    }
    else if (selectedAuthType == "cas" || selectedAuthType == "httpsOauth2")
    {
        credentialPages->setCurrentIndex(3);
    }
    else
    {
        credentialPages->setCurrentIndex(0);
    }
}

void ConfigurationGuideDialog::browseCertificateFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择证书文件",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        "P12 Certificate (*.p12 *.pfx);;All Files (*)"
    );
    if (!fileName.isEmpty())
    {
        certificateFileLineEdit->setText(fileName);
    }
}

void ConfigurationGuideDialog::fetchAuthenticationMethods()
{
    auto *authInfoWindow = new AuthInfoWindow(this);
    connect(
        authInfoWindow,
        &AuthInfoWindow::finishAuthInfo,
        this,
        &ConfigurationGuideDialog::selectAuthenticationMethod
    );
    authInfoWindow->fetchAuthInfo(
        serverAddressLineEdit->text().trimmed(),
        serverPortSpinBox->value()
    );
    authInfoWindow->exec();
}

bool ConfigurationGuideDialog::validateCurrentPage()
{
    if (pages->currentIndex() == 1)
    {
        if (serverAddressLineEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, "服务器地址无效", "服务器地址不能为空。");
            return false;
        }
    }
    else if (pages->currentIndex() == 2)
    {
        if (atrustRadioButton->isChecked() && selectedAuthType.isEmpty())
        {
            QMessageBox::warning(
                this,
                "尚未选择认证方式",
                "请先获取并选择服务器支持的认证方式。"
            );
            return false;
        }
    }
    else if (pages->currentIndex() == 3)
    {
        if (credentialPages->currentIndex() == 0
            && (usernameLineEdit->text().trimmed().isEmpty()
                || passwordLineEdit->text().isEmpty()))
        {
            QMessageBox::warning(
                this,
                "登录凭据不完整",
                "请填写 VPN 账号和密码。"
            );
            return false;
        }
        if (credentialPages->currentIndex() == 1
            && (countryCodeLineEdit->text().trimmed().isEmpty()
                || phoneNumberLineEdit->text().trimmed().isEmpty()))
        {
            QMessageBox::warning(
                this,
                "手机号不完整",
                "请填写国家代码和手机号码。"
            );
            return false;
        }
        if (credentialPages->currentIndex() == 2
            && certificateFileLineEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(
                this,
                "尚未选择证书",
                "请选择用于登录的 P12 或 PFX 证书文件。"
            );
            return false;
        }
    }
    return true;
}

void ConfigurationGuideDialog::updateNavigation()
{
    static const QStringList titles{
        "选择协议",
        "配置服务器",
        "选择认证方式",
        "配置登录凭据"
    };
    static const QStringList descriptions{
        "选择服务器实际使用的接入协议。",
        "填写 VPN 服务器提供方给出的地址和端口。",
        "选择服务器为你的账号提供的认证方式。",
        "填写当前认证方式连接时需要的登录信息。"
    };

    const int pageIndex = pages->currentIndex();
    stepLabel->setText(
        QString("步骤 %1 / %2").arg(pageIndex + 1).arg(pages->count())
    );
    titleLabel->setText(titles.value(pageIndex));
    descriptionLabel->setText(descriptions.value(pageIndex));
    backButton->setEnabled(pageIndex > 0);
    nextButton->setText(
        pageIndex == pages->count() - 1 ? "保存" : "下一步"
    );
    updateProtocolPage();
}

void ConfigurationGuideDialog::selectAuthenticationMethod(
    const QString &authType,
    const QString &loginDomain,
    const QString &loginUrl
)
{
    selectedAuthType = normalizedAuthType(authType);
    selectedLoginDomain = loginDomain;
    selectedLoginUrl = loginUrl;
    authenticationServer = serverAddressLineEdit->text().trimmed();
    authenticationPort = serverPortSpinBox->value();

    QString details = "当前选择：" + authenticationMethodName(selectedAuthType);
    if (!selectedLoginDomain.isEmpty())
    {
        details += "\n登录域：" + selectedLoginDomain;
    }
    selectedAuthenticationLabel->setText(details);
    fetchAuthenticationButton->setText("重新获取认证方式");
    updateCredentialsPage();
}

QString ConfigurationGuideDialog::authenticationMethodName(
    const QString &authType
) const
{
    if (authType == "psw")
    {
        return "用户名和密码";
    }
    if (authType == "smsCheckCode")
    {
        return "短信验证码";
    }
    if (authType == "cas")
    {
        return "CAS";
    }
    if (authType == "httpsOauth2")
    {
        return "OAuth2";
    }
    return authType.isEmpty() ? "尚未选择" : authType;
}
