#include "modelrouter.h"

namespace pdftranslator::skills {

void ModelRouter::upsertEndpoint(const ModelEndpointConfig &endpoint)
{
    if (!endpoint.id.trimmed().isEmpty()) {
        m_endpoints.insert(endpoint.id, endpoint);
    }
}

void ModelRouter::upsertBinding(const SkillExecutionBinding &binding)
{
    if (!binding.skillId.trimmed().isEmpty()) {
        m_bindings.insert(binding.skillId, binding);
    }
}

std::optional<ModelEndpointConfig> ModelRouter::endpoint(const QString &endpointId) const
{
    const QString normalized = endpointId.trimmed();
    if (normalized.isEmpty() || !m_endpoints.contains(normalized)) {
        return std::nullopt;
    }
    return m_endpoints.value(normalized);
}

std::optional<ResolvedSkillRoute> ModelRouter::resolve(const QString &skillId) const
{
    const QString normalized = skillId.trimmed();
    if (normalized.isEmpty() || !m_bindings.contains(normalized)) {
        return std::nullopt;
    }

    ResolvedSkillRoute route;
    route.binding = m_bindings.value(normalized);
    route.endpoint = endpoint(route.binding.endpointId);
    route.fallbackEndpoint = endpoint(route.binding.fallbackEndpointId);
    return route;
}

} // namespace pdftranslator::skills
