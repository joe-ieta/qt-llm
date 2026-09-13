#pragma once

#include "toolruntime_types.h"

#include <QString>

namespace qtllm::tools::runtime {

class IToolExecutor
{
public:
    virtual ~IToolExecutor() = default;

    virtual QString toolId() const = 0;
    virtual ToolExecutionResult execute(const ToolCallRequest &request,
                                        const ToolExecutionContext &context) = 0;

    virtual bool supportsCancellation() const { return false; }

    virtual bool cancel(const QString &callId)
    {
        Q_UNUSED(callId)
        return false;
    }
};

} // namespace qtllm::tools::runtime
