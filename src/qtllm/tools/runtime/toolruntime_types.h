#pragma once

#include "../../core/llmconfig.h"
#include "../../core/llmtypes.h"
#include "../../profile/clientprofile.h"
#include "../llmtooldefinition.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace qtllm::tools::runtime {

struct ToolCatalogSnapshot
{
    int schemaVersion = 1;
    QDateTime updatedAt = QDateTime::currentDateTimeUtc();
    QVector<tools::LlmToolDefinition> tools;
};

struct ClientToolPolicy
{
    QString clientId;
    QString mode = QStringLiteral("all"); // all | allowlist | denylist
    QStringList allowIds;
    QStringList denyIds;
    int maxToolsPerTurn = 8;
};

struct ToolExecutionContext
{
    QString clientId;
    QString sessionId;
    profile::ClientProfile profile;
    LlmConfig llmConfig;
    QVector<LlmMessage> historyWindow;
    QString requestId;
    QString traceId;
    QJsonObject extra;
};

struct ToolCallRequest
{
    QString callId;
    QString externalCallId;
    QString internalToolCallId;
    QString toolId;
    QJsonObject arguments;
    QString idempotencyKey;
};

struct ToolExecutionResult
{
    QString callId;
    QString externalCallId;
    QString internalToolCallId;
    QString toolId;
    bool success = false;
    QJsonObject output;
    QString errorCode;
    QString errorMessage;
    qint64 durationMs = 0;
    bool retryable = false;
};

struct ToolExecutionPolicy
{
    int maxParallelCalls = 4;
    int defaultTimeoutMs = 30000;
    int maxRetries = 0;
    bool failFast = false;
    QHash<QString, int> toolTimeoutOverrides;
    QHash<QString, int> toolConcurrencyOverrides;

    bool isToolAllowed(const QString &toolId, const ClientToolPolicy &clientPolicy) const
    {
        if (clientPolicy.mode == QStringLiteral("allowlist")) {
            return clientPolicy.allowIds.contains(toolId);
        }

        if (clientPolicy.mode == QStringLiteral("denylist")) {
            return !clientPolicy.denyIds.contains(toolId);
        }

        return !clientPolicy.denyIds.contains(toolId);
    }
};

} // namespace qtllm::tools::runtime
