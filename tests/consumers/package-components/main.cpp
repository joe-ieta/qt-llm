#include <events/llmeventdispatcher.h>
#include <runtime/managedllamacppruntime.h>
#include <tools/llmtoolregistry.h>

int main()
{
    qtllm::tools::LlmToolRegistry registry;
    qtllm::events::LlmEventDispatcher::instance().clearSinks();
    const bool runtimeRecognized =
        qtllm::runtime::ManagedLlamaCppRuntime::isManagedProvider(
            QStringLiteral("llama-cpp"));
    return !registry.contains(QStringLiteral("qtllm.component.missing"))
            && qtllm::events::LlmEventDispatcher::instance().sinkCount() == 0
            && runtimeRecognized
        ? 0
        : 1;
}
