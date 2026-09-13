#include "contextwindowservice.h"

#include <QJsonDocument>
#include <QtGlobal>
#include <utility>

namespace qtllm::context {

namespace {

int estimatedTextTokens(const QString &text)
{
    const int byteCount = text.toUtf8().size();
    return byteCount == 0 ? 0 : (byteCount + 2) / 3;
}

QVector<LlmMessage> selectedMessages(const QVector<LlmMessage> &messages,
                                     const QVector<bool> &selected)
{
    QVector<LlmMessage> result;
    result.reserve(messages.size());
    for (int i = 0; i < messages.size(); ++i) {
        if (selected.at(i)) {
            result.append(messages.at(i));
        }
    }
    return result;
}

void selectIndices(QVector<bool> *selected, const QVector<int> &indices)
{
    for (int index : indices) {
        (*selected)[index] = true;
    }
}

bool fitsMessageLimit(const QVector<LlmMessage> &messages,
                      const ContextWindowPolicy &policy)
{
    return policy.maxMessages <= 0 || messages.size() <= policy.maxMessages;
}

} // namespace

TokenCountResult EstimatedMessageTokenCounter::countMessages(
    const QVector<LlmMessage> &messages, const QString &model) const
{
    Q_UNUSED(model)

    int tokens = 0;
    for (const LlmMessage &message : messages) {
        tokens += 4;
        tokens += estimatedTextTokens(message.role);
        tokens += estimatedTextTokens(message.content);
        tokens += estimatedTextTokens(message.name);
        tokens += estimatedTextTokens(message.toolCallId);
        for (const LlmToolCall &toolCall : message.toolCalls) {
            tokens += 4;
            tokens += estimatedTextTokens(toolCall.id);
            tokens += estimatedTextTokens(toolCall.name);
            tokens += estimatedTextTokens(toolCall.type);
            tokens += (QJsonDocument(toolCall.arguments).toJson(QJsonDocument::Compact).size() + 2) / 3;
        }
    }

    TokenCountResult result;
    result.tokenCount = tokens;
    result.accuracy = TokenCountAccuracy::Estimated;
    return result;
}

ContextWindowService::ContextWindowService(
    std::shared_ptr<IMessageTokenCounter> tokenCounter)
{
    setTokenCounter(std::move(tokenCounter));
}

void ContextWindowService::setTokenCounter(
    std::shared_ptr<IMessageTokenCounter> tokenCounter)
{
    m_tokenCounter = tokenCounter
        ? std::move(tokenCounter)
        : std::make_shared<EstimatedMessageTokenCounter>();
}

std::shared_ptr<IMessageTokenCounter> ContextWindowService::tokenCounter() const
{
    return m_tokenCounter;
}

ContextWindowResult ContextWindowService::select(
    const QVector<LlmMessage> &messages,
    const ContextWindowPolicy &policy,
    const QString &model) const
{
    ContextWindowResult result;
    result.sourceMessageCount = messages.size();

    if (policy.contextWindowTokens < 0
        || policy.reservedOutputTokens < 0
        || policy.maxMessages < 0
        || policy.minimumRecentTurns < 1
        || (policy.contextWindowTokens > 0
            && policy.reservedOutputTokens >= policy.contextWindowTokens)) {
        result.status = ContextWindowStatus::InvalidBudget;
        result.errorCode = QStringLiteral("context_invalid_budget");
        result.errorMessage = QStringLiteral("Context window policy contains an invalid budget");
        return result;
    }

    result.availableInputTokens = policy.contextWindowTokens > 0
        ? policy.contextWindowTokens - policy.reservedOutputTokens
        : 0;

    QVector<int> systemIndices;
    QVector<QVector<int>> turns;
    for (int i = 0; i < messages.size(); ++i) {
        const QString role = messages.at(i).role.trimmed().toLower();
        if (role == QStringLiteral("system")) {
            systemIndices.append(i);
            continue;
        }
        if (role == QStringLiteral("user") || turns.isEmpty()) {
            turns.append(QVector<int>());
        }
        turns.last().append(i);
    }

    QVector<bool> selected(messages.size(), false);
    selectIndices(&selected, systemIndices);
    const int requiredTurnStart = qMax(0, turns.size() - policy.minimumRecentTurns);
    for (int i = requiredTurnStart; i < turns.size(); ++i) {
        selectIndices(&selected, turns.at(i));
    }

    QVector<LlmMessage> candidate = selectedMessages(messages, selected);
    TokenCountResult count = m_tokenCounter->countMessages(candidate, model);
    count.tokenCount = qMax(0, count.tokenCount);
    const bool requiredFitsTokens = policy.contextWindowTokens <= 0
        || count.tokenCount <= result.availableInputTokens;
    if (!fitsMessageLimit(candidate, policy) || !requiredFitsTokens) {
        result.status = ContextWindowStatus::RequiredContentTooLarge;
        result.inputTokenCount = count.tokenCount;
        result.tokenCountAccuracy = count.accuracy;
        result.errorCode = QStringLiteral("context_required_content_exceeds_budget");
        result.errorMessage = QStringLiteral("Required system and recent turn content exceeds the context budget");
        return result;
    }

    for (int turnIndex = requiredTurnStart - 1; turnIndex >= 0; --turnIndex) {
        QVector<bool> expanded = selected;
        selectIndices(&expanded, turns.at(turnIndex));
        QVector<LlmMessage> expandedMessages = selectedMessages(messages, expanded);
        if (!fitsMessageLimit(expandedMessages, policy)) {
            break;
        }

        TokenCountResult expandedCount =
            m_tokenCounter->countMessages(expandedMessages, model);
        expandedCount.tokenCount = qMax(0, expandedCount.tokenCount);
        if (policy.contextWindowTokens > 0
            && expandedCount.tokenCount > result.availableInputTokens) {
            break;
        }

        selected = std::move(expanded);
        candidate = std::move(expandedMessages);
        count = expandedCount;
    }

    result.messages = candidate;
    result.droppedMessageCount = messages.size() - candidate.size();
    result.inputTokenCount = count.tokenCount;
    result.tokenCountAccuracy = count.accuracy;
    return result;
}

} // namespace qtllm::context
