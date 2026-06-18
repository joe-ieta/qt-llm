#pragma once

#include "../core/iskill.h"

#include <memory>

namespace pdftranslator::skills {

class ModelRouter;

class LanguageDetectSkill : public ISkill
{
public:
    explicit LanguageDetectSkill(std::shared_ptr<ModelRouter> modelRouter);

    SkillDescriptor descriptor() const override;
    bool canHandle(const SkillContext &context) const override;
    SkillResult execute(const SkillContext &context) override;

private:
    struct HeuristicDetection
    {
        QString sourceLanguage;
        QString targetLanguage;
        double confidence = 0.0;
        QString evidence;
    };

    HeuristicDetection detectByHeuristics(const QString &text) const;
    SkillResult buildHeuristicResult(const HeuristicDetection &detection) const;
    SkillResult detectWithLlm(const QString &text) const;

private:
    std::shared_ptr<ModelRouter> m_modelRouter;
};

} // namespace pdftranslator::skills
