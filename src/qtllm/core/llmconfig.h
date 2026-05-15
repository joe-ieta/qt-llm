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
    bool providerAvailable = true;
    QString providerAvailabilityStatus;
    QString providerAvailabilityMessage;
    QString resolvedRuntimeRoot;
    QString resolvedModelPath;
    QString llamaCppGpuMode = QStringLiteral("auto"); // auto, cpu-only, prefer-gpu, explicit
    QString llamaCppPerformanceProfile = QStringLiteral("balanced"); // conservative, balanced, aggressive
    QString llamaCppContextMode = QStringLiteral("auto"); // auto, explicit
    QString resolvedLlamaCppGpuMode;
    QString runtimePlanSummary;
    QStringList runtimePlanWarnings;
    int localModelCount = 0;

    int timeoutMs = 60000;
    int maxRetries = 0;
    int retryDelayMs = 400;
    int llamaCppServerPort = 18080;
    int llamaCppContextSize = 4096;
    // Negative means auto GPU offload for managed llama.cpp, 0 disables offload,
    // positive values request an explicit layer count.
    int llamaCppGpuLayers = -1;
    int llamaCppThreadCount = 0;
    int llamaCppStartupTimeoutMs = 30000;
    int resolvedLlamaCppGpuLayers = -1;
    int resolvedLlamaCppThreadCount = 0;
    int resolvedLlamaCppContextSize = 0;
};

} // namespace qtllm
