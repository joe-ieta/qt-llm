# 示例：MCP 宿主初始化骨架

## 适用场景

- 需要管理 MCP server
- 需要把 MCP tool 同步到共享 registry
- 需要让聊天能力调用这些工具

## 示例代码

```cpp
#include "src/qtllm/chat/conversationclientfactory.h"
#include "src/qtllm/storage/conversationrepository.h"
#include "src/qtllm/tools/llmtoolregistry.h"
#include "src/qtllm/tools/toolenabledchatentry.h"
#include "src/qtllm/tools/runtime/toolexecutionlayer.h"
#include "src/qtllm/tools/mcp/defaultmcpclient.h"
#include "src/qtllm/tools/mcp/mcpservermanager.h"
#include "src/qtllm/tools/mcp/mcptoolsyncservice.h"

#include <QObject>

class McpHostController : public QObject
{
    Q_OBJECT
public:
    explicit McpHostController(QObject *parent = nullptr)
        : QObject(parent)
        , m_repository(std::make_shared<qtllm::storage::ConversationRepository>(
              QStringLiteral(".qtllm/clients")))
        , m_factory(new qtllm::chat::ConversationClientFactory(this))
        , m_registry(std::make_shared<qtllm::tools::LlmToolRegistry>())
        , m_executionLayer(std::make_shared<qtllm::tools::runtime::ToolExecutionLayer>())
        , m_mcpClient(std::make_shared<qtllm::tools::mcp::DefaultMcpClient>())
        , m_serverManager(std::make_shared<qtllm::tools::mcp::McpServerManager>())
    {
        m_factory->setRepository(m_repository);
        m_client = m_factory->acquire(QStringLiteral("mcp-assistant"));

        qtllm::LlmConfig config;
        config.providerName = QStringLiteral("openai-compatible");
        config.baseUrl = QStringLiteral("http://127.0.0.1:11434");
        config.model = QStringLiteral("qwen2.5:7b");
        config.stream = true;

        m_client->setConfig(config);
        m_client->setProviderByName(config.providerName);

        m_serverManager->load();

        m_executionLayer->setToolRegistry(m_registry);
        m_executionLayer->setMcpClient(m_mcpClient);
        m_executionLayer->setMcpServerRegistry(m_serverManager->registry());

        m_entry = new qtllm::tools::ToolEnabledChatEntry(m_client, m_registry, this);
        m_entry->setExecutionLayer(m_executionLayer);
        m_entry->setMcpClient(m_mcpClient);
        m_entry->setMcpServerRegistry(m_serverManager->registry());
    }

    bool registerServerAndSync(const qtllm::tools::mcp::McpServerDefinition &server, QString *errorMessage = nullptr)
    {
        if (!m_serverManager->registerServer(server, errorMessage)) {
            return false;
        }

        qtllm::tools::mcp::McpToolSyncService sync(m_registry, m_serverManager->registry(), m_mcpClient);
        return sync.syncServerTools(server.serverId, errorMessage);
    }

    void sendUserMessage(const QString &text)
    {
        if (m_entry && !text.trimmed().isEmpty()) {
            m_entry->sendUserMessage(text.trimmed());
        }
    }

private:
    std::shared_ptr<qtllm::storage::ConversationRepository> m_repository;
    qtllm::chat::ConversationClientFactory *m_factory = nullptr;
    QSharedPointer<qtllm::chat::ConversationClient> m_client;
    std::shared_ptr<qtllm::tools::LlmToolRegistry> m_registry;
    std::shared_ptr<qtllm::tools::runtime::ToolExecutionLayer> m_executionLayer;
    std::shared_ptr<qtllm::tools::mcp::DefaultMcpClient> m_mcpClient;
    std::shared_ptr<qtllm::tools::mcp::McpServerManager> m_serverManager;
    qtllm::tools::ToolEnabledChatEntry *m_entry = nullptr;
};
```

## 说明

- `McpServerManager` 管理 server 定义与持久化
- `McpToolSyncService` 负责把远端工具同步到共享 registry
- `ToolExecutionLayer` 负责真正执行 `mcp::...` 工具调用
- `ToolEnabledChatEntry` 负责把 MCP 工具暴露给模型

## 推荐配套阅读

- [../tools/mcp-integration.md](../tools/mcp-integration.md)
- [../tools/tool-runtime.md](../tools/tool-runtime.md)
