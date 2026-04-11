#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace qtllm::toolsinside {

struct ToolsInsideTraceFilter
{
    QString clientId;
    QString sessionId;
    int limit = 100;
};

struct ToolsInsideStoragePolicy
{
    bool persistRawPrompt = true;
    bool persistRawRequestPayload = true;
    bool persistRawProviderPayload = true;
    bool persistRawToolArguments = true;
    bool persistRawToolOutput = true;
    bool persistRawResponse = true;
};

class IToolsInsideRedactionPolicy
{
public:
    virtual ~IToolsInsideRedactionPolicy() = default;

    virtual QByteArray redact(const QString &artifactKind,
                              const QByteArray &payload,
                              const QJsonObject &metadata) const
    {
        Q_UNUSED(artifactKind)
        Q_UNUSED(metadata)
        return payload;
    }
};

class NoOpToolsInsideRedactionPolicy final : public IToolsInsideRedactionPolicy
{
public:
    QByteArray redact(const QString &artifactKind,
                      const QByteArray &payload,
                      const QJsonObject &metadata) const override
    {
        Q_UNUSED(artifactKind)
        Q_UNUSED(metadata)
        return payload;
    }
};

struct ToolsInsideClientSummary
{
    QString clientId;
    int sessionCount = 0;
    int traceCount = 0;
    QDateTime lastActivityUtc;
};

struct ToolsInsideSessionSummary
{
    QString clientId;
    QString sessionId;
    QString latestTraceId;
    int traceCount = 0;
    QDateTime lastActivityUtc;
};

struct ToolsInsideTraceSummary
{
    QString traceId;
    QString clientId;
    QString sessionId;
    QString rootRequestId;
    QString status;
    QString model;
    QString provider;
    QString vendor;
    QString finalAnswerArtifactId;
    QString turnInputPreview;
    QDateTime startedAtUtc = QDateTime::currentDateTimeUtc();
    QDateTime endedAtUtc;
    QJsonObject summary;
};

struct ToolsInsideSpanRecord
{
    QString spanId;
    QString traceId;
    QString parentSpanId;
    QString requestId;
    QString toolCallId;
    QString kind;
    QString name;
    QString status;
    QString errorCode;
    QDateTime startedAtUtc = QDateTime::currentDateTimeUtc();
    QDateTime endedAtUtc;
    QJsonObject summary;
};

struct ToolsInsideEventRecord
{
    QString eventId;
    QString traceId;
    QString spanId;
    QString requestId;
    QString toolCallId;
    QString category;
    QString name;
    QDateTime timestampUtc = QDateTime::currentDateTimeUtc();
    QJsonObject payload;
};

struct ToolsInsideArtifactRef
{
    QString artifactId;
    QString traceId;
    QString clientId;
    QString sessionId;
    QString kind;
    QString mimeType;
    QString storageType = QStringLiteral("file");
    QString relativePath;
    QString redactionState = QStringLiteral("raw");
    QString sha256;
    qint64 sizeBytes = 0;
    QDateTime createdAtUtc = QDateTime::currentDateTimeUtc();
    QJsonObject metadata;
};

struct ToolsInsideToolCallRecord
{
    QString toolCallId;
    QString externalCallId;
    QString traceId;
    QString requestId;
    QString toolId;
    QString toolName;
    QString toolCategory;
    QString serverId;
    QString invocationName;
    QString argumentsArtifactId;
    QString outputArtifactId;
    QString followupArtifactId;
    QString status;
    QString errorCode;
    QString errorMessage;
    bool retryable = false;
    int roundIndex = 0;
    qint64 durationMs = 0;
    QDateTime startedAtUtc = QDateTime::currentDateTimeUtc();
    QDateTime endedAtUtc;
    QJsonObject summary;
};

struct ToolsInsideSupportLink
{
    QString supportLinkId;
    QString traceId;
    QString toolCallId;
    QString targetKind;
    QString targetId;
    QString supportType;
    QString source;
    QString evidenceArtifactId;
    QString note;
    double confidence = 0.0;
    QDateTime createdAtUtc = QDateTime::currentDateTimeUtc();
    QJsonObject metadata;
};

struct ToolsInsideStorageStats
{
    int clientCount = 0;
    int sessionCount = 0;
    int traceCount = 0;
    qint64 artifactCount = 0;
    qint64 artifactBytes = 0;
    qint64 archivedArtifactCount = 0;
    qint64 archivedArtifactBytes = 0;
};

} // namespace qtllm::toolsinside
