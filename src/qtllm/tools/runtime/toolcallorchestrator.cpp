#include "toolcallorchestrator.h"

#include "../../identity/compactid.h"
#include "../../logging/qtllmlogger.h"
#include "../../toolsinside/toolsinsideruntime.h"
#include "../../toolsinside/toolsinsidetracerecorder.h"

#include <optional>
#include <QJsonObject>

namespace qtllm::tools::runtime {

namespace {

logging::LogContext logContextFromExecution(const ToolExecutionContext &context)
{
    logging::LogContext logContext;
    logContext.clientId = context.clientId;
    logContext.sessionId = context.sessionId;
    logContext.requestId = context.requestId;
    logContext.traceId = context.traceId;
    return logContext;
}

} // namespace

ToolCallOrchestrator::ToolCallOrchestrator()
    : m_executionLayer(std::make_shared<ToolExecutionLayer>())
    , m_protocolRouter(std::make_shared<protocol::ToolCallProtocolRouter>())
{
}

void ToolCallOrchestrator::setExecutionLayer(const std::shared_ptr<ToolExecutionLayer> &executionLayer)
{
    if (executionLayer) {
        m_executionLayer = executionLayer;
    }
}

void ToolCallOrchestrator::setPolicyRepository(const std::shared_ptr<ClientToolPolicyRepository> &policyRepository)
{
    m_policyRepository = policyRepository;
}

void ToolCallOrchestrator::setMaxConsecutiveFailures(int maxConsecutiveFailures)
{
    m_maxConsecutiveFailures = qMax(1, maxConsecutiveFailures);
}

void ToolCallOrchestrator::setMaxRounds(int maxRounds)
{
    m_maxRounds = qMax(1, maxRounds);
}

void ToolCallOrchestrator::resetSession(const QString &clientId, const QString &sessionId)
{
    const QString key = clientId.trimmed() + QStringLiteral("::") + sessionId.trimmed();
    m_stateBySession.remove(key);
}

ToolLoopOutcome ToolCallOrchestrator::processAssistantMessage(const QString &modelName,
                                                              const QString &modelVendor,
                                                              const QString &providerName,
                                                              const QString &assistantText,
                                                              const ToolExecutionContext &context) const
{
    ToolLoopOutcome outcome;
    if (!m_protocolRouter || !m_executionLayer) {
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.loop"),
                                              QStringLiteral("Tool loop skipped because dependencies are missing"),
                                              logContextFromExecution(context));
        return outcome;
    }

    const std::shared_ptr<protocol::IToolCallProtocolAdapter> adapter =
        m_protocolRouter->route(modelName, modelVendor, providerName);
    if (!adapter) {
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.loop"),
                                              QStringLiteral("Tool loop skipped because no protocol adapter was resolved"),
                                              logContextFromExecution(context),
                                              QJsonObject{{QStringLiteral("modelName"), modelName},
                                                          {QStringLiteral("providerName"), providerName}});
        return outcome;
    }

    const QList<ToolCallRequest> requests = adapter->parseToolCalls(assistantText);
    return processToolCalls(adapter, requests, assistantText, context);
}

ToolLoopOutcome ToolCallOrchestrator::processAssistantResponse(const QString &modelName,
                                                               const QString &modelVendor,
                                                               const QString &providerName,
                                                               const LlmResponse &response,
                                                               const ToolExecutionContext &context) const
{
    ToolLoopOutcome outcome;
    if (!m_protocolRouter || !m_executionLayer) {
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.loop"),
                                              QStringLiteral("Tool loop skipped because dependencies are missing"),
                                              logContextFromExecution(context));
        return outcome;
    }

    const std::shared_ptr<protocol::IToolCallProtocolAdapter> adapter =
        m_protocolRouter->route(modelName, modelVendor, providerName);
    if (!adapter) {
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.loop"),
                                              QStringLiteral("Tool loop skipped because no protocol adapter was resolved"),
                                              logContextFromExecution(context),
                                              QJsonObject{{QStringLiteral("modelName"), modelName},
                                                          {QStringLiteral("providerName"), providerName}});
        return outcome;
    }

    QList<ToolCallRequest> requests;
    const QVector<LlmToolCall> toolCalls = response.assistantMessage.toolCalls;
    for (const LlmToolCall &call : toolCalls) {
        ToolCallRequest request;
        request.callId = call.id;
        request.toolId = call.name;
        request.arguments = call.arguments;
        if (!request.toolId.trimmed().isEmpty()) {
            requests.append(request);
        }
    }

    if (requests.isEmpty()) {
        requests = adapter->parseToolCalls(response.text);
    }

    logging::QtLlmLogger::instance().debug(QStringLiteral("tool.loop"),
                                           QStringLiteral("Parsed tool calls from assistant response"),
                                           logContextFromExecution(context),
                                           QJsonObject{{QStringLiteral("toolCallCount"), requests.size()},
                                                       {QStringLiteral("finishReason"), response.finishReason}});
    return processToolCalls(adapter, requests, response.text, context);
}

ToolLoopOutcome ToolCallOrchestrator::processToolCalls(const std::shared_ptr<protocol::IToolCallProtocolAdapter> &adapter,
                                                       const QList<ToolCallRequest> &requests,
                                                       const QString &assistantText,
                                                       const ToolExecutionContext &context) const
{
    ToolLoopOutcome outcome;

    const QString key = stateKey(context);
    if (key.isEmpty()) {
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.loop"),
                                              QStringLiteral("Tool loop skipped because state key is empty"),
                                              logContextFromExecution(context));
        return outcome;
    }

    ToolLoopState state = m_stateBySession.value(key);
    const int currentRound = state.rounds + 1;
    outcome.roundIndex = currentRound;
    if (state.rounds >= m_maxRounds) {
        outcome.terminatedByFailureGuard = true;
        outcome.consecutiveFailures = state.consecutiveFailures;
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.loop"),
                                              QStringLiteral("Tool loop stopped because max rounds was reached"),
                                              logContextFromExecution(context),
                                              QJsonObject{{QStringLiteral("rounds"), state.rounds},
                                                          {QStringLiteral("maxRounds"), m_maxRounds}});
        return outcome;
    }

    if (requests.isEmpty()) {
        state.rounds = 0;
        state.consecutiveFailures = 0;
        m_stateBySession.insert(key, state);
        return outcome;
    }

    QList<ToolCallRequest> executionRequests = requests;
    for (ToolCallRequest &request : executionRequests) {
        if (request.externalCallId.trimmed().isEmpty()) {
            request.externalCallId = request.callId;
        }
        if (request.internalToolCallId.trimmed().isEmpty()) {
            request.internalToolCallId = identity::generateId(identity::IdKind::ToolCall);
        }
    }

    toolsinside::ToolsInsideRuntime::instance().recorder()->recordToolCallsParsed(context,
                                                                                   context.requestId,
                                                                                   adapter->adapterId(),
                                                                                   currentRound,
                                                                                   executionRequests);

    ClientToolPolicy policy;
    policy.clientId = context.clientId;
    if (m_policyRepository) {
        const std::optional<ClientToolPolicy> loaded = m_policyRepository->loadPolicy(context.clientId, nullptr);
        if (loaded.has_value()) {
            policy = *loaded;
        }
    }

    ToolExecutionContext executionContext = context;
    executionContext.extra.insert(QStringLiteral("toolLoopRoundIndex"), currentRound);
    toolsinside::ToolsInsideRuntime::instance().recorder()->recordToolBatchStarted(executionContext,
                                                                                    context.requestId,
                                                                                    currentRound,
                                                                                    executionRequests.size());
    const QList<ToolExecutionResult> results = m_executionLayer->executeBatch(executionRequests, executionContext, policy);
    int failureCount = 0;
    for (const ToolExecutionResult &result : results) {
        if (!result.success) {
            ++failureCount;
        }
    }

    state.rounds += 1;
    if (!results.isEmpty() && failureCount == results.size()) {
        state.consecutiveFailures += 1;
    } else {
        state.consecutiveFailures = 0;
    }

    m_stateBySession.insert(key, state);

    outcome.consecutiveFailures = state.consecutiveFailures;
    if (state.consecutiveFailures >= m_maxConsecutiveFailures || state.rounds >= m_maxRounds) {
        outcome.terminatedByFailureGuard = true;
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.loop"),
                                              QStringLiteral("Tool loop terminated by failure guard"),
                                              logContextFromExecution(context),
                                              QJsonObject{{QStringLiteral("rounds"), state.rounds},
                                                          {QStringLiteral("consecutiveFailures"), state.consecutiveFailures},
                                                          {QStringLiteral("maxConsecutiveFailures"), m_maxConsecutiveFailures}});
        toolsinside::ToolsInsideRuntime::instance().recorder()->recordFailureGuard(executionContext,
                                                                                    context.requestId,
                                                                                    currentRound,
                                                                                    state.consecutiveFailures,
                                                                                    QStringLiteral("failure_guard"));
        return outcome;
    }

    outcome.hasFollowUpPrompt = true;
    outcome.followUpPrompt = adapter->buildFollowUpPrompt(assistantText, results);
    toolsinside::ToolsInsideRuntime::instance().recorder()->recordFollowUpPrompt(executionContext,
                                                                                 context.requestId,
                                                                                 currentRound,
                                                                                 outcome.followUpPrompt,
                                                                                 results);
    logging::QtLlmLogger::instance().info(QStringLiteral("tool.loop"),
                                          QStringLiteral("Tool loop generated follow-up prompt"),
                                          logContextFromExecution(context),
                                          QJsonObject{{QStringLiteral("resultCount"), results.size()},
                                                      {QStringLiteral("failureCount"), failureCount},
                                                      {QStringLiteral("rounds"), state.rounds}});
    return outcome;
}

QString ToolCallOrchestrator::stateKey(const ToolExecutionContext &context) const
{
    const QString clientId = context.clientId.trimmed();
    const QString sessionId = context.sessionId.trimmed();
    if (clientId.isEmpty() || sessionId.isEmpty()) {
        return QString();
    }
    return clientId + QStringLiteral("::") + sessionId;
}

} // namespace qtllm::tools::runtime
