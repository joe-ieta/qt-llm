#include "runtimeprofilemapper.h"

namespace qtllm::host {

LlmConfig RuntimeProfileMapper::toConfig(const RuntimeProfile &profile)
{
    LlmConfig config;
    config.providerName = profile.providerName.trimmed();
    config.baseUrl = profile.baseUrl.trimmed();
    config.apiKey = profile.apiKey.trimmed();
    config.model = profile.model.trimmed();
    config.modelVendor = profile.modelVendor.trimmed();
    config.stream = profile.stream;
    config.llamaCppRuntimeRoot = profile.llamaCppRuntimeRoot.trimmed();
    config.llamaCppExecutablePath = profile.llamaCppExecutablePath.trimmed();
    config.llamaCppModelPath = profile.llamaCppModelPath.trimmed();
    config.llamaCppExtraArgs = profile.llamaCppExtraArgs;
    config.providerAvailable = profile.providerAvailable;
    config.providerAvailabilityStatus = profile.providerAvailabilityStatus;
    config.providerAvailabilityMessage = profile.providerAvailabilityMessage;
    config.resolvedRuntimeRoot = profile.resolvedRuntimeRoot;
    config.resolvedModelPath = profile.resolvedModelPath;
    config.llamaCppGpuMode = profile.llamaCppGpuMode.trimmed();
    config.llamaCppPerformanceProfile = profile.llamaCppPerformanceProfile.trimmed();
    config.llamaCppContextMode = profile.llamaCppContextMode.trimmed();
    config.resolvedLlamaCppGpuMode = profile.resolvedLlamaCppGpuMode;
    config.runtimePlanSummary = profile.runtimePlanSummary;
    config.runtimePlanWarnings = profile.runtimePlanWarnings;
    config.localModelCount = profile.localModelCount;
    config.timeoutMs = profile.timeoutMs;
    config.maxRetries = profile.maxRetries;
    config.retryDelayMs = profile.retryDelayMs;
    config.llamaCppServerPort = profile.llamaCppServerPort;
    config.llamaCppContextSize = profile.llamaCppContextSize;
    config.llamaCppGpuLayers = profile.llamaCppGpuLayers;
    config.llamaCppThreadCount = profile.llamaCppThreadCount;
    config.llamaCppStartupTimeoutMs = profile.llamaCppStartupTimeoutMs;
    config.resolvedLlamaCppGpuLayers = profile.resolvedLlamaCppGpuLayers;
    config.resolvedLlamaCppThreadCount = profile.resolvedLlamaCppThreadCount;
    config.resolvedLlamaCppContextSize = profile.resolvedLlamaCppContextSize;
    return config;
}

RuntimeProfile RuntimeProfileMapper::fromConfig(const LlmConfig &config)
{
    RuntimeProfile profile;
    profile.providerName = config.providerName;
    profile.baseUrl = config.baseUrl;
    profile.apiKey = config.apiKey;
    profile.model = config.model;
    profile.modelVendor = config.modelVendor;
    profile.stream = config.stream;
    profile.llamaCppRuntimeRoot = config.llamaCppRuntimeRoot;
    profile.llamaCppExecutablePath = config.llamaCppExecutablePath;
    profile.llamaCppModelPath = config.llamaCppModelPath;
    profile.llamaCppExtraArgs = config.llamaCppExtraArgs;
    profile.providerAvailable = config.providerAvailable;
    profile.providerAvailabilityStatus = config.providerAvailabilityStatus;
    profile.providerAvailabilityMessage = config.providerAvailabilityMessage;
    profile.resolvedRuntimeRoot = config.resolvedRuntimeRoot;
    profile.resolvedModelPath = config.resolvedModelPath;
    profile.llamaCppGpuMode = config.llamaCppGpuMode;
    profile.llamaCppPerformanceProfile = config.llamaCppPerformanceProfile;
    profile.llamaCppContextMode = config.llamaCppContextMode;
    profile.resolvedLlamaCppGpuMode = config.resolvedLlamaCppGpuMode;
    profile.runtimePlanSummary = config.runtimePlanSummary;
    profile.runtimePlanWarnings = config.runtimePlanWarnings;
    profile.localModelCount = config.localModelCount;
    profile.timeoutMs = config.timeoutMs;
    profile.maxRetries = config.maxRetries;
    profile.retryDelayMs = config.retryDelayMs;
    profile.llamaCppServerPort = config.llamaCppServerPort;
    profile.llamaCppContextSize = config.llamaCppContextSize;
    profile.llamaCppGpuLayers = config.llamaCppGpuLayers;
    profile.llamaCppThreadCount = config.llamaCppThreadCount;
    profile.llamaCppStartupTimeoutMs = config.llamaCppStartupTimeoutMs;
    profile.resolvedLlamaCppGpuLayers = config.resolvedLlamaCppGpuLayers;
    profile.resolvedLlamaCppThreadCount = config.resolvedLlamaCppThreadCount;
    profile.resolvedLlamaCppContextSize = config.resolvedLlamaCppContextSize;
    return profile;
}

LlmRequest RuntimeProfileMapper::toRequest(const RuntimeProfile &profile, const ChatRequest &request)
{
    LlmRequest llmRequest;
    llmRequest.model = profile.model.trimmed();
    llmRequest.stream = profile.stream;
    if (!request.systemPrompt.trimmed().isEmpty()) {
        llmRequest.messages.append({QStringLiteral("system"), request.systemPrompt.trimmed()});
    }
    llmRequest.messages.append({QStringLiteral("user"), request.userPrompt.trimmed()});
    return llmRequest;
}

} // namespace qtllm::host
