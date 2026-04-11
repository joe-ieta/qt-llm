#include "toolenabledchatentry.h"

#include "../identity/compactid.h"
#include "../logging/qtllmlogger.h"
#include "../toolsinside/toolsinsideruntime.h"
#include "../toolsinside/toolsinsidetracerecorder.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <optional>

namespace qtllm::tools {

namespace {

logging::LogContext chatContext(const QSharedPointer<chat::ConversationClient> &client,
                                const QString &requestId,
                                const QString &traceId)
{
    logging::LogContext context;
    if (client) {
        context.clientId = client->uid();
        context.sessionId = client->activeSessionId();
    }
    context.requestId = requestId;
    context.traceId = traceId;
    return context;
}

} // namespace

ToolEnabledChatEntry::ToolEnabledChatEntry(const QSharedPointer<chat::ConversationClient> &client,
                                           std::shared_ptr<LlmToolRegistry> toolRegistry,
                                           QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_toolRegistry(std::move(toolRegistry))
    , m_toolAdapter(std::make_unique<DefaultLlmToolAdapter>())
    , m_executionLayer(std::make_shared<runtime::ToolExecutionLayer>())
    , m_orchestrator(std::make_shared<runtime::ToolCallOrchestrator>())
{
    if (m_executionLayer) {
        m_executionLayer->setToolRegistry(m_toolRegistry);
    }

    if (m_orchestrator) {
        m_orchestrator->setExecutionLayer(m_executionLayer);
    }

    if (m_client) {
        m_client->setToolCallOrchestrator(m_orchestrator);

        connect(m_client.get(), &chat::ConversationClient::tokenReceived,
                this, &ToolEnabledChatEntry::tokenReceived);
        connect(m_client.get(), &chat::ConversationClient::reasoningTokenReceived,
                this, &ToolEnabledChatEntry::reasoningTokenReceived);
        connect(m_client.get(), &chat::ConversationClient::responseReceived,
                this, &ToolEnabledChatEntry::onClientResponse);
        connect(m_client.get(), &chat::ConversationClient::errorOccurred,
                this, &ToolEnabledChatEntry::errorOccurred);
    }
}

void ToolEnabledChatEntry::sendUserMessage(const QString &content)
{
    if (!m_client) {
        const QString message = QStringLiteral("ToolEnabledChatEntry has no bound client");
        logging::QtLlmLogger::instance().error(QStringLiteral("tool.selection"), message);
        emit errorOccurred(message);
        return;
    }

    const QString trimmed = content.trimmed();
    if (trimmed.isEmpty()) {
        logging::QtLlmLogger::instance().debug(QStringLiteral("tool.selection"),
                                               QStringLiteral("Tool-enabled send skipped because input is empty"),
                                               chatContext(m_client, m_requestId, m_traceId));
        return;
    }

    if (m_orchestrator) {
        m_orchestrator->resetSession(m_client->uid(), m_client->activeSessionId());
    }

    m_requestId.clear();
    m_traceId = identity::generateId(identity::IdKind::Trace);

    toolsinside::ToolsInsideRuntime::instance().recorder()->startTrace(m_client->uid(),
                                                                       m_client->activeSessionId(),
                                                                       m_traceId,
                                                                       trimmed,
                                                                       m_client->config().providerName,
                                                                       m_client->config().model,
                                                                       m_client->config().modelVendor);

    QStringList selectedToolIds;
    const QJsonArray tools = selectAndAdaptToolsForTurn(trimmed, &selectedToolIds);
    const QString toolSchemaText = QString::fromUtf8(QJsonDocument(tools).toJson(QJsonDocument::Indented));
    emit toolSelectionPrepared(selectedToolIds);
    emit toolSchemaPrepared(toolSchemaText);
    toolsinside::ToolsInsideRuntime::instance().recorder()->recordToolSelection(m_traceId, selectedToolIds, toolSchemaText);

    logging::QtLlmLogger::instance().info(QStringLiteral("tool.selection"),
                                          QStringLiteral("Prepared tools for user turn"),
                                          chatContext(m_client, m_requestId, m_traceId),
                                          QJsonObject{{QStringLiteral("selectedCount"), selectedToolIds.size()},
                                                      {QStringLiteral("toolSchemaCount"), tools.size()},
                                                      {QStringLiteral("inputLength"), trimmed.size()}});
    m_client->sendUserMessageWithTools(trimmed, tools, m_traceId);
}

void ToolEnabledChatEntry::setToolSelectionLayer(ToolSelectionLayer selectionLayer)
{
    m_selectionLayer = std::move(selectionLayer);
}

void ToolEnabledChatEntry::setToolAdapter(std::unique_ptr<ILlmToolAdapter> adapter)
{
    if (adapter) {
        m_toolAdapter = std::move(adapter);
    }
}

void ToolEnabledChatEntry::setExecutionLayer(const std::shared_ptr<runtime::ToolExecutionLayer> &executionLayer)
{
    if (executionLayer) {
        m_executionLayer = executionLayer;
        m_executionLayer->setToolRegistry(m_toolRegistry);
        if (m_orchestrator) {
            m_orchestrator->setExecutionLayer(executionLayer);
        }
    }
}

void ToolEnabledChatEntry::setClientPolicyRepository(
    const std::shared_ptr<runtime::ClientToolPolicyRepository> &policyRepository)
{
    m_clientPolicyRepository = policyRepository;
    if (m_orchestrator) {
        m_orchestrator->setPolicyRepository(policyRepository);
    }
}

void ToolEnabledChatEntry::setMcpClient(const std::shared_ptr<mcp::IMcpClient> &mcpClient)
{
    if (m_executionLayer) {
        m_executionLayer->setMcpClient(mcpClient);
    }
}

void ToolEnabledChatEntry::setMcpServerRegistry(const std::shared_ptr<mcp::McpServerRegistry> &serverRegistry)
{
    if (m_executionLayer) {
        m_executionLayer->setMcpServerRegistry(serverRegistry);
    }
}

void ToolEnabledChatEntry::setTraceContext(const QString &requestId, const QString &traceId)
{
    m_requestId = requestId;
    m_traceId = traceId;
}

QList<runtime::ToolExecutionResult> ToolEnabledChatEntry::executeToolCalls(
    const QList<runtime::ToolCallRequest> &requests)
{
    if (!m_client) {
        const QString message = QStringLiteral("ToolEnabledChatEntry has no bound client");
        logging::QtLlmLogger::instance().error(QStringLiteral("tool.execution"), message);
        emit errorOccurred(message);
        return {};
    }

    if (!m_executionLayer) {
        const QString message = QStringLiteral("ToolEnabledChatEntry has no execution layer");
        logging::QtLlmLogger::instance().error(QStringLiteral("tool.execution"), message,
                                               chatContext(m_client, m_requestId, m_traceId));
        emit errorOccurred(message);
        return {};
    }

    runtime::ClientToolPolicy policy;
    policy.clientId = m_client->uid();

    if (m_clientPolicyRepository) {
        const std::optional<runtime::ClientToolPolicy> loaded =
            m_clientPolicyRepository->loadPolicy(m_client->uid(), nullptr);
        if (loaded.has_value()) {
            policy = *loaded;
        }
    }

    return m_executionLayer->executeBatch(requests, buildExecutionContext(), policy);
}

QJsonArray ToolEnabledChatEntry::selectAndAdaptToolsForTurn(const QString &content, QStringList *selectedToolIds) const
{
    if (!m_client || !m_toolRegistry || !m_toolAdapter) {
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.selection"),
                                              QStringLiteral("Tool selection skipped because dependencies are missing"),
                                              chatContext(m_client, m_requestId, m_traceId));
        return QJsonArray();
    }

    QList<LlmToolDefinition> available = m_toolRegistry->enabledTools();
    const int totalAvailable = available.size();

    if (m_clientPolicyRepository) {
        const std::optional<runtime::ClientToolPolicy> policy =
            m_clientPolicyRepository->loadPolicy(m_client->uid(), nullptr);

        if (policy.has_value()) {
            QList<LlmToolDefinition> filtered;
            runtime::ToolExecutionPolicy execPolicy;
            for (const LlmToolDefinition &tool : available) {
                if (execPolicy.isToolAllowed(tool.toolId, *policy)) {
                    filtered.append(tool);
                }
            }
            available = filtered;
        }
    }

    QVector<LlmMessage> historyWindow = m_client->history();
    LlmMessage turnUserMessage;
    turnUserMessage.role = QStringLiteral("user");
    turnUserMessage.content = content;
    historyWindow.append(turnUserMessage);

    const QList<LlmToolDefinition> candidates =
        m_selectionLayer.selectTools(m_client->profile(), historyWindow, available);

    if (selectedToolIds) {
        selectedToolIds->clear();
        for (const LlmToolDefinition &tool : candidates) {
            selectedToolIds->append(tool.toolId);
        }
    }

    logging::QtLlmLogger::instance().debug(QStringLiteral("tool.selection"),
                                           QStringLiteral("Tool selection completed"),
                                           chatContext(m_client, m_requestId, m_traceId),
                                           QJsonObject{{QStringLiteral("registryEnabledCount"), totalAvailable},
                                                       {QStringLiteral("policyAllowedCount"), available.size()},
                                                       {QStringLiteral("selectedCount"), candidates.size()}});

    if (candidates.isEmpty()) {
        return QJsonArray();
    }

    return m_toolAdapter->adaptTools(candidates, m_client->config().providerName);
}

runtime::ToolExecutionContext ToolEnabledChatEntry::buildExecutionContext() const
{
    runtime::ToolExecutionContext context;
    if (!m_client) {
        return context;
    }

    context.clientId = m_client->uid();
    context.sessionId = m_client->activeSessionId();
    context.profile = m_client->profile();
    context.llmConfig = m_client->config();
    context.historyWindow = m_client->history();
    context.requestId = m_requestId;
    context.traceId = m_traceId;
    context.extra.insert(QStringLiteral("requestTimestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    return context;
}

void ToolEnabledChatEntry::onClientResponse(const LlmResponse &response)
{
    const QString text = response.assistantMessage.content.isEmpty() ? response.text : response.assistantMessage.content;
    logging::QtLlmLogger::instance().info(QStringLiteral("llm.response"),
                                          QStringLiteral("Conversation client response delivered to UI"),
                                          chatContext(m_client, m_requestId, m_traceId),
                                          QJsonObject{{QStringLiteral("assistantTextLength"), text.size()},
                                                      {QStringLiteral("toolCallCount"), response.assistantMessage.toolCalls.size()}});
    emit completed(text);
}

} // namespace qtllm::tools
