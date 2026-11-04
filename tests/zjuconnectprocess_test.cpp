#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <cstdio>

#include "infrastructure/coreprocess/zjuconnectprocess.h"

namespace
{
void print(FILE *stream, const QByteArray &bytes)
{
    std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stream);
    std::fflush(stream);
}

bool readResponse(const QByteArray &expected)
{
    char input[512]{};
    return std::fgets(input, sizeof(input), stdin)
        && QByteArray(input).trimmed() == expected;
}

bool saveImage(const QString &path, const QByteArray &image)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(image) == image.size();
}

// Run this executable as the core so the real QProcess adapter is exercised,
// including fragmented prompts, delayed newlines and stdin responses.
int runFakeCore(bool standardError)
{
    FILE *stream = standardError ? stderr : stdout;
    print(stream, "[CORE] Please enter the TOTP tok");
    QThread::msleep(40);
    print(stream, "en:");
    if (!readResponse("012345"))
        return 1;
    QThread::msleep(20);
    print(stream, "\nPlease enter your TOTP code:");
    if (!readResponse("012345"))
        return 2;
    print(stream, "\nPlease enter the callback URL:");
    if (!readResponse("https://vpn.example.edu/callback"))
        return 3;

    const QStringList arguments = QCoreApplication::arguments();
    const int graphOption = arguments.indexOf("-graph-code-file");
    if (graphOption < 0 || graphOption + 1 >= arguments.size())
        return 4;
    const QString graphFile = arguments.at(graphOption + 1);
    // Replace the same image on retry, matching the core's runtime contract.
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        if (!saveImage(graphFile, QByteArray::number(attempt)))
            return 5;
        print(stream, "\nPlease enter the graph check code JS");
        QThread::msleep(40);
        print(stream, "ON:");
        if (!readResponse("captcha-response"))
            return 6;
    }
    const QString reportedFile = QFileInfo(graphFile).dir().filePath("reported.jpg");
    if (!saveImage(reportedFile, "reported"))
        return 7;
    print(stream, "\nGraph check code saved to " + reportedFile.toUtf8() + "\n");
    if (!readResponse("captcha-response"))
        return 8;
    print(stream, "VPN client started\n");
    return 0;
}

bool handlesInteractiveOutput(bool standardError)
{
    ZjuConnectProcess process;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    int totpRequests = 0;
    int callbackRequests = 0;
    int captchaRequests = 0;
    bool connected = false;
    bool finished = false;
    bool valid = true;
    QObject::connect(&process, &CoreProcess::totpCode, [&]() {
        ++totpRequests;
        process.writeInput("012345\n");
    });
    QObject::connect(&process, &CoreProcess::ssoAuth, [&]() {
        ++callbackRequests;
        process.writeInput("https://vpn.example.edu/callback\n");
    });
    QObject::connect(&process, &CoreProcess::graphCaptcha, [&](const QString &path) {
        QFile image(path);
        const QByteArray expected = captchaRequests < 2
            ? QByteArray::number(captchaRequests) : QByteArray("reported");
        valid = valid && image.open(QIODevice::ReadOnly) && image.readAll() == expected;
        ++captchaRequests;
        process.writeInput("captcha-response\n");
    });
    QObject::connect(&process, &CoreProcess::connectionEstablished, [&]() { connected = true; });
    QObject::connect(&process, &CoreProcess::error, [&]() { valid = false; });
    QObject::connect(&process, &CoreProcess::finished, [&]() {
        finished = true;
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    ConnectionProfile profile;
    profile.program = QCoreApplication::applicationFilePath();
    profile.endpoint.protocol = "atrust";
    profile.profileId = "prompt-contract-test";
    profile.extraArguments = standardError ? "--fake-core-stderr" : "--fake-core-stdout";
    timeout.start(5000);
    process.start(profile);
    if (!finished)
        loop.exec();

    if (!finished || !valid || !connected || totpRequests != 2
        || callbackRequests != 1 || captchaRequests != 3)
    {
        qCritical() << "Interactive core contract failed" << standardError
                    << finished << valid << connected << totpRequests
                    << callbackRequests << captchaRequests;
        return false;
    }
    return true;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.contains("--fake-core-stdout"))
        return runFakeCore(false);
    if (arguments.contains("--fake-core-stderr"))
        return runFakeCore(true);
    QStandardPaths::setTestModeEnabled(true);
    return handlesInteractiveOutput(false) && handlesInteractiveOutput(true) ? 0 : 1;
}
