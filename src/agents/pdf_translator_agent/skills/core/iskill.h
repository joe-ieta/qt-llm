#pragma once

#include "skilltypes.h"

namespace pdftranslator::skills {

class ISkill
{
public:
    virtual ~ISkill() = default;

    virtual SkillDescriptor descriptor() const = 0;
    virtual bool canHandle(const SkillContext &context) const = 0;
    virtual SkillResult execute(const SkillContext &context) = 0;
};

} // namespace pdftranslator::skills
