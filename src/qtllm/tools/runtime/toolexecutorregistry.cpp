#include "toolexecutorregistry.h"

namespace qtllm::tools::runtime {

bool ToolExecutorRegistry::registerExecutor(const std::shared_ptr<IToolExecutor> &executor)
{
    if (!executor || executor->toolId().trimmed().isEmpty()) {
        return false;
    }

    m_executors.insert(executor->toolId().trimmed(), executor);
    return true;
}

bool ToolExecutorRegistry::unregisterExecutor(const QString &toolId)
{
    return m_executors.remove(toolId.trimmed());
}

std::shared_ptr<IToolExecutor> ToolExecutorRegistry::find(const QString &toolId) const
{
    return m_executors.value(toolId.trimmed());
}

void ToolExecutorRegistry::clear()
{
    m_executors.clear();
}

} // namespace qtllm::tools::runtime
