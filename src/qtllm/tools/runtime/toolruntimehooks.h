#pragma once

#include "toolruntime_types.h"

namespace qtllm::tools::runtime {

class ToolRuntimeHooks
{
public:
    virtual ~ToolRuntimeHooks() = default;

    virtual ToolAuthorizationDecision authorize(const ToolCallRequest &request,
                                                const ToolExecutionContext &context)
    {
        Q_UNUSED(request)
        Q_UNUSED(context)
        return ToolAuthorizationDecision::Allow;
    }

    virtual void beforeExecute(const ToolCallRequest &request,
                               const ToolExecutionContext &context)
    {
        Q_UNUSED(request)
        Q_UNUSED(context)
    }

    virtual void afterExecute(const ToolExecutionResult &result,
                              const ToolExecutionContext &context)
    {
        Q_UNUSED(result)
        Q_UNUSED(context)
    }
};

} // namespace qtllm::tools::runtime
