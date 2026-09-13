#pragma once

#include "clienttoolpolicyrepository.h"
#include "toolexecutionlayer.h"

#include "../protocol/toolcallprotocolrouter.h"

#include <QHash>

#include <memory>

namespace qtllm::tools::runtime {

struct ToolLoopOutcome
{
    bool hasFollowUpPrompt = false;
    QString followUpPrompt;
    bool terminatedByFailureGuard = false;
    int consecutiveFailures = 0;
    int roundIndex = 0;
    ToolExecutionMode executionMode = ToolExecutionMode::Internal;
    QList<ToolCallRequest> pendingToolCalls;
    QList<ToolExecutionResult> toolResults;
    bool awaitingAuthorization = false;
    bool awaitingExternalResults = false;
};

class ToolCallOrchestrator
{
public:
    ToolCallOrchestrator();

    void setExecutionLayer(const std::shared_ptr<ToolExecutionLayer> &executionLayer);
    void setPolicyRepository(const std::shared_ptr<ClientToolPolicyRepository> &policyRepository);

    void setMaxConsecutiveFailures(int maxConsecutiveFailures);
    void setMaxRounds(int maxRounds);
    void setExecutionMode(ToolExecutionMode mode);

    void resetSession(const QString &clientId, const QString &sessionId);

    ToolLoopOutcome processAssistantMessage(const QString &modelName,
                                           const QString &modelVendor,
                                           const QString &providerName,
                                           const QString &assistantText,
                                           const ToolExecutionContext &context) const;

    ToolLoopOutcome processAssistantResponse(const QString &modelName,
                                            const QString &modelVendor,
                                            const QString &providerName,
                                            const LlmResponse &response,
                                            const ToolExecutionContext &context) const;

    ToolLoopOutcome completeExternalResults(
        const QString &modelName,
        const QString &modelVendor,
        const QString &providerName,
        const QString &assistantText,
        const QList<ToolCallRequest> &pendingRequests,
        const QList<ToolExecutionResult> &results,
        const ToolExecutionContext &context) const;

private:
    struct ToolLoopState
    {
        int rounds = 0;
        int consecutiveFailures = 0;
    };

    QString stateKey(const ToolExecutionContext &context) const;
    ToolLoopOutcome processToolCalls(const std::shared_ptr<protocol::IToolCallProtocolAdapter> &adapter,
                                     const QList<ToolCallRequest> &requests,
                                     const QString &assistantText,
                                     const ToolExecutionContext &context) const;

private:
    std::shared_ptr<ToolExecutionLayer> m_executionLayer;
    std::shared_ptr<ClientToolPolicyRepository> m_policyRepository;
    std::shared_ptr<protocol::ToolCallProtocolRouter> m_protocolRouter;
    int m_maxConsecutiveFailures = 3;
    int m_maxRounds = 5;
    ToolExecutionMode m_executionMode = ToolExecutionMode::Internal;
    mutable QHash<QString, ToolLoopState> m_stateBySession;
};

} // namespace qtllm::tools::runtime
