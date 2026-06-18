#include "chunktranslateskill.h"

#include "../core/modelrouter.h"

#include "../../../../qtllm/core/llmtypes.h"
#include "../../../../qtllm/core/qtllmclient.h"

#include <QEventLoop>
#include <QTimer>

namespace pdftranslator::skills {

namespace {

const QString kSkillId = QStringLiteral("chunk-translate");

struct LlmCallResult
{
    bool success = false;
    QString text;
    QString errorMessage;
};

LlmCallResult callTranslationLlm(const ModelEndpointConfig &endpoint,
                                 const QString &sourceText,
                                 const QString &sourceLanguage,
                                 const QString &targetLanguage,
                                 const QString &glossaryText,
                                 const QString &extraInstructions,
                                 const QString &previousTranslation,
                                 const QString &userFeedback)
{
    qtllm::QtLLMClient client;
    client.setConfig(endpoint.llmConfig);
    if (!client.setProviderByName(endpoint.llmConfig.providerName)) {
        return {false, QString(), QStringLiteral("Failed to resolve provider for chunk-translate endpoint")};
    }

    qtllm::LlmRequest request;
    request.model = endpoint.llmConfig.model;
    request.stream = false;
    request.messages.append({
        QStringLiteral("system"),
        QStringLiteral("You are a document translation skill. Translate faithfully without adding commentary. "
                       "Preserve paragraph boundaries where possible. Return only the translated text.")
    });
    request.messages.append({
        QStringLiteral("user"),
        QStringLiteral("Translate the following text from %1 to %2.%4%5%6\n\n%3")
            .arg(sourceLanguage.isEmpty() ? QStringLiteral("auto-detected language") : sourceLanguage,
                 targetLanguage.isEmpty() ? QStringLiteral("target language") : targetLanguage,
                 sourceText,
                 glossaryText.isEmpty() ? QString() : QStringLiteral("\nUse this terminology guidance when applicable:\n%1").arg(glossaryText),
                 extraInstructions.isEmpty() ? QString() : QStringLiteral("\nAdditional translation instructions:\n%1").arg(extraInstructions),
                 previousTranslation.isEmpty()
                     ? QString()
                     : QStringLiteral("\nPrevious translation draft:\n%1\n%2")
                           .arg(previousTranslation,
                                userFeedback.isEmpty()
                                    ? QStringLiteral("Revise it if needed, but return only the improved translation.")
                                    : QStringLiteral("User feedback for revision:\n%1").arg(userFeedback)))
    });

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    LlmCallResult result;

    QObject::connect(&client, &qtllm::QtLLMClient::responseReceived, &loop, [&](const qtllm::LlmResponse &response) {
        result.success = response.success;
        result.text = response.assistantMessage.content.isEmpty() ? response.text : response.assistantMessage.content;
        result.errorMessage = response.errorMessage;
        loop.quit();
    });
    QObject::connect(&client, &qtllm::QtLLMClient::errorOccurred, &loop, [&](const QString &message) {
        result.errorMessage = message;
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        result.errorMessage = QStringLiteral("chunk-translate timed out");
        client.cancelCurrentRequest();
        loop.quit();
    });

    timeout.start(endpoint.llmConfig.timeoutMs > 0 ? endpoint.llmConfig.timeoutMs : 60000);
    client.sendRequest(request);
    loop.exec();
    return result;
}

} // namespace

ChunkTranslateSkill::ChunkTranslateSkill(std::shared_ptr<ModelRouter> modelRouter)
    : m_modelRouter(std::move(modelRouter))
{
}

SkillDescriptor ChunkTranslateSkill::descriptor() const
{
    SkillDescriptor descriptor;
    descriptor.id = kSkillId;
    descriptor.displayName = QStringLiteral("Chunk Translate");
    descriptor.description = QStringLiteral("Translates extracted text using a dedicated translation model binding.");
    descriptor.category = QStringLiteral("translation");
    descriptor.executionType = SkillExecutionType::Llm;
    return descriptor;
}

bool ChunkTranslateSkill::canHandle(const SkillContext &context) const
{
    return !context.sourceText.trimmed().isEmpty();
}

SkillResult ChunkTranslateSkill::execute(const SkillContext &context)
{
    SkillResult result;
    if (!m_modelRouter) {
        result.status = QStringLiteral("router_unavailable");
        result.errorMessage = QStringLiteral("Model router is not available");
        return result;
    }

    const std::optional<ResolvedSkillRoute> route = m_modelRouter->resolve(kSkillId);
    if (!route.has_value() || !route->endpoint.has_value()) {
        result.status = QStringLiteral("binding_missing");
        result.errorMessage = QStringLiteral("No endpoint binding configured for chunk-translate");
        return result;
    }

    const LlmCallResult llmResult = callTranslationLlm(*route->endpoint,
                                                       context.sourceText,
                                                       context.sourceLanguageHint,
                                                       context.targetLanguageHint,
                                                       context.extra.value(QStringLiteral("glossaryText")).toString(),
                                                       context.extra.value(QStringLiteral("translationInstructions")).toString(),
                                                       context.extra.value(QStringLiteral("previousTranslation")).toString(),
                                                       context.extra.value(QStringLiteral("userFeedback")).toString());
    if (!llmResult.success || llmResult.text.trimmed().isEmpty()) {
        result.status = QStringLiteral("translation_failed");
        result.errorMessage = llmResult.errorMessage.isEmpty()
            ? QStringLiteral("chunk-translate failed")
            : llmResult.errorMessage;
        result.retryable = true;
        return result;
    }

    result.success = true;
    result.status = QStringLiteral("completed");
    result.output.insert(QStringLiteral("translatedText"), llmResult.text.trimmed());
    result.output.insert(QStringLiteral("sourceLanguage"), context.sourceLanguageHint);
    result.output.insert(QStringLiteral("targetLanguage"), context.targetLanguageHint);
    return result;
}

} // namespace pdftranslator::skills
