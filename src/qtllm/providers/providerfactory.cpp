#include "providerfactory.h"

#include "ollamaprovider.h"
#include "openaiprovider.h"
#include "openaicompatibleprovider.h"
#include "llamacppprovider.h"
#include "vllmprovider.h"

#include <QString>

namespace qtllm {

std::unique_ptr<ILLMProvider> ProviderFactory::create(const QString &providerName)
{
    const QString provider = providerName.trimmed().toLower();

    if (provider == QStringLiteral("ollama")) {
        return std::make_unique<OllamaProvider>();
    }

    if (provider == QStringLiteral("vllm")) {
        return std::make_unique<VllmProvider>();
    }

    if (provider == QStringLiteral("llama-cpp")
        || provider == QStringLiteral("llamacpp")
        || provider == QStringLiteral("llama-cpp-local")) {
        return std::make_unique<LlamaCppProvider>();
    }

    if (provider == QStringLiteral("openai")) {
        return std::make_unique<OpenAIProvider>();
    }

    if (provider == QStringLiteral("openai-compatible")
        || provider == QStringLiteral("sglang")
        || provider == QStringLiteral("anthropic")
        || provider == QStringLiteral("google")
        || provider == QStringLiteral("gemini")
        || provider == QStringLiteral("deepseek")
        || provider == QStringLiteral("qwen")
        || provider == QStringLiteral("glm")
        || provider == QStringLiteral("zhipu")) {
        return std::make_unique<OpenAICompatibleProvider>();
    }

    return nullptr;
}

} // namespace qtllm
