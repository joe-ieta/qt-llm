#pragma once

#include "mcpscanlocationrepository.h"

#include <tools/mcp/defaultmcpclient.h>

#include <QHash>
#include <QString>
#include <QVector>

#include <memory>

struct McpDiscoveredServerEntry
{
    enum class AvailabilityStatus {
        Unknown,
        Available,
        Unavailable,
        InvalidConfig
    };

    qtllm::tools::mcp::McpServerDefinition server;
    QString sourceLabel;
    bool sourceIsManual = false;
    bool registered = false;
    AvailabilityStatus status = AvailabilityStatus::Unknown;
    QString statusMessage;
    QVector<qtllm::tools::mcp::McpToolDescriptor> tools;
    QVector<qtllm::tools::mcp::McpResourceDescriptor> resources;
    QVector<qtllm::tools::mcp::McpPromptDescriptor> prompts;
};

class McpServerDiscoveryService
{
public:
    explicit McpServerDiscoveryService(
        std::shared_ptr<qtllm::tools::mcp::IMcpClient> mcpClient = std::make_shared<qtllm::tools::mcp::DefaultMcpClient>());

    QVector<McpDiscoveredServerEntry> scan(const McpScanLocationConfig &config,
                                           const QVector<qtllm::tools::mcp::McpServerDefinition> &registeredServers,
                                           const QHash<QString, qtllm::tools::mcp::McpServerDefinition> &manualServers) const;

    bool populateDetails(McpDiscoveredServerEntry *entry, QString *errorMessage = nullptr) const;

    static QString normalizedServerId(const QString &serverId);
    static QString availabilityLabel(McpDiscoveredServerEntry::AvailabilityStatus status);
    static bool isRegistrable(const McpDiscoveredServerEntry &entry);

private:
    static QString expandPathPattern(QString pattern);
    static QStringList candidatePathsForDirectory(const QString &directoryPath);
    static QVector<qtllm::tools::mcp::McpServerDefinition> parseServersFromJson(const QJsonObject &root);
    static bool validateServer(const qtllm::tools::mcp::McpServerDefinition &server, QString *message);

    bool probeAvailability(McpDiscoveredServerEntry *entry, QString *errorMessage = nullptr) const;

private:
    std::shared_ptr<qtllm::tools::mcp::IMcpClient> m_mcpClient;
};
