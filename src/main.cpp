#include <QGuiApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QIcon>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSslSocket>
#include <QSysInfo>

#include "app/mobile_app_controller.h"
#include "core/logging.h"
#include "core/startup_timeline.h"

int main(int argc, char *argv[])
{
    auto &startup = amt::StartupTimeline::instance();
    startup.begin();
    QElapsedTimer stageTimer;
    stageTimer.start();
    QGuiApplication app(argc, argv);
    startup.mark(amt::StartupStage::GuiApplicationConstruction, stageTimer.elapsed());
    // Keep the legacy application and organization names to preserve QSettings and AppData paths.
    QGuiApplication::setApplicationName("AI Meeting Table");
    QGuiApplication::setOrganizationName("AI Meeting Table");
    QGuiApplication::setApplicationDisplayName("Synsemble");
    QGuiApplication::setWindowIcon(QIcon(":/branding/icon_logo.png"));
    QQuickStyle::setStyle("Material");

    stageTimer.restart();
    qCDebug(amt::diagnosticsLog).noquote() << QString("Qt SSL supported: %1").arg(QSslSocket::supportsSsl() ? "true" : "false");
    qCDebug(amt::diagnosticsLog).noquote() << QString("Qt SSL build version: %1").arg(QSslSocket::sslLibraryBuildVersionString());
    qCDebug(amt::diagnosticsLog).noquote() << QString("Qt SSL runtime version: %1").arg(QSslSocket::sslLibraryVersionString());
    qCDebug(amt::diagnosticsLog).noquote() << QString("Qt SSL active backend: %1").arg(QSslSocket::activeBackend());
    qCDebug(amt::diagnosticsLog).noquote() << QString("Qt SSL available backends: %1").arg(QSslSocket::availableBackends().join(", "));
    qCDebug(amt::diagnosticsLog).noquote() << QString("CPU architecture: %1").arg(QSysInfo::currentCpuArchitecture());
    startup.mark(amt::StartupStage::SslDiagnostics, stageTimer.elapsed());

    stageTimer.restart();
    amt::MobileAppController controller;
    startup.mark(amt::StartupStage::ControllerConstruction, stageTimer.elapsed());
    controller.initialize();
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &controller, [&controller](Qt::ApplicationState state) {
        if (state == Qt::ApplicationInactive
            || state == Qt::ApplicationSuspended
            || state == Qt::ApplicationHidden) {
            controller.flushCurrentSession();
        }
    });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controller, [&controller]() {
        controller.flushCurrentSession();
    });

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"appController", QVariant::fromValue(&controller)}});
    startup.mark(amt::StartupStage::BeforeQmlModuleLoad);
    stageTimer.restart();
    engine.loadFromModule("AIMeetingTable", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    startup.mark(amt::StartupStage::QmlRootCreation, stageTimer.elapsed());
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
        QObject::connect(window,
                         &QQuickWindow::frameSwapped,
                         &app,
                         [&startup]() { startup.markFirstFrameVisible(); },
                         Qt::SingleShotConnection);
    }

    return app.exec();
}
