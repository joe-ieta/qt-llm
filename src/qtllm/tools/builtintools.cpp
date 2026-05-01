#include "builtintools.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>

namespace qtllm::tools {

namespace {

LlmToolDefinition makeCurrentTimeTool()
{
    LlmToolDefinition tool;
    tool.toolId = QStringLiteral("current_time");
    tool.invocationName = QStringLiteral("current_time");
    tool.name = QStringLiteral("Current Time");
    tool.description = QStringLiteral("Get current date/time in local or specified timezone");
    tool.capabilityTags = QStringList({QStringLiteral("time"), QStringLiteral("date"), QStringLiteral("now"), QStringLiteral("today"), QStringLiteral("时间")});
    tool.category = QStringLiteral("builtin");
    tool.systemBuiltIn = true;
    tool.removable = false;
    tool.enabled = true;

    QJsonObject schema;
    schema.insert(QStringLiteral("type"), QStringLiteral("object"));
    QJsonObject properties;
    QJsonObject timezone;
    timezone.insert(QStringLiteral("type"), QStringLiteral("string"));
    timezone.insert(QStringLiteral("description"), QStringLiteral("IANA timezone, e.g. Asia/Shanghai"));
    properties.insert(QStringLiteral("timezone"), timezone);
    schema.insert(QStringLiteral("properties"), properties);
    tool.inputSchema = schema;
    return tool;
}

LlmToolDefinition makeCurrentWeatherTool()
{
    LlmToolDefinition tool;
    tool.toolId = QStringLiteral("current_weather");
    tool.invocationName = QStringLiteral("current_weather");
    tool.name = QStringLiteral("Current Weather");
    tool.description = QStringLiteral("Get current weather by latitude and longitude");
    tool.capabilityTags = QStringList({QStringLiteral("weather"), QStringLiteral("temperature"), QStringLiteral("天气"), QStringLiteral("气温")});
    tool.category = QStringLiteral("builtin");
    tool.systemBuiltIn = true;
    tool.removable = false;
    tool.enabled = true;

    QJsonObject schema;
    schema.insert(QStringLiteral("type"), QStringLiteral("object"));
    QJsonObject properties;

    QJsonObject latitude;
    latitude.insert(QStringLiteral("type"), QStringLiteral("number"));
    latitude.insert(QStringLiteral("description"), QStringLiteral("Latitude, e.g. 31.2304"));
    properties.insert(QStringLiteral("latitude"), latitude);

    QJsonObject longitude;
    longitude.insert(QStringLiteral("type"), QStringLiteral("number"));
    longitude.insert(QStringLiteral("description"), QStringLiteral("Longitude, e.g. 121.4737"));
    properties.insert(QStringLiteral("longitude"), longitude);

    QJsonObject timezone;
    timezone.insert(QStringLiteral("type"), QStringLiteral("string"));
    timezone.insert(QStringLiteral("description"), QStringLiteral("Optional IANA timezone, default auto"));
    properties.insert(QStringLiteral("timezone"), timezone);

    schema.insert(QStringLiteral("properties"), properties);

    QJsonArray required;
    required.append(QStringLiteral("latitude"));
    required.append(QStringLiteral("longitude"));
    schema.insert(QStringLiteral("required"), required);

    tool.inputSchema = schema;
    return tool;
}

} // namespace

QList<LlmToolDefinition> builtInTools()
{
    return QList<LlmToolDefinition>({makeCurrentTimeTool(), makeCurrentWeatherTool()});
}

bool isBuiltInToolId(const QString &toolId)
{
    const QString id = toolId.trimmed();
    return id == QStringLiteral("current_time") || id == QStringLiteral("current_weather");
}

QList<LlmToolDefinition> mergeWithBuiltInTools(const QList<LlmToolDefinition> &tools, bool *changed)
{
    QHash<QString, LlmToolDefinition> merged;
    for (const LlmToolDefinition &tool : tools) {
        if (!tool.toolId.trimmed().isEmpty()) {
            merged.insert(tool.toolId.trimmed(), tool);
        }
    }

    bool localChanged = false;
    const QList<LlmToolDefinition> fixed = builtInTools();
    for (const LlmToolDefinition &builtIn : fixed) {
        const auto it = merged.constFind(builtIn.toolId);
        if (it == merged.constEnd()) {
            merged.insert(builtIn.toolId, builtIn);
            localChanged = true;
            continue;
        }

        LlmToolDefinition normalized = *it;
        normalized.systemBuiltIn = true;
        normalized.category = QStringLiteral("builtin");
        normalized.removable = false;
        normalized.enabled = true;

        if (normalized.invocationName.isEmpty()) {
            normalized.invocationName = builtIn.invocationName;
        }
        if (normalized.name.isEmpty()) {
            normalized.name = builtIn.name;
        }
        if (normalized.description.isEmpty()) {
            normalized.description = builtIn.description;
        }
        if (normalized.inputSchema.isEmpty()) {
            normalized.inputSchema = builtIn.inputSchema;
        }
        if (normalized.capabilityTags.isEmpty()) {
            normalized.capabilityTags = builtIn.capabilityTags;
        }

        const bool differs = (normalized.category != it->category)
            || (normalized.systemBuiltIn != it->systemBuiltIn)
            || (normalized.removable != it->removable)
            || (normalized.enabled != it->enabled)
            || (normalized.invocationName != it->invocationName)
            || (normalized.name != it->name)
            || (normalized.description != it->description)
            || (normalized.inputSchema != it->inputSchema)
            || (normalized.capabilityTags != it->capabilityTags);

        if (differs) {
            localChanged = true;
            merged.insert(builtIn.toolId, normalized);
        }
    }

    if (changed) {
        *changed = localChanged;
    }

    return merged.values();
}

} // namespace qtllm::tools
