#include "mcpserverregistry.h"

#include "../../logging/qtllmlogger.h"

#include <QJsonObject>
#include <QRegularExpression>

namespace qtllm::tools::mcp {

namespace {

QString normalizeId(const QString &serverId)
{
    return serverId.trimmed().toLower();
}

QJsonObject serverFields(const McpServerDefinition &server)
{
    QJsonObject fields;
    fields.insert(QStringLiteral("serverId"), server.serverId);
    fields.insert(QStringLiteral("transport"), server.transport);
    fields.insert(QStringLiteral("enabled"), server.enabled);
    fields.insert(QStringLiteral("name"), server.name);
    return fields;
}

bool validateServer(const McpServerDefinition &server, QString *errorMessage)
{
    const QString id = normalizeId(server.serverId);
    if (!McpServerRegistry::isValidServerId(id)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid serverId. Use 1-32 chars: [a-z0-9_-]");
        }
        return false;
    }

    const QString transport = server.transport.trimmed().toLower();
    if (transport != QStringLiteral("stdio")
        && transport != QStringLiteral("streamable-http")
        && transport != QStringLiteral("sse")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unsupported transport: ") + server.transport;
        }
        return false;
    }

    if (transport == QStringLiteral("stdio") && server.command.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("stdio transport requires command");
        }
        return false;
    }

    if ((transport == QStringLiteral("streamable-http") || transport == QStringLiteral("sse"))
        && server.url.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("HTTP/SSE transport requires url");
        }
        return false;
    }

    return true;
}

} // namespace

bool McpServerRegistry::registerServer(const McpServerDefinition &server, QString *errorMessage)
{
    if (!validateServer(server, errorMessage)) {
        logging::QtLlmLogger::instance().error(QStringLiteral("mcp.registry"),
                                               QStringLiteral("MCP server registration rejected"),
                                               {},
                                               serverFields(server));
        return false;
    }

    const QString id = normalizeId(server.serverId);
    if (m_servers.contains(id)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("MCP server already exists: ") + id;
        }
        logging::QtLlmLogger::instance().warn(QStringLiteral("mcp.registry"),
                                              QStringLiteral("MCP server registration skipped because id already exists"),
                                              {},
                                              QJsonObject{{QStringLiteral("serverId"), id}});
        return false;
    }

    McpServerDefinition normalized = server;
    normalized.serverId = id;
    normalized.transport = normalized.transport.trimmed().toLower();
    m_servers.insert(id, normalized);
    logging::QtLlmLogger::instance().info(QStringLiteral("mcp.registry"),
                                          QStringLiteral("MCP server registered"),
                                          {},
                                          serverFields(normalized));
    return true;
}

bool McpServerRegistry::updateServer(const McpServerDefinition &server, QString *errorMessage)
{
    if (!validateServer(server, errorMessage)) {
        logging::QtLlmLogger::instance().error(QStringLiteral("mcp.registry"),
                                               QStringLiteral("MCP server update rejected"),
                                               {},
                                               serverFields(server));
        return false;
    }

    const QString id = normalizeId(server.serverId);
    if (!m_servers.contains(id)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("MCP server does not exist: ") + id;
        }
        logging::QtLlmLogger::instance().warn(QStringLiteral("mcp.registry"),
                                              QStringLiteral("MCP server update requested for missing server"),
                                              {},
                                              QJsonObject{{QStringLiteral("serverId"), id}});
        return false;
    }

    McpServerDefinition normalized = server;
    normalized.serverId = id;
    normalized.transport = normalized.transport.trimmed().toLower();
    m_servers.insert(id, normalized);
    logging::QtLlmLogger::instance().info(QStringLiteral("mcp.registry"),
                                          QStringLiteral("MCP server updated"),
                                          {},
                                          serverFields(normalized));
    return true;
}

bool McpServerRegistry::removeServer(const QString &serverId, QString *errorMessage)
{
    const QString id = normalizeId(serverId);
    if (id.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("serverId is empty");
        }
        logging::QtLlmLogger::instance().warn(QStringLiteral("mcp.registry"),
                                              QStringLiteral("MCP server remove rejected because serverId is empty"));
        return false;
    }

    if (m_servers.remove(id)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("MCP server does not exist: ") + id;
        }
        logging::QtLlmLogger::instance().warn(QStringLiteral("mcp.registry"),
                                              QStringLiteral("MCP server remove requested for missing server"),
                                              {},
                                              QJsonObject{{QStringLiteral("serverId"), id}});
        return false;
    }

    logging::QtLlmLogger::instance().info(QStringLiteral("mcp.registry"),
                                          QStringLiteral("MCP server removed"),
                                          {},
                                          QJsonObject{{QStringLiteral("serverId"), id}});
    return true;
}

bool McpServerRegistry::contains(const QString &serverId) const
{
    return m_servers.contains(normalizeId(serverId));
}

std::optional<McpServerDefinition> McpServerRegistry::find(const QString &serverId) const
{
    const QString id = normalizeId(serverId);
    const auto it = m_servers.constFind(id);
    if (it == m_servers.constEnd()) {
        return std::nullopt;
    }
    return *it;
}

QVector<McpServerDefinition> McpServerRegistry::allServers() const
{
    return m_servers.values().toVector();
}

void McpServerRegistry::clear()
{
    const int count = m_servers.size();
    m_servers.clear();
    logging::QtLlmLogger::instance().info(QStringLiteral("mcp.registry"),
                                          QStringLiteral("MCP server registry cleared"),
                                          {},
                                          QJsonObject{{QStringLiteral("removedCount"), count}});
}

void McpServerRegistry::replaceAll(const QVector<McpServerDefinition> &servers, QString *errorMessage)
{
    m_servers.clear();

    int accepted = 0;
    int rejected = 0;
    QString lastError;
    for (const McpServerDefinition &server : servers) {
        QString localError;
        if (registerServer(server, &localError)) {
            ++accepted;
            continue;
        }

        ++rejected;
        if (!localError.isEmpty()) {
            lastError = localError;
            logging::QtLlmLogger::instance().warn(QStringLiteral("mcp.registry"),
                                                  QStringLiteral("Invalid MCP server skipped during registry replace"),
                                                  {},
                                                  QJsonObject{{QStringLiteral("serverId"), server.serverId},
                                                              {QStringLiteral("error"), localError}});
        }
    }

    if (errorMessage && !lastError.isEmpty()) {
        *errorMessage = lastError;
    }

    logging::QtLlmLogger::instance().info(QStringLiteral("mcp.registry"),
                                          QStringLiteral("MCP server registry replaced"),
                                          {},
                                          QJsonObject{{QStringLiteral("requestedCount"), servers.size()},
                                                      {QStringLiteral("acceptedCount"), accepted},
                                                      {QStringLiteral("rejectedCount"), rejected}});
}

bool McpServerRegistry::isValidServerId(const QString &serverId)
{
    static const QRegularExpression re(QStringLiteral("^[a-z0-9_-]{1,32}$"));
    return re.match(serverId.trimmed().toLower()).hasMatch();
}

} // namespace qtllm::tools::mcp
