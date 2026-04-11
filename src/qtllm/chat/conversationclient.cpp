#include "conversationclient.h"

#include "../core/qtllmclient.h"
#include "../identity/compactid.h"
#include "../providers/illmprovider.h"
#include "../tools/runtime/toolcallorchestrator.h"
#include "../toolsinside/toolsinsideruntime.h"
#include "../toolsinside/toolsinsidetracerecorder.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtGlobal>

namespace qtllm::chat {

namespace {

QString defaultSessionTitle(int index)
{
    return QStringLiteral("Session %1").arg(index);
}

QJsonObject toJson(const LlmToolCall &toolCall)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), toolCall.id);
    obj.insert(QStringLiteral("name"), toolCall.name);
    obj.insert(QStringLiteral("arguments"), toolCall.arguments);
    obj.insert(QStringLiteral("type"), toolCall.type);
    return obj;
}

QJsonObject toJson(const LlmMessage &message)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("role"), message.role);
    obj.insert(QStringLiteral("content"), message.content);
    if (!message.name.isEmpty()) {
        obj.insert(QStringLiteral("name"), message.name);
    }
    if (!message.toolCallId.isEmpty()) {
        obj.insert(QStringLiteral("tool_call_id"), message.toolCallId);
    }
    if (!message.toolCalls.isEmpty()) {
        QJsonArray toolCalls;
        for (const LlmToolCall &toolCall : message.toolCalls) {
            toolCalls.append(toJson(toolCall));
        }
        obj.insert(QStringLiteral("tool_calls"), toolCalls);
    }
    return obj;
}

QString requestToJsonText(const LlmRequest &request)
{
    QJsonObject root;
    root.insert(QStringLiteral("model"), request.model);
    root.insert(QStringLiteral("stream"), request.stream);
    root.insert(QStringLiteral("tools"), request.tools);

    QJsonArray messages;
    for (const LlmMessage &message : request.messages) {
        messages.append(toJson(message));
    }
    root.insert(QStringLiteral("messages"), messages);

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

} // namespace

ConversationClient::ConversationClient(QString uid, QObject *parent)
    : QObject(parent)
    , m_uid(std::move(uid))
    , m_llmClient(new QtLLMClient(this))
{
    ensureAtLeastOneSession();

    connect(m_llmClient, &QtLLMClient::tokenReceived, this, [this](const QString &token) {
        m_pendingAssistantText += token;
        emit tokenReceived(token);
    });

    connect(m_llmClient, &QtLLMClient::reasoningTokenReceived, this, [this](const QString &token) {
        emit reasoningTokenReceived(token);
    });

    connect(m_llmClient, &QtLLMClient::responseReceived, this, [this](const LlmResponse &response) {
        LlmResponse normalized = response;

        QString assistantText = normalized.assistantMessage.content;
        if (assistantText.isEmpty()) {
            assistantText = m_pendingAssistantText.isEmpty() ? normalized.text : m_pendingAssistantText;
            normalized.assistantMessage.role = QStringLiteral("assistant");
            normalized.assistantMessage.content = assistantText;
            normalized.text = assistantText;
        }

        m_pendingAssistantText.clear();

        if (!assistantText.isEmpty()) {
            appendMessage(QStringLiteral("assistant"), assistantText);
        }

        emit responseReceived(normalized);
        emit completed(assistantText);
    });

    connect(m_llmClient, &QtLLMClient::errorOccurred, this, [this](const QString &message) {
        m_pendingAssistantText.clear();
        emit errorOccurred(message);
    });

    connect(m_llmClient, &QtLLMClient::providerPayloadPrepared,
            this, &ConversationClient::providerPayloadPrepared);
}

QString ConversationClient::uid() const
{
    return m_uid;
}

void ConversationClient::setConfig(const LlmConfig &config)
{
    m_config = config;
    m_llmClient->setConfig(config);
    emit configChanged();
}

LlmConfig ConversationClient::config() const
{
    return m_config;
}

void ConversationClient::setProvider(std::unique_ptr<ILLMProvider> provider)
{
    m_llmClient->setProvider(std::move(provider));
}

bool ConversationClient::setProviderByName(const QString &providerName)
{
    return m_llmClient->setProviderByName(providerName);
}

void ConversationClient::setToolCallOrchestrator(
    const std::shared_ptr<tools::runtime::ToolCallOrchestrator> &orchestrator)
{
    m_llmClient->setToolCallOrchestrator(orchestrator);
}

void ConversationClient::setProfile(const profile::ClientProfile &profile)
{
    m_profile = profile;
    emit profileChanged();
}

profile::ClientProfile ConversationClient::profile() const
{
    return m_profile;
}

QString ConversationClient::activeSessionId() const
{
    return m_activeSessionId;
}

QString ConversationClient::createSession(const QString &title)
{
    ConversationSessionSnapshot session;
    session.sessionId = identity::generateId(identity::IdKind::Session);
    session.title = title.trimmed().isEmpty() ? defaultSessionTitle(m_sessions.size() + 1) : title.trimmed();
    session.createdAt = QDateTime::currentDateTimeUtc();
    session.updatedAt = session.createdAt;
    m_sessions.append(session);

    m_activeSessionId = session.sessionId;

    emit sessionsChanged();
    emit activeSessionChanged(m_activeSessionId);
    emit historyChanged();

    return session.sessionId;
}

bool ConversationClient::switchSession(const QString &sessionId)
{
    const QString id = sessionId.trimmed();
    if (id.isEmpty()) {
        return false;
    }

    if (m_activeSessionId == id) {
        return true;
    }

    if (findSessionIndex(id) < 0) {
        return false;
    }

    m_activeSessionId = id;
    emit activeSessionChanged(m_activeSessionId);
    emit historyChanged();
    return true;
}

QStringList ConversationClient::sessionIds() const
{
    QStringList ids;
    ids.reserve(m_sessions.size());
    for (const ConversationSessionSnapshot &session : m_sessions) {
        ids.append(session.sessionId);
    }
    return ids;
}

QString ConversationClient::sessionTitle(const QString &sessionId) const
{
    const int index = findSessionIndex(sessionId);
    if (index < 0) {
        return QString();
    }
    return m_sessions.at(index).title;
}

void ConversationClient::setHistory(const QVector<LlmMessage> &history)
{
    ConversationSessionSnapshot *session = activeSession();
    if (!session) {
        return;
    }

    session->history = history;
    session->updatedAt = QDateTime::currentDateTimeUtc();
    emit historyChanged();
    emit sessionsChanged();
}

QVector<LlmMessage> ConversationClient::history() const
{
    const ConversationSessionSnapshot *session = activeSession();
    return session ? session->history : QVector<LlmMessage>();
}

void ConversationClient::clearHistory()
{
    ConversationSessionSnapshot *session = activeSession();
    if (!session) {
        return;
    }

    session->history.clear();
    session->updatedAt = QDateTime::currentDateTimeUtc();
    emit historyChanged();
    emit sessionsChanged();
}

void ConversationClient::sendUserMessage(const QString &content)
{
    sendUserMessageWithTools(content, QJsonArray());
}

void ConversationClient::sendUserMessageWithTools(const QString &content,
                                           const QJsonArray &tools,
                                           const QString &traceId)
{
    const QString trimmed = content.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    const QString resolvedTraceId = traceId.trimmed().isEmpty()
        ? identity::generateId(identity::IdKind::Trace)
        : traceId.trimmed();

    appendMessage(QStringLiteral("user"), trimmed);
    m_pendingAssistantText.clear();
    m_llmClient->setToolLoopContext(m_uid, m_activeSessionId, resolvedTraceId);
    const LlmRequest request = buildRequestForNextTurn(tools);
    const QString requestJson = requestToJsonText(request);
    emit requestPrepared(requestJson);
    toolsinside::ToolsInsideRuntime::instance().recorder()->recordRequestPrepared(m_uid,
                                                                                  m_activeSessionId,
                                                                                  resolvedTraceId,
                                                                                  requestJson);
    m_llmClient->sendRequest(request);
}

ConversationSnapshot ConversationClient::snapshot() const
{
    ConversationSnapshot state;
    state.uid = m_uid;
    state.config = m_config;
    state.profile = m_profile;
    state.activeSessionId = m_activeSessionId;
    state.sessions = m_sessions;
    return state;
}

void ConversationClient::restoreFromSnapshot(const ConversationSnapshot &snapshot)
{
    m_uid = snapshot.uid;
    setConfig(snapshot.config);
    setProfile(snapshot.profile);
    m_sessions = snapshot.sessions;
    m_activeSessionId = snapshot.activeSessionId;
    ensureAtLeastOneSession();

    emit sessionsChanged();
    emit activeSessionChanged(m_activeSessionId);
    emit historyChanged();
}

LlmRequest ConversationClient::buildRequestForNextTurn(const QJsonArray &tools) const
{
    LlmRequest request;
    request.model = m_config.model;
    request.stream = m_config.stream;

    if (!m_profile.systemPrompt.trimmed().isEmpty()) {
        LlmMessage system;
        system.role = QStringLiteral("system");
        system.content = m_profile.systemPrompt.trimmed();
        request.messages.append(system);
    }

    if (!m_profile.persona.trimmed().isEmpty()) {
        LlmMessage persona;
        persona.role = QStringLiteral("system");
        persona.content = QStringLiteral("Persona: ") + m_profile.persona.trimmed();
        request.messages.append(persona);
    }

    if (!m_profile.thinkingStyle.trimmed().isEmpty()) {
        LlmMessage thinking;
        thinking.role = QStringLiteral("system");
        thinking.content = QStringLiteral("Thinking style: ") + m_profile.thinkingStyle.trimmed();
        request.messages.append(thinking);
    }

    request.tools = tools;

    const QVector<LlmMessage> messages = history();
    const int maxMessages = qMax(1, m_profile.memoryPolicy.maxHistoryMessages);
    const int startIndex = qMax(0, messages.size() - maxMessages);
    for (int i = startIndex; i < messages.size(); ++i) {
        request.messages.append(messages.at(i));
    }

    return request;
}

void ConversationClient::appendMessage(const QString &role, const QString &content)
{
    ConversationSessionSnapshot *session = activeSession();
    if (!session) {
        return;
    }

    LlmMessage message;
    message.role = role;
    message.content = content;
    session->history.append(message);
    session->updatedAt = QDateTime::currentDateTimeUtc();

    emit historyChanged();
    emit sessionsChanged();
}

int ConversationClient::findSessionIndex(const QString &sessionId) const
{
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions.at(i).sessionId == sessionId) {
            return i;
        }
    }
    return -1;
}

ConversationSessionSnapshot *ConversationClient::activeSession()
{
    const int index = findSessionIndex(m_activeSessionId);
    if (index < 0) {
        return nullptr;
    }
    return &m_sessions[index];
}

const ConversationSessionSnapshot *ConversationClient::activeSession() const
{
    const int index = findSessionIndex(m_activeSessionId);
    if (index < 0) {
        return nullptr;
    }
    return &m_sessions.at(index);
}

void ConversationClient::ensureAtLeastOneSession()
{
    if (!m_sessions.isEmpty()) {
        if (findSessionIndex(m_activeSessionId) < 0) {
            m_activeSessionId = m_sessions.first().sessionId;
        }
        return;
    }

    ConversationSessionSnapshot session;
    session.sessionId = identity::generateId(identity::IdKind::Session);
    session.title = defaultSessionTitle(1);
    session.createdAt = QDateTime::currentDateTimeUtc();
    session.updatedAt = session.createdAt;
    m_sessions.append(session);
    m_activeSessionId = session.sessionId;
}

} // namespace qtllm::chat



