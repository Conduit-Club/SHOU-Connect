#include "ssologinwebview.h"
#include "ui_ssologinwebview.h"

#include <QDialogButtonBox>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPointer>
#include <QTimer>
#include <QToolButton>
#include <QUrlQuery>
#include <QVariant>
#include <QWebEngineHistory>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QtWebEngineCore>
#include <QUrl>

namespace
{
// 返回状态串: ok=已提交 / captcha=页面要求验证码 / mfa=页面要求二次验证 /
// noform=找不到密码框 / nouser=找不到用户名框 / nosubmit=找不到提交途径
const char *const AutoFillScript = R"JS(
(function(creds) {
    var u = creds[0], p = creds[1];
    var pwd = document.querySelector('#fm1Input #password, input[type="password"]');
    if (!pwd) return 'noform';
    if (window.casServerCaptchaEnabled && window.casServerCaptchaShow) return 'captcha';
    var form = document.querySelector('#fm1Input') || pwd.form;
    if (!form) return 'noform';
    var visibleFields = Array.prototype.filter.call(
        form.querySelectorAll('input'),
        function(el) { return el.offsetParent !== null; }
    );
    var extraInputs = visibleFields.filter(function(el) {
        return el !== pwd && el.type !== 'hidden' && el.type !== 'checkbox'
            && el.type !== 'submit' && el.type !== 'button'
            && el.type !== 'text' && el.type !== 'tel';
    });
    if (extraInputs.length > 0) return 'captcha';
    var user = form.querySelector('#username, input[type="text"], input[type="tel"]');
    if (!user) return 'nouser';
    function setVal(el, v) {
        el.focus();
        el.value = v;
        el.dispatchEvent(new Event('input', {bubbles: true}));
        el.dispatchEvent(new Event('change', {bubbles: true}));
    }
    setVal(user, u);
    setVal(pwd, p);
    var btn = form.querySelector('[name="button"], button[type="submit"], input[type="submit"]');
    if (btn) btn.click();
    else return 'nosubmit';
    return 'ok';
})(%1);
)JS";

const char *const ReadyProbeScript = R"JS(
(function() {
    var pwd = document.querySelector('#fm1Input #password, input[type="password"]');
    if (!pwd) return 'waiting';
    if (window.casServerCaptchaEnabled && window.casServerCaptchaShow) return 'captcha';
    var form = document.querySelector('#fm1Input') || pwd.form;
    if (!form) return 'waiting';
    var button = form.querySelector('[name="button"], button[type="submit"], input[type="submit"]');
    if (!button || button.offsetParent === null) return 'waiting';
    var visibleFields = Array.prototype.filter.call(
        form.querySelectorAll('input'),
        function(el) { return el.offsetParent !== null; }
    );
    var extraInputs = visibleFields.filter(function(el) {
        return el !== pwd && el.type !== 'hidden' && el.type !== 'checkbox'
            && el.type !== 'submit' && el.type !== 'button'
            && el.type !== 'text' && el.type !== 'tel';
    });
    if (extraInputs.length > 0) return 'captcha';
    if (window.encrypt && window.encrypt.encrypt
        && typeof window.encrypt.getKey === 'function') {
        try {
            var key = window.encrypt.getKey();
            if (!key || !key.n) return 'waiting';
            var modulus = key.n.toString(16);
            if (!modulus || modulus.length < 128
                || modulus === window.autoFillInitialModulus)
                return 'waiting';
        } catch (error) {
            return 'waiting';
        }
    }
    return 'ready';
})()
)JS";

const char *const LoginErrorProbeScript = R"JS(
(function() {
    var errorNodes = document.querySelectorAll('.el-message--error, .el-message__content');
    for (var i = 0; i < errorNodes.length; i++) {
        if ((errorNodes[i].innerText || '').trim()) return 'error';
    }
    var pwd = document.querySelector('#fm1Input #password, input[type="password"]');
    if (!pwd) return 'waiting';
    if (window.casServerCaptchaEnabled && window.casServerCaptchaShow) return 'captcha';
    var dialog = document.querySelector('.el-dialog__wrapper');
    if (dialog && dialog.offsetParent !== null) return 'mfa';
    return 'loginpage';
})()
)JS";
}

SsoLoginWebView::SsoLoginWebView(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SsoLoginWebView)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);

    manualFallbackTimer = new QTimer(this);
    manualFallbackTimer->setSingleShot(true);
    manualFallbackTimer->setInterval(30000);
    connect(manualFallbackTimer, &QTimer::timeout, this, [this]()
    {
        if (!loginCompletedEmitted)
        {
            showForManualLogin(QStringLiteral("等待 SSO 重定向超时"));
        }
    });

    loginErrorTimer = new QTimer(this);
    loginErrorTimer->setInterval(500);
    connect(loginErrorTimer, &QTimer::timeout, this,
            &SsoLoginWebView::detectLoginError);

    QWebEngineScript keyCaptureScript;
    keyCaptureScript.setName(QStringLiteral("SsoLoginKeyCapture"));
    keyCaptureScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
    keyCaptureScript.setWorldId(QWebEngineScript::MainWorld);
    keyCaptureScript.setRunsOnSubFrames(false);
    keyCaptureScript.setSourceCode(QStringLiteral(R"JS(
(function() {
    Object.defineProperty(window, 'encrypt', {
        configurable: true,
        get: function() { return window.autoFillEncrypt; },
        set: function(value) {
            window.autoFillEncrypt = value;
            if (!window.autoFillInitialModulus && value
                && typeof value.getKey === 'function') {
                try {
                    var key = value.getKey();
                    if (key && key.n)
                        window.autoFillInitialModulus = key.n.toString(16);
                } catch (error) {}
            }
        }
    });
})()
)JS"));
    ui->webEngineView->page()->scripts().insert(keyCaptureScript);

    setupConnections();
}

SsoLoginWebView::~SsoLoginWebView()
{
    delete ui;
}

void SsoLoginWebView::setupConnections()
{
    connect(ui->backButton, &QToolButton::clicked, ui->webEngineView, &QWebEngineView::back);
    connect(ui->forwardButton, &QToolButton::clicked, ui->webEngineView, &QWebEngineView::forward);
    connect(ui->reloadButton, &QToolButton::clicked, ui->webEngineView, &QWebEngineView::reload);
    connect(ui->dialogButtonBox, &QDialogButtonBox::rejected, this, [&]()
    {
        if (!loginCompletedEmitted)
        {
            loginCompletedEmitted = true;
            emit loginCompleted(QString());
        }
    });

    connect(ui->addressLineEdit, &QLineEdit::returnPressed, this, [&]()
    {
        const QUrl target = QUrl::fromUserInput(ui->addressLineEdit->text());
        ui->webEngineView->load(target);
    });

    connect(ui->webEngineView, &QWebEngineView::urlChanged, this, [&](const QUrl &url)
    {
        ui->addressLineEdit->setText(url.toString());
        if (autoFillSubmitted)
        {
            ++autoFillNavigationCount;
        }
        handleUrlChanged(url);
    });

    connect(ui->webEngineView, &QWebEngineView::loadStarted, this, [this]()
    {
        if (autoFillSubmitted)
        {
            ++autoFillNavigationCount;
        }
    });

    connect(ui->webEngineView, &QWebEngineView::loadFinished, this,
            &SsoLoginWebView::handleLoadFinished);

    connect(ui->webEngineView->page(), &QWebEnginePage::navigationRequested, this,
            [&](QWebEngineNavigationRequest &request) {
                if (isCallbackTarget(request.url()))
                {
                    completeLogin(request.url().toString());
                    request.reject();
                    deleteLater();
                }
                else
                    request.accept();
            });
}

void SsoLoginWebView::setInitialUrl(const QUrl &url)
{
    if (!url.isEmpty())
    {
        ui->addressLineEdit->setText(url.toString());
        ui->webEngineView->load(url);
    }
}

void SsoLoginWebView::setCallbackServerUrl(const QUrl &url)
{
    callbackServerUrl = url;
}

void SsoLoginWebView::setAutoFillCredentials(const QString &username, const QString &password)
{
    autoFillUsername = username;
    autoFillPassword = password;
}

bool SsoLoginWebView::isSilentLogin() const
{
    return !autoFillUsername.isEmpty() && !autoFillPassword.isEmpty();
}

void SsoLoginWebView::handleLoadFinished(bool ok)
{
    if (loginCompletedEmitted)
    {
        return;
    }
    if (!ok)
    {
        showForManualLogin(QStringLiteral("登录页加载失败"));
        return;
    }
    if (isSilentLogin() && !autoFillAttempted)
    {
        startAutoFillWhenReady();
        return;
    }
    if (autoFillSubmitted && autoFillNavigationCount > 0)
    {
        detectAbnormalLoginPage();
    }
}

void SsoLoginWebView::attemptAutoFill()
{
    if (autoFillAttempted || loginCompletedEmitted)
    {
        return;
    }
    autoFillAttempted = true;
    readinessProbeScheduled = false;
    loginErrorTimer->stop();
    const QJsonArray credentials{autoFillUsername, autoFillPassword};
    const QString script = QString::fromLatin1(AutoFillScript).arg(
        QString::fromUtf8(QJsonDocument(credentials).toJson(QJsonDocument::Compact))
    );

    QPointer<SsoLoginWebView> self(this);
    ui->webEngineView->page()->runJavaScript(
        script,
        [self](const QVariant &result)
        {
            if (self == nullptr || self->loginCompletedEmitted)
            {
                return;
            }
            const QString status = result.toString();
            if (status == QLatin1String("ok"))
            {
                self->autoFillSubmitted = true;
                self->autoFillNavigationCount = 0;
                self->manualFallbackTimer->start();
                self->loginErrorTimer->start();
                qInfo().noquote() << "SSO 自动填充已提交,等待重定向";
            }
            else if (status == QLatin1String("captcha")
                     || status == QLatin1String("mfa"))
            {
                self->showForManualLogin(
                    QStringLiteral("需要%1").arg(
                        status == QLatin1String("captcha")
                            ? QStringLiteral("验证码")
                            : QStringLiteral("二次验证")
                    )
                );
            }
            else if (status == QLatin1String("waiting"))
            {
                self->autoFillAttempted = false;
                QTimer::singleShot(100, self, &SsoLoginWebView::startAutoFillWhenReady);
            }
            else
            {
                self->autoFillAttempted = false;
                self->showForManualLogin(
                    QStringLiteral("SSO 自动填充失败(%1)").arg(status));
            }
        });
}

void SsoLoginWebView::startAutoFillWhenReady()
{
    if (!isSilentLogin() || autoFillAttempted || autoFillSubmitted
        || loginCompletedEmitted || readinessProbeScheduled)
    {
        return;
    }
    readinessProbeScheduled = true;
    QPointer<SsoLoginWebView> self(this);
    ui->webEngineView->page()->runJavaScript(
        QString::fromLatin1(ReadyProbeScript),
        [self](const QVariant &result)
        {
            if (self == nullptr || self->loginCompletedEmitted)
            {
                return;
            }
            self->readinessProbeScheduled = false;
            const QString status = result.toString();
            if (status == QLatin1String("ready"))
            {
                self->attemptAutoFill();
            }
            else if (status == QLatin1String("waiting"))
            {
                QTimer::singleShot(100, self, &SsoLoginWebView::startAutoFillWhenReady);
            }
            else if (status == QLatin1String("captcha"))
            {
                self->showForManualLogin(QStringLiteral("需要验证码"));
            }
        });
}

void SsoLoginWebView::detectAbnormalLoginPage()
{
    QPointer<SsoLoginWebView> self(this);
    ui->webEngineView->page()->runJavaScript(
        QString::fromLatin1(LoginErrorProbeScript),
        [self](const QVariant &result)
        {
            if (self == nullptr || self->loginCompletedEmitted)
            {
                return;
            }
            const QString status = result.toString();
            if (status == QLatin1String("loginpage"))
            {
                self->showForManualLogin(
                    QStringLiteral("自动登录被拒绝(密码错误或需要验证码)"));
            }
            else if (status == QLatin1String("error"))
            {
                self->showForManualLogin(QStringLiteral("自动登录失败"));
            }
            else if (status == QLatin1String("captcha"))
            {
                self->showForManualLogin(QStringLiteral("需要验证码"));
            }
            else if (status == QLatin1String("mfa"))
            {
                self->showForManualLogin(QStringLiteral("需要二次验证"));
            }
        });
}

void SsoLoginWebView::detectLoginError()
{
    if (loginCompletedEmitted || isVisible() || !autoFillSubmitted
        || autoFillNavigationCount == 0)
    {
        return;
    }
    QPointer<SsoLoginWebView> self(this);
    ui->webEngineView->page()->runJavaScript(
        QString::fromLatin1(LoginErrorProbeScript),
        [self](const QVariant &result)
        {
            if (self == nullptr || self->loginCompletedEmitted || self->isVisible())
            {
                return;
            }
            const QString status = result.toString();
            if (status == QLatin1String("error"))
            {
                self->showForManualLogin(QStringLiteral("自动登录失败"));
            }
            else if (status == QLatin1String("captcha"))
            {
                self->showForManualLogin(QStringLiteral("需要验证码"));
            }
            else if (status == QLatin1String("mfa"))
            {
                self->showForManualLogin(QStringLiteral("需要二次验证"));
            }
            else if (status == QLatin1String("loginpage"))
            {
                self->showForManualLogin(
                    QStringLiteral("自动登录被拒绝(密码错误或需要验证码)"));
            }
        });
}

void SsoLoginWebView::showForManualLogin(const QString &reason)
{
    if (manualFallbackTimer != nullptr)
    {
        manualFallbackTimer->stop();
    }
    loginErrorTimer->stop();
    if (isVisible())
    {
        return;
    }
    qInfo().noquote() << "SSO 转为手动登录:" << reason;
    show();
    raise();
    activateWindow();
}

bool SsoLoginWebView::isCallbackUrl(const QUrl &url) const
{
    const auto effectivePort = [](const QUrl &value)
    {
        const int defaultPort =
            value.scheme().compare("https", Qt::CaseInsensitive) == 0 ? 443 : 80;
        return value.port(defaultPort);
    };
    return url.scheme().compare(
               callbackServerUrl.scheme(),
               Qt::CaseInsensitive
           ) == 0
        && url.host().compare(
               callbackServerUrl.host(),
               Qt::CaseInsensitive
           ) == 0
        && effectivePort(url) == effectivePort(callbackServerUrl);
}

void SsoLoginWebView::completeLogin(const QString &url)
{
    if (loginCompletedEmitted)
    {
        return;
    }
    loginCompletedEmitted = true;
    loginErrorTimer->stop();
    manualFallbackTimer->stop();
    close();
    emit loginCompleted(url);
}

bool SsoLoginWebView::isCallbackTarget(const QUrl &url) const
{
    if (!isCallbackUrl(url) || url.path() != QLatin1String("/passport/v1/auth/cas"))
    {
        return false;
    }
    return QUrlQuery(url).hasQueryItem(QStringLiteral("ticket"));
}

void SsoLoginWebView::handleUrlChanged(const QUrl &url)
{
    if (loginCompletedEmitted)
    {
        return;
    }
    if (isCallbackTarget(url))
    {
        completeLogin(url.toString());
        return;
    }
    if (isSilentLogin() && !autoFillAttempted && isCallbackUrl(url)
        && !isCallbackTarget(url))
    {
        startAutoFillWhenReady();
    }
}

QUrl SsoLoginWebView::currentUrl() const
{
    return ui->webEngineView->url();
}

void SsoLoginWebView::closeEvent(QCloseEvent *event)
{
    if (!loginCompletedEmitted)
    {
        loginCompletedEmitted = true;
        loginErrorTimer->stop();
        manualFallbackTimer->stop();
        emit loginCompleted(QString());
    }

    QDialog::closeEvent(event);
}
