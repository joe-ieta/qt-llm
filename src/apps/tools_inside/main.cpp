#include "toolsinsidebrowser.h"

#include "../../qtllm/host/managedllamacppappbootstrap.h"

#include <QApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qtllm"));
    QCoreApplication::setApplicationName(QStringLiteral("tools_inside"));
    qtllm::host::startManagedLlamaCppRuntimeForApp(&app);

    ToolsInsideBrowser browser;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("toolsInsideBrowser"), &browser);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
