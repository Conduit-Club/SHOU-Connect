#ifndef SSOLOGINWEBVIEW_H
#define SSOLOGINWEBVIEW_H

#include <QDialog>
#include <QUrl>
#include <QString>
#include <QCloseEvent>

class QTimer;

namespace Ui
{
class SsoLoginWebView;
}

class SsoLoginWebView : public QDialog
{
    Q_OBJECT

public:
    explicit SsoLoginWebView(QWidget *parent = nullptr);
    ~SsoLoginWebView() override;

    void setInitialUrl(const QUrl &url);
    void setCallbackServerUrl(const QUrl &url);
    void setAutoFillCredentials(const QString &username, const QString &password);
    bool isSilentLogin() const;
    QUrl currentUrl() const;

signals:
    void loginCompleted(const QString &url);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupConnections();
    bool isCallbackUrl(const QUrl &url) const;
    void completeLogin(const QString &url);
    bool isCallbackTarget(const QUrl &url) const;
    void handleLoadFinished(bool ok);
    void handleUrlChanged(const QUrl &url);
    void attemptAutoFill();
    void startAutoFillWhenReady();
    void detectAbnormalLoginPage();
    void detectLoginError();
    void showForManualLogin(const QString &reason);

    Ui::SsoLoginWebView *ui;
    QUrl callbackServerUrl;
    bool loginCompletedEmitted = false;
    QString autoFillUsername;
    QString autoFillPassword;
    bool autoFillAttempted = false;
    bool autoFillSubmitted = false;
    int autoFillNavigationCount = 0;
    bool readinessProbeScheduled = false;
    QTimer *manualFallbackTimer = nullptr;
    QTimer *loginErrorTimer = nullptr;
};

#endif // SSOLOGINWEBVIEW_H
