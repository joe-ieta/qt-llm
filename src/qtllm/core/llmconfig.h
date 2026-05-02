#pragma once

#include <QString>
#include <QStringList>

namespace qtllm {

struct LlmConfig
{
    QString providerName;
    QString baseUrl;
    QString apiKey;
    QString model;
    QString modelVendor; // Optional explicit vendor/family hint for protocol routing.
    QString runtimeName;
    QString llamaCppRuntimeRoot;
    QString llamaCppExecutablePath;
    QString llamaCppModelPath;
    QStringList llamaCppExtraArgs;
    bool stream = true;

    int timeoutMs = 60000;
    int maxRetries = 0;
    int retryDelayMs = 400;
    int llamaCppServerPort = 18080;
    int llamaCppContextSize = 4096;
    int llamaCppGpuLayers = -1;
    int llamaCppThreadCount = 0;
    int llamaCppStartupTimeoutMs = 30000;
};

} // namespace qtllm
