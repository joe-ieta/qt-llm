#include <core/llmconfig.h>
#include <host/runtimeprofile.h>

int main()
{
    const qtllm::LlmConfig config;
    const qtllm::host::RuntimeProfile profile;
    return config.stream && profile.timeoutMs == 60000 ? 0 : 1;
}
