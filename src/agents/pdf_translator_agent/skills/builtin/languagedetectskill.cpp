#include "languagedetectskill.h"

#include "../core/modelrouter.h"

#include "../../../../qtllm/core/llmtypes.h"
#include "../../../../qtllm/core/qtllmclient.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStringList>
#include <QTimer>

namespace pdftranslator::skills {

namespace {

const QString kSkillId = QStringLiteral("language-detect");

QString recommendedTargetForSource(const QString &sourceLanguage)
{
    if (sourceLanguage == QStringLiteral("zh-CN")) {
        return QStringLiteral("en");
    }
    return QStringLiteral("zh-CN");
}

QString extractJsonObject(const QString &text)
{
    int depth = 0;
    int start = -1;

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('{')) {
            if (depth == 0) {
                start = i;
            }
            ++depth;
        } else if (ch == QLatin1Char('}')) {
            if (depth > 0) {
                --depth;
                if (depth == 0 && start >= 0) {
                    return text.mid(start, i - start + 1);
                }
            }
        }
    }

    return QString();
}

bool isLikelyChineseStopWord(const QString &text)
{
    static const QStringList markers = {
        QStringLiteral("\u7684"),
        QStringLiteral("\u4e86"),
        QStringLiteral("\u5728"),
        QStringLiteral("\u662f"),
        QStringLiteral("\u6211\u4eec"),
        QStringLiteral("\u53ef\u4ee5")
    };

    for (const QString &marker : markers) {
        if (text.contains(marker)) {
            return true;
        }
    }
    return false;
}

int countEnglishStopWords(const QString &text)
{
    static const QStringList markers = {
        QStringLiteral(" the "),
        QStringLiteral(" and "),
        QStringLiteral(" is "),
        QStringLiteral(" are "),
        QStringLiteral(" of "),
        QStringLiteral(" to ")
    };

    const QString lowered = QStringLiteral(" ") + text.toLower() + QStringLiteral(" ");
    int hits = 0;
    for (const QString &marker : markers) {
        if (lowered.contains(marker)) {
            ++hits;
        }
    }
    return hits;
}

struct LlmCallResult
{
    bool success = false;
    QString text;
    QString errorMessage;
};

LlmCallResult callLlmForJson(const ModelEndpointConfig &endpoint, const QString &inputText)
{
    qtllm::QtLLMClient client;
    client.setConfig(endpoint.llmConfig);
    if (!client.setProviderByName(endpoint.llmConfig.providerName)) {
        return {false, QString(), QStringLiteral("Failed to resolve provider for language-detect endpoint")};
    }

    qtllm::LlmRequest request;
    request.model = endpoint.llmConfig.model;
    request.stream = false;
    request.messages.append({
        QStringLiteral("system"),
        QStringLiteral("You are a language detection skill. Detect whether the text is Simplified Chinese or English. "
                       "Return only one JSON object with keys sourceLanguage, targetLanguage, confidence, evidence, method. "
                       "Use sourceLanguage values zh-CN or en. Use targetLanguage as the opposite language. "
                       "Set method to llm.")
    });
    request.messages.append({
        QStringLiteral("user"),
        QStringLiteral("Detect the language of this text and return JSON only:\n\n%1").arg(inputText)
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
        result.errorMessage = QStringLiteral("language-detect LLM fallback timed out");
        client.cancelCurrentRequest();
        loop.quit();
    });

    timeout.start(endpoint.llmConfig.timeoutMs > 0 ? endpoint.llmConfig.timeoutMs : 30000);
    client.sendRequest(request);
    loop.exec();

    return result;
}

} // namespace

LanguageDetectSkill::LanguageDetectSkill(std::shared_ptr<ModelRouter> modelRouter)
    : m_modelRouter(std::move(modelRouter))
{
}

SkillDescriptor LanguageDetectSkill::descriptor() const
{
    SkillDescriptor descriptor;
    descriptor.id = kSkillId;
    descriptor.displayName = QStringLiteral("Language Detect");
    descriptor.description = QStringLiteral("Detects whether source text is English or Simplified Chinese.");
    descriptor.category = QStringLiteral("analysis");
    descriptor.executionType = SkillExecutionType::Hybrid;
    return descriptor;
}

bool LanguageDetectSkill::canHandle(const SkillContext &context) const
{
    return !context.sourceText.trimmed().isEmpty();
}

SkillResult LanguageDetectSkill::execute(const SkillContext &context)
{
    const QString normalizedText = context.sourceText.trimmed();
    if (normalizedText.isEmpty()) {
        SkillResult result;
        result.success = false;
        result.status = QStringLiteral("invalid_input");
        result.errorMessage = QStringLiteral("language-detect requires non-empty sourceText");
        return result;
    }

    const HeuristicDetection heuristic = detectByHeuristics(normalizedText);
    if (heuristic.confidence >= 0.85) {
        return buildHeuristicResult(heuristic);
    }

    SkillResult llmResult = detectWithLlm(normalizedText);
    if (llmResult.success) {
        llmResult.warnings.append({QStringLiteral("heuristic_low_confidence"),
                                   QStringLiteral("Heuristic detection confidence was below threshold; LLM fallback used.")});
        return llmResult;
    }

    SkillResult fallback = buildHeuristicResult(heuristic);
    fallback.warnings.append({QStringLiteral("llm_fallback_failed"),
                              llmResult.errorMessage.isEmpty()
                                  ? QStringLiteral("LLM fallback was unavailable; heuristic result returned.")
                                  : llmResult.errorMessage});
    return fallback;
}

LanguageDetectSkill::HeuristicDetection LanguageDetectSkill::detectByHeuristics(const QString &text) const
{
    int chineseChars = 0;
    int englishLetters = 0;

    for (const QChar ch : text) {
        const ushort unicode = ch.unicode();
        if ((unicode >= 0x4E00 && unicode <= 0x9FFF) || (unicode >= 0x3400 && unicode <= 0x4DBF)) {
            ++chineseChars;
        } else if (ch.isLetter() && unicode < 128) {
            ++englishLetters;
        }
    }

    const int englishStopWordHits = countEnglishStopWords(text);
    const bool hasChineseStopWord = isLikelyChineseStopWord(text);
    const int totalSignal = chineseChars + englishLetters;

    HeuristicDetection detection;
    if (totalSignal <= 0) {
        detection.sourceLanguage = QStringLiteral("en");
        detection.targetLanguage = QStringLiteral("zh-CN");
        detection.confidence = 0.15;
        detection.evidence = QStringLiteral("No strong alphabetic or CJK signal was detected.");
        return detection;
    }

    if (chineseChars > englishLetters || hasChineseStopWord) {
        detection.sourceLanguage = QStringLiteral("zh-CN");
        detection.targetLanguage = QStringLiteral("en");
        detection.confidence = static_cast<double>(chineseChars + (hasChineseStopWord ? 10 : 0))
            / static_cast<double>(totalSignal + 10);
        detection.evidence = QStringLiteral("Detected %1 Chinese characters and %2 English letters.")
                                 .arg(chineseChars)
                                 .arg(englishLetters);
        return detection;
    }

    detection.sourceLanguage = QStringLiteral("en");
    detection.targetLanguage = QStringLiteral("zh-CN");
    detection.confidence = static_cast<double>(englishLetters + englishStopWordHits * 8)
        / static_cast<double>(totalSignal + 20);
    detection.evidence = QStringLiteral("Detected %1 English letters, %2 Chinese characters, %3 English stop-word hits.")
                             .arg(englishLetters)
                             .arg(chineseChars)
                             .arg(englishStopWordHits);
    return detection;
}

SkillResult LanguageDetectSkill::buildHeuristicResult(const HeuristicDetection &detection) const
{
    SkillResult result;
    result.success = true;
    result.status = QStringLiteral("completed");
    result.output.insert(QStringLiteral("sourceLanguage"), detection.sourceLanguage);
    result.output.insert(QStringLiteral("targetLanguage"), detection.targetLanguage);
    result.output.insert(QStringLiteral("confidence"), detection.confidence);
    result.output.insert(QStringLiteral("evidence"), detection.evidence);
    result.output.insert(QStringLiteral("method"), QStringLiteral("rule"));
    return result;
}

SkillResult LanguageDetectSkill::detectWithLlm(const QString &text) const
{
    SkillResult result;
    if (!m_modelRouter) {
        result.errorMessage = QStringLiteral("Model router is not available");
        return result;
    }

    const std::optional<ResolvedSkillRoute> route = m_modelRouter->resolve(kSkillId);
    if (!route.has_value() || !route->endpoint.has_value()) {
        result.errorMessage = QStringLiteral("No LLM endpoint binding configured for language-detect");
        return result;
    }

    const LlmCallResult llmCall = callLlmForJson(*route->endpoint, text);
    if (!llmCall.success || llmCall.text.trimmed().isEmpty()) {
        result.errorMessage = llmCall.errorMessage.isEmpty()
            ? QStringLiteral("language-detect LLM call failed")
            : llmCall.errorMessage;
        return result;
    }

    const QString jsonText = extractJsonObject(llmCall.text);
    if (jsonText.isEmpty()) {
        result.errorMessage = QStringLiteral("language-detect LLM response did not contain a JSON object");
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.errorMessage = QStringLiteral("language-detect LLM JSON parsing failed: %1").arg(parseError.errorString());
        return result;
    }

    const QJsonObject object = doc.object();
    const QString sourceLanguage = object.value(QStringLiteral("sourceLanguage")).toString().trimmed();
    QString targetLanguage = object.value(QStringLiteral("targetLanguage")).toString().trimmed();
    if (sourceLanguage != QStringLiteral("zh-CN") && sourceLanguage != QStringLiteral("en")) {
        result.errorMessage = QStringLiteral("language-detect LLM returned unsupported sourceLanguage");
        return result;
    }

    if (targetLanguage.isEmpty()) {
        targetLanguage = recommendedTargetForSource(sourceLanguage);
    }

    result.success = true;
    result.status = QStringLiteral("completed");
    result.output.insert(QStringLiteral("sourceLanguage"), sourceLanguage);
    result.output.insert(QStringLiteral("targetLanguage"), targetLanguage);
    result.output.insert(QStringLiteral("confidence"), object.value(QStringLiteral("confidence")).toDouble(0.5));
    result.output.insert(QStringLiteral("evidence"), object.value(QStringLiteral("evidence")).toString());
    result.output.insert(QStringLiteral("method"), object.value(QStringLiteral("method")).toString(QStringLiteral("llm")));
    return result;
}

} // namespace pdftranslator::skills
