#include "qtllmclient.h"

#include "../structuredoutput/structuredoutputservice.h"

#include "../identity/compactid.h"
#include "../events/llmeventdispatcher.h"
#include "../logging/qtllmlogger.h"
#include "../network/httpexecutor.h"
#include "../providers/illmprovider.h"
#include "../providers/providerfactory.h"
#include "../runtime/managedllamacppruntime.h"
#include "../streaming/streamchunkparser.h"
#include "../tools/runtime/toolcallorchestrator.h"
#include "../tools/runtime/toolruntime_types.h"

#include <QDateTime>
#include <QJsonObject>
#include <QUrlQuery>

namespace qtllm {

namespace {

logging::LogContext logContext(const QString &clientId, const QString &sessionId, const QString &requestId, const QString &traceId)
{
    logging::LogContext context;
    context.clientId = clientId;
    context.sessionId = sessionId;
    context.requestId = requestId;
    context.traceId = traceId;
    return context;
}

QString redactedUrl(const QNetworkRequest &request)
{
    QUrl url = request.url();
    QUrlQuery query(url);
    if (query.hasQueryItem(QStringLiteral("key"))) {
        query.removeAllQueryItems(QStringLiteral("key"));
        query.addQueryItem(QStringLiteral("key"), QStringLiteral("***"));
        url.setQuery(query);
    }
    return url.toString();
}

} // namespace

QtLLMClient::QtLLMClient(QObject *parent)
    : QObject(parent)
    , m_executor(new HttpExecutor(this))
    , m_streamParser(std::make_unique<StreamChunkParser>())
    , m_toolOrchestrator(std::make_shared<tools::runtime::ToolCallOrchestrator>())
{
    wireExecutor();
}

QtLLMClient::~QtLLMClient() = default;

void QtLLMClient::setConfig(const LlmConfig &config)
{
    m_config = config;
    if (m_provider) {
        m_provider->setConfig(m_config);
    }
}

void QtLLMClient::setProvider(std::unique_ptr<ILLMProvider> provider)
{
    m_provider = std::move(provider);
    if (m_provider) {
        m_provider->setConfig(m_config);
    }
}

bool QtLLMClient::setProviderByName(const QString &providerName)
{
    std::unique_ptr<ILLMProvider> provider = ProviderFactory::create(providerName);
    if (!provider) {
        const QString message = QStringLiteral("Unsupported provider: ") + providerName;
        logging::QtLlmLogger::instance().error(QStringLiteral("llm.provider"),
                                               QStringLiteral("Provider resolution failed"),
                                               logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId),
                                               QJsonObject{{QStringLiteral("providerName"), providerName}});
        emit errorOccurred(message);
        return false;
    }

    setProvider(std::move(provider));
    return true;
}

void QtLLMClient::setToolCallOrchestrator(
    const std::shared_ptr<tools::runtime::ToolCallOrchestrator> &orchestrator)
{
    if (orchestrator) {
        m_toolOrchestrator = orchestrator;
    }
}

void QtLLMClient::setToolLoopContext(const QString &clientId, const QString &sessionId, const QString &traceId)
{
    m_toolLoopClientId = clientId.trimmed();
    m_toolLoopSessionId = sessionId.trimmed();
    m_toolLoopTraceId = traceId.trimmed();
}

void QtLLMClient::sendPrompt(const QString &prompt)
{
    LlmRequest request;
    request.model = m_config.model;
    request.stream = m_config.stream;
    request.messages.append({QStringLiteral("user"), prompt});
    sendRequest(request);
}

void QtLLMClient::sendRequest(const LlmRequest &request)
{
    if (m_requestActive) {
        emit requestRejected(QStringLiteral("request_in_progress"),
                             QStringLiteral("A request is already active"));
        return;
    }

    beginRequest(request.requestId);

    if (!m_provider) {
        if (!m_config.providerName.isEmpty()) {
            if (!setProviderByName(m_config.providerName)) {
                LlmResponse response;
                response.errorMessage = QStringLiteral("Unsupported provider: ") + m_config.providerName;
                response.error.category = LlmErrorCategory::Configuration;
                response.error.code = QStringLiteral("unsupported_provider");
                response.error.message = response.errorMessage;
                finishRequest(response, false);
                return;
            }
        } else {
            const QString message = QStringLiteral("No provider configured");
            logging::QtLlmLogger::instance().error(QStringLiteral("llm.provider"), message,
                                                   logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId));
            LlmResponse response;
            response.errorMessage = message;
            response.error.category = LlmErrorCategory::Configuration;
            response.error.code = QStringLiteral("provider_not_configured");
            response.error.message = message;
            finishRequest(response);
            return;
        }
    }

    QString constraintCode;
    QString constraintMessage;
    if (!StructuredOutputService::validateConstraint(
            request.output, &constraintCode, &constraintMessage)) {
        LlmResponse response;
        response.errorMessage = constraintMessage;
        response.error.category = LlmErrorCategory::Configuration;
        response.error.code = constraintCode;
        response.error.message = constraintMessage;
        finishRequest(response);
        return;
    }

    if (request.output.format != StructuredOutputFormat::Text
        && request.output.mode == OutputConstraintMode::Native) {
        const QString model = request.model.trimmed().isEmpty() ? m_config.model : request.model;
        const ModelCapabilitySnapshot snapshot = StructuredOutputService::capabilities(
            m_provider->name(), model, m_config.modelVendor);
        const CapabilityDescriptor capability =
            request.output.format == StructuredOutputFormat::JsonSchema
                ? snapshot.jsonSchemaOutput
                : snapshot.jsonOutput;
        if (capability.adapterSupport == CapabilitySupport::Unsupported) {
            LlmResponse response;
            response.errorMessage = QStringLiteral(
                "Native structured output is unsupported by the provider adapter");
            response.error.category = LlmErrorCategory::Configuration;
            response.error.code = QStringLiteral("structured_output_unsupported");
            response.error.message = response.errorMessage;
            finishRequest(response);
            return;
        }
    }

    QString runtimeError;
    if (!ensureManagedRuntime(&runtimeError)) {
        LlmResponse response;
        response.errorMessage = runtimeError;
        response.error.category = LlmErrorCategory::Runtime;
        response.error.code = QStringLiteral("managed_runtime_unavailable");
        response.error.message = runtimeError;
        finishRequest(response);
        return;
    }

    dispatchRequest(request);
}

void QtLLMClient::cancelCurrentRequest()
{
    if (!m_requestActive || m_terminalEmitted) {
        return;
    }

    logging::QtLlmLogger::instance().info(QStringLiteral("llm.request"),
                                          QStringLiteral("Current request cancellation requested"),
                                          logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId));
    emit cancellationRequested(m_activeRequestId);
    events::LlmEventDispatcher::instance().recordCancellationRequested(
        m_toolLoopClientId, m_toolLoopSessionId, m_toolLoopTraceId, m_activeRequestId);
    m_executor->cancel();
}

void QtLLMClient::beginRequest(const QString &requestId)
{
    m_requestActive = true;
    m_terminalEmitted = false;
    m_activeRequestId = requestId.trimmed().isEmpty()
        ? identity::generateId(identity::IdKind::Request)
        : requestId.trimmed();
    if (m_toolLoopTraceId.trimmed().isEmpty()) {
        m_toolLoopTraceId = identity::generateId(identity::IdKind::Trace);
    }
    emit requestStarted(m_activeRequestId);
}

void QtLLMClient::dispatchRequest(const LlmRequest &request)
{
    m_accumulatedText.clear();
    m_accumulatedReasoning.clear();
    m_streamParser->clear();

    LlmRequest resolved = request;
    if (resolved.model.trimmed().isEmpty()) {
        resolved.model = m_config.model;
    }
    m_activeRequest = resolved;

    const QNetworkRequest networkRequest = m_provider->buildRequest(resolved);
    const QByteArray payload = m_provider->buildPayload(resolved);
    const QString payloadJson = QString::fromUtf8(payload);
    const QString url = redactedUrl(networkRequest);

    logging::QtLlmLogger::instance().info(QStringLiteral("llm.request"),
                                          QStringLiteral("Dispatching LLM request"),
                                          logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId),
                                          QJsonObject{{QStringLiteral("providerName"), m_config.providerName},
                                                      {QStringLiteral("model"), resolved.model},
                                                      {QStringLiteral("stream"), resolved.stream},
                                                      {QStringLiteral("messageCount"), resolved.messages.size()},
                                                      {QStringLiteral("toolCount"), resolved.tools.size()},
                                                      {QStringLiteral("payloadBytes"), payload.size()},
                                                      {QStringLiteral("url"), url}});

    emit providerPayloadPrepared(url, payloadJson);
    events::LlmEventDispatcher::instance().recordRequestDispatched(m_toolLoopClientId,
                                                                                     m_toolLoopSessionId,
                                                                                     m_toolLoopTraceId,
                                                                                     m_activeRequestId,
                                                                                     m_config.providerName,
                                                                                     resolved.model,
                                                                                     url,
                                                                                     payloadJson,
                                                                                     resolved.messages.size(),
                                                                                     resolved.tools.size());

    HttpRequestOptions options;
    options.timeoutMs = m_config.timeoutMs;
    options.maxRetries = m_config.maxRetries;
    options.retryDelayMs = m_config.retryDelayMs;

    m_executor->post(networkRequest, payload, options);
}

void QtLLMClient::finishRequest(LlmResponse response, bool emitCompatibilitySignal)
{
    if (!m_requestActive || m_terminalEmitted) {
        return;
    }

    m_terminalEmitted = true;
    m_requestActive = false;
    response.requestId = m_activeRequestId;
    if (!response.success) {
        if (response.error.message.isEmpty()) {
            response.error.message = response.errorMessage;
        }
        if (response.errorMessage.isEmpty()) {
            response.errorMessage = response.error.message;
        }
        response.canceled = response.error.category == LlmErrorCategory::Canceled;
    }

    m_streamParser->clear();
    emit requestFinished(response);

    if (!emitCompatibilitySignal) {
        return;
    }

    if (response.success) {
        emit responseReceived(response);
        emit completed(response.assistantMessage.content.isEmpty() ? response.text
                                                                    : response.assistantMessage.content);
        return;
    }

    emit errorOccurred(response.errorMessage);
}

bool QtLLMClient::ensureManagedRuntime(QString *errorMessage)
{
    if (!runtime::ManagedLlamaCppRuntime::isManagedProvider(m_config.providerName)) {
        return true;
    }

    if (!m_llamaCppRuntime) {
        m_llamaCppRuntime = std::make_unique<runtime::ManagedLlamaCppRuntime>(this);
    }

    QString runtimeError;
    LlmConfig runtimeConfig = m_config;
    if (!m_llamaCppRuntime->ensureRunning(&runtimeConfig, &runtimeError)) {
        logging::QtLlmLogger::instance().error(QStringLiteral("llm.runtime"),
                                               QStringLiteral("Managed llama.cpp runtime failed"),
                                               logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId),
                                               QJsonObject{{QStringLiteral("error"), runtimeError}});
        if (errorMessage) {
            *errorMessage = runtimeError;
        }
        return false;
    }

    m_config = runtimeConfig;
    if (m_provider) {
        m_provider->setConfig(m_config);
    }
    return true;
}

void QtLLMClient::wireExecutor()
{
    connect(m_executor, &HttpExecutor::dataReceived, this, [this](const QByteArray &chunk) {
        if (!m_requestActive || !m_provider || !m_activeRequest.stream) {
            return;
        }

        const QStringList lines = m_streamParser->append(chunk);
        for (const QString &line : lines) {
            const QList<LlmStreamDelta> deltas = m_provider->parseStreamDeltas(line.toUtf8());
            for (const LlmStreamDelta &delta : deltas) {
                if (delta.channel == QStringLiteral("reasoning")) {
                    m_accumulatedReasoning += delta.text;
                    emit reasoningTokenReceived(delta.text);
                    events::LlmEventDispatcher::instance().recordStreamDelta(
                        m_toolLoopTraceId, m_activeRequestId, QStringLiteral("reasoning"), delta.text.size());
                    events::LlmEventDispatcher::instance().recordFirstStreamToken(
                        m_toolLoopTraceId, m_activeRequestId, QStringLiteral("reasoning"));
                    continue;
                }

                m_accumulatedText += delta.text;
                emit tokenReceived(delta.text);
                events::LlmEventDispatcher::instance().recordStreamDelta(
                    m_toolLoopTraceId, m_activeRequestId, QStringLiteral("content"), delta.text.size());
                events::LlmEventDispatcher::instance().recordFirstStreamToken(
                    m_toolLoopTraceId, m_activeRequestId, QStringLiteral("content"));
            }
        }
    });

    connect(m_executor, &HttpExecutor::requestFinished, this, [this](const QByteArray &data) {
        if (!m_provider) {
            return;
        }

        if (m_activeRequest.stream) {
            const QString pendingLine = m_streamParser->takePendingLine();
            if (!pendingLine.isEmpty()) {
                const QList<LlmStreamDelta> deltas = m_provider->parseStreamDeltas(pendingLine.toUtf8());
                for (const LlmStreamDelta &delta : deltas) {
                    if (delta.channel == QStringLiteral("reasoning")) {
                        m_accumulatedReasoning += delta.text;
                        emit reasoningTokenReceived(delta.text);
                        continue;
                    }

                    m_accumulatedText += delta.text;
                    emit tokenReceived(delta.text);
                }
            }
        }

        LlmResponse response = m_provider->parseResponse(data);

        if (!m_accumulatedText.isEmpty() && response.assistantMessage.content.isEmpty()) {
            response.assistantMessage.role = QStringLiteral("assistant");
            response.assistantMessage.content = m_accumulatedText;
            response.text = m_accumulatedText;
            response.success = true;
        }

        if (!response.success) {
            logging::QtLlmLogger::instance().error(QStringLiteral("llm.response"),
                                                   QStringLiteral("LLM response parsing failed"),
                                                   logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId),
                                                   QJsonObject{{QStringLiteral("error"), response.errorMessage},
                                                               {QStringLiteral("responseBytes"), data.size()}});
            events::LlmEventDispatcher::instance().recordTraceError(m_toolLoopClientId,
                                                                                      m_toolLoopSessionId,
                                                                                      m_toolLoopTraceId,
                                                                                      m_activeRequestId,
                                                                                      response.errorMessage,
                                                                                      QStringLiteral("llm.response"));
            response.error.category = LlmErrorCategory::Protocol;
            response.error.code = QStringLiteral("response_parse_failed");
            response.error.message = response.errorMessage;
            finishRequest(response);
            return;
        }

        QString finalText = response.assistantMessage.content;
        if (finalText.isEmpty()) {
            finalText = response.text;
        }

        logging::QtLlmLogger::instance().info(QStringLiteral("llm.response"),
                                              QStringLiteral("LLM response received"),
                                              logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId),
                                              QJsonObject{{QStringLiteral("assistantTextLength"), finalText.size()},
                                                          {QStringLiteral("reasoningLength"), m_accumulatedReasoning.size()},
                                                          {QStringLiteral("toolCallCount"), response.assistantMessage.toolCalls.size()},
                                                          {QStringLiteral("finishReason"), response.finishReason}});

        events::LlmEventDispatcher::instance().recordResponseParsed(
            m_toolLoopTraceId, m_activeRequestId, response, finalText);

        if (m_toolOrchestrator && !m_toolLoopClientId.isEmpty() && !m_toolLoopSessionId.isEmpty()) {
            tools::runtime::ToolExecutionContext context;
            context.clientId = m_toolLoopClientId;
            context.sessionId = m_toolLoopSessionId;
            context.llmConfig = m_config;
            context.historyWindow = m_activeRequest.messages;
            context.requestId = m_activeRequestId;
            context.traceId = m_toolLoopTraceId;
            context.extra.insert(QStringLiteral("requestTimestamp"),
                                 QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

            const tools::runtime::ToolLoopOutcome outcome = m_toolOrchestrator->processAssistantResponse(
                m_activeRequest.model,
                m_config.modelVendor,
                m_config.providerName,
                response,
                context);

            if (outcome.terminatedByFailureGuard) {
                const QString failureMessage = QStringLiteral("Tool loop stopped after repeated failures");
                logging::QtLlmLogger::instance().warn(QStringLiteral("tool.loop"),
                                                      failureMessage,
                                                      logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId));
                events::LlmEventDispatcher::instance().recordTraceError(m_toolLoopClientId,
                                                                                          m_toolLoopSessionId,
                                                                                          m_toolLoopTraceId,
                                                                                          m_activeRequestId,
                                                                                          failureMessage,
                                                                                          QStringLiteral("tool.loop"));
                response.success = false;
                response.errorMessage = failureMessage;
                response.error.category = LlmErrorCategory::Tool;
                response.error.code = QStringLiteral("tool_failure_guard");
                response.error.message = failureMessage;
                finishRequest(response);
                return;
            }

            if (outcome.hasFollowUpPrompt && !outcome.followUpPrompt.trimmed().isEmpty()) {
                LlmMessage assistant = response.assistantMessage;
                if (assistant.role.trimmed().isEmpty()) {
                    assistant.role = QStringLiteral("assistant");
                }
                if (assistant.content.isEmpty()) {
                    assistant.content = finalText;
                }

                LlmMessage user;
                user.role = QStringLiteral("user");
                user.content = outcome.followUpPrompt;

                m_activeRequest.messages.append(assistant);
                m_activeRequest.messages.append(user);
                dispatchRequest(m_activeRequest);
                return;
            }
        }

        response.text = finalText;
        response.structuredOutput = StructuredOutputService::validate(
            finalText, m_activeRequest.output);
        if (response.structuredOutput.requested
            && (!response.structuredOutput.syntaxValid
                || !response.structuredOutput.schemaValid)) {
            response.success = false;
            response.errorMessage = response.structuredOutput.errorMessage;
            response.error.category = response.structuredOutput.syntaxValid
                ? LlmErrorCategory::Schema
                : LlmErrorCategory::Parsing;
            response.error.code = response.structuredOutput.errorCode;
            response.error.message = response.structuredOutput.errorMessage;
            events::LlmEventDispatcher::instance().recordTraceError(
                m_toolLoopClientId,
                m_toolLoopSessionId,
                m_toolLoopTraceId,
                m_activeRequestId,
                response.errorMessage,
                QStringLiteral("llm.structured_output"));
            finishRequest(response);
            return;
        }

        events::LlmEventDispatcher::instance().recordTraceCompleted(m_toolLoopClientId,
                                                                                      m_toolLoopSessionId,
                                                                                      m_toolLoopTraceId,
                                                                                      m_activeRequestId,
                                                                                      finalText,
                                                                                      response.finishReason);
        finishRequest(response);
    });

    connect(m_executor, &HttpExecutor::attemptStarted, this, [this](int attempt) {
        events::LlmEventDispatcher::instance().recordRequestAttemptStarted(
            m_toolLoopClientId, m_toolLoopSessionId, m_toolLoopTraceId, m_activeRequestId, attempt);
    });

    connect(m_executor, &HttpExecutor::attemptReset, this, [this](int previousAttempt, int nextAttempt) {
        events::LlmEventDispatcher::instance().recordRequestAttemptReset(
            m_toolLoopClientId, m_toolLoopSessionId, m_toolLoopTraceId, m_activeRequestId,
            previousAttempt, nextAttempt);
        m_accumulatedText.clear();
        m_accumulatedReasoning.clear();
        m_streamParser->clear();
        emit streamReset(m_activeRequestId, nextAttempt);
    });

    connect(m_executor, &HttpExecutor::requestFailed, this, [this](const HttpRequestError &requestError) {
        logging::QtLlmLogger::instance().error(QStringLiteral("llm.request"),
                                               QStringLiteral("HTTP executor reported request error"),
                                               logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId),
                                               QJsonObject{{QStringLiteral("error"), requestError.message},
                                                           {QStringLiteral("errorCode"), requestError.code},
                                                           {QStringLiteral("httpStatus"), requestError.httpStatus},
                                                           {QStringLiteral("attempt"), requestError.attempt}});
        events::LlmEventDispatcher::instance().recordTraceError(m_toolLoopClientId,
                                                                                  m_toolLoopSessionId,
                                                                                  m_toolLoopTraceId,
                                                                                  m_activeRequestId,
                                                                                  requestError.message,
                                                                                  QStringLiteral("llm.request"));

        LlmResponse response;
        response.errorMessage = requestError.message;
        response.error.code = requestError.code;
        response.error.message = requestError.message;
        response.error.diagnostic = requestError.diagnostic;
        response.error.retryable = requestError.retryable;
        response.error.httpStatus = requestError.httpStatus;
        response.error.attempt = requestError.attempt;
        switch (requestError.category) {
        case HttpErrorCategory::Canceled:
            response.error.category = LlmErrorCategory::Canceled;
            break;
        case HttpErrorCategory::Timeout:
        case HttpErrorCategory::Network:
        case HttpErrorCategory::Http:
            response.error.category = LlmErrorCategory::Transport;
            break;
        }
        finishRequest(response);
    });
}

} // namespace qtllm

