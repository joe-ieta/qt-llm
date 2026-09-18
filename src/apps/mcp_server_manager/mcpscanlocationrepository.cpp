#include "mcpscanlocationrepository.h"

#include <logging/qtllmlogger.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

namespace {

QStringList readStringArray(const QJsonObject &root, const QString &key)
{
    QStringList values;
    const QJsonArray array = root.value(key).toArray();
    for (const QJsonValue &value : array) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            values.append(text);
        }
    }
    return values;
}

QJsonArray toJsonArray(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values) {
        array.append(value);
    }
    return array;
}

} // namespace

QJsonObject McpScanLocationConfig::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), schemaVersion);
    root.insert(QStringLiteral("builtInFilePatterns"), toJsonArray(builtInFilePatterns));
    root.insert(QStringLiteral("customDirectories"), toJsonArray(customDirectories));
    return root;
}

McpScanLocationConfig McpScanLocationConfig::fromJson(const QJsonObject &root)
{
    McpScanLocationConfig config;
    config.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(config.schemaVersion);
    config.builtInFilePatterns = readStringArray(root, QStringLiteral("builtInFilePatterns"));
    config.customDirectories = readStringArray(root, QStringLiteral("customDirectories"));

    if (config.builtInFilePatterns.isEmpty()) {
        config = defaultConfig();
    }
    return config;
}

McpScanLocationConfig McpScanLocationConfig::defaultConfig()
{
    McpScanLocationConfig config;
    config.builtInFilePatterns = QStringList({
        QStringLiteral("${cwd}/.qtllm/mcp/servers.json"),
        QStringLiteral("${cwd}/mcp_servers.json"),
        QStringLiteral("${cwd}/.cursor/mcp.json"),
        QStringLiteral("${cwd}/.vscode/mcp.json"),
        QStringLiteral("${home}/.cursor/mcp.json"),
        QStringLiteral("${home}/.vscode/mcp.json"),
        QStringLiteral("${home}/.claude/claude_desktop_config.json"),
        QStringLiteral("${appData}/Claude/claude_desktop_config.json")
    });
    return config;
}

McpScanLocationRepository::McpScanLocationRepository(QString rootDirectory)
    : m_rootDirectory(std::move(rootDirectory))
{
}

McpScanLocationConfig McpScanLocationRepository::loadOrCreate(QString *errorMessage) const
{
    QFile file(configPath());
    if (!file.exists()) {
        const McpScanLocationConfig config = McpScanLocationConfig::defaultConfig();
        save(config, errorMessage);
        return config;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open MCP scan config: ") + file.errorString();
        }
        return McpScanLocationConfig::defaultConfig();
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to parse MCP scan config: ") + parseError.errorString();
        }
        return McpScanLocationConfig::defaultConfig();
    }

    return McpScanLocationConfig::fromJson(doc.object());
}

bool McpScanLocationRepository::save(const McpScanLocationConfig &config, QString *errorMessage) const
{
    if (!ensureRootDirectory(errorMessage)) {
        return false;
    }

    QSaveFile file(configPath());
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open MCP scan config for writing: ") + file.errorString();
        }
        return false;
    }

    if (file.write(QJsonDocument(config.toJson()).toJson(QJsonDocument::Indented)) < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write MCP scan config: ") + file.errorString();
        }
        return false;
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to commit MCP scan config: ") + file.errorString();
        }
        return false;
    }

    return true;
}

QString McpScanLocationRepository::configPath() const
{
    return QDir(m_rootDirectory).filePath(QStringLiteral("scan_locations.json"));
}

bool McpScanLocationRepository::ensureRootDirectory(QString *errorMessage) const
{
    QDir dir(m_rootDirectory);
    if (dir.exists()) {
        return true;
    }

    if (dir.mkpath(QStringLiteral("."))) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Failed to create MCP config directory: ") + m_rootDirectory;
    }

    qtllm::logging::QtLlmLogger::instance().error(QStringLiteral("ui.mcp.scan"),
                                                  QStringLiteral("Failed to create MCP scan config directory"),
                                                  {},
                                                  QJsonObject{{QStringLiteral("rootDirectory"), m_rootDirectory}});
    return false;
}
