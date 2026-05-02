#include "llamacppprovider.h"

namespace qtllm {

QString LlamaCppProvider::name() const
{
    return QStringLiteral("llama-cpp");
}

} // namespace qtllm
