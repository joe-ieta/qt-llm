#include "runtimefacade.h"

#include "runtimeprofilemapper.h"
#include "../core/qtllmclient.h"
#include "../identity/compactid.h"
#include "../runtime/managedllamacppruntime.h"
#include "../toolsinside/toolsinsideruntime.h"
#include "../toolsinside/toolsinsidetracerecorder.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace qtllm::host {

namespace {

QString resolvedId(const QString &value, identity::IdKind kind)
{
    return value.trimmed().isEmpty() ? identity::generateId(kind) : value.trimmed();
}

QJsonObject requestToJson(const LlmRequest &request)
{
    QJsonObject root;
    root.insert(QStringLiteral("model"), request.model);
    root.insert(QStringLiteral("stream"), request.stream);
    root.insert(QStringLiteral("messageCount"), request.messages.size());
    root.insert(QStringLiteral("toolCount"), request.tools.size());
    return root;
}

} // namespace

RuntimeFacade::RuntimeFacade(QObject *parent)
    : QObject(parent)
    , m_client(new QtLLMClient(this))
{
    qRegisterMetaType<qtllm::host::ChatResult>("qtllm::host::ChatResult");
    qRegisterMetaType<qtllm::host::LocalModelInfo>("qtllm::host::LocalModelInfo");

    connect(m_client, &QtLLMClient::tokenReceived, this, &RuntimeFacade::tokenReceived);
    connect(m_client, &QtLLMClient::reasoningTokenReceived, this, &RuntimeFacade::reasoningTokenReceived);
    connect(m_client, &QtLLMClient::providerPayloadPrepared, this, &RuntimeFacade::providerPayloadPrepared);

    connect(m_client, &QtLLMClient::completed, this, [this](const QString &text) {
        ChatResult result = baseResult(m_activeRequest);
        result.success = true;
        result.text = text;
        m_requestActive = false;
        emit runtimeStatusChanged(QStringLiteral("completed"), result.traceId);
        emit completed(result);
    });

    connect(m_client, &QtLLMClient::errorOccurred, this, [this](const QString &message) {
        ChatResult result = baseResult(m_activeRequest);
        result.success = false;
        result.errorCode = QStringLiteral("request_failed");
        result.errorMessage = message;
        m_requestActive = false;
        emit runtimeStatusChanged(QStringLiteral("error"), message);
        emit errorOccurred(result);
    });
}

RuntimeFacade::~RuntimeFacade() = default;

void RuntimeFacade::setProfile(const RuntimeProfile &profile)
{
    m_profile = profile;
    if (!m_profile.workspaceRoot.trimmed().isEmpty()) {
        toolsinside::ToolsInsideRuntime::instance().configureWorkspaceRoot(m_profile.workspaceRoot.trimmed());
    }
    QString message;
    refreshRuntimeAvailability(&message);
    emit runtimeStatusChanged(m_profile.providerAvailable
                                  ? QStringLiteral("provider_available")
                                  : QStringLiteral("provider_unavailable"),
                              message);
}

RuntimeProfile RuntimeFacade::profile() const
{
    return m_profile;
}

QList<LocalModelInfo> RuntimeFacade::listLocalModels(QString *errorMessage) const
{
    return m_modelCatalog.listLocalModels(m_profile, errorMessage);
}

bool RuntimeFacade::refreshRuntimeAvailability(QString *message)
{
    LlmConfig config = RuntimeProfileMapper::toConfig(m_profile);
    bool available = true;
    QString detail;
    if (runtime::ManagedLlamaCppRuntime::isManagedProvider(config.providerName)) {
        available = runtime::ManagedLlamaCppRuntime::updateRuntimeAvailability(&config, nullptr, &detail);
    } else {
        config.providerAvailable = true;
        config.providerAvailabilityStatus = QStringLiteral("unchecked");
        config.providerAvailabilityMessage = QStringLiteral("Provider availability is checked by remote request execution");
        detail = config.providerAvailabilityMessage;
    }

    m_profile = RuntimeProfileMapper::fromConfig(config);
    m_client->setConfig(config);
    if (message) {
        *message = detail;
    }
    return available;
}

void RuntimeFacade::send(const ChatRequest &request)
{
    if (m_requestActive) {
        finishWithError(request,
                        QStringLiteral("request_in_progress"),
                        QStringLiteral("RuntimeFacade already has an active request"));
        return;
    }
    if (request.userPrompt.trimmed().isEmpty()) {
        finishWithError(request,
                        QStringLiteral("empty_prompt"),
                        QStringLiteral("ChatRequest.userPrompt is empty"));
        return;
    }
    if (m_profile.providerName.trimmed().isEmpty()) {
        finishWithError(request,
                        QStringLiteral("missing_provider"),
                        QStringLiteral("RuntimeProfile.providerName is empty"));
        return;
    }

    m_activeRequest = request;
    m_requestActive = true;
    if (!configureForRequest(m_activeRequest)) {
        return;
    }

    const LlmRequest llmRequest = RuntimeProfileMapper::toRequest(m_profile, m_activeRequest);
    const QString requestJson = QString::fromUtf8(QJsonDocument(requestToJson(llmRequest)).toJson(QJsonDocument::Compact));

    toolsinside::ToolsInsideRuntime::instance().recorder()->startTrace(m_activeRequest.clientId,
                                                                        m_activeRequest.sessionId,
                                                                        m_activeRequest.traceId,
                                                                        m_activeRequest.userPrompt,
                                                                        m_profile.providerName,
                                                                        m_profile.model,
                                                                        m_profile.modelVendor);
    toolsinside::ToolsInsideRuntime::instance().recorder()->recordRequestPrepared(m_activeRequest.clientId,
                                                                                  m_activeRequest.sessionId,
                                                                                  m_activeRequest.traceId,
                                                                                  requestJson);

    emit runtimeStatusChanged(QStringLiteral("request_started"), m_activeRequest.traceId);
    m_client->sendRequest(llmRequest);
}

ChatResult RuntimeFacade::sendBlocking(const ChatRequest &request, int timeoutMs)
{
    ChatResult result;
    bool finished = false;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QMetaObject::Connection completedConnection;
    QMetaObject::Connection errorConnection;
    QMetaObject::Connection timeoutConnection;

    completedConnection = connect(this, &RuntimeFacade::completed, &loop, [&](const ChatResult &completedResult) {
        result = completedResult;
        finished = true;
        loop.quit();
    });
    errorConnection = connect(this, &RuntimeFacade::errorOccurred, &loop, [&](const ChatResult &errorResult) {
        result = errorResult;
        finished = true;
        loop.quit();
    });
    timeoutConnection = connect(&timer, &QTimer::timeout, &loop, [&]() {
        cancel();
        result = baseResult(m_activeRequest);
        result.success = false;
        result.errorCode = QStringLiteral("timeout");
        result.errorMessage = QStringLiteral("RuntimeFacade blocking request timed out");
        finished = true;
        loop.quit();
    });

    send(request);
    if (finished) {
        disconnect(completedConnection);
        disconnect(errorConnection);
        disconnect(timeoutConnection);
        return result;
    }

    if (timeoutMs > 0) {
        timer.start(timeoutMs);
    } else if (m_profile.timeoutMs > 0) {
        timer.start(m_profile.timeoutMs);
    }

    loop.exec();

    disconnect(completedConnection);
    disconnect(errorConnection);
    disconnect(timeoutConnection);
    return result;
}

void RuntimeFacade::cancel()
{
    if (!m_requestActive) {
        return;
    }
    emit runtimeStatusChanged(QStringLiteral("cancel_requested"), m_activeRequest.traceId);
    m_client->cancelCurrentRequest();
}

ChatResult RuntimeFacade::baseResult(const ChatRequest &request) const
{
    ChatResult result;
    result.providerName = m_profile.providerName;
    result.model = m_profile.model;
    result.clientId = request.clientId;
    result.sessionId = request.sessionId;
    result.traceId = request.traceId;
    result.metadata = request.metadata;
    return result;
}

bool RuntimeFacade::configureForRequest(const ChatRequest &request)
{
    m_activeRequest.clientId = resolvedId(request.clientId, identity::IdKind::Client);
    m_activeRequest.sessionId = resolvedId(request.sessionId, identity::IdKind::Session);
    m_activeRequest.traceId = resolvedId(request.traceId, identity::IdKind::Trace);

    m_client->setConfig(RuntimeProfileMapper::toConfig(m_profile));
    m_client->setToolLoopContext(m_activeRequest.clientId, m_activeRequest.sessionId, m_activeRequest.traceId);
    const QString providerName = m_profile.providerName.trimmed();
    if (!providerName.isEmpty() && providerName != m_configuredProviderName) {
        if (!m_client->setProviderByName(providerName)) {
            m_requestActive = false;
            return false;
        }
        m_configuredProviderName = providerName;
    }
    return true;
}

void RuntimeFacade::finishWithError(const ChatRequest &request, const QString &code, const QString &message)
{
    ChatResult result = baseResult(request);
    result.success = false;
    result.errorCode = code;
    result.errorMessage = message;
    emit runtimeStatusChanged(QStringLiteral("error"), message);
    emit errorOccurred(result);
}

} // namespace qtllm::host
