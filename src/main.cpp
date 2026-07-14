#include <QGuiApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QSslSocket>
#include <QSysInfo>

#include "app/mobile_app_controller.h"
#include "core/logging.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("AI Meeting Table");
    QGuiApplication::setOrganizationName("AI Meeting Table");
    QGuiApplication::setWindowIcon(QIcon(":/branding/icon_logo.png"));
    QQuickStyle::setStyle("Material");

    qCDebug(amt::diagnosticsLog).noquote() << QString("Qt SSL supported: %1").arg(QSslSocket::supportsSsl() ? "true" : "false");
    qCDebug(amt::diagnosticsLog).noquote() << QString("Qt SSL build version: %1").arg(QSslSocket::sslLibraryBuildVersionString());
    qCDebug(amt::diagnosticsLog).noquote() << QString("Qt SSL runtime version: %1").arg(QSslSocket::sslLibraryVersionString());
    qCDebug(amt::diagnosticsLog).noquote() << QString("Qt SSL active backend: %1").arg(QSslSocket::activeBackend());
    qCDebug(amt::diagnosticsLog).noquote() << QString("Qt SSL available backends: %1").arg(QSslSocket::availableBackends().join(", "));
    qCDebug(amt::diagnosticsLog).noquote() << QString("CPU architecture: %1").arg(QSysInfo::currentCpuArchitecture());

    amt::MobileAppController controller;
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
    engine.loadFromModule("AIMeetingTable", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return app.exec();
}
