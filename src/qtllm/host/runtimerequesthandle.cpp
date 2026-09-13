#include "runtimerequesthandle.h"

#include "runtimeprofilemapper.h"
#include "../core/qtllmclient.h"
#include "../events/llmeventdispatcher.h"
#include "../identity/compactid.h"
#include "../structuredoutput/structuredoutputservice.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace qtllm::host {

namespace {

QString resolvedId(const QString &value, identity::IdKind kind)
{
    return value.trimmed().isEmpty() ? identity::generateId(kind) : value.trimmed();
}

bool hasInput(const ChatRequest &request)
{
    return !request.messages.isEmpty() || !request.userPrompt.trimmed().isEmpty();
}

QString turnInput(const ChatRequest &request)
{
    if (!request.userPrompt.trimmed().isEmpty()) {
        return request.userPrompt.trimmed();
    }
    return request.messages.isEmpty() ? QString() : request.messages.constLast().content;
}

QJsonObject requestToJson(const LlmRequest &request)
{
    return QJsonObject{{QStringLiteral("model"), request.model},
                       {QStringLiteral("stream"), request.stream},
                       {QStringLiteral("messageCount"), request.messages.size()},
                       {QStringLiteral("toolCount"), request.tools.size()},
                       {QStringLiteral("requestId"), request.requestId}};
}

} // namespace

RuntimeRequestHandle::RuntimeRequestHandle(const RuntimeProfile &profile,
                                           const ChatRequest &request,
                                           QObject *parent)
    : QObject(parent)
    , m_profile(profile)
    , m_request(request)
    , m_requestId(identity::generateId(identity::IdKind::Request))
    , m_client(new QtLLMClient(this))
{
    qRegisterMetaType<qtllm::host::RuntimeRequestState>("qtllm::host::RuntimeRequestState");
    qRegisterMetaType<qtllm::host::ChatResult>("qtllm::host::ChatResult");

    m_request.clientId = resolvedId(m_request.clientId, identity::IdKind::Client);
    m_request.sessionId = resolvedId(m_request.sessionId, identity::IdKind::Session);
    m_request.traceId = resolvedId(m_request.traceId, identity::IdKind::Trace);

    m_result.requestId = m_requestId;
    m_result.providerName = m_profile.providerName;
    m_result.model = m_request.model.trimmed().isEmpty() ? m_profile.model : m_request.model.trimmed();
    m_result.clientId = m_request.clientId;
    m_result.sessionId = m_request.sessionId;
    m_result.traceId = m_request.traceId;
    m_result.metadata = m_request.metadata;

    connect(m_client, &QtLLMClient::tokenReceived, this, [this](const QString &token) {
        if (!m_finished) {
            emit tokenReceived(m_requestId, token);
        }
    });
    connect(m_client, &QtLLMClient::reasoningTokenReceived, this, [this](const QString &token) {
        if (!m_finished) {
            emit reasoningTokenReceived(m_requestId, token);
        }
    });
    connect(m_client, &QtLLMClient::streamReset, this, [this](const QString &, int nextAttempt) {
        if (!m_finished) {
            emit streamReset(m_requestId, nextAttempt);
        }
    });
    connect(m_client, &QtLLMClient::providerPayloadPrepared, this,
            [this](const QString &url, const QString &payloadJson) {
                if (!m_finished) {
                    emit providerPayloadPrepared(m_requestId, url, payloadJson);
                }
            });
    connect(m_client, &QtLLMClient::requestFinished, this,
            [this](const LlmResponse &response) { finishFromResponse(response); });
    connect(m_client, &QtLLMClient::requestRejected, this,
            [this](const QString &code, const QString &message) {
                finishWithError(LlmErrorCategory::Runtime, code, message);
            });
}

RuntimeRequestHandle::~RuntimeRequestHandle() = default;

QString RuntimeRequestHandle::requestId() const
{
    return m_requestId;
}

RuntimeRequestState RuntimeRequestHandle::state() const
{
    return m_state;
}

bool RuntimeRequestHandle::isFinished() const
{
    return m_finished;
}

ChatRequest RuntimeRequestHandle::request() const
{
    return m_request;
}

ChatResult RuntimeRequestHandle::result() const
{
    return m_result;
}

void RuntimeRequestHandle::start()
{
    if (m_started || m_finished) {
        return;
    }
    m_started = true;

    if (!hasInput(m_request)) {
        finishWithError(LlmErrorCategory::Configuration,
                        QStringLiteral("empty_prompt"),
                        QStringLiteral("ChatRequest has no messages or userPrompt"));
        return;
    }
    if (m_profile.providerName.trimmed().isEmpty()) {
        finishWithError(LlmErrorCategory::Configuration,
                        QStringLiteral("missing_provider"),
                        QStringLiteral("RuntimeProfile.providerName is empty"));
        return;
    }

    QString constraintCode;
    QString constraintMessage;
    if (!StructuredOutputService::validateConstraint(
            m_request.output, &constraintCode, &constraintMessage)) {
        finishWithError(LlmErrorCategory::Configuration,
                        constraintCode,
                        constraintMessage);
        return;
    }

    if (m_request.output.format != StructuredOutputFormat::Text
        && m_request.output.mode == OutputConstraintMode::Native) {
        const QString model = m_request.model.trimmed().isEmpty()
            ? m_profile.model
            : m_request.model.trimmed();
        const ModelCapabilitySnapshot snapshot = StructuredOutputService::capabilities(
            m_profile.providerName,
            model,
            m_profile.modelVendor,
            m_profile.modelCapabilities);
        const CapabilityDescriptor capability =
            m_request.output.format == StructuredOutputFormat::JsonSchema
                ? snapshot.jsonSchemaOutput
                : snapshot.jsonOutput;
        if (capability.effectiveSupport() == CapabilitySupport::Unsupported) {
            finishWithError(LlmErrorCategory::Configuration,
                            QStringLiteral("structured_output_unsupported"),
                            QStringLiteral("Native structured output is unsupported by the adapter or model configuration"));
            return;
        }
    }

    m_client->setConfig(RuntimeProfileMapper::toConfig(m_profile));
    m_client->setToolLoopContext(m_request.clientId, m_request.sessionId, m_request.traceId);
    if (!m_client->setProviderByName(m_profile.providerName)) {
        finishWithError(LlmErrorCategory::Configuration,
                        QStringLiteral("unsupported_provider"),
                        QStringLiteral("Unsupported provider: ") + m_profile.providerName);
        return;
    }

    LlmRequest llmRequest = RuntimeProfileMapper::toRequest(m_profile, m_request);
    llmRequest.requestId = m_requestId;
    const QString requestJson = QString::fromUtf8(
        QJsonDocument(requestToJson(llmRequest)).toJson(QJsonDocument::Compact));

    events::LlmEventDispatcher::instance().startTrace(
        m_request.clientId, m_request.sessionId, m_request.traceId, turnInput(m_request),
        m_profile.providerName, llmRequest.model, m_profile.modelVendor);
    events::LlmEventDispatcher::instance().recordRequestPrepared(
        m_request.clientId, m_request.sessionId, m_request.traceId, requestJson);

    transitionTo(RuntimeRequestState::Running);
    m_client->sendRequest(llmRequest);
}

void RuntimeRequestHandle::cancel()
{
    if (m_finished) {
        return;
    }

    transitionTo(RuntimeRequestState::CancelRequested);
    if (!m_started) {
        finishWithError(LlmErrorCategory::Canceled,
                        QStringLiteral("request_canceled"),
                        QStringLiteral("Request canceled"),
                        true);
        return;
    }
    m_client->cancelCurrentRequest();
}

void RuntimeRequestHandle::transitionTo(RuntimeRequestState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(m_requestId, m_state);
}

void RuntimeRequestHandle::finishFromResponse(const LlmResponse &response)
{
    if (m_finished) {
        return;
    }

    m_result.response = response;
    m_result.requestId = response.requestId.isEmpty() ? m_requestId : response.requestId;
    m_result.success = response.success;
    m_result.text = response.assistantMessage.content.isEmpty() ? response.text
                                                                : response.assistantMessage.content;
    m_result.canceled = response.canceled;
    m_result.error = response.error;
    m_result.errorCode = response.error.code;
    m_result.errorMessage = response.errorMessage;
    m_finished = true;

    transitionTo(response.success
                     ? RuntimeRequestState::Succeeded
                     : (response.canceled ? RuntimeRequestState::Canceled
                                          : RuntimeRequestState::Failed));
    emit finished(m_result);
}

void RuntimeRequestHandle::finishWithError(LlmErrorCategory category,
                                           const QString &code,
                                           const QString &message,
                                           bool canceled)
{
    if (m_finished) {
        return;
    }

    m_result.success = false;
    m_result.canceled = canceled;
    m_result.errorCode = code;
    m_result.errorMessage = message;
    m_result.error.category = category;
    m_result.error.code = code;
    m_result.error.message = message;
    m_result.response.requestId = m_requestId;
    m_result.response.success = false;
    m_result.response.canceled = canceled;
    m_result.response.errorMessage = message;
    m_result.response.error = m_result.error;
    m_finished = true;

    transitionTo(canceled ? RuntimeRequestState::Canceled : RuntimeRequestState::Failed);
    emit finished(m_result);
}

} // namespace qtllm::host
