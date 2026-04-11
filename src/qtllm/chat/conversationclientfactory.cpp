#include "conversationclientfactory.h"

#include "../identity/compactid.h"
#include "../storage/conversationrepository.h"

namespace qtllm::chat {

ConversationClientFactory::ConversationClientFactory(QObject *parent)
    : QObject(parent)
{
}

void ConversationClientFactory::setRepository(std::shared_ptr<storage::ConversationRepository> repository)
{
    m_repository = std::move(repository);
}

QSharedPointer<ConversationClient> ConversationClientFactory::acquire(const QString &uid)
{
    const QString resolvedUid = uid.trimmed().isEmpty() ? generateUid() : uid.trimmed();

    if (m_clients.contains(resolvedUid)) {
        return m_clients.value(resolvedUid);
    }

    QSharedPointer<ConversationClient> client = createClient(resolvedUid);

    if (m_repository) {
        const std::optional<ConversationSnapshot> restored = m_repository->loadSnapshot(resolvedUid);
        if (restored.has_value()) {
            client->restoreFromSnapshot(*restored);
        }
    }

    m_clients.insert(resolvedUid, client);

    if (m_repository) {
        auto saveTrigger = [this, resolvedUid]() {
            saveClient(resolvedUid, nullptr);
        };

        connect(client.get(), &ConversationClient::historyChanged, this, saveTrigger);
        connect(client.get(), &ConversationClient::sessionsChanged, this, saveTrigger);
        connect(client.get(), &ConversationClient::activeSessionChanged, this, saveTrigger);
        connect(client.get(), &ConversationClient::configChanged, this, saveTrigger);
        connect(client.get(), &ConversationClient::profileChanged, this, saveTrigger);
    }

    return client;
}

QSharedPointer<ConversationClient> ConversationClientFactory::find(const QString &uid) const
{
    return m_clients.value(uid.trimmed());
}

QStringList ConversationClientFactory::clientIds() const
{
    return m_clients.keys();
}

QStringList ConversationClientFactory::persistedClientIds() const
{
    if (!m_repository) {
        return {};
    }
    return m_repository->listClientIds();
}

bool ConversationClientFactory::saveClient(const QString &uid, QString *errorMessage) const
{
    if (!m_repository) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ConversationRepository is not configured");
        }
        return false;
    }

    const QSharedPointer<ConversationClient> client = m_clients.value(uid.trimmed());
    if (!client) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unknown client uid: ") + uid;
        }
        return false;
    }

    return m_repository->saveSnapshot(client->snapshot(), errorMessage);
}

bool ConversationClientFactory::saveAll(QString *errorMessage) const
{
    if (!m_repository) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ConversationRepository is not configured");
        }
        return false;
    }

    for (auto it = m_clients.constBegin(); it != m_clients.constEnd(); ++it) {
        QString localError;
        if (!m_repository->saveSnapshot(it.value()->snapshot(), &localError)) {
            if (errorMessage) {
                *errorMessage = localError;
            }
            return false;
        }
    }

    return true;
}

QSharedPointer<ConversationClient> ConversationClientFactory::createClient(const QString &uid) const
{
    return QSharedPointer<ConversationClient>::create(uid);
}

QString ConversationClientFactory::generateUid() const
{
    return identity::generateId(identity::IdKind::Client);
}

} // namespace qtllm::chat
