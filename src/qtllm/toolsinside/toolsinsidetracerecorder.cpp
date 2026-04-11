#include "toolsinsidetracerecorder.h"

#include "../identity/compactid.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QMutexLocker>

namespace qtllm::toolsinside {

namespace {

QJsonObject toJson(const qtllm::LlmToolCall &call)
{
    return QJsonObject{
        {QStringLiteral("id"), call.id},
        {QStringLiteral("name"), call.name},
        {QStringLiteral("type"), call.type},
        {QStringLiteral("arguments"), call.arguments}
    };
}

QJsonObject toJson(const qtllm::tools::runtime::ToolCallRequest &request)
{
    return QJsonObject{
        {QStringLiteral("callId"), request.callId},
        {QStringLiteral("externalCallId"), request.externalCallId},
        {QStringLiteral("internalToolCallId"), request.internalToolCallId},
        {QStringLiteral("toolId"), request.toolId},
        {QStringLiteral("arguments"), request.arguments},
        {QStringLiteral("idempotencyKey"), request.idempotencyKey}
    };
}

QJsonObject toJson(const qtllm::tools::runtime::ToolExecutionResult &result)
{
    return QJsonObject{
        {QStringLiteral("callId"), result.callId},
        {QStringLiteral("externalCallId"), result.externalCallId},
        {QStringLiteral("internalToolCallId"), result.internalToolCallId},
        {QStringLiteral("toolId"), result.toolId},
        {QStringLiteral("success"), result.success},
        {QStringLiteral("output"), result.output},
        {QStringLiteral("errorCode"), result.errorCode},
        {QStringLiteral("errorMessage"), result.errorMessage},
        {QStringLiteral("durationMs"), static_cast<qint64>(result.durationMs)},
        {QStringLiteral("retryable"), result.retryable}
    };
}

} // namespace

ToolsInsideTraceRecorder::ToolsInsideTraceRecorder(std::shared_ptr<ToolsInsideRepository> repository,
                                                   std::shared_ptr<ToolsInsideArtifactStore> artifactStore,
                                                   const ToolsInsideStoragePolicy &storagePolicy,
                                                   std::shared_ptr<IToolsInsideRedactionPolicy> redactionPolicy)
    : m_repository(std::move(repository))
    , m_artifactStore(std::move(artifactStore))
    , m_storagePolicy(storagePolicy)
    , m_redactionPolicy(redactionPolicy ? std::move(redactionPolicy) : std::make_shared<NoOpToolsInsideRedactionPolicy>())
{
}

QString ToolsInsideTraceRecorder::startTrace(const QString &clientId,
                                             const QString &sessionId,
                                             const QString &traceId,
                                             const QString &turnInput,
                                             const QString &provider,
                                             const QString &model,
                                             const QString &vendor)
{
    if (!m_repository) {
        return traceId.trimmed();
    }

    const QString resolvedTraceId = traceId.trimmed().isEmpty()
        ? identity::generateId(identity::IdKind::Trace)
        : traceId.trimmed();

    ToolsInsideTraceSummary trace;
    trace.traceId = resolvedTraceId;
    trace.clientId = clientId;
    trace.sessionId = sessionId;
    trace.status = QStringLiteral("running");
    trace.provider = provider;
    trace.model = model;
    trace.vendor = vendor;
    trace.turnInputPreview = turnInput.left(512);
    m_repository->upsertTrace(trace, nullptr);

    {
        QMutexLocker locker(&m_mutex);
        m_clientIdByTrace.insert(resolvedTraceId, clientId);
        m_sessionIdByTrace.insert(resolvedTraceId, sessionId);
    }

    if (m_storagePolicy.persistRawPrompt) {
        persistArtifact(clientId,
                        sessionId,
                        resolvedTraceId,
                        QStringLiteral("turn_input"),
                        QStringLiteral("text/plain"),
                        turnInput.toUtf8(),
                        QJsonObject{{QStringLiteral("length"), turnInput.size()}},
                        QStringLiteral("txt"));
    }

    ToolsInsideEventRecord event;
    event.eventId = identity::generateId(identity::IdKind::Event);
    event.traceId = resolvedTraceId;
    event.category = QStringLiteral("trace");
    event.name = QStringLiteral("turn_started");
    event.payload = QJsonObject{
        {QStringLiteral("provider"), provider},
        {QStringLiteral("model"), model},
        {QStringLiteral("vendor"), vendor},
        {QStringLiteral("inputLength"), turnInput.size()}
    };
    m_repository->upsertEvent(event, nullptr);
    return resolvedTraceId;
}

void ToolsInsideTraceRecorder::recordToolSelection(const QString &traceId,
                                                   const QStringList &toolIds,
                                                   const QString &schemaText)
{
    if (!m_repository) {
        return;
    }

    QString clientId;
    QString sessionId;
    {
        QMutexLocker locker(&m_mutex);
        clientId = m_clientIdByTrace.value(traceId);
        sessionId = m_sessionIdByTrace.value(traceId);
    }

    QString schemaArtifactId;
    if (!schemaText.trimmed().isEmpty()) {
        schemaArtifactId = persistArtifact(clientId,
                                           sessionId,
                                           traceId,
                                           QStringLiteral("tool_schema"),
                                           QStringLiteral("application/json"),
                                           schemaText.toUtf8(),
                                           QJsonObject{{QStringLiteral("toolCount"), toolIds.size()}},
                                           QStringLiteral("json"));
    }

    QJsonArray selectedToolIds;
    for (const QString &toolId : toolIds) {
        selectedToolIds.append(toolId);
    }

    ToolsInsideEventRecord event;
    event.eventId = identity::generateId(identity::IdKind::Event);
    event.traceId = traceId;
    event.category = QStringLiteral("tool.selection");
    event.name = QStringLiteral("tool_selection_completed");
    event.payload = QJsonObject{
        {QStringLiteral("selectedToolIds"), selectedToolIds},
        {QStringLiteral("selectedCount"), toolIds.size()},
        {QStringLiteral("schemaArtifactId"), schemaArtifactId}
    };
    m_repository->upsertEvent(event, nullptr);
}

void ToolsInsideTraceRecorder::recordRequestPrepared(const QString &clientId,
                                                     const QString &sessionId,
                                                     const QString &traceId,
                                                     const QString &requestJson)
{
    if (!m_repository) {
        return;
    }

    QString artifactId;
    if (m_storagePolicy.persistRawRequestPayload) {
        artifactId = persistArtifact(clientId,
                                     sessionId,
                                     traceId,
                                     QStringLiteral("request_payload"),
                                     QStringLiteral("application/json"),
                                     requestJson.toUtf8(),
                                     QJsonObject(),
                                     QStringLiteral("json"));
    }

    ToolsInsideEventRecord event;
    event.eventId = identity::generateId(identity::IdKind::Event);
    event.traceId = traceId;
    event.category = QStringLiteral("llm.request");
    event.name = QStringLiteral("request_prepared");
    event.payload = QJsonObject{{QStringLiteral("artifactId"), artifactId}};
    m_repository->upsertEvent(event, nullptr);
}

void ToolsInsideTraceRecorder::recordRequestDispatched(const QString &clientId,
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
    if (!m_repository) {
        return;
    }

    ToolsInsideTraceSummary trace;
    trace.traceId = traceId;
    trace.clientId = clientId;
    trace.sessionId = sessionId;
    trace.rootRequestId = requestId;
    trace.status = QStringLiteral("running");
    trace.provider = provider;
    trace.model = model;
    m_repository->upsertTrace(trace, nullptr);

    const QString spanId = ensureSpanId(requestSpanKey(traceId, requestId));
    ToolsInsideSpanRecord span;
    span.spanId = spanId;
    span.traceId = traceId;
    span.requestId = requestId;
    span.kind = QStringLiteral("llm_request");
    span.name = QStringLiteral("LLM Request");
    span.status = QStringLiteral("running");
    span.summary = QJsonObject{
        {QStringLiteral("provider"), provider},
        {QStringLiteral("model"), model},
        {QStringLiteral("url"), url}
    };
    m_repository->upsertSpan(span, nullptr);

    QString payloadArtifactId;
    if (m_storagePolicy.persistRawProviderPayload) {
        payloadArtifactId = persistArtifact(clientId,
                                            sessionId,
                                            traceId,
                                            QStringLiteral("provider_payload"),
                                            QStringLiteral("application/json"),
                                            payloadJson.toUtf8(),
                                            QJsonObject{{QStringLiteral("requestId"), requestId}},
                                            QStringLiteral("json"));
    }

    ToolsInsideEventRecord event;
    event.eventId = identity::generateId(identity::IdKind::Event);
    event.traceId = traceId;
    event.spanId = spanId;
    event.requestId = requestId;
    event.category = QStringLiteral("llm.request");
    event.name = QStringLiteral("request_dispatched");
    event.payload = QJsonObject{
        {QStringLiteral("provider"), provider},
        {QStringLiteral("model"), model},
        {QStringLiteral("url"), url},
        {QStringLiteral("messageCount"), messageCount},
        {QStringLiteral("toolCount"), toolCount},
        {QStringLiteral("payloadArtifactId"), payloadArtifactId}
    };
    m_repository->upsertEvent(event, nullptr);
}

void ToolsInsideTraceRecorder::recordFirstStreamToken(const QString &traceId,
                                                      const QString &requestId,
                                                      const QString &channel)
{
    if (!m_repository) {
        return;
    }

    const QString key = traceId + QStringLiteral("::") + requestId;
    bool shouldRecord = false;
    {
        QMutexLocker locker(&m_mutex);
        if (channel == QStringLiteral("reasoning")) {
            shouldRecord = !m_firstReasoningTokenSeen.value(key, false);
            if (shouldRecord) {
                m_firstReasoningTokenSeen.insert(key, true);
            }
        } else {
            shouldRecord = !m_firstContentTokenSeen.value(key, false);
            if (shouldRecord) {
                m_firstContentTokenSeen.insert(key, true);
            }
        }
    }

    if (!shouldRecord) {
        return;
    }

    ToolsInsideEventRecord event;
    event.eventId = identity::generateId(identity::IdKind::Event);
    event.traceId = traceId;
    event.spanId = ensureSpanId(requestSpanKey(traceId, requestId));
    event.requestId = requestId;
    event.category = QStringLiteral("llm.stream");
    event.name = channel == QStringLiteral("reasoning")
        ? QStringLiteral("stream_first_reasoning_token")
        : QStringLiteral("stream_first_content_token");
    m_repository->upsertEvent(event, nullptr);
}

void ToolsInsideTraceRecorder::recordResponseParsed(const QString &traceId,
                                                    const QString &requestId,
                                                    const qtllm::LlmResponse &response,
                                                    const QString &assistantText)
{
    if (!m_repository) {
        return;
    }

    QJsonArray toolCalls;
    for (const qtllm::LlmToolCall &call : response.assistantMessage.toolCalls) {
        toolCalls.append(toJson(call));
    }

    ToolsInsideEventRecord event;
    event.eventId = identity::generateId(identity::IdKind::Event);
    event.traceId = traceId;
    event.spanId = ensureSpanId(requestSpanKey(traceId, requestId));
    event.requestId = requestId;
    event.category = QStringLiteral("llm.response");
    event.name = QStringLiteral("response_received");
    event.payload = QJsonObject{
        {QStringLiteral("success"), response.success},
        {QStringLiteral("finishReason"), response.finishReason},
        {QStringLiteral("assistantTextLength"), assistantText.size()},
        {QStringLiteral("toolCalls"), toolCalls}
    };
    m_repository->upsertEvent(event, nullptr);
}

void ToolsInsideTraceRecorder::recordToolCallsParsed(const qtllm::tools::runtime::ToolExecutionContext &context,
                                                     const QString &requestId,
                                                     const QString &adapterId,
                                                     int roundIndex,
                                                     const QList<qtllm::tools::runtime::ToolCallRequest> &requests)
{
    if (!m_repository) {
        return;
    }

    QJsonArray array;
    for (const qtllm::tools::runtime::ToolCallRequest &request : requests) {
        array.append(toJson(request));
    }

    ToolsInsideEventRecord event;
    event.eventId = identity::generateId(identity::IdKind::Event);
    event.traceId = context.traceId;
    event.spanId = ensureSpanId(requestSpanKey(context.traceId, requestId));
    event.requestId = requestId;
    event.category = QStringLiteral("tool.loop");
    event.name = QStringLiteral("tool_calls_detected");
    event.payload = QJsonObject{
        {QStringLiteral("adapterId"), adapterId},
        {QStringLiteral("roundIndex"), roundIndex},
        {QStringLiteral("requests"), array}
    };
    m_repository->upsertEvent(event, nullptr);
}

void ToolsInsideTraceRecorder::recordToolBatchStarted(const qtllm::tools::runtime::ToolExecutionContext &context,
                                                      const QString &requestId,
                                                      int roundIndex,
                                                      int requestCount)
{
    if (!m_repository) {
        return;
    }

    const QString spanId = ensureSpanId(batchSpanKey(context.traceId, requestId, roundIndex));
    ToolsInsideSpanRecord span;
    span.spanId = spanId;
    span.traceId = context.traceId;
    span.parentSpanId = ensureSpanId(requestSpanKey(context.traceId, requestId));
    span.requestId = requestId;
    span.kind = QStringLiteral("tool_batch");
    span.name = QStringLiteral("Tool Batch");
    span.status = QStringLiteral("running");
    span.summary = QJsonObject{
        {QStringLiteral("roundIndex"), roundIndex},
        {QStringLiteral("requestCount"), requestCount}
    };
    m_repository->upsertSpan(span, nullptr);
}

void ToolsInsideTraceRecorder::recordToolCallStarted(const qtllm::tools::runtime::ToolExecutionContext &context,
                                                     const QString &requestId,
                                                     int roundIndex,
                                                     const qtllm::tools::runtime::ToolCallRequest &request)
{
    if (!m_repository) {
        return;
    }

    QString argumentsArtifactId;
    if (m_storagePolicy.persistRawToolArguments) {
        argumentsArtifactId = persistArtifact(context.clientId,
                                             context.sessionId,
                                             context.traceId,
                                             QStringLiteral("tool_arguments"),
                                             QStringLiteral("application/json"),
                                             QJsonDocument(request.arguments).toJson(QJsonDocument::Compact),
                                             QJsonObject{{QStringLiteral("toolId"), request.toolId},
                                                         {QStringLiteral("callId"), request.callId}},
                                             QStringLiteral("json"));
    }

    ToolsInsideToolCallRecord toolCall;
    toolCall.toolCallId = request.internalToolCallId.trimmed().isEmpty()
        ? identity::generateId(identity::IdKind::ToolCall)
        : request.internalToolCallId.trimmed();
    toolCall.externalCallId = request.externalCallId.trimmed().isEmpty() ? request.callId.trimmed() : request.externalCallId.trimmed();
    toolCall.traceId = context.traceId;
    toolCall.requestId = requestId;
    toolCall.toolId = request.toolId;
    toolCall.argumentsArtifactId = argumentsArtifactId;
    toolCall.roundIndex = roundIndex;
    toolCall.status = QStringLiteral("running");
    m_repository->upsertToolCall(toolCall, nullptr);

    const QString spanId = ensureSpanId(toolSpanKey(context.traceId, requestId, toolCall.toolCallId));
    ToolsInsideSpanRecord span;
    span.spanId = spanId;
    span.traceId = context.traceId;
    span.parentSpanId = ensureSpanId(batchSpanKey(context.traceId, requestId, roundIndex));
    span.requestId = requestId;
    span.toolCallId = toolCall.toolCallId;
    span.kind = QStringLiteral("tool_call");
    span.name = request.toolId;
    span.status = QStringLiteral("running");
    m_repository->upsertSpan(span, nullptr);
}

void ToolsInsideTraceRecorder::recordToolCallFinished(const qtllm::tools::runtime::ToolExecutionContext &context,
                                                      const QString &requestId,
                                                      int roundIndex,
                                                      const qtllm::tools::runtime::ToolCallRequest &request,
                                                      const qtllm::tools::runtime::ToolExecutionResult &result)
{
    if (!m_repository) {
        return;
    }

    QString outputArtifactId;
    if (m_storagePolicy.persistRawToolOutput) {
        outputArtifactId = persistArtifact(context.clientId,
                                           context.sessionId,
                                           context.traceId,
                                           QStringLiteral("tool_output"),
                                           QStringLiteral("application/json"),
                                           QJsonDocument(result.output).toJson(QJsonDocument::Compact),
                                           QJsonObject{{QStringLiteral("toolId"), result.toolId},
                                                       {QStringLiteral("callId"), result.callId},
                                                       {QStringLiteral("success"), result.success}},
                                           QStringLiteral("json"));
    }

    ToolsInsideToolCallRecord toolCall;
    toolCall.toolCallId = result.internalToolCallId.trimmed().isEmpty()
        ? (request.internalToolCallId.trimmed().isEmpty() ? identity::generateId(identity::IdKind::ToolCall) : request.internalToolCallId.trimmed())
        : result.internalToolCallId.trimmed();
    toolCall.externalCallId = result.externalCallId.trimmed().isEmpty()
        ? (request.externalCallId.trimmed().isEmpty() ? result.callId.trimmed() : request.externalCallId.trimmed())
        : result.externalCallId.trimmed();
    toolCall.traceId = context.traceId;
    toolCall.requestId = requestId;
    toolCall.toolId = result.toolId.isEmpty() ? request.toolId : result.toolId;
    toolCall.toolName = toolCall.toolId;
    toolCall.toolCategory = toolCall.toolId.startsWith(QStringLiteral("mcp::")) ? QStringLiteral("mcp") : QStringLiteral("builtin");
    toolCall.serverId = toolCall.toolCategory == QStringLiteral("mcp") ? toolCall.toolId.section(QStringLiteral("::"), 1, 1) : QString();
    toolCall.invocationName = request.toolId;
    toolCall.outputArtifactId = outputArtifactId;
    toolCall.roundIndex = roundIndex;
    toolCall.status = result.success ? QStringLiteral("success") : QStringLiteral("failed");
    toolCall.errorCode = result.errorCode;
    toolCall.errorMessage = result.errorMessage;
    toolCall.retryable = result.retryable;
    toolCall.durationMs = result.durationMs;
    toolCall.endedAtUtc = QDateTime::currentDateTimeUtc();
    toolCall.summary = QJsonObject{{QStringLiteral("request"), toJson(request)},
                                   {QStringLiteral("result"), toJson(result)}};
    m_repository->upsertToolCall(toolCall, nullptr);

    ToolsInsideSpanRecord span;
    span.spanId = ensureSpanId(toolSpanKey(context.traceId, requestId, toolCall.toolCallId));
    span.traceId = context.traceId;
    span.parentSpanId = ensureSpanId(batchSpanKey(context.traceId, requestId, roundIndex));
    span.requestId = requestId;
    span.toolCallId = toolCall.toolCallId;
    span.kind = QStringLiteral("tool_call");
    span.name = toolCall.toolId;
    span.status = toolCall.status;
    span.errorCode = toolCall.errorCode;
    span.endedAtUtc = toolCall.endedAtUtc;
    span.summary = toolCall.summary;
    m_repository->upsertSpan(span, nullptr);

    ToolsInsideEventRecord event;
    event.eventId = identity::generateId(identity::IdKind::Event);
    event.traceId = context.traceId;
    event.spanId = span.spanId;
    event.requestId = requestId;
    event.toolCallId = toolCall.toolCallId;
    event.category = QStringLiteral("tool.execution");
    event.name = QStringLiteral("tool_execution_finished");
    event.payload = toJson(result);
    m_repository->upsertEvent(event, nullptr);
}

void ToolsInsideTraceRecorder::recordFollowUpPrompt(const qtllm::tools::runtime::ToolExecutionContext &context,
                                                    const QString &requestId,
                                                    int roundIndex,
                                                    const QString &prompt,
                                                    const QList<qtllm::tools::runtime::ToolExecutionResult> &results)
{
    if (!m_repository) {
        return;
    }

    const QString artifactId = persistArtifact(context.clientId,
                                               context.sessionId,
                                               context.traceId,
                                               QStringLiteral("followup_prompt"),
                                               QStringLiteral("text/plain"),
                                               prompt.toUtf8(),
                                               QJsonObject{{QStringLiteral("roundIndex"), roundIndex},
                                                           {QStringLiteral("resultCount"), results.size()}},
                                               QStringLiteral("txt"));

    QJsonArray resultArray;
    for (const qtllm::tools::runtime::ToolExecutionResult &result : results) {
        resultArray.append(toJson(result));

        ToolsInsideSupportLink link;
        link.supportLinkId = identity::generateId(identity::IdKind::SupportLink);
        link.traceId = context.traceId;
        link.toolCallId = result.internalToolCallId.trimmed().isEmpty()
            ? (result.callId.trimmed().isEmpty() ? QString() : result.callId.trimmed())
            : result.internalToolCallId.trimmed();
        link.targetKind = QStringLiteral("artifact");
        link.targetId = artifactId;
        link.supportType = QStringLiteral("consumed_by_followup");
        link.source = QStringLiteral("runtime");
        link.confidence = 1.0;
        m_repository->upsertSupportLink(link, nullptr);

        ToolsInsideToolCallRecord toolCall;
        toolCall.toolCallId = result.internalToolCallId.trimmed().isEmpty()
            ? (result.callId.trimmed().isEmpty() ? identity::generateId(identity::IdKind::ToolCall) : result.callId.trimmed())
            : result.internalToolCallId.trimmed();
        toolCall.traceId = context.traceId;
        toolCall.requestId = requestId;
        toolCall.externalCallId = result.externalCallId.trimmed().isEmpty() ? result.callId.trimmed() : result.externalCallId.trimmed();
        toolCall.followupArtifactId = artifactId;
        toolCall.roundIndex = roundIndex;
        toolCall.status = result.success ? QStringLiteral("success") : QStringLiteral("failed");
        m_repository->upsertToolCall(toolCall, nullptr);
    }

    ToolsInsideEventRecord event;
    event.eventId = identity::generateId(identity::IdKind::Event);
    event.traceId = context.traceId;
    event.spanId = ensureSpanId(batchSpanKey(context.traceId, requestId, roundIndex));
    event.requestId = requestId;
    event.category = QStringLiteral("tool.loop");
    event.name = QStringLiteral("followup_prompt_built");
    event.payload = QJsonObject{
        {QStringLiteral("artifactId"), artifactId},
        {QStringLiteral("roundIndex"), roundIndex},
        {QStringLiteral("results"), resultArray}
    };
    m_repository->upsertEvent(event, nullptr);

    ToolsInsideSpanRecord batchSpan;
    batchSpan.spanId = ensureSpanId(batchSpanKey(context.traceId, requestId, roundIndex));
    batchSpan.traceId = context.traceId;
    batchSpan.requestId = requestId;
    batchSpan.kind = QStringLiteral("tool_batch");
    batchSpan.name = QStringLiteral("Tool Batch");
    batchSpan.status = QStringLiteral("completed");
    batchSpan.endedAtUtc = QDateTime::currentDateTimeUtc();
    m_repository->upsertSpan(batchSpan, nullptr);
}

void ToolsInsideTraceRecorder::recordFailureGuard(const qtllm::tools::runtime::ToolExecutionContext &context,
                                                  const QString &requestId,
                                                  int roundIndex,
                                                  int consecutiveFailures,
                                                  const QString &reason)
{
    if (!m_repository) {
        return;
    }

    ToolsInsideEventRecord event;
    event.eventId = identity::generateId(identity::IdKind::Event);
    event.traceId = context.traceId;
    event.spanId = ensureSpanId(requestSpanKey(context.traceId, requestId));
    event.requestId = requestId;
    event.category = QStringLiteral("tool.loop");
    event.name = QStringLiteral("failure_guard_triggered");
    event.payload = QJsonObject{
        {QStringLiteral("roundIndex"), roundIndex},
        {QStringLiteral("consecutiveFailures"), consecutiveFailures},
        {QStringLiteral("reason"), reason}
    };
    m_repository->upsertEvent(event, nullptr);
}

void ToolsInsideTraceRecorder::recordTraceCompleted(const QString &clientId,
                                                    const QString &sessionId,
                                                    const QString &traceId,
                                                    const QString &requestId,
                                                    const QString &finalText,
                                                    const QString &finishReason)
{
    if (!m_repository) {
        return;
    }

    const QString artifactId = persistArtifact(clientId,
                                               sessionId,
                                               traceId,
                                               QStringLiteral("final_answer"),
                                               QStringLiteral("text/plain"),
                                               finalText.toUtf8(),
                                               QJsonObject{{QStringLiteral("finishReason"), finishReason}},
                                               QStringLiteral("txt"));

    ToolsInsideEventRecord event;
    event.eventId = identity::generateId(identity::IdKind::Event);
    event.traceId = traceId;
    event.spanId = ensureSpanId(requestSpanKey(traceId, requestId));
    event.requestId = requestId;
    event.category = QStringLiteral("trace");
    event.name = QStringLiteral("trace_completed");
    event.payload = QJsonObject{
        {QStringLiteral("artifactId"), artifactId},
        {QStringLiteral("finishReason"), finishReason},
        {QStringLiteral("finalTextLength"), finalText.size()}
    };
    m_repository->upsertEvent(event, nullptr);

    ToolsInsideSpanRecord span;
    span.spanId = ensureSpanId(requestSpanKey(traceId, requestId));
    span.traceId = traceId;
    span.requestId = requestId;
    span.kind = QStringLiteral("llm_request");
    span.name = QStringLiteral("LLM Request");
    span.status = QStringLiteral("completed");
    span.endedAtUtc = QDateTime::currentDateTimeUtc();
    m_repository->upsertSpan(span, nullptr);

    m_repository->finishTrace(traceId, QStringLiteral("completed"), artifactId, QDateTime::currentDateTimeUtc(), nullptr);
}

void ToolsInsideTraceRecorder::recordTraceError(const QString &clientId,
                                                const QString &sessionId,
                                                const QString &traceId,
                                                const QString &requestId,
                                                const QString &message,
                                                const QString &category)
{
    Q_UNUSED(clientId)
    Q_UNUSED(sessionId)
    if (!m_repository) {
        return;
    }

    ToolsInsideEventRecord event;
    event.eventId = identity::generateId(identity::IdKind::Event);
    event.traceId = traceId;
    event.spanId = ensureSpanId(requestSpanKey(traceId, requestId));
    event.requestId = requestId;
    event.category = category;
    event.name = QStringLiteral("trace_failed");
    event.payload = QJsonObject{{QStringLiteral("error"), message}};
    m_repository->upsertEvent(event, nullptr);

    ToolsInsideSpanRecord span;
    span.spanId = ensureSpanId(requestSpanKey(traceId, requestId));
    span.traceId = traceId;
    span.requestId = requestId;
    span.kind = QStringLiteral("llm_request");
    span.name = QStringLiteral("LLM Request");
    span.status = QStringLiteral("failed");
    span.errorCode = category;
    span.endedAtUtc = QDateTime::currentDateTimeUtc();
    m_repository->upsertSpan(span, nullptr);

    m_repository->finishTrace(traceId, QStringLiteral("failed"), QString(), QDateTime::currentDateTimeUtc(), nullptr);
}

QString ToolsInsideTraceRecorder::ensureSpanId(const QString &key) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_spanIdByKey.contains(key)) {
        m_spanIdByKey.insert(key, identity::generateId(identity::IdKind::Span));
    }
    return m_spanIdByKey.value(key);
}

QString ToolsInsideTraceRecorder::requestSpanKey(const QString &traceId, const QString &requestId) const
{
    return traceId + QStringLiteral("::request::") + requestId;
}

QString ToolsInsideTraceRecorder::toolSpanKey(const QString &traceId, const QString &requestId, const QString &toolCallId) const
{
    return traceId + QStringLiteral("::request::") + requestId + QStringLiteral("::tool::") + toolCallId;
}

QString ToolsInsideTraceRecorder::batchSpanKey(const QString &traceId, const QString &requestId, int roundIndex) const
{
    return traceId + QStringLiteral("::request::") + requestId + QStringLiteral("::batch::") + QString::number(roundIndex);
}

QString ToolsInsideTraceRecorder::persistArtifact(const QString &clientId,
                                                  const QString &sessionId,
                                                  const QString &traceId,
                                                  const QString &kind,
                                                  const QString &mimeType,
                                                  const QByteArray &payload,
                                                  const QJsonObject &metadata,
                                                  const QString &preferredExtension) const
{
    if (!m_repository || !m_artifactStore) {
        return QString();
    }

    QString errorMessage;
    const ToolsInsideArtifactRef artifact = m_artifactStore->writeArtifact(clientId,
                                                                           sessionId,
                                                                           traceId,
                                                                           kind,
                                                                           mimeType,
                                                                           payload,
                                                                           *m_redactionPolicy,
                                                                           metadata,
                                                                           preferredExtension,
                                                                           &errorMessage);
    if (artifact.artifactId.isEmpty()) {
        return QString();
    }

    m_repository->upsertArtifact(artifact, nullptr);
    return artifact.artifactId;
}

} // namespace qtllm::toolsinside
