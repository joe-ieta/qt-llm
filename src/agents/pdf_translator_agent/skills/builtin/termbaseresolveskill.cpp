#include "termbaseresolveskill.h"

#include "../core/modelrouter.h"
#include "../mcp/mcpgateway.h"

#include <QJsonArray>

namespace pdftranslator::skills {

namespace {

const QString kSkillId = QStringLiteral("term-base-resolve");

QJsonArray normalizeTerms(const QJsonObject &output)
{
    QJsonArray terms = output.value(QStringLiteral("terms")).toArray();
    if (!terms.isEmpty()) {
        return terms;
    }

    const QJsonValue contentValue = output.value(QStringLiteral("content"));
    if (contentValue.isArray()) {
        return contentValue.toArray();
    }

    if (contentValue.isObject()) {
        const QJsonArray nestedTerms = contentValue.toObject().value(QStringLiteral("terms")).toArray();
        if (!nestedTerms.isEmpty()) {
            return nestedTerms;
        }
    }

    return QJsonArray();
}

} // namespace

TermBaseResolveSkill::TermBaseResolveSkill(std::shared_ptr<ModelRouter> modelRouter,
                                           std::shared_ptr<mcp::McpGateway> mcpGateway)
    : m_modelRouter(std::move(modelRouter))
    , m_mcpGateway(std::move(mcpGateway))
{
}

SkillDescriptor TermBaseResolveSkill::descriptor() const
{
    SkillDescriptor descriptor;
    descriptor.id = kSkillId;
    descriptor.displayName = QStringLiteral("Term Base Resolve");
    descriptor.description = QStringLiteral("Resolves document-specific terminology through an MCP terminology service.");
    descriptor.category = QStringLiteral("quality");
    descriptor.executionType = SkillExecutionType::Mcp;
    return descriptor;
}

bool TermBaseResolveSkill::canHandle(const SkillContext &context) const
{
    return !context.sourceText.trimmed().isEmpty();
}

SkillResult TermBaseResolveSkill::execute(const SkillContext &context)
{
    SkillResult result;
    if (!m_modelRouter || !m_mcpGateway) {
        result.status = QStringLiteral("unavailable");
        result.errorMessage = QStringLiteral("Term base MCP dependencies are not available");
        return result;
    }

    const std::optional<ResolvedSkillRoute> route = m_modelRouter->resolve(kSkillId);
    if (!route.has_value() || route->binding.mcpServerId.trimmed().isEmpty() || route->binding.mcpToolName.trimmed().isEmpty()) {
        result.status = QStringLiteral("not_configured");
        result.errorMessage = QStringLiteral("No MCP binding configured for term-base-resolve");
        return result;
    }

    QString errorMessage;
    const qtllm::tools::mcp::McpToolCallResult mcpResult = m_mcpGateway->callTool(
        route->binding.mcpServerId,
        route->binding.mcpToolName,
        QJsonObject{
            {QStringLiteral("text"), context.sourceText.left(6000)},
            {QStringLiteral("sourceLanguage"), context.sourceLanguageHint},
            {QStringLiteral("targetLanguage"), context.targetLanguageHint},
            {QStringLiteral("documentPath"), context.documentPath}
        },
        &errorMessage);

    if (!mcpResult.success) {
        result.status = QStringLiteral("mcp_failed");
        result.errorMessage = errorMessage.isEmpty() ? mcpResult.errorMessage : errorMessage;
        result.retryable = mcpResult.retryable;
        return result;
    }

    result.success = true;
    result.status = QStringLiteral("completed");
    result.output.insert(QStringLiteral("terms"), normalizeTerms(mcpResult.output));
    result.output.insert(QStringLiteral("mcpServerId"), route->binding.mcpServerId);
    result.output.insert(QStringLiteral("mcpToolName"), route->binding.mcpToolName);
    return result;
}

} // namespace pdftranslator::skills
