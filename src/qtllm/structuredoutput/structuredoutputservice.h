#pragma once

#include "structuredoutputtypes.h"
#include "../core/llmtypes.h"

#include <QJsonObject>
#include <QVector>

namespace qtllm {

class StructuredOutputService
{
public:
    static bool validateConstraint(const OutputConstraint &constraint,
                                   QString *errorCode = nullptr,
                                   QString *errorMessage = nullptr);
    static StructuredOutputResult validate(const QString &text,
                                           const OutputConstraint &constraint);

    static QVector<LlmMessage> constrainedMessages(const QVector<LlmMessage> &messages,
                                                   const OutputConstraint &constraint);

    static ModelCapabilitySnapshot capabilities(
        const QString &providerName,
        const QString &model,
        const QString &modelVendor = QString(),
        const ModelCapabilityOverrides &overrides = ModelCapabilityOverrides());

    static QJsonObject openAiResponsesText(const OutputConstraint &constraint);
    static QJsonObject openAiChatResponseFormat(const OutputConstraint &constraint);
    static QJsonObject googleGenerationConfig(const OutputConstraint &constraint);
};

} // namespace qtllm
