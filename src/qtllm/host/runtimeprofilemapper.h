#pragma once

#include "runtimeprofile.h"
#include "../core/llmconfig.h"
#include "../core/llmtypes.h"

namespace qtllm::host {

class RuntimeProfileMapper
{
public:
    static LlmConfig toConfig(const RuntimeProfile &profile);
    static RuntimeProfile fromConfig(const LlmConfig &config);
    static LlmRequest toRequest(const RuntimeProfile &profile, const ChatRequest &request);
};

} // namespace qtllm::host
