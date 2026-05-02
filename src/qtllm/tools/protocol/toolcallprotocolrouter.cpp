#include "toolcallprotocolrouter.h"

#include "providerprotocoladapters.h"
#include <QStringList>

namespace qtllm::tools::protocol {

namespace {

bool containsAny(const QString &text, const QStringList &terms)
{
    const QString lowered = text.trimmed().toLower();
    for (const QString &term : terms) {
        if (lowered.contains(term)) {
            return true;
        }
    }
    return false;
}

QString inferVendorFromModelName(const QString &modelName)
{
    const QString lowered = modelName.trimmed().toLower();
    if (lowered.isEmpty()) {
        return QString();
    }

    // China major model vendors/families
    if (containsAny(lowered, QStringList({QStringLiteral("deepseek")}))) {
        return QStringLiteral("deepseek");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("qwen"), QStringLiteral("tongyi")}))) {
        return QStringLiteral("qwen");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("glm"), QStringLiteral("chatglm"), QStringLiteral("zhipu")}))) {
        return QStringLiteral("zhipu");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("kimi"), QStringLiteral("moonshot")}))) {
        return QStringLiteral("moonshot");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("doubao"), QStringLiteral("volcengine")}))) {
        return QStringLiteral("bytedance");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("hunyuan"), QStringLiteral("tencent")}))) {
        return QStringLiteral("tencent");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("baichuan")}))) {
        return QStringLiteral("baichuan");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("yi-") , QStringLiteral("lingyi"), QStringLiteral("01-ai")}))) {
        return QStringLiteral("lingyi");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("abab"), QStringLiteral("minimax")}))) {
        return QStringLiteral("minimax");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("ernie"), QStringLiteral("wenxin"), QStringLiteral("baidu")}))) {
        return QStringLiteral("baidu");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("spark"), QStringLiteral("xinghuo"), QStringLiteral("xfyun")}))) {
        return QStringLiteral("xfyun");
    }

    // US major model vendors/families
    if (containsAny(lowered, QStringList({QStringLiteral("gpt"), QStringLiteral("o1"), QStringLiteral("o3"), QStringLiteral("o4"), QStringLiteral("openai")}))) {
        return QStringLiteral("openai");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("claude"), QStringLiteral("anthropic")}))) {
        return QStringLiteral("anthropic");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("gemini"), QStringLiteral("palm"), QStringLiteral("google")}))) {
        return QStringLiteral("google");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("llama"), QStringLiteral("meta/")}))) {
        return QStringLiteral("meta");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("grok"), QStringLiteral("xai")}))) {
        return QStringLiteral("xai");
    }
    if (containsAny(lowered, QStringList({QStringLiteral("mistral"), QStringLiteral("mixtral"), QStringLiteral("codestral")}))) {
        return QStringLiteral("mistral");
    }

    return QString();
}

} // namespace

std::shared_ptr<IToolCallProtocolAdapter> ToolCallProtocolRouter::route(const QString &modelName,
                                                                         const QString &modelVendor,
                                                                         const QString &providerName) const
{
    QString vendor = modelVendor.trimmed().toLower();
    if (vendor.isEmpty()) {
        vendor = inferVendorFromModelName(modelName);
    }

    // Vendor-first routing.
    if (vendor == QStringLiteral("openai") || vendor == QStringLiteral("anthropic")
        || vendor == QStringLiteral("google") || vendor == QStringLiteral("xai")
        || vendor == QStringLiteral("mistral") || vendor == QStringLiteral("deepseek")
        || vendor == QStringLiteral("qwen") || vendor == QStringLiteral("zhipu")
        || vendor == QStringLiteral("moonshot") || vendor == QStringLiteral("bytedance")
        || vendor == QStringLiteral("tencent") || vendor == QStringLiteral("baichuan")
        || vendor == QStringLiteral("lingyi") || vendor == QStringLiteral("minimax")
        || vendor == QStringLiteral("baidu") || vendor == QStringLiteral("xfyun")) {
        // Most cloud APIs converge toward OpenAI-style tool_call schema.
        return std::make_shared<OpenAIToolCallProtocolAdapter>();
    }

    if (vendor == QStringLiteral("meta")) {
        return std::make_shared<VllmToolCallProtocolAdapter>();
    }

    // Provider fallback (runtime environment), only when vendor cannot be inferred.
    const QString provider = providerName.trimmed().toLower();
    if (provider == QStringLiteral("ollama")) {
        return std::make_shared<OllamaToolCallProtocolAdapter>();
    }
    if (provider == QStringLiteral("vllm")) {
        return std::make_shared<VllmToolCallProtocolAdapter>();
    }
    if (provider == QStringLiteral("openai") || provider == QStringLiteral("openai-compatible")
        || provider == QStringLiteral("sglang") || provider == QStringLiteral("llama-cpp")
        || provider == QStringLiteral("llamacpp") || provider == QStringLiteral("llama-cpp-local")) {
        return std::make_shared<OpenAIToolCallProtocolAdapter>();
    }

    return std::make_shared<GenericToolCallProtocolAdapter>();
}

} // namespace qtllm::tools::protocol
