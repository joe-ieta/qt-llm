#include "qtllmclient.h"

#include "../identity/compactid.h"
#include "../logging/qtllmlogger.h"
#include "../network/httpexecutor.h"
#include "../providers/illmprovider.h"
#include "../providers/providerfactory.h"
#include "../runtime/managedllamacppruntime.h"
#include "../streaming/streamchunkparser.h"
#include "../tools/runtime/toolcallorchestrator.h"
#include "../tools/runtime/toolruntime_types.h"
#include "../toolsinside/toolsinsideruntime.h"
#include "../toolsinside/toolsinsidetracerecorder.h"

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
    if (!m_provider) {
        if (!m_config.providerName.isEmpty()) {
            if (!setProviderByName(m_config.providerName)) {
                return;
            }
        } else {
            const QString message = QStringLiteral("No provider configured");
            logging::QtLlmLogger::instance().error(QStringLiteral("llm.provider"), message,
                                                   logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId));
            emit errorOccurred(message);
            return;
        }
    }

    if (!ensureManagedRuntime()) {
        return;
    }

    dispatchRequest(request);
}

void QtLLMClient::cancelCurrentRequest()
{
    logging::QtLlmLogger::instance().info(QStringLiteral("llm.request"),
                                          QStringLiteral("Current request cancellation requested"),
                                          logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId));
    m_executor->cancel();
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
    m_activeRequestId = identity::generateId(identity::IdKind::Request);
    if (m_toolLoopTraceId.trimmed().isEmpty()) {
        m_toolLoopTraceId = identity::generateId(identity::IdKind::Trace);
    }

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
    toolsinside::ToolsInsideRuntime::instance().recorder()->recordRequestDispatched(m_toolLoopClientId,
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

bool QtLLMClient::ensureManagedRuntime()
{
    if (!runtime::ManagedLlamaCppRuntime::isManagedProvider(m_config.providerName)) {
        return true;
    }

    if (!m_llamaCppRuntime) {
        m_llamaCppRuntime = std::make_unique<runtime::ManagedLlamaCppRuntime>(this);
    }

    QString errorMessage;
    LlmConfig runtimeConfig = m_config;
    if (!m_llamaCppRuntime->ensureRunning(&runtimeConfig, &errorMessage)) {
        logging::QtLlmLogger::instance().error(QStringLiteral("llm.runtime"),
                                               QStringLiteral("Managed llama.cpp runtime failed"),
                                               logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId),
                                               QJsonObject{{QStringLiteral("error"), errorMessage}});
        emit errorOccurred(errorMessage);
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
        if (!m_provider || !m_activeRequest.stream) {
            return;
        }

        const QStringList lines = m_streamParser->append(chunk);
        for (const QString &line : lines) {
            const QList<LlmStreamDelta> deltas = m_provider->parseStreamDeltas(line.toUtf8());
            for (const LlmStreamDelta &delta : deltas) {
                if (delta.channel == QStringLiteral("reasoning")) {
                    m_accumulatedReasoning += delta.text;
                    emit reasoningTokenReceived(delta.text);
                    toolsinside::ToolsInsideRuntime::instance().recorder()->recordFirstStreamToken(m_toolLoopTraceId, m_activeRequestId, QStringLiteral("reasoning"));
                    continue;
                }

                m_accumulatedText += delta.text;
                emit tokenReceived(delta.text);
                toolsinside::ToolsInsideRuntime::instance().recorder()->recordFirstStreamToken(m_toolLoopTraceId, m_activeRequestId, QStringLiteral("content"));
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
            toolsinside::ToolsInsideRuntime::instance().recorder()->recordTraceError(m_toolLoopClientId,
                                                                                      m_toolLoopSessionId,
                                                                                      m_toolLoopTraceId,
                                                                                      m_activeRequestId,
                                                                                      response.errorMessage,
                                                                                      QStringLiteral("llm.response"));
            emit errorOccurred(response.errorMessage);
            m_streamParser->clear();
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

        toolsinside::ToolsInsideRuntime::instance().recorder()->recordResponseParsed(m_toolLoopTraceId, m_activeRequestId, response, finalText);

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
                logging::QtLlmLogger::instance().warn(QStringLiteral("tool.loop"),
                                                      QStringLiteral("Tool loop stopped after repeated failures"),
                                                      logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId));
                toolsinside::ToolsInsideRuntime::instance().recorder()->recordTraceError(m_toolLoopClientId,
                                                                                          m_toolLoopSessionId,
                                                                                          m_toolLoopTraceId,
                                                                                          m_activeRequestId,
                                                                                          QStringLiteral("Tool loop stopped after repeated failures"),
                                                                                          QStringLiteral("tool.loop"));
                emit errorOccurred(QStringLiteral("Tool loop stopped after repeated failures"));
                emit responseReceived(response);
                emit completed(finalText);
                m_streamParser->clear();
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

        toolsinside::ToolsInsideRuntime::instance().recorder()->recordTraceCompleted(m_toolLoopClientId,
                                                                                      m_toolLoopSessionId,
                                                                                      m_toolLoopTraceId,
                                                                                      m_activeRequestId,
                                                                                      finalText,
                                                                                      response.finishReason);
        emit responseReceived(response);
        emit completed(finalText);
        m_streamParser->clear();
    });

    connect(m_executor, &HttpExecutor::errorOccurred, this, [this](const QString &message) {
        m_streamParser->clear();
        logging::QtLlmLogger::instance().error(QStringLiteral("llm.request"),
                                               QStringLiteral("HTTP executor reported request error"),
                                               logContext(m_toolLoopClientId, m_toolLoopSessionId, m_activeRequestId, m_toolLoopTraceId),
                                               QJsonObject{{QStringLiteral("error"), message}});
        toolsinside::ToolsInsideRuntime::instance().recorder()->recordTraceError(m_toolLoopClientId,
                                                                                  m_toolLoopSessionId,
                                                                                  m_toolLoopTraceId,
                                                                                  m_activeRequestId,
                                                                                  message,
                                                                                  QStringLiteral("llm.request"));
        emit errorOccurred(message);
    });
}

} // namespace qtllm

