#pragma once

#include "skilltypes.h"

#include "../../../../qtllm/core/llmconfig.h"

#include <QHash>
#include <QString>
#include <optional>

namespace pdftranslator::skills {

struct ModelEndpointConfig
{
    QString id;
    QString displayName;
    qtllm::LlmConfig llmConfig;
    bool supportsStructuredOutput = true;
    bool supportsVision = false;
    int maxConcurrency = 1;
};

struct SkillExecutionBinding
{
    QString skillId;
    QString endpointId;
    QString fallbackEndpointId;
    SkillExecutionType executionType = SkillExecutionType::Builtin;
    QString mcpServerId;
    QString mcpToolName;
};

struct ResolvedSkillRoute
{
    SkillExecutionBinding binding;
    std::optional<ModelEndpointConfig> endpoint;
    std::optional<ModelEndpointConfig> fallbackEndpoint;
};

class ModelRouter
{
public:
    void upsertEndpoint(const ModelEndpointConfig &endpoint);
    void upsertBinding(const SkillExecutionBinding &binding);

    std::optional<ModelEndpointConfig> endpoint(const QString &endpointId) const;
    std::optional<ResolvedSkillRoute> resolve(const QString &skillId) const;

private:
    QHash<QString, ModelEndpointConfig> m_endpoints;
    QHash<QString, SkillExecutionBinding> m_bindings;
};

} // namespace pdftranslator::skills
