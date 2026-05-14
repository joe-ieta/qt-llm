#include "chatwindow.h"

#include "../../qtllm/host/managedllamacppappbootstrap.h"

#include <QApplication>
#include <QLoggingCategory>

int main(int argc, char *argv[])
{
    QLoggingCategory::setFilterRules(QStringLiteral("qt.network.monitor.warning=false"));
    QApplication app(argc, argv);
    qtllm::host::startManagedLlamaCppRuntimeForApp(&app);

    ChatWindow window;
    window.show();

    return app.exec();
}
