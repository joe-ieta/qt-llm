#include "skillregistry.h"

namespace pdftranslator::skills {

bool SkillRegistry::registerSkill(const std::shared_ptr<ISkill> &skill)
{
    if (!skill) {
        return false;
    }

    const SkillDescriptor descriptor = skill->descriptor();
    if (descriptor.id.trimmed().isEmpty()) {
        return false;
    }

    m_skills.insert(descriptor.id, skill);
    return true;
}

std::shared_ptr<ISkill> SkillRegistry::skill(const QString &id) const
{
    return m_skills.value(id.trimmed());
}

QList<SkillDescriptor> SkillRegistry::descriptors() const
{
    QList<SkillDescriptor> result;
    result.reserve(m_skills.size());

    for (auto it = m_skills.constBegin(); it != m_skills.constEnd(); ++it) {
        result.append(it.value()->descriptor());
    }

    return result;
}

} // namespace pdftranslator::skills
