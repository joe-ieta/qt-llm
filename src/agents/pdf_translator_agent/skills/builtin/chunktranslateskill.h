#pragma once

#include "../core/iskill.h"

#include <memory>

namespace pdftranslator::skills {

class ModelRouter;

class ChunkTranslateSkill : public ISkill
{
public:
    explicit ChunkTranslateSkill(std::shared_ptr<ModelRouter> modelRouter);

    SkillDescriptor descriptor() const override;
    bool canHandle(const SkillContext &context) const override;
    SkillResult execute(const SkillContext &context) override;

private:
    std::shared_ptr<ModelRouter> m_modelRouter;
};

} // namespace pdftranslator::skills
