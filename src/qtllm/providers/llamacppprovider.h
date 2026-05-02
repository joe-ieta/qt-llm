#pragma once

#include "openaicompatibleprovider.h"

namespace qtllm {

class LlamaCppProvider : public OpenAICompatibleProvider
{
public:
    QString name() const override;
};

} // namespace qtllm
