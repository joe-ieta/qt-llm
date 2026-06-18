#pragma once

#include "../core/iskill.h"

#include <memory>

namespace pdftranslator::skills {

class ModelRouter;

namespace mcp {
class McpGateway;
}

class PdfTextExtractSkill : public ISkill
{
public:
    PdfTextExtractSkill(std::shared_ptr<ModelRouter> modelRouter,
                        std::shared_ptr<mcp::McpGateway> mcpGateway);

    SkillDescriptor descriptor() const override;
    bool canHandle(const SkillContext &context) const override;
    SkillResult execute(const SkillContext &context) override;

private:
    std::shared_ptr<ModelRouter> m_modelRouter;
    std::shared_ptr<mcp::McpGateway> m_mcpGateway;
};

} // namespace pdftranslator::skills
