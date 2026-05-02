# 示例：基于 `ConversationClient` 的聊天窗口

## 适用场景

- 标准聊天窗口
- 需要 history
- 需要 session
- 需要持久化恢复

## 示例代码

```cpp
#include "src/qtllm/chat/conversationclientfactory.h"
#include "src/qtllm/storage/conversationrepository.h"

#include <QObject>
#include <QListWidget>
#include <QPlainTextEdit>

class ConversationChatController : public QObject
{
    Q_OBJECT
public:
    explicit ConversationChatController(QListWidget *sessionList,
                                        QPlainTextEdit *transcript,
                                        QObject *parent = nullptr)
        : QObject(parent)
        , m_sessionList(sessionList)
        , m_transcript(transcript)
        , m_repository(std::make_shared<qtllm::storage::ConversationRepository>(
              QStringLiteral(".qtllm/clients")))
        , m_factory(new qtllm::chat::ConversationClientFactory(this))
    {
        m_factory->setRepository(m_repository);
        m_client = m_factory->acquire(QStringLiteral("assistant-main"));

        qtllm::LlmConfig config;
        config.providerName = QStringLiteral("openai-compatible");
        config.baseUrl = QStringLiteral("http://127.0.0.1:11434");
        config.model = QStringLiteral("qwen2.5:7b");
        config.stream = true;

        m_client->setConfig(config);
        m_client->setProviderByName(config.providerName);

        connect(m_client.get(), &qtllm::chat::ConversationClient::tokenReceived,
                this, [this](const QString &token) {
            if (m_transcript) {
                m_transcript->insertPlainText(token);
            }
        });

        connect(m_client.get(), &qtllm::chat::ConversationClient::historyChanged,
                this, &ConversationChatController::refreshHistory);
        connect(m_client.get(), &qtllm::chat::ConversationClient::sessionsChanged,
                this, &ConversationChatController::refreshSessions);
        connect(m_client.get(), &qtllm::chat::ConversationClient::activeSessionChanged,
                this, &ConversationChatController::refreshSessions);

        refreshSessions();
        refreshHistory();
    }

    void createSession(const QString &title)
    {
        m_client->createSession(title);
    }

    void switchToSession(const QString &sessionId)
    {
        m_client->switchSession(sessionId);
    }

    void sendUserMessage(const QString &text)
    {
        if (!m_client || text.trimmed().isEmpty()) {
            return;
        }

        if (m_transcript) {
            m_transcript->appendPlainText(QStringLiteral("\\n[user] ") + text.trimmed());
        }
        m_client->sendUserMessage(text.trimmed());
    }

private:
    void refreshSessions()
    {
        if (!m_sessionList || !m_client) {
            return;
        }

        m_sessionList->clear();
        const QStringList ids = m_client->sessionIds();
        for (const QString &id : ids) {
            QListWidgetItem *item = new QListWidgetItem(m_client->sessionTitle(id), m_sessionList);
            item->setData(Qt::UserRole, id);
            if (id == m_client->activeSessionId()) {
                item->setSelected(true);
            }
        }
    }

    void refreshHistory()
    {
        if (!m_transcript || !m_client) {
            return;
        }

        m_transcript->clear();
        for (const qtllm::LlmMessage &message : m_client->history()) {
            m_transcript->appendPlainText(QStringLiteral("[%1] %2").arg(message.role, message.content));
        }
    }

private:
    QListWidget *m_sessionList = nullptr;
    QPlainTextEdit *m_transcript = nullptr;
    std::shared_ptr<qtllm::storage::ConversationRepository> m_repository;
    qtllm::chat::ConversationClientFactory *m_factory = nullptr;
    QSharedPointer<qtllm::chat::ConversationClient> m_client;
};
```

## 说明

- `ConversationClientFactory` 负责复用和恢复 client
- `ConversationRepository` 负责会话快照持久化
- `ConversationClient` 负责 history、session 和消息发送

## 推荐配套阅读

- [../core/conversationclient.md](../core/conversationclient.md)
- [../core/conversationclientfactory-and-storage.md](../core/conversationclientfactory-and-storage.md)
