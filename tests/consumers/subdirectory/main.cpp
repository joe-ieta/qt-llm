#include <core/llmconfig.h>
#include <host/runtimeprofile.h>

int main()
{
    const qtllm::LlmConfig config;
    const qtllm::host::RuntimeProfile profile;
    return config.timeoutMs == 60000 && profile.llamaCppServerPort == 18080 ? 0 : 1;
}
