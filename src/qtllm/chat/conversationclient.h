#pragma once

#include "conversationsnapshot.h"
#include "../context/contextwindowservice.h"

#include <QJsonArray>
#include <QObject>
#include <QVector>
#include <memory>

namespace qtllm {

class ILLMProvider;
class QtLLMClient;

namespace tools::runtime {
class ToolCallOrchestrator;
}

namespace chat {

class ConversationClient : public QObject
{
    Q_OBJECT
public:
    explicit ConversationClient(QString uid, QObject *parent = nullptr);

    QString uid() const;

    void setConfig(const LlmConfig &config);
    LlmConfig config() const;

    void setProvider(std::unique_ptr<ILLMProvider> provider);
    bool setProviderByName(const QString &providerName);

    void setToolCallOrchestrator(const std::shared_ptr<tools::runtime::ToolCallOrchestrator> &orchestrator);

    void setProfile(const profile::ClientProfile &profile);
    profile::ClientProfile profile() const;

    QString activeSessionId() const;
    QString createSession(const QString &title = QString());
    bool switchSession(const QString &sessionId);
    QStringList sessionIds() const;
    QString sessionTitle(const QString &sessionId) const;

    void setHistory(const QVector<LlmMessage> &history);
    QVector<LlmMessage> history() const;
    void clearHistory();

    void setMessageTokenCounter(
        std::shared_ptr<context::IMessageTokenCounter> tokenCounter);
    context::ContextWindowResult lastContextWindowResult() const;

    void sendUserMessage(const QString &content);
    void sendUserMessageWithTools(const QString &content,
                                  const QJsonArray &tools,
                                  const QString &traceId = QString());

    ConversationSnapshot snapshot() const;
    void restoreFromSnapshot(const ConversationSnapshot &snapshot);

signals:
    void tokenReceived(const QString &token);
    void reasoningTokenReceived(const QString &token);
    void completed(const QString &text);
    void responseReceived(const LlmResponse &response);
    void errorOccurred(const QString &message);
    void requestPrepared(const QString &payloadJson);
    void providerPayloadPrepared(const QString &url, const QString &payloadJson);
    void historyChanged();
    void sessionsChanged();
    void activeSessionChanged(const QString &sessionId);
    void configChanged();
    void profileChanged();
    void contextWindowEvaluated(const qtllm::context::ContextWindowResult &result);

private:
    context::ContextWindowResult buildRequestForNextTurn(
        const QJsonArray &tools, LlmRequest *request) const;
    void appendMessage(const QString &role, const QString &content);
    int findSessionIndex(const QString &sessionId) const;
    ConversationSessionSnapshot *activeSession();
    const ConversationSessionSnapshot *activeSession() const;
    void ensureAtLeastOneSession();

private:
    QString m_uid;
    LlmConfig m_config;
    profile::ClientProfile m_profile;
    QVector<ConversationSessionSnapshot> m_sessions;
    QString m_activeSessionId;
    QString m_pendingAssistantText;
    context::ContextWindowService m_contextWindowService;
    context::ContextWindowResult m_lastContextWindowResult;
    QtLLMClient *m_llmClient;
};

} // namespace chat
} // namespace qtllm

