#include "llmtoolregistry.h"

#include "builtintools.h"
#include "../logging/qtllmlogger.h"

#include <QJsonObject>

namespace qtllm::tools {

namespace {

QJsonObject toolFields(const LlmToolDefinition &tool)
{
    QJsonObject fields;
    fields.insert(QStringLiteral("toolId"), tool.toolId);
    fields.insert(QStringLiteral("invocationName"), tool.invocationName);
    fields.insert(QStringLiteral("name"), tool.name);
    fields.insert(QStringLiteral("category"), tool.category);
    fields.insert(QStringLiteral("enabled"), tool.enabled);
    return fields;
}

} // namespace

LlmToolRegistry::LlmToolRegistry()
{
    const QList<LlmToolDefinition> defaults = builtInTools();
    for (const LlmToolDefinition &tool : defaults) {
        registerTool(tool);
    }
}

bool LlmToolRegistry::registerTool(const LlmToolDefinition &tool)
{
    if (tool.toolId.trimmed().isEmpty()) {
        logging::QtLlmLogger::instance().error(QStringLiteral("tool.registry"),
                                               QStringLiteral("Tool registration rejected because toolId is empty"));
        return false;
    }

    LlmToolDefinition normalized = tool;
    if (normalized.systemBuiltIn) {
        normalized.category = QStringLiteral("builtin");
        normalized.removable = false;
        normalized.enabled = true;
    }

    if (normalized.invocationName.trimmed().isEmpty()) {
        normalized.invocationName = normalized.toolId;
    }
    if (normalized.name.trimmed().isEmpty()) {
        normalized.name = normalized.invocationName;
    }

    for (auto it = m_tools.constBegin(); it != m_tools.constEnd(); ++it) {
        if (it.key() != normalized.toolId && it.value().invocationName == normalized.invocationName) {
            QJsonObject fields = toolFields(normalized);
            fields.insert(QStringLiteral("conflictingToolId"), it.key());
            logging::QtLlmLogger::instance().error(QStringLiteral("tool.registry"),
                                                   QStringLiteral("Tool registration rejected because invocation name conflicts"),
                                                   {},
                                                   fields);
            return false;
        }
    }

    const bool existed = m_tools.contains(normalized.toolId);
    m_tools.insert(normalized.toolId, normalized);
    logging::QtLlmLogger::instance().info(QStringLiteral("tool.registry"),
                                          existed
                                              ? QStringLiteral("Tool definition replaced")
                                              : QStringLiteral("Tool registered"),
                                          {},
                                          toolFields(normalized));
    return true;
}

bool LlmToolRegistry::unregisterTool(const QString &toolId)
{
    const QString id = toolId.trimmed();
    if (id.isEmpty()) {
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.registry"),
                                              QStringLiteral("Tool unregister skipped because toolId is empty"));
        return false;
    }

    const auto it = m_tools.constFind(id);
    if (it != m_tools.constEnd() && !it->removable) {
        logging::QtLlmLogger::instance().warn(QStringLiteral("tool.registry"),
                                              QStringLiteral("Attempted to remove non-removable tool"),
                                              {},
                                              toolFields(*it));
        return false;
    }

    const bool removed = m_tools.remove(id);
    logging::QtLlmLogger::instance().info(QStringLiteral("tool.registry"),
                                          removed
                                              ? QStringLiteral("Tool removed")
                                              : QStringLiteral("Tool remove requested for missing tool"),
                                          {},
                                          QJsonObject{{QStringLiteral("toolId"), id}});
    return removed;
}

void LlmToolRegistry::clear()
{
    m_tools.clear();
    const QList<LlmToolDefinition> defaults = builtInTools();
    for (const LlmToolDefinition &tool : defaults) {
        registerTool(tool);
    }
    logging::QtLlmLogger::instance().info(QStringLiteral("tool.registry"),
                                          QStringLiteral("Tool registry reset to built-in defaults"),
                                          {},
                                          QJsonObject{{QStringLiteral("count"), defaults.size()}});
}

void LlmToolRegistry::replaceAll(const QList<LlmToolDefinition> &tools)
{
    m_tools.clear();
    const QList<LlmToolDefinition> merged = mergeWithBuiltInTools(tools);
    int accepted = 0;
    for (const LlmToolDefinition &tool : merged) {
        if (!tool.toolId.trimmed().isEmpty() && registerTool(tool)) {
            ++accepted;
        }
    }

    logging::QtLlmLogger::instance().info(QStringLiteral("tool.registry"),
                                          QStringLiteral("Tool registry replaced"),
                                          {},
                                          QJsonObject{{QStringLiteral("requestedCount"), tools.size()},
                                                      {QStringLiteral("mergedCount"), merged.size()},
                                                      {QStringLiteral("acceptedCount"), accepted}});
}

QList<LlmToolDefinition> LlmToolRegistry::allTools() const
{
    return m_tools.values();
}

QList<LlmToolDefinition> LlmToolRegistry::enabledTools() const
{
    QList<LlmToolDefinition> result;
    for (const LlmToolDefinition &tool : m_tools) {
        if (tool.enabled) {
            result.append(tool);
        }
    }
    return result;
}

bool LlmToolRegistry::contains(const QString &toolId) const
{
    return m_tools.contains(toolId.trimmed());
}

bool LlmToolRegistry::isRemovable(const QString &toolId) const
{
    const auto it = m_tools.constFind(toolId.trimmed());
    if (it == m_tools.constEnd()) {
        return false;
    }
    return it->removable;
}

QString LlmToolRegistry::resolveToolId(const QString &toolIdOrName) const
{
    const QString candidate = toolIdOrName.trimmed();
    if (candidate.isEmpty()) {
        return QString();
    }

    if (m_tools.contains(candidate)) {
        return candidate;
    }

    for (auto it = m_tools.constBegin(); it != m_tools.constEnd(); ++it) {
        const LlmToolDefinition &tool = it.value();
        if (tool.invocationName == candidate) {
            return tool.toolId;
        }
    }

    for (auto it = m_tools.constBegin(); it != m_tools.constEnd(); ++it) {
        const LlmToolDefinition &tool = it.value();
        if (tool.invocationName.compare(candidate, Qt::CaseInsensitive) == 0) {
            return tool.toolId;
        }
    }

    for (auto it = m_tools.constBegin(); it != m_tools.constEnd(); ++it) {
        const LlmToolDefinition &tool = it.value();
        if (tool.name.compare(candidate, Qt::CaseInsensitive) == 0) {
            return tool.toolId;
        }
    }

    logging::QtLlmLogger::instance().warn(QStringLiteral("tool.registry"),
                                          QStringLiteral("Tool id/name resolution fell back to original value"),
                                          {},
                                          QJsonObject{{QStringLiteral("value"), candidate}});
    return candidate;
}

} // namespace qtllm::tools
