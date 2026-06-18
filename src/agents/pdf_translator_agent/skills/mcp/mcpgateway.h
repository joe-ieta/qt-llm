#pragma once

#include "../../../../qtllm/tools/mcp/defaultmcpclient.h"
#include "../../../../qtllm/tools/mcp/mcpservermanager.h"

#include <memory>
#include <optional>

namespace pdftranslator::skills::mcp {

class McpGateway
{
public:
    McpGateway();

    bool loadServers(QString *errorMessage = nullptr);
    QVector<qtllm::tools::mcp::McpServerDefinition> allServers() const;
    std::optional<qtllm::tools::mcp::McpServerDefinition> findServer(const QString &serverId) const;

    qtllm::tools::mcp::McpToolCallResult callTool(const QString &serverId,
                                                  const QString &toolName,
                                                  const QJsonObject &arguments,
                                                  QString *errorMessage = nullptr) const;

private:
    std::shared_ptr<qtllm::tools::mcp::McpServerManager> m_serverManager;
    std::shared_ptr<qtllm::tools::mcp::IMcpClient> m_client;
};

} // namespace pdftranslator::skills::mcp
