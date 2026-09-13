#pragma once

#include "conversationclient.h"
#include "../qtllm_global.h"

#include <QHash>
#include <QObject>
#include <QSharedPointer>

#include <memory>

namespace qtllm::storage {
class ConversationRepository;
}

namespace qtllm::chat {

class QTLLM_CONVERSATION_EXPORT ConversationClientFactory : public QObject
{
    Q_OBJECT
public:
    explicit ConversationClientFactory(QObject *parent = nullptr);

    void setRepository(std::shared_ptr<storage::ConversationRepository> repository);

    QSharedPointer<ConversationClient> acquire(const QString &uid = QString());
    QSharedPointer<ConversationClient> find(const QString &uid) const;

    QStringList clientIds() const;
    QStringList persistedClientIds() const;

    bool saveClient(const QString &uid, QString *errorMessage = nullptr) const;
    bool saveAll(QString *errorMessage = nullptr) const;

private:
    QSharedPointer<ConversationClient> createClient(const QString &uid) const;
    QString generateUid() const;

private:
    QHash<QString, QSharedPointer<ConversationClient>> m_clients;
    std::shared_ptr<storage::ConversationRepository> m_repository;
};

} // namespace qtllm::chat
