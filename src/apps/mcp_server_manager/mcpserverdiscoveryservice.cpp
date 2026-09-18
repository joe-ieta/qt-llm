#include "mcpserverdiscoveryservice.h"

#include <tools/mcp/mcpserverregistry.h>

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QUrl>

namespace {

QString compactJson(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

} // namespace

McpServerDiscoveryService::McpServerDiscoveryService(
    std::shared_ptr<qtllm::tools::mcp::IMcpClient> mcpClient)
    : m_mcpClient(std::move(mcpClient))
{
}

QVector<McpDiscoveredServerEntry> McpServerDiscoveryService::scan(
    const McpScanLocationConfig &config,
    const QVector<qtllm::tools::mcp::McpServerDefinition> &registeredServers,
    const QHash<QString, qtllm::tools::mcp::McpServerDefinition> &manualServers) const
{
    QHash<QString, McpDiscoveredServerEntry> entries;
    QStringList filePaths;

    for (const QString &pattern : config.builtInFilePatterns) {
        const QString expanded = expandPathPattern(pattern);
        if (!expanded.isEmpty()) {
            filePaths.append(QDir::cleanPath(expanded));
        }
    }

    for (const QString &directory : config.customDirectories) {
        const QString expandedDirectory = expandPathPattern(directory);
        filePaths.append(candidatePathsForDirectory(expandedDirectory));
    }

    filePaths.removeDuplicates();

    for (const QString &path : filePaths) {
        QFile file(path);
        if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            continue;
        }

        const QVector<qtllm::tools::mcp::McpServerDefinition> parsed = parseServersFromJson(doc.object());
        for (const qtllm::tools::mcp::McpServerDefinition &server : parsed) {
            const QString id = normalizedServerId(server.serverId);
            if (id.isEmpty()) {
                continue;
            }

            McpDiscoveredServerEntry entry = entries.value(id);
            entry.server = server;
            entry.sourceLabel = path;
            entries.insert(id, entry);
        }
    }

    for (auto it = manualServers.constBegin(); it != manualServers.constEnd(); ++it) {
        McpDiscoveredServerEntry entry = entries.value(it.key());
        entry.server = it.value();
        entry.sourceIsManual = true;
        entry.sourceLabel = QStringLiteral("Manual entry");
        entries.insert(it.key(), entry);
    }

    for (const qtllm::tools::mcp::McpServerDefinition &registeredServer : registeredServers) {
        const QString id = normalizedServerId(registeredServer.serverId);
        if (id.isEmpty()) {
            continue;
        }

        McpDiscoveredServerEntry entry = entries.value(id);
        if (entry.server.serverId.isEmpty()) {
            entry.server = registeredServer;
            entry.sourceLabel = QStringLiteral("Registered only");
        }
        entry.registered = true;
        entries.insert(id, entry);
    }

    QVector<McpDiscoveredServerEntry> result;
    result.reserve(entries.size());
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        McpDiscoveredServerEntry entry = it.value();
        probeAvailability(&entry);
        result.append(entry);
    }

    std::sort(result.begin(), result.end(), [](const McpDiscoveredServerEntry &left, const McpDiscoveredServerEntry &right) {
        if (left.registered != right.registered) {
            return left.registered && !right.registered;
        }
        return normalizedServerId(left.server.serverId) < normalizedServerId(right.server.serverId);
    });
    return result;
}

bool McpServerDiscoveryService::populateDetails(McpDiscoveredServerEntry *entry, QString *errorMessage) const
{
    if (!entry) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No MCP server entry selected");
        }
        return false;
    }

    QString validationMessage;
    if (!validateServer(entry->server, &validationMessage)) {
        entry->status = McpDiscoveredServerEntry::AvailabilityStatus::InvalidConfig;
        entry->statusMessage = validationMessage;
        if (errorMessage) {
            *errorMessage = validationMessage;
        }
        return false;
    }

    QString toolsError;
    entry->tools = m_mcpClient ? m_mcpClient->listTools(entry->server, &toolsError)
                               : QVector<qtllm::tools::mcp::McpToolDescriptor>();
    QString resourcesError;
    entry->resources = m_mcpClient ? m_mcpClient->listResources(entry->server, &resourcesError)
                                   : QVector<qtllm::tools::mcp::McpResourceDescriptor>();
    QString promptsError;
    entry->prompts = m_mcpClient ? m_mcpClient->listPrompts(entry->server, &promptsError)
                                 : QVector<qtllm::tools::mcp::McpPromptDescriptor>();

    QStringList errors;
    if (!toolsError.isEmpty()) {
        errors.append(QStringLiteral("tools: %1").arg(toolsError));
    }
    if (!resourcesError.isEmpty()) {
        errors.append(QStringLiteral("resources: %1").arg(resourcesError));
    }
    if (!promptsError.isEmpty()) {
        errors.append(QStringLiteral("prompts: %1").arg(promptsError));
    }

    if (!errors.isEmpty()) {
        entry->statusMessage = errors.join(QStringLiteral(" | "));
        if (errorMessage) {
            *errorMessage = entry->statusMessage;
        }
        return false;
    }

    entry->status = McpDiscoveredServerEntry::AvailabilityStatus::Available;
    entry->statusMessage = QStringLiteral("Ready");
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

QString McpServerDiscoveryService::normalizedServerId(const QString &serverId)
{
    return serverId.trimmed().toLower();
}

QString McpServerDiscoveryService::availabilityLabel(McpDiscoveredServerEntry::AvailabilityStatus status)
{
    switch (status) {
    case McpDiscoveredServerEntry::AvailabilityStatus::Available:
        return QStringLiteral("Available");
    case McpDiscoveredServerEntry::AvailabilityStatus::Unavailable:
        return QStringLiteral("Unavailable");
    case McpDiscoveredServerEntry::AvailabilityStatus::InvalidConfig:
        return QStringLiteral("Invalid");
    case McpDiscoveredServerEntry::AvailabilityStatus::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

bool McpServerDiscoveryService::isRegistrable(const McpDiscoveredServerEntry &entry)
{
    return !entry.registered && entry.status == McpDiscoveredServerEntry::AvailabilityStatus::Available;
}

QString McpServerDiscoveryService::expandPathPattern(QString pattern)
{
    pattern.replace(QStringLiteral("${cwd}"), QDir::currentPath());
    pattern.replace(QStringLiteral("${home}"), QDir::homePath());
    pattern.replace(QStringLiteral("${appData}"), QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    return QDir::cleanPath(pattern);
}

QStringList McpServerDiscoveryService::candidatePathsForDirectory(const QString &directoryPath)
{
    if (directoryPath.trimmed().isEmpty()) {
        return {};
    }

    const QDir dir(directoryPath);
    return {
        dir.filePath(QStringLiteral(".qtllm/mcp/servers.json")),
        dir.filePath(QStringLiteral("mcp_servers.json")),
        dir.filePath(QStringLiteral(".cursor/mcp.json")),
        dir.filePath(QStringLiteral(".vscode/mcp.json"))
    };
}

QVector<qtllm::tools::mcp::McpServerDefinition> McpServerDiscoveryService::parseServersFromJson(const QJsonObject &root)
{
    QVector<qtllm::tools::mcp::McpServerDefinition> out;

    const QJsonArray serversArray = root.value(QStringLiteral("servers")).toArray();
    for (const QJsonValue &value : serversArray) {
        const qtllm::tools::mcp::McpServerDefinition server =
            qtllm::tools::mcp::McpServerDefinition::fromJson(value.toObject());
        if (!normalizedServerId(server.serverId).isEmpty()) {
            out.append(server);
        }
    }

    const QJsonObject mcpServers = root.value(QStringLiteral("mcpServers")).toObject();
    for (auto it = mcpServers.constBegin(); it != mcpServers.constEnd(); ++it) {
        const QJsonObject item = it.value().toObject();
        qtllm::tools::mcp::McpServerDefinition server;
        server.serverId = normalizedServerId(it.key());
        server.name = item.value(QStringLiteral("name")).toString(server.serverId);
        server.transport = item.value(QStringLiteral("transport")).toString();
        if (server.transport.isEmpty()) {
            server.transport = item.value(QStringLiteral("url")).toString().isEmpty()
                ? QStringLiteral("stdio")
                : QStringLiteral("streamable-http");
        }
        server.command = item.value(QStringLiteral("command")).toString();
        server.url = item.value(QStringLiteral("url")).toString();
        server.env = item.value(QStringLiteral("env")).toObject();
        server.headers = item.value(QStringLiteral("headers")).toObject();
        server.enabled = item.value(QStringLiteral("enabled")).toBool(true);
        server.timeoutMs = item.value(QStringLiteral("timeoutMs")).toInt(30000);

        const QJsonArray args = item.value(QStringLiteral("args")).toArray();
        for (const QJsonValue &argValue : args) {
            const QString arg = argValue.toString();
            if (!arg.isEmpty()) {
                server.args.append(arg);
            }
        }

        if (!server.serverId.isEmpty()) {
            out.append(server);
        }
    }

    return out;
}

bool McpServerDiscoveryService::validateServer(const qtllm::tools::mcp::McpServerDefinition &server, QString *message)
{
    if (!qtllm::tools::mcp::McpServerRegistry::isValidServerId(normalizedServerId(server.serverId))) {
        if (message) {
            *message = QStringLiteral("Invalid serverId. Use 1-32 chars: [a-z0-9_-]");
        }
        return false;
    }

    const QString transport = server.transport.trimmed().toLower();
    if (transport == QStringLiteral("stdio")) {
        if (server.command.trimmed().isEmpty()) {
            if (message) {
                *message = QStringLiteral("stdio requires command");
            }
            return false;
        }

        if (!QFile::exists(server.command) && QStandardPaths::findExecutable(server.command).isEmpty()) {
            if (message) {
                *message = QStringLiteral("Command not found: ") + server.command;
            }
            return false;
        }
        return true;
    }

    if (transport == QStringLiteral("streamable-http") || transport == QStringLiteral("sse")) {
        const QUrl url(server.url.trimmed());
        if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
            if (message) {
                *message = QStringLiteral("Invalid URL: ") + server.url;
            }
            return false;
        }
        return true;
    }

    if (message) {
        *message = QStringLiteral("Unsupported transport: ") + server.transport;
    }
    return false;
}

bool McpServerDiscoveryService::probeAvailability(McpDiscoveredServerEntry *entry, QString *errorMessage) const
{
    if (!entry) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No MCP server entry supplied");
        }
        return false;
    }

    QString validationMessage;
    if (!validateServer(entry->server, &validationMessage)) {
        entry->status = McpDiscoveredServerEntry::AvailabilityStatus::InvalidConfig;
        entry->statusMessage = validationMessage;
        if (errorMessage) {
            *errorMessage = validationMessage;
        }
        return false;
    }

    QString probeError;
    entry->tools = m_mcpClient ? m_mcpClient->listTools(entry->server, &probeError)
                               : QVector<qtllm::tools::mcp::McpToolDescriptor>();

    if (!probeError.isEmpty() && entry->tools.isEmpty()) {
        entry->status = McpDiscoveredServerEntry::AvailabilityStatus::Unavailable;
        entry->statusMessage = probeError;
        if (errorMessage) {
            *errorMessage = probeError;
        }
        return false;
    }

    entry->status = McpDiscoveredServerEntry::AvailabilityStatus::Available;
    entry->statusMessage = entry->tools.isEmpty()
        ? QStringLiteral("Reachable, no tools reported")
        : QStringLiteral("%1 tools available").arg(entry->tools.size());
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}
