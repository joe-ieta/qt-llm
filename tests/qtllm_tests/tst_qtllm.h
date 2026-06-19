#ifndef TST_QTLLM_H
#define TST_QTLLM_H

#include <QObject>

class QtLlmCoreTests : public QObject
{
    Q_OBJECT

private slots:
    void compactIdGeneratesExpectedPrefixes();
    void compactIdOrdersMonotonicallyWithinProcess();
    void compactIdValidatesAndDecodes();
    void compactIdRejectsMalformedValues();
    void conversationClientFactoryGeneratesCompactClientIds();
    void conversationClientGeneratesCompactSessionIds();
    void conversationRepositoryPersistsCompactConversationIds();
    void toolStudioGeneratesCompactWorkspaceNodeAndPlacementIds();
    void toolStudioExportPackageUsesCompactPackageId();
    void providerFactoryCreatesKnownProviders();
    void providerFactoryCreatesVendorAliases();
    void providerFactoryRejectsUnknownProvider();
    void managedLlamaCppRuntimeListsSupplementalQtllmModels();
    void managedLlamaCppRuntimeDefaultsGpuLayersToAuto();
    void managedLlamaCppRuntimeCpuOnlyPolicyDisablesGpuLayers();
    void managedLlamaCppRuntimeReusesExistingServerPort();
    void managedLlamaCppRuntimeWaitsForHttpReadiness();
    void openAiCompatibleBuildRequestNormalizesPath();
    void openAiCompatibleBuildRequestAnthropic();
    void openAiCompatibleBuildRequestGoogle();
    void openAiCompatibleBuildPayloadProducesJson();
    void openAiCompatibleBuildPayloadAnthropicTools();
    void openAiCompatibleParseResponse();
    void openAiCompatibleParseAnthropicResponse();
    void openAiCompatibleParseGoogleResponse();
    void openAiCompatibleParseStreamTokens();
    void openAiCompatibleParseEventPrefixedSse();
    void openAiCompatibleParseStreamDeltasReasoningAndToolCalls();
    void openAiCompatibleParseOllamaJsonLines();
    void openAiBuildRequestNormalizesResponsesPath();
    void openAiBuildPayloadSanitizesTools();
    void openAiParseResponseParsesFunctionCalls();
    void openAiParseResponseParsesEventPrefixedSse();
    void openAiParseResponseParsesStreamingFunctionCallEvents();
    void mcpToolSyncRegistersImportedTools();
    void toolExecutionLayerExecutesMcpToolByInvocationName();
    void fileLogSinkRotatesPerClient();
    void defaultMcpClientReadsToolsOverStdio();
    void defaultMcpClientCallsToolOverHttpLikeTransport();
    void streamChunkParserHandlesFragmentedInput();
    void streamChunkParserTakePendingLine();
};

#endif
