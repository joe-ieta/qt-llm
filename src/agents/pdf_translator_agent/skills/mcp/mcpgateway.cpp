#include "mcpgateway.h"

#include "../../../../qtllm/tools/runtime/toolruntime_types.h"

namespace pdftranslator::skills::mcp {

McpGateway::McpGateway()
    : m_serverManager(std::make_shared<qtllm::tools::mcp::McpServerManager>())
    , m_client(std::make_shared<qtllm::tools::mcp::DefaultMcpClient>())
{
    m_serverManager->load(nullptr);
}

bool McpGateway::loadServers(QString *errorMessage)
{
    return m_serverManager ? m_serverManager->load(errorMessage) : false;
}

QVector<qtllm::tools::mcp::McpServerDefinition> McpGateway::allServers() const
{
    return m_serverManager ? m_serverManager->allServers() : QVector<qtllm::tools::mcp::McpServerDefinition>();
}

std::optional<qtllm::tools::mcp::McpServerDefinition> McpGateway::findServer(const QString &serverId) const
{
    return m_serverManager ? m_serverManager->find(serverId) : std::nullopt;
}

qtllm::tools::mcp::McpToolCallResult McpGateway::callTool(const QString &serverId,
                                                          const QString &toolName,
                                                          const QJsonObject &arguments,
                                                          QString *errorMessage) const
{
    qtllm::tools::mcp::McpToolCallResult result;
    if (!m_serverManager || !m_client) {
        result.errorCode = QStringLiteral("mcp_unavailable");
        result.errorMessage = QStringLiteral("MCP gateway is not initialized");
        return result;
    }

    const std::optional<qtllm::tools::mcp::McpServerDefinition> server = m_serverManager->find(serverId);
    if (!server.has_value()) {
        result.errorCode = QStringLiteral("mcp_server_missing");
        result.errorMessage = QStringLiteral("MCP server not found: ") + serverId;
        if (errorMessage) {
            *errorMessage = result.errorMessage;
        }
        return result;
    }

    qtllm::tools::runtime::ToolExecutionContext context;
    context.clientId = QStringLiteral("pdf-translator-agent");
    context.sessionId = QStringLiteral("mcp-skill");
    context.requestId = QStringLiteral("mcp-skill-call");
    context.traceId = QStringLiteral("mcp-skill-call");

    return m_client->callTool(*server, toolName, arguments, context, errorMessage);
}

} // namespace pdftranslator::skills::mcp
