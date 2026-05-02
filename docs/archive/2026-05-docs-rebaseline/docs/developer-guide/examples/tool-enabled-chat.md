# 示例：带工具调用的聊天骨架

## 适用场景

- 需要让模型调用工具
- 需要显示本轮选中的工具和 schema
- 后续可能接入 MCP

## 示例代码

```cpp
#include "src/qtllm/chat/conversationclientfactory.h"
#include "src/qtllm/storage/conversationrepository.h"
#include "src/qtllm/tools/llmtoolregistry.h"
#include "src/qtllm/tools/toolenabledchatentry.h"
#include "src/qtllm/tools/runtime/toolexecutionlayer.h"
#include "src/qtllm/tools/runtime/toolcallorchestrator.h"

#include <QObject>
#include <QPlainTextEdit>

class ToolChatController : public QObject
{
    Q_OBJECT
public:
    explicit ToolChatController(QPlainTextEdit *output,
                                QPlainTextEdit *toolInfo,
                                QObject *parent = nullptr)
        : QObject(parent)
        , m_output(output)
        , m_toolInfo(toolInfo)
        , m_repository(std::make_shared<qtllm::storage::ConversationRepository>(
              QStringLiteral(".qtllm/clients")))
        , m_factory(new qtllm::chat::ConversationClientFactory(this))
        , m_registry(std::make_shared<qtllm::tools::LlmToolRegistry>())
        , m_executionLayer(std::make_shared<qtllm::tools::runtime::ToolExecutionLayer>())
    {
        m_factory->setRepository(m_repository);
        m_client = m_factory->acquire(QStringLiteral("tool-assistant"));

        qtllm::LlmConfig config;
        config.providerName = QStringLiteral("openai-compatible");
        config.baseUrl = QStringLiteral("http://127.0.0.1:11434");
        config.model = QStringLiteral("qwen2.5:7b");
        config.stream = true;

        m_client->setConfig(config);
        m_client->setProviderByName(config.providerName);

        m_executionLayer->setToolRegistry(m_registry);

        m_entry = new qtllm::tools::ToolEnabledChatEntry(m_client, m_registry, this);
        m_entry->setExecutionLayer(m_executionLayer);

        connect(m_entry, &qtllm::tools::ToolEnabledChatEntry::toolSelectionPrepared,
                this, [this](const QStringList &toolIds) {
            if (m_toolInfo) {
                m_toolInfo->appendPlainText(QStringLiteral("[selected] ") + toolIds.join(QStringLiteral(", ")));
            }
        });

        connect(m_entry, &qtllm::tools::ToolEnabledChatEntry::toolSchemaPrepared,
                this, [this](const QString &schemaText) {
            if (m_toolInfo) {
                m_toolInfo->appendPlainText(QStringLiteral("\\n[schema]\\n") + schemaText);
            }
        });

        connect(m_entry, &qtllm::tools::ToolEnabledChatEntry::tokenReceived,
                this, [this](const QString &token) {
            if (m_output) {
                m_output->insertPlainText(token);
            }
        });

        connect(m_entry, &qtllm::tools::ToolEnabledChatEntry::errorOccurred,
                this, [this](const QString &message) {
            if (m_output) {
                m_output->appendPlainText(QStringLiteral("\\n[error] ") + message);
            }
        });
    }

    void sendUserMessage(const QString &text)
    {
        if (!m_entry || text.trimmed().isEmpty()) {
            return;
        }

        if (m_output) {
            m_output->appendPlainText(QStringLiteral("\\n[user] ") + text.trimmed());
        }
        m_entry->sendUserMessage(text.trimmed());
    }

private:
    QPlainTextEdit *m_output = nullptr;
    QPlainTextEdit *m_toolInfo = nullptr;
    std::shared_ptr<qtllm::storage::ConversationRepository> m_repository;
    qtllm::chat::ConversationClientFactory *m_factory = nullptr;
    QSharedPointer<qtllm::chat::ConversationClient> m_client;
    std::shared_ptr<qtllm::tools::LlmToolRegistry> m_registry;
    std::shared_ptr<qtllm::tools::runtime::ToolExecutionLayer> m_executionLayer;
    qtllm::tools::ToolEnabledChatEntry *m_entry = nullptr;
};
```

## 说明

- `ToolEnabledChatEntry` 是应用侧总入口
- `LlmToolRegistry` 是共享工具目录
- `ToolExecutionLayer` 是执行层
- 如果后续接 MCP，只需要继续给执行层补 `setMcpClient(...)` 和 `setMcpServerRegistry(...)`

## 推荐配套阅读

- [../tools/tool-enabled-chat-entry.md](../tools/tool-enabled-chat-entry.md)
- [../tools/tool-runtime.md](../tools/tool-runtime.md)
