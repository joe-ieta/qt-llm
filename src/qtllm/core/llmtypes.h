#pragma once

#include "../structuredoutput/structuredoutputtypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QVector>

namespace qtllm {

struct LlmToolCall
{
    QString id;
    QString name;
    QJsonObject arguments;
    QString type = QStringLiteral("function");
};

struct LlmMessage
{
    QString role;
    QString content;

    // Thinking/reasoning channel text (DeepSeek reasoning_content and similar).
    // Providers that use a thinking mode require it to be passed back on
    // assistant tool-call turns.
    QString reasoningContent;

    // Optional fields for tool-calling protocol.
    QString name;
    QString toolCallId;
    QVector<LlmToolCall> toolCalls;
};

// Provider-reported token usage for a completed request. Missing usage means
// unknown, so `available` is false and the token counts must not be
// interpreted as zero. The counts are only meaningful when `available` is
// true. `available` stays last to preserve positional aggregate initialization
// for existing callers.
struct LlmUsage
{
    int inputTokens = 0;
    int outputTokens = 0;
    int totalTokens = 0;
    int reasoningTokens = 0;
    int cachedInputTokens = 0;
    bool available = false;
};

struct LlmRequest
{
    QVector<LlmMessage> messages;
    QString model;
    bool stream = true;

    // OpenAI-compatible tools schema array.
    QJsonArray tools;

    // Optional caller-supplied logical request ID.
    QString requestId;

    // Optional structured-output contract. Text preserves legacy behavior.
    OutputConstraint output;
};

enum class LlmErrorCategory
{
    None,
    Configuration,
    Runtime,
    Transport,
    Protocol,
    Tool,
    Canceled,
    Parsing,
    Schema
};

struct LlmError
{
    LlmErrorCategory category = LlmErrorCategory::None;
    QString code;
    QString message;
    QString diagnostic;
    bool retryable = false;
    int httpStatus = 0;
    int attempt = 0;
};

struct LlmResponse
{
    QString text;
    bool success = false;
    QString errorMessage;

    // Structured assistant response fields.
    LlmMessage assistantMessage;
    QString finishReason;

    // Logical request lifecycle fields. Existing fields remain source-compatible.
    QString requestId;
    bool canceled = false;
    LlmError error;
    StructuredOutputResult structuredOutput;

    // Token usage reported by the provider. Check usage.available before
    // reading the token counts.
    LlmUsage usage;
};

struct LlmStreamDelta
{
    QString channel = QStringLiteral("content");
    QString text;
};

} // namespace qtllm

Q_DECLARE_METATYPE(qtllm::LlmError)
Q_DECLARE_METATYPE(qtllm::LlmResponse)
