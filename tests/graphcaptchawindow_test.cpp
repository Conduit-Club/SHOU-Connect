#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QTemporaryDir>

#include "presentation/dialogs/graphcaptchawindow/graphcaptchawindow.h"
#include "presentation/dialogs/graphcaptchawindow/captchacanvas.h"

namespace
{
QString createCaptchaImage(QTemporaryDir &directory)
{
    const QString path = directory.filePath("captcha.png");
    QPixmap image(120, 40);
    image.fill(Qt::white);
    return image.save(path) ? path : QString{};
}

bool submitsTextCaptcha()
{
    QTemporaryDir directory;
    const QString imagePath = createCaptchaImage(directory);
    GraphCaptchaWindow window;
    window.setGraph(imagePath, true);

    auto *codeEdit = window.findChild<QLineEdit *>("codeEdit");
    auto *buttonBox = window.findChild<QDialogButtonBox *>("buttonBox");
    QByteArray submitted;
    QObject::connect(&window, &GraphCaptchaWindow::finishCaptcha,
                     [&](const QByteArray &captcha) { submitted = captcha; });

    if (imagePath.isEmpty() || codeEdit == nullptr || buttonBox == nullptr
        || codeEdit->isHidden())
    {
        qCritical() << "text captcha controls were not configured";
        return false;
    }

    codeEdit->setText(" A1b2 ");
    buttonBox->button(QDialogButtonBox::Ok)->click();
    if (submitted != "A1b2")
    {
        qCritical() << "text captcha submission failed:" << submitted;
        return false;
    }
    return true;
}

bool cancelsWithoutSubmittingPlaceholder()
{
    QTemporaryDir directory;
    GraphCaptchaWindow window;
    window.setGraph(createCaptchaImage(directory), true);

    int submissions = 0;
    int cancellations = 0;
    QObject::connect(&window, &GraphCaptchaWindow::finishCaptcha,
                     [&](const QByteArray &) { ++submissions; });
    QObject::connect(&window, &GraphCaptchaWindow::cancelled,
                     [&]() { ++cancellations; });

    window.findChild<QDialogButtonBox *>("buttonBox")
        ->button(QDialogButtonBox::Cancel)->click();
    if (submissions != 0 || cancellations != 1)
    {
        qCritical() << "captcha cancellation emitted an invalid response";
        return false;
    }
    return true;
}

bool preservesCoordinateCaptchaResponse()
{
    QTemporaryDir directory;
    GraphCaptchaWindow window;
    window.setGraph(createCaptchaImage(directory), false);

    QByteArray submitted;
    QObject::connect(&window, &GraphCaptchaWindow::finishCaptcha,
                     [&](const QByteArray &captcha) { submitted = captcha; });
    window.findChild<QDialogButtonBox *>("buttonBox")
        ->button(QDialogButtonBox::Ok)->click();

    const QJsonObject response = QJsonDocument::fromJson(submitted).object();
    if (response.value("width").toInt() != 120
        || response.value("height").toInt() != 40
        || !response.value("coordinates").isArray())
    {
        qCritical() << "coordinate captcha response changed:" << submitted;
        return false;
    }
    return true;
}

bool mapsScaledClicksToImageCoordinates()
{
    CaptchaCanvas canvas;
    QPixmap image(120, 40);
    image.fill(Qt::white);
    canvas.setImage(image);
    canvas.resize(240, 160);

    const QPointF clickPosition(120, 80);
    QMouseEvent click(QEvent::MouseButtonPress, clickPosition, clickPosition,
                      clickPosition, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &click);

    const auto &points = canvas.pointsPx();
    if (points.size() != 1 || qAbs(points[0].x() - 60.0) > 0.01
        || qAbs(points[0].y() - 20.0) > 0.01)
    {
        qCritical() << "scaled captcha click was mapped incorrectly:" << points;
        return false;
    }
    return true;
}

bool reloadsImageAndClearsPointsOnRetry()
{
    QTemporaryDir directory;
    const QString path = createCaptchaImage(directory);
    GraphCaptchaWindow window;
    window.setGraph(path, false);
    auto *canvas = window.findChild<CaptchaCanvas *>("canvas");
    if (!canvas || path.isEmpty())
        return false;
    canvas->resize(240, 160);
    const QPointF position(120, 80);
    QMouseEvent click(QEvent::MouseButtonPress, position, position, position,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &click);
    if (canvas->pointsPx().isEmpty())
        return false;
    QPixmap replacement(60, 90);
    replacement.fill(Qt::blue);
    if (!replacement.save(path))
        return false;
    window.setGraph(path, false);
    return canvas->image().size() == replacement.size()
        && canvas->image().toImage().pixelColor(0, 0) == QColor(Qt::blue)
        && canvas->pointsPx().isEmpty();
}
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    return submitsTextCaptcha()
        && cancelsWithoutSubmittingPlaceholder()
        && preservesCoordinateCaptchaResponse()
        && mapsScaledClicksToImageCoordinates()
        && reloadsImageAndClearsPointsOnRetry()
        ? 0
        : 1;
}
