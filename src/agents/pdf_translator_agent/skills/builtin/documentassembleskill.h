#pragma once

#include "../core/iskill.h"

namespace pdftranslator::skills {

class DocumentAssembleSkill : public ISkill
{
public:
    SkillDescriptor descriptor() const override;
    bool canHandle(const SkillContext &context) const override;
    SkillResult execute(const SkillContext &context) override;
};

} // namespace pdftranslator::skills
