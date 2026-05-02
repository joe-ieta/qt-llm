#pragma once

#include "llmconfig.h"
#include "llmtypes.h"

#include <QObject>
#include <memory>

namespace qtllm {

class ILLMProvider;
class HttpExecutor;
class StreamChunkParser;

namespace runtime {
class ManagedLlamaCppRuntime;
}

namespace tools::runtime {
class ToolCallOrchestrator;
}

class QtLLMClient : public QObject
{
    Q_OBJECT
public:
    explicit QtLLMClient(QObject *parent = nullptr);
    ~QtLLMClient() override;

    void setConfig(const LlmConfig &config);
    void setProvider(std::unique_ptr<ILLMProvider> provider);
    bool setProviderByName(const QString &providerName);

    void setToolCallOrchestrator(const std::shared_ptr<tools::runtime::ToolCallOrchestrator> &orchestrator);
    void setToolLoopContext(const QString &clientId, const QString &sessionId, const QString &traceId = QString());

    void sendPrompt(const QString &prompt);
    void sendRequest(const LlmRequest &request);
    void cancelCurrentRequest();

signals:
    void tokenReceived(const QString &token);
    void reasoningTokenReceived(const QString &token);
    void completed(const QString &text);
    void responseReceived(const LlmResponse &response);
    void errorOccurred(const QString &message);
    void providerPayloadPrepared(const QString &url, const QString &payloadJson);

private:
    void wireExecutor();
    void dispatchRequest(const LlmRequest &request);
    bool ensureManagedRuntime();

private:
    LlmConfig m_config;
    std::unique_ptr<ILLMProvider> m_provider;
    std::unique_ptr<runtime::ManagedLlamaCppRuntime> m_llamaCppRuntime;
    HttpExecutor *m_executor;
    std::unique_ptr<StreamChunkParser> m_streamParser;
    QString m_accumulatedText;
    QString m_accumulatedReasoning;
    QString m_activeRequestId;
    LlmRequest m_activeRequest;
    std::shared_ptr<tools::runtime::ToolCallOrchestrator> m_toolOrchestrator;
    QString m_toolLoopClientId;
    QString m_toolLoopSessionId;
    QString m_toolLoopTraceId;
};

} // namespace qtllm


