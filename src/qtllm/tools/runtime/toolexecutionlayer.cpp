#include "toolexecutionlayer.h"

#include "builtinexecutors.h"
#include "../builtintools.h"
#include "../mcp/defaultmcpclient.h"
#include "../mcp/mcpserverregistry.h"
#include "../../logging/qtllmlogger.h"
#include "../../events/llmeventdispatcher.h"

#include <QElapsedTimer>
#include <QJsonObject>

#include <future>
#include <vector>

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

ToolExecutionLayer::ToolExecutionLayer()
    : m_registry(std::make_shared<ToolExecutorRegistry>())
    , m_mcpClient(std::make_shared<mcp::DefaultMcpClient>())
{
    m_registry->registerExecutor(std::make_shared<CurrentTimeToolExecutor>());
    m_registry->registerExecutor(std::make_shared<CurrentWeatherToolExecutor>());
}

void ToolExecutionLayer::setRegistry(const std::shared_ptr<ToolExecutorRegistry> &registry)
{
    if (registry) {
        m_registry = registry;
    }
}

void ToolExecutionLayer::setToolRegistry(const std::shared_ptr<tools::LlmToolRegistry> &toolRegistry)
{
    m_toolRegistry = toolRegistry;
}

void ToolExecutionLayer::setPolicy(const ToolExecutionPolicy &policy)
{
    m_policy = policy;
}

void ToolExecutionLayer::setHooks(const std::shared_ptr<ToolRuntimeHooks> &hooks)
{
    m_hooks = hooks;
}

void ToolExecutionLayer::setDryRunFailureMode(bool enabled)
{
    m_dryRunFailureMode = enabled;
}

void ToolExecutionLayer::setMcpClient(const std::shared_ptr<mcp::IMcpClient> &mcpClient)
{
    m_mcpClient = mcpClient;
}

void ToolExecutionLayer::setMcpServerRegistry(const std::shared_ptr<mcp::McpServerRegistry> &serverRegistry)
{
    m_mcpServerRegistry = serverRegistry;
}

ToolBatchPreparation ToolExecutionLayer::prepareBatch(
    const QList<ToolCallRequest> &requests,
    const ToolExecutionContext &context,
    const ClientToolPolicy &clientPolicy) const
{
    ToolBatchPreparation preparation;
    const int limit = qMax(1, clientPolicy.maxToolsPerTurn);

    for (int index = 0; index < requests.size(); ++index) {
        ToolCallRequest request = requests.at(index);
        request.toolId = resolveToolId(request.toolId);

        ToolExecutionResult result;
        result.callId = request.callId;
        result.externalCallId = request.externalCallId.trimmed().isEmpty()
            ? request.callId
            : request.externalCallId;
        result.internalToolCallId = request.internalToolCallId;
        result.toolId = request.toolId;

        if (index >= limit) {
            result.status = ToolExecutionStatus::LimitExceeded;
            result.errorCode = QStringLiteral("tool_limit_exceeded");
            result.errorMessage = QStringLiteral("Tool call exceeds maxToolsPerTurn");
            preparation.terminalResults.append(result);
            continue;
        }

        if (!m_policy.isToolAllowed(request.toolId, clientPolicy)) {
            result.status = ToolExecutionStatus::Denied;
            result.errorCode = QStringLiteral("tool_denied");
            result.errorMessage = QStringLiteral("Tool is not allowed by client policy");
            preparation.terminalResults.append(result);
            continue;
        }

        const ToolAuthorizationDecision decision = m_hooks
            ? m_hooks->authorize(request, context)
            : ToolAuthorizationDecision::Allow;
        if (decision == ToolAuthorizationDecision::Deny) {
            result.status = ToolExecutionStatus::Denied;
            result.errorCode = QStringLiteral("tool_authorization_denied");
            result.errorMessage = QStringLiteral("Tool execution was denied by authorization hook");
            preparation.terminalResults.append(result);
            continue;
        }
        if (decision == ToolAuthorizationDecision::Pending) {
            result.status = ToolExecutionStatus::AwaitingAuthorization;
            result.errorCode = QStringLiteral("tool_authorization_pending");
            result.errorMessage = QStringLiteral("Tool execution is waiting for authorization");
            preparation.terminalResults.append(result);
            preparation.awaitingAuthorization = true;
            continue;
        }

        preparation.readyRequests.append(request);
    }

    return preparation;
}

QList<ToolExecutionResult> ToolExecutionLayer::executeBatch(const QList<ToolCallRequest> &requests,
                                                            const ToolExecutionContext &context,
                                                            const ClientToolPolicy &clientPolicy) const
{
    const ToolBatchPreparation preparation = prepareBatch(requests, context, clientPolicy);
    QList<ToolExecutionResult> results = preparation.terminalResults;
    const QList<ToolCallRequest> &readyRequests = preparation.readyRequests;
    if (readyRequests.isEmpty()) {
        return results;
    }

    logging::QtLlmLogger::instance().debug(QStringLiteral("tool.execution"),
                                           QStringLiteral("Executing tool batch"),
                                           logContextFromExecution(context),
                                           QJsonObject{{QStringLiteral("requestedCount"), requests.size()},
                                                       {QStringLiteral("executedCount"), readyRequests.size()},
                                                       {QStringLiteral("parallel"), m_policy.enableParallelExecution}});

    if (!m_policy.enableParallelExecution || m_policy.failFast || readyRequests.size() < 2) {
        for (const ToolCallRequest &request : readyRequests) {
            ToolExecutionResult result = executeSingle(request, context, clientPolicy);
            results.append(result);
            if (m_policy.failFast && !result.success) {
                logging::QtLlmLogger::instance().warn(
                    QStringLiteral("tool.execution"),
                    QStringLiteral("Tool batch aborted because fail-fast is enabled"),
                    logContextFromExecution(context),
                    QJsonObject{{QStringLiteral("toolId"), result.toolId},
                                {QStringLiteral("callId"), result.callId}});
                break;
            }
        }
        return results;
    }

    int parallelLimit = qMax(1, m_policy.maxParallelCalls);
    for (const ToolCallRequest &request : readyRequests) {
        const int toolLimit = m_policy.toolConcurrencyOverrides.value(request.toolId, 0);
        if (toolLimit > 0) {
            parallelLimit = qMin(parallelLimit, toolLimit);
        }
    }

    for (int offset = 0; offset < readyRequests.size(); offset += parallelLimit) {
        const int batchSize = qMin(parallelLimit, readyRequests.size() - offset);
        std::vector<std::future<ToolExecutionResult>> futures;
        futures.reserve(static_cast<std::size_t>(batchSize));
        for (int index = 0; index < batchSize; ++index) {
            const ToolCallRequest request = readyRequests.at(offset + index);
            futures.emplace_back(std::async(std::launch::async,
                                            [this, request, context, clientPolicy]() {
                                                return executeSingle(request,
                                                                     context,
                                                                     clientPolicy);
                                            }));
        }
        for (std::future<ToolExecutionResult> &future : futures) {
            results.append(future.get());
        }
    }

    return results;
}

bool ToolExecutionLayer::cancelBySession(const QString &clientId, const QString &sessionId) const
{
    const QString key = clientId + QChar(0x1f) + sessionId;
    QList<ActiveExecution> active;
    {
        QMutexLocker locker(&m_activeState->mutex);
        active = m_activeState->executions.value(key);
    }

    bool canceled = false;
    for (const ActiveExecution &execution : active) {
        if (execution.executor && execution.executor->supportsCancellation()) {
            canceled = execution.executor->cancel(execution.callId) || canceled;
        }
    }
    return canceled;
}

QString ToolExecutionLayer::resolveToolId(const QString &toolIdOrName) const
{
    if (!m_toolRegistry) {
        return toolIdOrName.trimmed();
    }
    return m_toolRegistry->resolveToolId(toolIdOrName);
}

ToolExecutionResult ToolExecutionLayer::executeSingle(const ToolCallRequest &request,
                                                      const ToolExecutionContext &context,
                                                      const ClientToolPolicy &clientPolicy) const
{
    const QString resolvedToolId = resolveToolId(request.toolId);
    const int roundIndex = context.extra.value(QStringLiteral("toolLoopRoundIndex")).toInt();

    ToolExecutionResult result;
    result.callId = request.callId;
    result.externalCallId = request.externalCallId.trimmed().isEmpty() ? request.callId : request.externalCallId;
    result.internalToolCallId = request.internalToolCallId;
    result.toolId = resolvedToolId;

    ToolCallRequest resolvedRequest = request;
    resolvedRequest.toolId = resolvedToolId;
    events::LlmEventDispatcher::instance().recordToolCallStarted(context,
                                                                                  context.requestId,
                                                                                  roundIndex,
                                                                                  resolvedRequest);

    auto finalizeResult = [&](ToolExecutionResult out) {
        if (out.callId.isEmpty()) {
            out.callId = resolvedRequest.callId;
        }
        if (out.externalCallId.isEmpty()) {
            out.externalCallId = resolvedRequest.externalCallId.trimmed().isEmpty()
                ? resolvedRequest.callId
                : resolvedRequest.externalCallId;
        }
        if (out.internalToolCallId.isEmpty()) {
            out.internalToolCallId = resolvedRequest.internalToolCallId;
        }
        if (out.toolId.isEmpty()) {
            out.toolId = resolvedRequest.toolId;
        }
        if (out.status == ToolExecutionStatus::Pending) {
            out.status = out.success ? ToolExecutionStatus::Succeeded
                                     : ToolExecutionStatus::Failed;
        }
        events::LlmEventDispatcher::instance().recordToolCallFinished(context,
                                                                                       context.requestId,
                                                                                       roundIndex,
                                                                                       resolvedRequest,
                                                                                       out);
        return out;
    };

    if (!m_policy.isToolAllowed(resolvedToolId, clientPolicy)) {
        result.errorCode = QStringLiteral("tool_denied");
        result.errorMessage = QStringLiteral("Tool is not allowed by client policy");
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.execution"),
                                              QStringLiteral("Tool execution denied by policy"),
                                              logContextFromExecution(context),
                                              QJsonObject{{QStringLiteral("toolId"), resolvedToolId},
                                                          {QStringLiteral("callId"), request.callId}});
        return finalizeResult(result);
    }

    if (m_dryRunFailureMode) {
        result.errorCode = QStringLiteral("tool_unavailable");
        result.errorMessage = QStringLiteral("Tool framework is active but real execution is not enabled");
        result.retryable = false;
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.execution"),
                                              QStringLiteral("Tool execution aborted because dry-run failure mode is enabled"),
                                              logContextFromExecution(context),
                                              QJsonObject{{QStringLiteral("toolId"), resolvedToolId},
                                                          {QStringLiteral("callId"), request.callId}});
        return finalizeResult(result);
    }

    if (resolvedRequest.toolId.startsWith(QStringLiteral("mcp::"))) {
        return finalizeResult(executeMcpTool(resolvedRequest, context));
    }

    if (!m_registry) {
        result.errorCode = QStringLiteral("registry_missing");
        result.errorMessage = QStringLiteral("ToolExecutorRegistry is not configured");
        logging::QtLlmLogger::instance().error(QStringLiteral("tool.execution"),
                                               QStringLiteral("Tool execution failed because executor registry is missing"),
                                               logContextFromExecution(context));
        return finalizeResult(result);
    }

    const std::shared_ptr<IToolExecutor> executor = m_registry->find(resolvedRequest.toolId);
    if (!executor) {
        if (!tools::isBuiltInToolId(resolvedRequest.toolId)) {
            result.errorCode = QStringLiteral("external_executor_not_implemented");
            result.errorMessage = QStringLiteral("External tool execution is not implemented for tool: ") + resolvedRequest.toolId;
            result.retryable = false;
            logging::QtLlmLogger::instance().warn(QStringLiteral("tool.execution"),
                                                  QStringLiteral("External tool execution path is not implemented"),
                                                  logContextFromExecution(context),
                                                  QJsonObject{{QStringLiteral("toolId"), resolvedRequest.toolId}});
            return finalizeResult(result);
        }

        result.errorCode = QStringLiteral("executor_missing");
        result.errorMessage = QStringLiteral("No executor registered for built-in tool: ") + resolvedRequest.toolId;
        logging::QtLlmLogger::instance().error(QStringLiteral("tool.execution"),
                                               QStringLiteral("Built-in tool executor is missing"),
                                               logContextFromExecution(context),
                                               QJsonObject{{QStringLiteral("toolId"), resolvedRequest.toolId}});
        return finalizeResult(result);
    }

    if (m_hooks) {
        m_hooks->beforeExecute(resolvedRequest, context);
    }

    QElapsedTimer timer;
    timer.start();

    const int timeoutMs = m_policy.toolTimeoutOverrides.value(
        resolvedRequest.toolId, m_policy.defaultTimeoutMs);
    const int retryCount = resolvedRequest.retryAllowed
            && !resolvedRequest.idempotencyKey.trimmed().isEmpty()
        ? qMax(0, m_policy.maxRetries)
        : 0;
    const QString sessionKey = context.clientId + QChar(0x1f) + context.sessionId;
    ToolExecutionResult executed;
    for (int attempt = 0; attempt <= retryCount; ++attempt) {
        {
            QMutexLocker locker(&m_activeState->mutex);
            m_activeState->executions[sessionKey].append(
                {resolvedRequest.callId, executor});
        }

        executed = executor->execute(resolvedRequest, context);

        {
            QMutexLocker locker(&m_activeState->mutex);
            auto activeIt = m_activeState->executions.find(sessionKey);
            if (activeIt != m_activeState->executions.end()) {
                for (int index = 0; index < activeIt->size(); ++index) {
                    if (activeIt->at(index).callId == resolvedRequest.callId) {
                        activeIt->removeAt(index);
                        break;
                    }
                }
                if (activeIt->isEmpty()) {
                    m_activeState->executions.erase(activeIt);
                }
            }
        }

        executed.attemptCount = attempt + 1;
        if (executed.success || !executed.retryable || attempt == retryCount) {
            break;
        }
    }
    if (executed.callId.isEmpty()) {
        executed.callId = resolvedRequest.callId;
    }
    if (executed.externalCallId.isEmpty()) {
        executed.externalCallId = resolvedRequest.externalCallId.trimmed().isEmpty()
            ? resolvedRequest.callId
            : resolvedRequest.externalCallId;
    }
    if (executed.internalToolCallId.isEmpty()) {
        executed.internalToolCallId = resolvedRequest.internalToolCallId;
    }
    if (executed.toolId.isEmpty()) {
        executed.toolId = resolvedRequest.toolId;
    }
    executed.durationMs = timer.elapsed();
    executed.cancelSupported = executor->supportsCancellation();
    if (timeoutMs > 0 && executed.durationMs > timeoutMs) {
        executed.success = false;
        executed.timedOut = true;
        executed.status = ToolExecutionStatus::TimedOut;
        executed.errorCode = QStringLiteral("tool_timeout");
        executed.errorMessage =
            QStringLiteral("Tool execution exceeded the configured timeout");
        if (executed.cancelSupported) {
            executor->cancel(resolvedRequest.callId);
        }
    }

    logging::QtLlmLogger::instance().info(QStringLiteral("tool.execution"),
                                          executed.success
                                              ? QStringLiteral("Tool executed")
                                              : QStringLiteral("Tool execution failed"),
                                          logContextFromExecution(context),
                                          QJsonObject{{QStringLiteral("toolId"), executed.toolId},
                                                      {QStringLiteral("callId"), executed.callId},
                                                      {QStringLiteral("durationMs"), static_cast<qint64>(executed.durationMs)},
                                                      {QStringLiteral("errorCode"), executed.errorCode}});

    if (m_hooks) {
        m_hooks->afterExecute(executed, context);
    }

    return finalizeResult(executed);
}

ToolExecutionResult ToolExecutionLayer::executeMcpTool(const ToolCallRequest &request,
                                                       const ToolExecutionContext &context) const
{
    ToolExecutionResult result;
    result.callId = request.callId;
    result.externalCallId = request.externalCallId.trimmed().isEmpty() ? request.callId : request.externalCallId;
    result.internalToolCallId = request.internalToolCallId;
    result.toolId = request.toolId;

    const QStringList parts = request.toolId.split(QStringLiteral("::"));
    if (parts.size() < 3) {
        result.errorCode = QStringLiteral("mcp_tool_id_invalid");
        result.errorMessage = QStringLiteral("Invalid MCP tool id, expected mcp::<serverId>::<toolName>");
        result.retryable = false;
        logging::QtLlmLogger::instance().error(QStringLiteral("tool.execution"),
                                               QStringLiteral("MCP tool execution failed because toolId format is invalid"),
                                               logContextFromExecution(context),
                                               QJsonObject{{QStringLiteral("toolId"), request.toolId}});
        return result;
    }

    const QString serverId = parts.at(1).trimmed().toLower();
    const QString toolName = parts.mid(2).join(QStringLiteral("::"));

    if (!m_mcpServerRegistry) {
        result.errorCode = QStringLiteral("mcp_server_registry_missing");
        result.errorMessage = QStringLiteral("MCP server registry is not configured");
        result.retryable = false;
        logging::QtLlmLogger::instance().error(QStringLiteral("tool.execution"),
                                               QStringLiteral("MCP tool execution failed because server registry is missing"),
                                               logContextFromExecution(context),
                                               QJsonObject{{QStringLiteral("serverId"), serverId}});
        return result;
    }

    const std::optional<mcp::McpServerDefinition> serverOpt = m_mcpServerRegistry->find(serverId);
    if (!serverOpt.has_value()) {
        result.errorCode = QStringLiteral("mcp_server_not_found");
        result.errorMessage = QStringLiteral("MCP server not found: ") + serverId;
        result.retryable = false;
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.execution"),
                                              QStringLiteral("MCP tool execution failed because server was not found"),
                                              logContextFromExecution(context),
                                              QJsonObject{{QStringLiteral("serverId"), serverId},
                                                          {QStringLiteral("toolName"), toolName}});
        return result;
    }

    if (!serverOpt->enabled) {
        result.errorCode = QStringLiteral("mcp_server_disabled");
        result.errorMessage = QStringLiteral("MCP server is disabled: ") + serverId;
        result.retryable = false;
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.execution"),
                                              QStringLiteral("MCP tool execution skipped because server is disabled"),
                                              logContextFromExecution(context),
                                              QJsonObject{{QStringLiteral("serverId"), serverId},
                                                          {QStringLiteral("toolName"), toolName}});
        return result;
    }

    if (!m_mcpClient) {
        result.errorCode = QStringLiteral("mcp_client_missing");
        result.errorMessage = QStringLiteral("MCP client is not configured");
        result.retryable = false;
        logging::QtLlmLogger::instance().error(QStringLiteral("tool.execution"),
                                               QStringLiteral("MCP tool execution failed because MCP client is missing"),
                                               logContextFromExecution(context),
                                               QJsonObject{{QStringLiteral("serverId"), serverId},
                                                           {QStringLiteral("toolName"), toolName}});
        return result;
    }

    if (m_hooks) {
        m_hooks->beforeExecute(request, context);
    }

    QElapsedTimer timer;
    timer.start();

    QString errorMessage;
    const mcp::McpToolCallResult call = m_mcpClient->callTool(*serverOpt,
                                                               toolName,
                                                               request.arguments,
                                                               context,
                                                               &errorMessage);

    result.durationMs = timer.elapsed();
    result.success = call.success;
    result.output = call.output;
    result.errorCode = call.errorCode;
    result.errorMessage = call.errorMessage.isEmpty() ? errorMessage : call.errorMessage;
    result.retryable = call.retryable;

    logging::QtLlmLogger::instance().info(QStringLiteral("tool.execution"),
                                          result.success
                                              ? QStringLiteral("MCP tool executed")
                                              : QStringLiteral("MCP tool execution failed"),
                                          logContextFromExecution(context),
                                          QJsonObject{{QStringLiteral("serverId"), serverId},
                                                      {QStringLiteral("toolName"), toolName},
                                                      {QStringLiteral("callId"), request.callId},
                                                      {QStringLiteral("durationMs"), static_cast<qint64>(result.durationMs)},
                                                      {QStringLiteral("errorCode"), result.errorCode}});

    if (m_hooks) {
        m_hooks->afterExecute(result, context);
    }

    return result;
}

} // namespace qtllm::tools::runtime
