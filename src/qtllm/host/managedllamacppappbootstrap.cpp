#include "managedllamacppappbootstrap.h"

#include "../core/llmconfig.h"
#include "../logging/qtllmlogger.h"
#include "../runtime/managedllamacppruntime.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QObject>

namespace qtllm::host {

runtime::ManagedLlamaCppRuntime *startManagedLlamaCppRuntimeForApp(QObject *parent,
                                                                   QString *statusMessage)
{
    LlmConfig config;
    config.providerName = QStringLiteral("llama-cpp");

    auto *runtime = new runtime::ManagedLlamaCppRuntime(parent);

    QString errorMessage;
    if (!runtime->ensureRunning(&config, &errorMessage)) {
        if (statusMessage) {
            *statusMessage = errorMessage;
        }
        logging::QtLlmLogger::instance().warn(
            QStringLiteral("llm.runtime"),
            QStringLiteral("Managed llama.cpp runtime was not started during app bootstrap"),
            logging::LogContext(),
            QJsonObject{{QStringLiteral("message"), errorMessage},
                        {QStringLiteral("availabilityStatus"), config.providerAvailabilityStatus},
                        {QStringLiteral("runtimeRoot"), config.resolvedRuntimeRoot},
                        {QStringLiteral("modelPath"), config.resolvedModelPath}});
        runtime->deleteLater();
        return nullptr;
    }

    if (statusMessage) {
        *statusMessage = QStringLiteral("Managed llama.cpp runtime started: %1").arg(config.baseUrl);
    }
    logging::QtLlmLogger::instance().info(
        QStringLiteral("llm.runtime"),
        QStringLiteral("Managed llama.cpp runtime started during app bootstrap"),
        logging::LogContext(),
        QJsonObject{{QStringLiteral("baseUrl"), config.baseUrl},
                    {QStringLiteral("runtimeRoot"), config.resolvedRuntimeRoot},
                    {QStringLiteral("modelPath"), config.resolvedModelPath},
                    {QStringLiteral("model"), config.model},
                    {QStringLiteral("gpuLayers"), config.llamaCppGpuLayers},
                    {QStringLiteral("availabilityMessage"), config.providerAvailabilityMessage}});
    QObject::connect(QCoreApplication::instance(),
                     &QCoreApplication::aboutToQuit,
                     runtime,
                     &runtime::ManagedLlamaCppRuntime::stop);
    return runtime;
}

} // namespace qtllm::host
