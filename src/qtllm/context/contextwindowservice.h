#pragma once

#include "../core/llmtypes.h"

#include <QMetaType>
#include <QString>
#include <QVector>
#include <memory>

namespace qtllm::context {

enum class TokenCountAccuracy
{
    Estimated,
    Exact
};

struct TokenCountResult
{
    int tokenCount = 0;
    TokenCountAccuracy accuracy = TokenCountAccuracy::Estimated;
};

class IMessageTokenCounter
{
public:
    virtual ~IMessageTokenCounter() = default;

    virtual TokenCountResult countMessages(const QVector<LlmMessage> &messages,
                                           const QString &model) const = 0;
};

class EstimatedMessageTokenCounter final : public IMessageTokenCounter
{
public:
    TokenCountResult countMessages(const QVector<LlmMessage> &messages,
                                   const QString &model) const override;
};

struct ContextWindowPolicy
{
    // Zero means no token-based input limit.
    int contextWindowTokens = 0;
    int reservedOutputTokens = 0;

    // Zero means no message-count limit. The limit includes system messages.
    int maxMessages = 0;

    // Recent user-initiated turns are always retained as indivisible groups.
    int minimumRecentTurns = 1;
};

enum class ContextWindowStatus
{
    Ready,
    InvalidBudget,
    RequiredContentTooLarge
};

struct ContextWindowResult
{
    ContextWindowStatus status = ContextWindowStatus::Ready;
    QVector<LlmMessage> messages;
    int sourceMessageCount = 0;
    int droppedMessageCount = 0;
    int inputTokenCount = 0;
    int availableInputTokens = 0;
    TokenCountAccuracy tokenCountAccuracy = TokenCountAccuracy::Estimated;
    QString errorCode;
    QString errorMessage;

    bool isSuccess() const
    {
        return status == ContextWindowStatus::Ready;
    }
};

class ContextWindowService
{
public:
    explicit ContextWindowService(
        std::shared_ptr<IMessageTokenCounter> tokenCounter = {});

    void setTokenCounter(std::shared_ptr<IMessageTokenCounter> tokenCounter);
    std::shared_ptr<IMessageTokenCounter> tokenCounter() const;

    ContextWindowResult select(const QVector<LlmMessage> &messages,
                               const ContextWindowPolicy &policy,
                               const QString &model = QString()) const;

private:
    std::shared_ptr<IMessageTokenCounter> m_tokenCounter;
};

} // namespace qtllm::context

Q_DECLARE_METATYPE(qtllm::context::TokenCountAccuracy)
Q_DECLARE_METATYPE(qtllm::context::ContextWindowStatus)
Q_DECLARE_METATYPE(qtllm::context::ContextWindowResult)
