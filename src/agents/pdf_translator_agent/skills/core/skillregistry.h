#pragma once

#include "iskill.h"

#include <QHash>
#include <QList>
#include <QString>
#include <memory>

namespace pdftranslator::skills {

class SkillRegistry
{
public:
    bool registerSkill(const std::shared_ptr<ISkill> &skill);
    std::shared_ptr<ISkill> skill(const QString &id) const;
    QList<SkillDescriptor> descriptors() const;

private:
    QHash<QString, std::shared_ptr<ISkill>> m_skills;
};

} // namespace pdftranslator::skills
