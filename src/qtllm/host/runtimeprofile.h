#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace qtllm::host {

struct RuntimeProfile
{
    QString providerName;
    QString baseUrl;
    QString apiKey;
    QString model;
    QString modelVendor;
    bool stream = true;

    QString workspaceRoot;
    QString llamaCppRuntimeRoot;
    QString llamaCppExecutablePath;
    QString llamaCppModelPath;
    QStringList llamaCppExtraArgs;
    bool providerAvailable = true;
    QString providerAvailabilityStatus;
    QString providerAvailabilityMessage;
    QString resolvedRuntimeRoot;
    QString resolvedModelPath;
    int localModelCount = 0;
    int timeoutMs = 60000;
    int maxRetries = 0;
    int retryDelayMs = 400;
    int llamaCppServerPort = 18080;
    int llamaCppContextSize = 4096;
    int llamaCppGpuLayers = -1;
    int llamaCppThreadCount = 0;
    int llamaCppStartupTimeoutMs = 30000;
};

struct ChatRequest
{
    QString systemPrompt;
    QString userPrompt;
    QString clientId;
    QString sessionId;
    QString traceId;
    QVariantMap metadata;
};

struct ChatResult
{
    bool success = false;
    QString text;
    QString providerName;
    QString model;
    QString clientId;
    QString sessionId;
    QString traceId;
    QString errorCode;
    QString errorMessage;
    QVariantMap metadata;
};

struct LocalModelInfo
{
    QString id;
    QString displayName;
    QString filePath;
    QString providerName;
    QString runtimeRoot;
    qint64 sizeBytes = 0;
};

} // namespace qtllm::host

Q_DECLARE_METATYPE(qtllm::host::RuntimeProfile)
Q_DECLARE_METATYPE(qtllm::host::ChatRequest)
Q_DECLARE_METATYPE(qtllm::host::ChatResult)
Q_DECLARE_METATYPE(qtllm::host::LocalModelInfo)
