#include "toolstudiowindow.h"

#include "../../qtllm/host/managedllamacppappbootstrap.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("qtllm"));
    QApplication::setApplicationName(QStringLiteral("toolstudio"));
    qtllm::host::startManagedLlamaCppRuntimeForApp(&app);

    ToolStudioWindow window;
    window.show();
    return app.exec();
}
