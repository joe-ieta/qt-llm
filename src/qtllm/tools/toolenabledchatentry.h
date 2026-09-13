#pragma once

#include "llmtooladapter.h"
#include "llmtoolregistry.h"
#include "toolselectionlayer.h"

#include "runtime/clienttoolpolicyrepository.h"
#include "runtime/toolcallorchestrator.h"
#include "runtime/toolexecutionlayer.h"

#include "../chat/conversationclient.h"
#include "../qtllm_global.h"

#include <QJsonArray>
#include <QObject>
#include <QSharedPointer>
#include <QStringList>

#include <memory>

namespace qtllm::tools::mcp {
class IMcpClient;
class McpServerRegistry;
}

namespace qtllm::tools {

class QTLLM_CONVERSATION_EXPORT ToolEnabledChatEntry : public QObject
{
    Q_OBJECT
public:
    explicit ToolEnabledChatEntry(const QSharedPointer<chat::ConversationClient> &client,
                                  std::shared_ptr<LlmToolRegistry> toolRegistry,
                                  QObject *parent = nullptr);

    void sendUserMessage(const QString &content);

    void setToolSelectionLayer(ToolSelectionLayer selectionLayer);
    void setToolAdapter(std::unique_ptr<ILlmToolAdapter> adapter);

    void setExecutionLayer(const std::shared_ptr<runtime::ToolExecutionLayer> &executionLayer);
    void setClientPolicyRepository(const std::shared_ptr<runtime::ClientToolPolicyRepository> &policyRepository);
    void setMcpClient(const std::shared_ptr<mcp::IMcpClient> &mcpClient);
    void setMcpServerRegistry(const std::shared_ptr<mcp::McpServerRegistry> &serverRegistry);
    void setTraceContext(const QString &requestId, const QString &traceId);

    QList<runtime::ToolExecutionResult> executeToolCalls(const QList<runtime::ToolCallRequest> &requests);

signals:
    void tokenReceived(const QString &token);
    void reasoningTokenReceived(const QString &token);
    void completed(const QString &text);
    void errorOccurred(const QString &message);
    void toolSelectionPrepared(const QStringList &toolIds);
    void toolSchemaPrepared(const QString &schemaText);

private:
    QJsonArray selectAndAdaptToolsForTurn(const QString &content, QStringList *selectedToolIds = nullptr) const;
    runtime::ToolExecutionContext buildExecutionContext() const;
    void onClientResponse(const LlmResponse &response);

private:
    QSharedPointer<chat::ConversationClient> m_client;
    std::shared_ptr<LlmToolRegistry> m_toolRegistry;
    ToolSelectionLayer m_selectionLayer;
    std::unique_ptr<ILlmToolAdapter> m_toolAdapter;
    std::shared_ptr<runtime::ToolExecutionLayer> m_executionLayer;
    std::shared_ptr<runtime::ToolCallOrchestrator> m_orchestrator;
    std::shared_ptr<runtime::ClientToolPolicyRepository> m_clientPolicyRepository;
    QString m_requestId;
    QString m_traceId;
};

} // namespace qtllm::tools
