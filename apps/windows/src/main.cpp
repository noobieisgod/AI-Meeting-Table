#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QSplashScreen>
#include <QTimer>
#include <QEventLoop>

#include <QPainter>

#include "app/application_context.h"
#include "ui/main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("AI Meeting Table");
    QApplication::setOrganizationName("AI Meeting Table");
    QApplication::setWindowIcon(QIcon(":/branding/icon_logo.png"));

    QPixmap iconPixmap(":/branding/icon_logo.png");
    iconPixmap = iconPixmap.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPixmap solidPixmap(300, 350);
    solidPixmap.fill(Qt::white);
    QPainter painter(&solidPixmap);
    painter.drawPixmap((300 - iconPixmap.width()) / 2, 20, iconPixmap);
    painter.end();

    QSplashScreen splash(solidPixmap);
    splash.showMessage("AI Meeting Table\nFetching available AI models...", Qt::AlignBottom | Qt::AlignHCenter, Qt::black);
    splash.show();
    app.processEvents();

    amt::ApplicationContext context;
    if (!context.initialize()) {
        QMessageBox::critical(nullptr, "AI Meeting Table", "Failed to initialize local storage.");
        return 1;
    }

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(context.modelCatalogManager(), &amt::ModelCatalogManager::fetchCompleted, &loop, &QEventLoop::quit);

    context.modelCatalogManager()->fetchModelsAsync();
    timeoutTimer.start(8000);
    loop.exec();

    amt::MainWindow window(&context);
    window.setWindowIcon(QIcon(":/branding/icon_logo.png"));
    window.show();
    splash.finish(&window);

    return app.exec();
}
