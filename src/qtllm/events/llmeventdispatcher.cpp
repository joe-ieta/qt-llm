#include "llmeventdispatcher.h"

#include <QReadLocker>
#include <QWriteLocker>

namespace qtllm::events {

LlmEventDispatcher &LlmEventDispatcher::instance()
{
    static LlmEventDispatcher dispatcher;
    return dispatcher;
}

void LlmEventDispatcher::addSink(const std::shared_ptr<ILlmEventSink> &sink)
{
    if (!sink) {
        return;
    }
    QWriteLocker locker(&m_lock);
    if (!m_sinks.contains(sink)) {
        m_sinks.append(sink);
    }
}

void LlmEventDispatcher::removeSink(const std::shared_ptr<ILlmEventSink> &sink)
{
    QWriteLocker locker(&m_lock);
    m_sinks.removeAll(sink);
}

void LlmEventDispatcher::clearSinks()
{
    QWriteLocker locker(&m_lock);
    m_sinks.clear();
}

int LlmEventDispatcher::sinkCount() const
{
    QReadLocker locker(&m_lock);
    return m_sinks.size();
}

QVector<std::shared_ptr<ILlmEventSink>> LlmEventDispatcher::sinksSnapshot() const
{
    QReadLocker locker(&m_lock);
    return m_sinks;
}

#define QTLLM_DISPATCH(method, ...) \
    const auto sinks = sinksSnapshot(); \
    for (const auto &sink : sinks) { \
        sink->method(__VA_ARGS__); \
    }

QString LlmEventDispatcher::startTrace(const QString &clientId, const QString &sessionId,
                                       const QString &traceId, const QString &turnInput,
                                       const QString &provider, const QString &model,
                                       const QString &vendor)
{
    QString resolvedTraceId = traceId;
    const auto sinks = sinksSnapshot();
    for (const auto &sink : sinks) {
        const QString sinkTraceId = sink->startTrace(clientId, sessionId, resolvedTraceId,
                                                     turnInput, provider, model, vendor);
        if (resolvedTraceId.isEmpty() && !sinkTraceId.isEmpty()) {
            resolvedTraceId = sinkTraceId;
        }
    }
    return resolvedTraceId;
}

void LlmEventDispatcher::recordToolSelection(const QString &traceId, const QStringList &toolIds,
                                             const QString &schemaText)
{
    QTLLM_DISPATCH(recordToolSelection, traceId, toolIds, schemaText)
}

void LlmEventDispatcher::recordRequestPrepared(const QString &clientId, const QString &sessionId,
                                               const QString &traceId, const QString &requestJson)
{
    QTLLM_DISPATCH(recordRequestPrepared, clientId, sessionId, traceId, requestJson)
}

void LlmEventDispatcher::recordRequestDispatched(
    const QString &clientId, const QString &sessionId, const QString &traceId,
    const QString &requestId, const QString &provider, const QString &model,
    const QString &url, const QString &payloadJson, int messageCount, int toolCount)
{
    QTLLM_DISPATCH(recordRequestDispatched, clientId, sessionId, traceId, requestId,
                   provider, model, url, payloadJson, messageCount, toolCount)
}

void LlmEventDispatcher::recordRequestAttemptStarted(
    const QString &clientId, const QString &sessionId, const QString &traceId,
    const QString &requestId, int attempt)
{
    QTLLM_DISPATCH(recordRequestAttemptStarted, clientId, sessionId, traceId, requestId, attempt)
}

void LlmEventDispatcher::recordRequestAttemptReset(
    const QString &clientId, const QString &sessionId, const QString &traceId,
    const QString &requestId, int previousAttempt, int nextAttempt)
{
    QTLLM_DISPATCH(recordRequestAttemptReset, clientId, sessionId, traceId, requestId,
                   previousAttempt, nextAttempt)
}

void LlmEventDispatcher::recordStreamDelta(const QString &traceId, const QString &requestId,
                                           const QString &channel, int textLength)
{
    QTLLM_DISPATCH(recordStreamDelta, traceId, requestId, channel, textLength)
}

void LlmEventDispatcher::recordFirstStreamToken(const QString &traceId, const QString &requestId,
                                                const QString &channel)
{
    QTLLM_DISPATCH(recordFirstStreamToken, traceId, requestId, channel)
}

void LlmEventDispatcher::recordResponseParsed(const QString &traceId, const QString &requestId,
                                              const qtllm::LlmResponse &response,
                                              const QString &assistantText)
{
    QTLLM_DISPATCH(recordResponseParsed, traceId, requestId, response, assistantText)
}

void LlmEventDispatcher::recordToolCallsParsed(
    const qtllm::tools::runtime::ToolExecutionContext &context, const QString &requestId,
    const QString &adapterId, int roundIndex,
    const QList<qtllm::tools::runtime::ToolCallRequest> &requests)
{
    QTLLM_DISPATCH(recordToolCallsParsed, context, requestId, adapterId, roundIndex, requests)
}

void LlmEventDispatcher::recordToolBatchStarted(
    const qtllm::tools::runtime::ToolExecutionContext &context, const QString &requestId,
    int roundIndex, int requestCount)
{
    QTLLM_DISPATCH(recordToolBatchStarted, context, requestId, roundIndex, requestCount)
}

void LlmEventDispatcher::recordToolCallStarted(
    const qtllm::tools::runtime::ToolExecutionContext &context, const QString &requestId,
    int roundIndex, const qtllm::tools::runtime::ToolCallRequest &request)
{
    QTLLM_DISPATCH(recordToolCallStarted, context, requestId, roundIndex, request)
}

void LlmEventDispatcher::recordToolCallFinished(
    const qtllm::tools::runtime::ToolExecutionContext &context, const QString &requestId,
    int roundIndex, const qtllm::tools::runtime::ToolCallRequest &request,
    const qtllm::tools::runtime::ToolExecutionResult &result)
{
    QTLLM_DISPATCH(recordToolCallFinished, context, requestId, roundIndex, request, result)
}

void LlmEventDispatcher::recordFollowUpPrompt(
    const qtllm::tools::runtime::ToolExecutionContext &context, const QString &requestId,
    int roundIndex, const QString &prompt,
    const QList<qtllm::tools::runtime::ToolExecutionResult> &results)
{
    QTLLM_DISPATCH(recordFollowUpPrompt, context, requestId, roundIndex, prompt, results)
}

void LlmEventDispatcher::recordFailureGuard(
    const qtllm::tools::runtime::ToolExecutionContext &context, const QString &requestId,
    int roundIndex, int consecutiveFailures, const QString &reason)
{
    QTLLM_DISPATCH(recordFailureGuard, context, requestId, roundIndex, consecutiveFailures, reason)
}

void LlmEventDispatcher::recordCancellationRequested(
    const QString &clientId, const QString &sessionId, const QString &traceId,
    const QString &requestId)
{
    QTLLM_DISPATCH(recordCancellationRequested, clientId, sessionId, traceId, requestId)
}

void LlmEventDispatcher::recordTraceCompleted(
    const QString &clientId, const QString &sessionId, const QString &traceId,
    const QString &requestId, const QString &finalText, const QString &finishReason)
{
    QTLLM_DISPATCH(recordTraceCompleted, clientId, sessionId, traceId, requestId,
                   finalText, finishReason)
}

void LlmEventDispatcher::recordTraceError(
    const QString &clientId, const QString &sessionId, const QString &traceId,
    const QString &requestId, const QString &message, const QString &category)
{
    QTLLM_DISPATCH(recordTraceError, clientId, sessionId, traceId, requestId, message, category)
}

#undef QTLLM_DISPATCH

} // namespace qtllm::events
