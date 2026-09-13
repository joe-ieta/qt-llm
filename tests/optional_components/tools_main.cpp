#include "tools/llmtoolregistry.h"

int main()
{
    qtllm::tools::LlmToolRegistry registry;
    return registry.contains(QStringLiteral("qtllm.component.missing")) ? 1 : 0;
}
