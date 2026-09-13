#pragma once

#include "../core/llmtypes.h"
#include "../tools/runtime/toolruntime_types.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace qtllm::events {

class ILlmEventSink
{
public:
    virtual ~ILlmEventSink() = default;

    virtual QString startTrace(const QString &clientId,
                               const QString &sessionId,
                               const QString &traceId,
                               const QString &turnInput,
                               const QString &provider,
                               const QString &model,
                               const QString &vendor)
    {
        Q_UNUSED(clientId)
        Q_UNUSED(sessionId)
        Q_UNUSED(turnInput)
        Q_UNUSED(provider)
        Q_UNUSED(model)
        Q_UNUSED(vendor)
        return traceId;
    }

    virtual void recordToolSelection(const QString &traceId,
                                     const QStringList &toolIds,
                                     const QString &schemaText)
    {
        Q_UNUSED(traceId)
        Q_UNUSED(toolIds)
        Q_UNUSED(schemaText)
    }

    virtual void recordRequestPrepared(const QString &clientId,
                                       const QString &sessionId,
                                       const QString &traceId,
                                       const QString &requestJson)
    {
        Q_UNUSED(clientId)
        Q_UNUSED(sessionId)
        Q_UNUSED(traceId)
        Q_UNUSED(requestJson)
    }

    virtual void recordRequestDispatched(const QString &clientId,
                                         const QString &sessionId,
                                         const QString &traceId,
                                         const QString &requestId,
                                         const QString &provider,
                                         const QString &model,
                                         const QString &url,
                                         const QString &payloadJson,
                                         int messageCount,
                                         int toolCount)
    {
        Q_UNUSED(clientId)
        Q_UNUSED(sessionId)
        Q_UNUSED(traceId)
        Q_UNUSED(requestId)
        Q_UNUSED(provider)
        Q_UNUSED(model)
        Q_UNUSED(url)
        Q_UNUSED(payloadJson)
        Q_UNUSED(messageCount)
        Q_UNUSED(toolCount)
    }

    virtual void recordRequestAttemptStarted(const QString &clientId,
                                             const QString &sessionId,
                                             const QString &traceId,
                                             const QString &requestId,
                                             int attempt)
    {
        Q_UNUSED(clientId)
        Q_UNUSED(sessionId)
        Q_UNUSED(traceId)
        Q_UNUSED(requestId)
        Q_UNUSED(attempt)
    }

    virtual void recordRequestAttemptReset(const QString &clientId,
                                           const QString &sessionId,
                                           const QString &traceId,
                                           const QString &requestId,
                                           int previousAttempt,
                                           int nextAttempt)
    {
        Q_UNUSED(clientId)
        Q_UNUSED(sessionId)
        Q_UNUSED(traceId)
        Q_UNUSED(requestId)
        Q_UNUSED(previousAttempt)
        Q_UNUSED(nextAttempt)
    }

    virtual void recordStreamDelta(const QString &traceId,
                                   const QString &requestId,
                                   const QString &channel,
                                   int textLength)
    {
        Q_UNUSED(traceId)
        Q_UNUSED(requestId)
        Q_UNUSED(channel)
        Q_UNUSED(textLength)
    }

    virtual void recordFirstStreamToken(const QString &traceId,
                                        const QString &requestId,
                                        const QString &channel)
    {
        Q_UNUSED(traceId)
        Q_UNUSED(requestId)
        Q_UNUSED(channel)
    }

    virtual void recordResponseParsed(const QString &traceId,
                                      const QString &requestId,
                                      const qtllm::LlmResponse &response,
                                      const QString &assistantText)
    {
        Q_UNUSED(traceId)
        Q_UNUSED(requestId)
        Q_UNUSED(response)
        Q_UNUSED(assistantText)
    }

    virtual void recordToolCallsParsed(const qtllm::tools::runtime::ToolExecutionContext &context,
                                       const QString &requestId,
                                       const QString &adapterId,
                                       int roundIndex,
                                       const QList<qtllm::tools::runtime::ToolCallRequest> &requests)
    {
        Q_UNUSED(context)
        Q_UNUSED(requestId)
        Q_UNUSED(adapterId)
        Q_UNUSED(roundIndex)
        Q_UNUSED(requests)
    }

    virtual void recordToolBatchStarted(const qtllm::tools::runtime::ToolExecutionContext &context,
                                        const QString &requestId,
                                        int roundIndex,
                                        int requestCount)
    {
        Q_UNUSED(context)
        Q_UNUSED(requestId)
        Q_UNUSED(roundIndex)
        Q_UNUSED(requestCount)
    }

    virtual void recordToolCallStarted(const qtllm::tools::runtime::ToolExecutionContext &context,
                                       const QString &requestId,
                                       int roundIndex,
                                       const qtllm::tools::runtime::ToolCallRequest &request)
    {
        Q_UNUSED(context)
        Q_UNUSED(requestId)
        Q_UNUSED(roundIndex)
        Q_UNUSED(request)
    }

    virtual void recordToolCallFinished(const qtllm::tools::runtime::ToolExecutionContext &context,
                                        const QString &requestId,
                                        int roundIndex,
                                        const qtllm::tools::runtime::ToolCallRequest &request,
                                        const qtllm::tools::runtime::ToolExecutionResult &result)
    {
        Q_UNUSED(context)
        Q_UNUSED(requestId)
        Q_UNUSED(roundIndex)
        Q_UNUSED(request)
        Q_UNUSED(result)
    }

    virtual void recordFollowUpPrompt(const qtllm::tools::runtime::ToolExecutionContext &context,
                                      const QString &requestId,
                                      int roundIndex,
                                      const QString &prompt,
                                      const QList<qtllm::tools::runtime::ToolExecutionResult> &results)
    {
        Q_UNUSED(context)
        Q_UNUSED(requestId)
        Q_UNUSED(roundIndex)
        Q_UNUSED(prompt)
        Q_UNUSED(results)
    }

    virtual void recordFailureGuard(const qtllm::tools::runtime::ToolExecutionContext &context,
                                    const QString &requestId,
                                    int roundIndex,
                                    int consecutiveFailures,
                                    const QString &reason)
    {
        Q_UNUSED(context)
        Q_UNUSED(requestId)
        Q_UNUSED(roundIndex)
        Q_UNUSED(consecutiveFailures)
        Q_UNUSED(reason)
    }

    virtual void recordCancellationRequested(const QString &clientId,
                                             const QString &sessionId,
                                             const QString &traceId,
                                             const QString &requestId)
    {
        Q_UNUSED(clientId)
        Q_UNUSED(sessionId)
        Q_UNUSED(traceId)
        Q_UNUSED(requestId)
    }

    virtual void recordTraceCompleted(const QString &clientId,
                                      const QString &sessionId,
                                      const QString &traceId,
                                      const QString &requestId,
                                      const QString &finalText,
                                      const QString &finishReason)
    {
        Q_UNUSED(clientId)
        Q_UNUSED(sessionId)
        Q_UNUSED(traceId)
        Q_UNUSED(requestId)
        Q_UNUSED(finalText)
        Q_UNUSED(finishReason)
    }

    virtual void recordTraceError(const QString &clientId,
                                  const QString &sessionId,
                                  const QString &traceId,
                                  const QString &requestId,
                                  const QString &message,
                                  const QString &category)
    {
        Q_UNUSED(clientId)
        Q_UNUSED(sessionId)
        Q_UNUSED(traceId)
        Q_UNUSED(requestId)
        Q_UNUSED(message)
        Q_UNUSED(category)
    }
};

} // namespace qtllm::events
