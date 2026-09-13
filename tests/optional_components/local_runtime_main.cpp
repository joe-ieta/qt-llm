#include "runtime/managedllamacppruntime.h"

int main()
{
    return qtllm::runtime::ManagedLlamaCppRuntime::isManagedProvider(
               QStringLiteral("llama-cpp"))
        ? 0
        : 1;
}
