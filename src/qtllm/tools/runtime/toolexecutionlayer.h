#pragma once

#include "toolruntime_types.h"
#include "toolexecutorregistry.h"
#include "toolruntimehooks.h"
#include "../llmtoolregistry.h"

#include <QHash>
#include <QList>
#include <QMutex>

#include <memory>

namespace qtllm::tools::mcp {
class IMcpClient;
class McpServerRegistry;
}

namespace qtllm::tools::runtime {

class ToolExecutionLayer
{
public:
    ToolExecutionLayer();

    void setRegistry(const std::shared_ptr<ToolExecutorRegistry> &registry);
    void setToolRegistry(const std::shared_ptr<tools::LlmToolRegistry> &toolRegistry);
    void setPolicy(const ToolExecutionPolicy &policy);
    void setHooks(const std::shared_ptr<ToolRuntimeHooks> &hooks);
    void setDryRunFailureMode(bool enabled);

    void setMcpClient(const std::shared_ptr<mcp::IMcpClient> &mcpClient);
    void setMcpServerRegistry(const std::shared_ptr<mcp::McpServerRegistry> &serverRegistry);

    ToolBatchPreparation prepareBatch(
        const QList<ToolCallRequest> &requests,
        const ToolExecutionContext &context,
        const ClientToolPolicy &clientPolicy = ClientToolPolicy()) const;

    QList<ToolExecutionResult> executeBatch(const QList<ToolCallRequest> &requests,
                                            const ToolExecutionContext &context,
                                            const ClientToolPolicy &clientPolicy = ClientToolPolicy()) const;

    bool cancelBySession(const QString &clientId, const QString &sessionId) const;

private:
    struct ActiveExecution
    {
        QString callId;
        std::shared_ptr<IToolExecutor> executor;
    };

    struct ActiveExecutionState
    {
        QMutex mutex;
        QHash<QString, QList<ActiveExecution>> executions;
    };

    QString resolveToolId(const QString &toolIdOrName) const;
    ToolExecutionResult executeSingle(const ToolCallRequest &request,
                                      const ToolExecutionContext &context,
                                      const ClientToolPolicy &clientPolicy) const;

    ToolExecutionResult executeMcpTool(const ToolCallRequest &request,
                                       const ToolExecutionContext &context) const;

private:
    std::shared_ptr<ToolExecutorRegistry> m_registry;
    std::shared_ptr<tools::LlmToolRegistry> m_toolRegistry;
    std::shared_ptr<ToolRuntimeHooks> m_hooks;
    ToolExecutionPolicy m_policy;
    bool m_dryRunFailureMode = false;
    std::shared_ptr<mcp::IMcpClient> m_mcpClient;
    std::shared_ptr<mcp::McpServerRegistry> m_mcpServerRegistry;
    std::shared_ptr<ActiveExecutionState> m_activeState =
        std::make_shared<ActiveExecutionState>();
};

} // namespace qtllm::tools::runtime
