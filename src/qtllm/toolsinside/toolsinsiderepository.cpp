#include "toolsinsiderepository.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

namespace qtllm::toolsinside {

namespace {

QString toJsonText(const QJsonObject &object)
{
    return object.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QString threadConnectionName(const QString &baseConnectionName)
{
    return QStringLiteral("%1_%2").arg(baseConnectionName,
                                      QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16));
}

QJsonObject parseJsonObject(const QVariant &value)
{
    const QJsonDocument doc = QJsonDocument::fromJson(value.toByteArray());
    return doc.isObject() ? doc.object() : QJsonObject();
}

QString isoDate(const QDateTime &dateTime)
{
    return dateTime.isValid() ? dateTime.toUTC().toString(Qt::ISODateWithMs) : QString();
}

QDateTime fromIsoDate(const QVariant &value)
{
    const QDateTime dateTime = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    return dateTime.isValid() ? dateTime : QDateTime();
}

bool execQuery(QSqlQuery &query, QString *errorMessage)
{
    if (query.exec()) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

ToolsInsideTraceSummary traceFromQuery(const QSqlQuery &query)
{
    ToolsInsideTraceSummary trace;
    trace.traceId = query.value(0).toString();
    trace.clientId = query.value(1).toString();
    trace.sessionId = query.value(2).toString();
    trace.rootRequestId = query.value(3).toString();
    trace.status = query.value(4).toString();
    trace.model = query.value(5).toString();
    trace.provider = query.value(6).toString();
    trace.vendor = query.value(7).toString();
    trace.startedAtUtc = fromIsoDate(query.value(8));
    trace.endedAtUtc = fromIsoDate(query.value(9));
    trace.finalAnswerArtifactId = query.value(10).toString();
    trace.turnInputPreview = query.value(11).toString();
    trace.summary = parseJsonObject(query.value(12));
    return trace;
}

ToolsInsideArtifactRef artifactFromQuery(const QSqlQuery &query)
{
    ToolsInsideArtifactRef artifact;
    artifact.artifactId = query.value(0).toString();
    artifact.traceId = query.value(1).toString();
    artifact.clientId = query.value(2).toString();
    artifact.sessionId = query.value(3).toString();
    artifact.kind = query.value(4).toString();
    artifact.mimeType = query.value(5).toString();
    artifact.storageType = query.value(6).toString();
    artifact.relativePath = query.value(7).toString();
    artifact.redactionState = query.value(8).toString();
    artifact.sha256 = query.value(9).toString();
    artifact.sizeBytes = query.value(10).toLongLong();
    artifact.createdAtUtc = fromIsoDate(query.value(11));
    artifact.metadata = parseJsonObject(query.value(12));
    return artifact;
}

} // namespace

ToolsInsideRepository::ToolsInsideRepository(QString databasePath)
    : m_databasePath(std::move(databasePath))
    , m_connectionName(QStringLiteral("toolsinside_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

ToolsInsideRepository::~ToolsInsideRepository()
{
    if (QCoreApplication::instance() == nullptr) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    const QString connectionName = threadConnectionName(m_connectionName);
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase database = QSqlDatabase::database(connectionName);
        if (database.isOpen()) {
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ToolsInsideRepository::setDatabasePath(const QString &databasePath)
{
    QMutexLocker locker(&m_mutex);
    m_databasePath = databasePath.trimmed().isEmpty()
        ? QStringLiteral(".qtllm/tools_inside/index.db")
        : databasePath.trimmed();
    const QString connectionName = threadConnectionName(m_connectionName);
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase database = QSqlDatabase::database(connectionName);
        if (database.isOpen()) {
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }
}

QString ToolsInsideRepository::databasePath() const
{
    return m_databasePath;
}

bool ToolsInsideRepository::ensureInitialized(QString *errorMessage) const
{
    QMutexLocker locker(&m_mutex);
    return ensureConnectionLocked(errorMessage) && runMigrationsLocked(errorMessage);
}

bool ToolsInsideRepository::upsertTrace(const ToolsInsideTraceSummary &trace, QString *errorMessage) const
{
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "INSERT INTO ti_traces (trace_id, client_id, session_id, root_request_id, status, model, provider, vendor, "
        "started_at_utc, ended_at_utc, final_answer_artifact_id, turn_input_preview, summary_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(trace_id) DO UPDATE SET "
        "client_id = excluded.client_id, "
        "session_id = excluded.session_id, "
        "root_request_id = CASE WHEN excluded.root_request_id = '' THEN ti_traces.root_request_id ELSE excluded.root_request_id END, "
        "status = excluded.status, "
        "model = excluded.model, "
        "provider = excluded.provider, "
        "vendor = excluded.vendor, "
        "started_at_utc = excluded.started_at_utc, "
        "ended_at_utc = excluded.ended_at_utc, "
        "final_answer_artifact_id = CASE WHEN excluded.final_answer_artifact_id = '' THEN ti_traces.final_answer_artifact_id ELSE excluded.final_answer_artifact_id END, "
        "turn_input_preview = excluded.turn_input_preview, "
        "summary_json = excluded.summary_json"));
    query.addBindValue(trace.traceId);
    query.addBindValue(trace.clientId);
    query.addBindValue(trace.sessionId);
    query.addBindValue(trace.rootRequestId);
    query.addBindValue(trace.status);
    query.addBindValue(trace.model);
    query.addBindValue(trace.provider);
    query.addBindValue(trace.vendor);
    query.addBindValue(isoDate(trace.startedAtUtc));
    query.addBindValue(isoDate(trace.endedAtUtc));
    query.addBindValue(trace.finalAnswerArtifactId);
    query.addBindValue(trace.turnInputPreview);
    query.addBindValue(toJsonText(trace.summary));
    return execQuery(query, errorMessage);
}

bool ToolsInsideRepository::finishTrace(const QString &traceId,
                                        const QString &status,
                                        const QString &finalAnswerArtifactId,
                                        const QDateTime &endedAtUtc,
                                        QString *errorMessage) const
{
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "UPDATE ti_traces SET status = ?, ended_at_utc = ?, "
        "final_answer_artifact_id = CASE WHEN ? = '' THEN final_answer_artifact_id ELSE ? END "
        "WHERE trace_id = ?"));
    query.addBindValue(status);
    query.addBindValue(isoDate(endedAtUtc));
    query.addBindValue(finalAnswerArtifactId);
    query.addBindValue(finalAnswerArtifactId);
    query.addBindValue(traceId);
    return execQuery(query, errorMessage);
}

bool ToolsInsideRepository::upsertSpan(const ToolsInsideSpanRecord &span, QString *errorMessage) const
{
    ToolsInsideSpanRecord stored = span;
    if (stored.spanId.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ToolsInsideSpanRecord.spanId must not be empty");
        }
        return false;
    }
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "INSERT INTO ti_spans (span_id, trace_id, parent_span_id, request_id, tool_call_id, kind, name, status, error_code, "
        "started_at_utc, ended_at_utc, summary_json) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(span_id) DO UPDATE SET "
        "trace_id = excluded.trace_id, parent_span_id = excluded.parent_span_id, request_id = excluded.request_id, "
        "tool_call_id = excluded.tool_call_id, kind = excluded.kind, name = excluded.name, status = excluded.status, "
        "error_code = excluded.error_code, started_at_utc = excluded.started_at_utc, ended_at_utc = excluded.ended_at_utc, "
        "summary_json = excluded.summary_json"));
    query.addBindValue(stored.spanId);
    query.addBindValue(stored.traceId);
    query.addBindValue(stored.parentSpanId);
    query.addBindValue(stored.requestId);
    query.addBindValue(stored.toolCallId);
    query.addBindValue(stored.kind);
    query.addBindValue(stored.name);
    query.addBindValue(stored.status);
    query.addBindValue(stored.errorCode);
    query.addBindValue(isoDate(stored.startedAtUtc));
    query.addBindValue(isoDate(stored.endedAtUtc));
    query.addBindValue(toJsonText(stored.summary));
    return execQuery(query, errorMessage);
}

bool ToolsInsideRepository::upsertEvent(const ToolsInsideEventRecord &event, QString *errorMessage) const
{
    ToolsInsideEventRecord stored = event;
    if (stored.eventId.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ToolsInsideEventRecord.eventId must not be empty");
        }
        return false;
    }
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "INSERT INTO ti_events (event_id, trace_id, span_id, request_id, tool_call_id, category, name, timestamp_utc, payload_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(event_id) DO UPDATE SET "
        "trace_id = excluded.trace_id, span_id = excluded.span_id, request_id = excluded.request_id, tool_call_id = excluded.tool_call_id, "
        "category = excluded.category, name = excluded.name, timestamp_utc = excluded.timestamp_utc, payload_json = excluded.payload_json"));
    query.addBindValue(stored.eventId);
    query.addBindValue(stored.traceId);
    query.addBindValue(stored.spanId);
    query.addBindValue(stored.requestId);
    query.addBindValue(stored.toolCallId);
    query.addBindValue(stored.category);
    query.addBindValue(stored.name);
    query.addBindValue(isoDate(stored.timestampUtc));
    query.addBindValue(toJsonText(stored.payload));
    return execQuery(query, errorMessage);
}

bool ToolsInsideRepository::upsertArtifact(const ToolsInsideArtifactRef &artifact, QString *errorMessage) const
{
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "INSERT INTO ti_artifacts (artifact_id, trace_id, client_id, session_id, kind, mime_type, storage_type, relative_path, "
        "redaction_state, sha256, size_bytes, created_at_utc, metadata_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(artifact_id) DO UPDATE SET "
        "trace_id = excluded.trace_id, client_id = excluded.client_id, session_id = excluded.session_id, kind = excluded.kind, "
        "mime_type = excluded.mime_type, storage_type = excluded.storage_type, relative_path = excluded.relative_path, "
        "redaction_state = excluded.redaction_state, sha256 = excluded.sha256, size_bytes = excluded.size_bytes, "
        "created_at_utc = excluded.created_at_utc, metadata_json = excluded.metadata_json"));
    query.addBindValue(artifact.artifactId);
    query.addBindValue(artifact.traceId);
    query.addBindValue(artifact.clientId);
    query.addBindValue(artifact.sessionId);
    query.addBindValue(artifact.kind);
    query.addBindValue(artifact.mimeType);
    query.addBindValue(artifact.storageType);
    query.addBindValue(artifact.relativePath);
    query.addBindValue(artifact.redactionState);
    query.addBindValue(artifact.sha256);
    query.addBindValue(artifact.sizeBytes);
    query.addBindValue(isoDate(artifact.createdAtUtc));
    query.addBindValue(toJsonText(artifact.metadata));
    return execQuery(query, errorMessage);
}

bool ToolsInsideRepository::upsertToolCall(const ToolsInsideToolCallRecord &toolCall, QString *errorMessage) const
{
    ToolsInsideToolCallRecord stored = toolCall;
    if (stored.toolCallId.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ToolsInsideToolCallRecord.toolCallId must not be empty");
        }
        return false;
    }
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "INSERT INTO ti_tool_calls (tool_call_id, external_call_id, trace_id, request_id, tool_id, tool_name, tool_category, server_id, invocation_name, "
        "arguments_artifact_id, output_artifact_id, followup_artifact_id, status, error_code, error_message, retryable, round_index, "
        "duration_ms, started_at_utc, ended_at_utc, summary_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(tool_call_id) DO UPDATE SET "
        "external_call_id = excluded.external_call_id, trace_id = excluded.trace_id, request_id = excluded.request_id, tool_id = excluded.tool_id, tool_name = excluded.tool_name, "
        "tool_category = excluded.tool_category, server_id = excluded.server_id, invocation_name = excluded.invocation_name, "
        "arguments_artifact_id = CASE WHEN excluded.arguments_artifact_id = '' THEN ti_tool_calls.arguments_artifact_id ELSE excluded.arguments_artifact_id END, "
        "output_artifact_id = CASE WHEN excluded.output_artifact_id = '' THEN ti_tool_calls.output_artifact_id ELSE excluded.output_artifact_id END, "
        "followup_artifact_id = CASE WHEN excluded.followup_artifact_id = '' THEN ti_tool_calls.followup_artifact_id ELSE excluded.followup_artifact_id END, "
        "status = excluded.status, error_code = excluded.error_code, error_message = excluded.error_message, retryable = excluded.retryable, "
        "round_index = excluded.round_index, duration_ms = excluded.duration_ms, started_at_utc = excluded.started_at_utc, "
        "ended_at_utc = excluded.ended_at_utc, summary_json = excluded.summary_json"));
    query.addBindValue(stored.toolCallId);
    query.addBindValue(stored.externalCallId);
    query.addBindValue(stored.traceId);
    query.addBindValue(stored.requestId);
    query.addBindValue(stored.toolId);
    query.addBindValue(stored.toolName);
    query.addBindValue(stored.toolCategory);
    query.addBindValue(stored.serverId);
    query.addBindValue(stored.invocationName);
    query.addBindValue(stored.argumentsArtifactId);
    query.addBindValue(stored.outputArtifactId);
    query.addBindValue(stored.followupArtifactId);
    query.addBindValue(stored.status);
    query.addBindValue(stored.errorCode);
    query.addBindValue(stored.errorMessage);
    query.addBindValue(stored.retryable ? 1 : 0);
    query.addBindValue(stored.roundIndex);
    query.addBindValue(stored.durationMs);
    query.addBindValue(isoDate(stored.startedAtUtc));
    query.addBindValue(isoDate(stored.endedAtUtc));
    query.addBindValue(toJsonText(stored.summary));
    return execQuery(query, errorMessage);
}

bool ToolsInsideRepository::upsertSupportLink(const ToolsInsideSupportLink &supportLink, QString *errorMessage) const
{
    ToolsInsideSupportLink stored = supportLink;
    if (stored.supportLinkId.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ToolsInsideSupportLink.supportLinkId must not be empty");
        }
        return false;
    }
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "INSERT INTO ti_support_links (support_link_id, trace_id, tool_call_id, target_kind, target_id, support_type, source, "
        "evidence_artifact_id, note, confidence, created_at_utc, metadata_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(support_link_id) DO UPDATE SET "
        "trace_id = excluded.trace_id, tool_call_id = excluded.tool_call_id, target_kind = excluded.target_kind, target_id = excluded.target_id, "
        "support_type = excluded.support_type, source = excluded.source, evidence_artifact_id = excluded.evidence_artifact_id, "
        "note = excluded.note, confidence = excluded.confidence, created_at_utc = excluded.created_at_utc, metadata_json = excluded.metadata_json"));
    query.addBindValue(stored.supportLinkId);
    query.addBindValue(stored.traceId);
    query.addBindValue(stored.toolCallId);
    query.addBindValue(stored.targetKind);
    query.addBindValue(stored.targetId);
    query.addBindValue(stored.supportType);
    query.addBindValue(stored.source);
    query.addBindValue(stored.evidenceArtifactId);
    query.addBindValue(stored.note);
    query.addBindValue(stored.confidence);
    query.addBindValue(isoDate(stored.createdAtUtc));
    query.addBindValue(toJsonText(stored.metadata));
    return execQuery(query, errorMessage);
}


bool ToolsInsideRepository::updateTraceStatus(const QString &traceId,
                                              const QString &status,
                                              QString *errorMessage) const
{
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral("UPDATE ti_traces SET status = ? WHERE trace_id = ?"));
    query.addBindValue(status);
    query.addBindValue(traceId);
    return execQuery(query, errorMessage);
}

bool ToolsInsideRepository::updateArtifactPathPrefix(const QString &traceId,
                                                     const QString &oldPrefix,
                                                     const QString &newPrefix,
                                                     QString *errorMessage) const
{
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return false;
    }

    const QString prefix = oldPrefix.endsWith(QLatin1Char('/')) ? oldPrefix : oldPrefix + QLatin1Char('/');
    const QString replacement = newPrefix.endsWith(QLatin1Char('/')) ? newPrefix : newPrefix + QLatin1Char('/');

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "UPDATE ti_artifacts SET relative_path = ? || substr(relative_path, ?) "
        "WHERE trace_id = ? AND relative_path LIKE ?"));
    query.addBindValue(replacement);
    query.addBindValue(prefix.size() + 1);
    query.addBindValue(traceId);
    query.addBindValue(prefix + QStringLiteral("%"));
    return execQuery(query, errorMessage);
}

bool ToolsInsideRepository::deleteTrace(const QString &traceId, QString *errorMessage) const
{
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return false;
    }

    const QStringList statements = {
        QStringLiteral("DELETE FROM ti_support_links WHERE trace_id = ?"),
        QStringLiteral("DELETE FROM ti_tool_calls WHERE trace_id = ?"),
        QStringLiteral("DELETE FROM ti_events WHERE trace_id = ?"),
        QStringLiteral("DELETE FROM ti_spans WHERE trace_id = ?"),
        QStringLiteral("DELETE FROM ti_artifacts WHERE trace_id = ?"),
        QStringLiteral("DELETE FROM ti_traces WHERE trace_id = ?")
    };

    for (const QString &statement : statements) {
        QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
        query.prepare(statement);
        query.addBindValue(traceId);
        if (!execQuery(query, errorMessage)) {
            return false;
        }
    }

    return true;
}

QList<ToolsInsideClientSummary> ToolsInsideRepository::listClients(QString *errorMessage) const
{
    QList<ToolsInsideClientSummary> clients;
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return clients;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "SELECT client_id, COUNT(DISTINCT session_id), COUNT(*), MAX(started_at_utc) "
        "FROM ti_traces GROUP BY client_id ORDER BY MAX(started_at_utc) DESC"));
    if (!execQuery(query, errorMessage)) {
        return clients;
    }

    while (query.next()) {
        ToolsInsideClientSummary client;
        client.clientId = query.value(0).toString();
        client.sessionCount = query.value(1).toInt();
        client.traceCount = query.value(2).toInt();
        client.lastActivityUtc = fromIsoDate(query.value(3));
        clients.append(client);
    }
    return clients;
}

QList<ToolsInsideSessionSummary> ToolsInsideRepository::listSessions(const QString &clientId,
                                                                     QString *errorMessage) const
{
    QList<ToolsInsideSessionSummary> sessions;
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return sessions;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "SELECT session_id, COUNT(*), MAX(started_at_utc), "
        "(SELECT trace_id FROM ti_traces AS latest WHERE latest.client_id = ti_traces.client_id AND latest.session_id = ti_traces.session_id ORDER BY latest.started_at_utc DESC LIMIT 1) "
        "FROM ti_traces WHERE client_id = ? GROUP BY client_id, session_id ORDER BY MAX(started_at_utc) DESC"));
    query.addBindValue(clientId);
    if (!execQuery(query, errorMessage)) {
        return sessions;
    }

    while (query.next()) {
        ToolsInsideSessionSummary session;
        session.clientId = clientId;
        session.sessionId = query.value(0).toString();
        session.traceCount = query.value(1).toInt();
        session.lastActivityUtc = fromIsoDate(query.value(2));
        session.latestTraceId = query.value(3).toString();
        sessions.append(session);
    }
    return sessions;
}

std::optional<ToolsInsideTraceSummary> ToolsInsideRepository::getTrace(const QString &traceId,
                                                                       QString *errorMessage) const
{
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return std::nullopt;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "SELECT trace_id, client_id, session_id, root_request_id, status, model, provider, vendor, "
        "started_at_utc, ended_at_utc, final_answer_artifact_id, turn_input_preview, summary_json "
        "FROM ti_traces WHERE trace_id = ? LIMIT 1"));
    query.addBindValue(traceId);
    if (!execQuery(query, errorMessage)) {
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }

    return traceFromQuery(query);
}

QList<ToolsInsideArtifactRef> ToolsInsideRepository::listArtifacts(const QString &traceId,
                                                                   QString *errorMessage) const
{
    QList<ToolsInsideArtifactRef> artifacts;
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return artifacts;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "SELECT artifact_id, trace_id, client_id, session_id, kind, mime_type, storage_type, relative_path, "
        "redaction_state, sha256, size_bytes, created_at_utc, metadata_json "
        "FROM ti_artifacts WHERE trace_id = ? ORDER BY created_at_utc ASC"));
    query.addBindValue(traceId);
    if (!execQuery(query, errorMessage)) {
        return artifacts;
    }

    while (query.next()) {
        artifacts.append(artifactFromQuery(query));
    }
    return artifacts;
}

ToolsInsideStorageStats ToolsInsideRepository::getStorageStats(QString *errorMessage) const
{
    ToolsInsideStorageStats stats;
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return stats;
    }

    {
        QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
        query.prepare(QStringLiteral(
            "SELECT COUNT(DISTINCT client_id), COUNT(DISTINCT client_id || '::' || session_id), COUNT(*) FROM ti_traces"));
        if (!execQuery(query, errorMessage)) {
            return stats;
        }
        if (query.next()) {
            stats.clientCount = query.value(0).toInt();
            stats.sessionCount = query.value(1).toInt();
            stats.traceCount = query.value(2).toInt();
        }
    }

    {
        QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
        query.prepare(QStringLiteral(
            "SELECT COUNT(*), COALESCE(SUM(size_bytes), 0), "
            "SUM(CASE WHEN relative_path LIKE 'archive/%' THEN 1 ELSE 0 END), "
            "COALESCE(SUM(CASE WHEN relative_path LIKE 'archive/%' THEN size_bytes ELSE 0 END), 0) "
            "FROM ti_artifacts"));
        if (!execQuery(query, errorMessage)) {
            return stats;
        }
        if (query.next()) {
            stats.artifactCount = query.value(0).toLongLong();
            stats.artifactBytes = query.value(1).toLongLong();
            stats.archivedArtifactCount = query.value(2).toLongLong();
            stats.archivedArtifactBytes = query.value(3).toLongLong();
        }
    }

    return stats;
}

QList<ToolsInsideTraceSummary> ToolsInsideRepository::listTraces(const ToolsInsideTraceFilter &filter,
                                                                 QString *errorMessage) const
{
    QList<ToolsInsideTraceSummary> traces;
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return traces;
    }

    QString sql = QStringLiteral(
        "SELECT trace_id, client_id, session_id, root_request_id, status, model, provider, vendor, "
        "started_at_utc, ended_at_utc, final_answer_artifact_id, turn_input_preview, summary_json "
        "FROM ti_traces WHERE 1 = 1");
    if (!filter.clientId.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND client_id = ?");
    }
    if (!filter.sessionId.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND session_id = ?");
    }
    sql += QStringLiteral(" ORDER BY started_at_utc DESC LIMIT ?");

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(sql);
    if (!filter.clientId.trimmed().isEmpty()) {
        query.addBindValue(filter.clientId.trimmed());
    }
    if (!filter.sessionId.trimmed().isEmpty()) {
        query.addBindValue(filter.sessionId.trimmed());
    }
    query.addBindValue(filter.limit > 0 ? filter.limit : 100);
    if (!execQuery(query, errorMessage)) {
        return traces;
    }

    while (query.next()) {
        ToolsInsideTraceSummary trace;
        trace.traceId = query.value(0).toString();
        trace.clientId = query.value(1).toString();
        trace.sessionId = query.value(2).toString();
        trace.rootRequestId = query.value(3).toString();
        trace.status = query.value(4).toString();
        trace.model = query.value(5).toString();
        trace.provider = query.value(6).toString();
        trace.vendor = query.value(7).toString();
        trace.startedAtUtc = fromIsoDate(query.value(8));
        trace.endedAtUtc = fromIsoDate(query.value(9));
        trace.finalAnswerArtifactId = query.value(10).toString();
        trace.turnInputPreview = query.value(11).toString();
        trace.summary = parseJsonObject(query.value(12));
        traces.append(trace);
    }
    return traces;
}

QList<ToolsInsideEventRecord> ToolsInsideRepository::listEvents(const QString &traceId, QString *errorMessage) const
{
    QList<ToolsInsideEventRecord> events;
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return events;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "SELECT event_id, trace_id, span_id, request_id, tool_call_id, category, name, timestamp_utc, payload_json "
        "FROM ti_events WHERE trace_id = ? ORDER BY timestamp_utc ASC"));
    query.addBindValue(traceId);
    if (!execQuery(query, errorMessage)) {
        return events;
    }

    while (query.next()) {
        ToolsInsideEventRecord event;
        event.eventId = query.value(0).toString();
        event.traceId = query.value(1).toString();
        event.spanId = query.value(2).toString();
        event.requestId = query.value(3).toString();
        event.toolCallId = query.value(4).toString();
        event.category = query.value(5).toString();
        event.name = query.value(6).toString();
        event.timestampUtc = fromIsoDate(query.value(7));
        event.payload = parseJsonObject(query.value(8));
        events.append(event);
    }
    return events;
}

QList<ToolsInsideSpanRecord> ToolsInsideRepository::listSpans(const QString &traceId, QString *errorMessage) const
{
    QList<ToolsInsideSpanRecord> spans;
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return spans;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "SELECT span_id, trace_id, parent_span_id, request_id, tool_call_id, kind, name, status, error_code, started_at_utc, ended_at_utc, summary_json "
        "FROM ti_spans WHERE trace_id = ? ORDER BY started_at_utc ASC"));
    query.addBindValue(traceId);
    if (!execQuery(query, errorMessage)) {
        return spans;
    }

    while (query.next()) {
        ToolsInsideSpanRecord span;
        span.spanId = query.value(0).toString();
        span.traceId = query.value(1).toString();
        span.parentSpanId = query.value(2).toString();
        span.requestId = query.value(3).toString();
        span.toolCallId = query.value(4).toString();
        span.kind = query.value(5).toString();
        span.name = query.value(6).toString();
        span.status = query.value(7).toString();
        span.errorCode = query.value(8).toString();
        span.startedAtUtc = fromIsoDate(query.value(9));
        span.endedAtUtc = fromIsoDate(query.value(10));
        span.summary = parseJsonObject(query.value(11));
        spans.append(span);
    }
    return spans;
}

QList<ToolsInsideToolCallRecord> ToolsInsideRepository::listToolCalls(const QString &traceId, QString *errorMessage) const
{
    QList<ToolsInsideToolCallRecord> toolCalls;
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return toolCalls;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "SELECT tool_call_id, external_call_id, trace_id, request_id, tool_id, tool_name, tool_category, server_id, invocation_name, "
        "arguments_artifact_id, output_artifact_id, followup_artifact_id, status, error_code, error_message, retryable, round_index, "
        "duration_ms, started_at_utc, ended_at_utc, summary_json FROM ti_tool_calls WHERE trace_id = ? ORDER BY started_at_utc ASC"));
    query.addBindValue(traceId);
    if (!execQuery(query, errorMessage)) {
        return toolCalls;
    }

    while (query.next()) {
        ToolsInsideToolCallRecord record;
        record.toolCallId = query.value(0).toString();
        record.externalCallId = query.value(1).toString();
        record.traceId = query.value(2).toString();
        record.requestId = query.value(3).toString();
        record.toolId = query.value(4).toString();
        record.toolName = query.value(5).toString();
        record.toolCategory = query.value(6).toString();
        record.serverId = query.value(7).toString();
        record.invocationName = query.value(8).toString();
        record.argumentsArtifactId = query.value(9).toString();
        record.outputArtifactId = query.value(10).toString();
        record.followupArtifactId = query.value(11).toString();
        record.status = query.value(12).toString();
        record.errorCode = query.value(13).toString();
        record.errorMessage = query.value(14).toString();
        record.retryable = query.value(15).toInt() != 0;
        record.roundIndex = query.value(16).toInt();
        record.durationMs = query.value(17).toLongLong();
        record.startedAtUtc = fromIsoDate(query.value(18));
        record.endedAtUtc = fromIsoDate(query.value(19));
        record.summary = parseJsonObject(query.value(20));
        toolCalls.append(record);
    }
    return toolCalls;
}

QList<ToolsInsideSupportLink> ToolsInsideRepository::listSupportLinks(const QString &traceId, QString *errorMessage) const
{
    QList<ToolsInsideSupportLink> links;
    QMutexLocker locker(&m_mutex);
    if (!ensureConnectionLocked(errorMessage) || !runMigrationsLocked(errorMessage)) {
        return links;
    }

    QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    query.prepare(QStringLiteral(
        "SELECT support_link_id, trace_id, tool_call_id, target_kind, target_id, support_type, source, evidence_artifact_id, note, confidence, created_at_utc, metadata_json "
        "FROM ti_support_links WHERE trace_id = ? ORDER BY created_at_utc ASC"));
    query.addBindValue(traceId);
    if (!execQuery(query, errorMessage)) {
        return links;
    }

    while (query.next()) {
        ToolsInsideSupportLink link;
        link.supportLinkId = query.value(0).toString();
        link.traceId = query.value(1).toString();
        link.toolCallId = query.value(2).toString();
        link.targetKind = query.value(3).toString();
        link.targetId = query.value(4).toString();
        link.supportType = query.value(5).toString();
        link.source = query.value(6).toString();
        link.evidenceArtifactId = query.value(7).toString();
        link.note = query.value(8).toString();
        link.confidence = query.value(9).toDouble();
        link.createdAtUtc = fromIsoDate(query.value(10));
        link.metadata = parseJsonObject(query.value(11));
        links.append(link);
    }
    return links;
}

bool ToolsInsideRepository::ensureConnectionLocked(QString *errorMessage) const
{
    if (!QSqlDatabase::contains(threadConnectionName(m_connectionName))) {
        QDir dir(QFileInfo(m_databasePath).absolutePath());
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to create tools_inside database directory: ") + dir.path();
            }
            return false;
        }

        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), threadConnectionName(m_connectionName));
        database.setDatabaseName(m_databasePath);
    }

    QSqlDatabase database = QSqlDatabase::database(threadConnectionName(m_connectionName));
    if (database.isOpen()) {
        return true;
    }

    if (database.open()) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = database.lastError().text();
    }
    return false;
}

bool ToolsInsideRepository::runMigrationsLocked(QString *errorMessage) const
{
    constexpr int kSchemaVersion = 2;

    {
        QSqlQuery versionQuery(QSqlDatabase::database(threadConnectionName(m_connectionName)));
        if (!versionQuery.exec(QStringLiteral("PRAGMA user_version"))) {
            if (errorMessage) {
                *errorMessage = versionQuery.lastError().text();
            }
            return false;
        }

        int currentVersion = 0;
        if (versionQuery.next()) {
            currentVersion = versionQuery.value(0).toInt();
        }

        if (currentVersion != 0 && currentVersion != kSchemaVersion) {
            const QStringList resetStatements = {
                QStringLiteral("DROP TABLE IF EXISTS ti_support_links"),
                QStringLiteral("DROP TABLE IF EXISTS ti_tool_calls"),
                QStringLiteral("DROP TABLE IF EXISTS ti_events"),
                QStringLiteral("DROP TABLE IF EXISTS ti_spans"),
                QStringLiteral("DROP TABLE IF EXISTS ti_artifacts"),
                QStringLiteral("DROP TABLE IF EXISTS ti_traces")
            };

            for (const QString &statement : resetStatements) {
                QSqlQuery resetQuery(QSqlDatabase::database(threadConnectionName(m_connectionName)));
                if (!resetQuery.exec(statement)) {
                    if (errorMessage) {
                        *errorMessage = resetQuery.lastError().text();
                    }
                    return false;
                }
            }
        }
    }

    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS ti_traces (trace_id TEXT PRIMARY KEY, client_id TEXT NOT NULL, session_id TEXT NOT NULL, root_request_id TEXT, status TEXT, model TEXT, provider TEXT, vendor TEXT, started_at_utc TEXT, ended_at_utc TEXT, final_answer_artifact_id TEXT, turn_input_preview TEXT, summary_json TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS ti_spans (span_id TEXT PRIMARY KEY, trace_id TEXT NOT NULL, parent_span_id TEXT, request_id TEXT, tool_call_id TEXT, kind TEXT NOT NULL, name TEXT NOT NULL, status TEXT, error_code TEXT, started_at_utc TEXT, ended_at_utc TEXT, summary_json TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS ti_events (event_id TEXT PRIMARY KEY, trace_id TEXT NOT NULL, span_id TEXT, request_id TEXT, tool_call_id TEXT, category TEXT NOT NULL, name TEXT NOT NULL, timestamp_utc TEXT NOT NULL, payload_json TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS ti_artifacts (artifact_id TEXT PRIMARY KEY, trace_id TEXT NOT NULL, client_id TEXT NOT NULL, session_id TEXT NOT NULL, kind TEXT NOT NULL, mime_type TEXT, storage_type TEXT, relative_path TEXT NOT NULL, redaction_state TEXT, sha256 TEXT, size_bytes INTEGER, created_at_utc TEXT, metadata_json TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS ti_tool_calls (tool_call_id TEXT PRIMARY KEY, external_call_id TEXT, trace_id TEXT NOT NULL, request_id TEXT, tool_id TEXT, tool_name TEXT, tool_category TEXT, server_id TEXT, invocation_name TEXT, arguments_artifact_id TEXT, output_artifact_id TEXT, followup_artifact_id TEXT, status TEXT, error_code TEXT, error_message TEXT, retryable INTEGER, round_index INTEGER, duration_ms INTEGER, started_at_utc TEXT, ended_at_utc TEXT, summary_json TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS ti_support_links (support_link_id TEXT PRIMARY KEY, trace_id TEXT NOT NULL, tool_call_id TEXT NOT NULL, target_kind TEXT NOT NULL, target_id TEXT NOT NULL, support_type TEXT NOT NULL, source TEXT, evidence_artifact_id TEXT, note TEXT, confidence REAL, created_at_utc TEXT, metadata_json TEXT)")
    };

    for (const QString &statement : statements) {
        QSqlQuery query(QSqlDatabase::database(threadConnectionName(m_connectionName)));
        if (!query.exec(statement)) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            return false;
        }
    }

    QSqlQuery versionUpdate(QSqlDatabase::database(threadConnectionName(m_connectionName)));
    if (!versionUpdate.exec(QStringLiteral("PRAGMA user_version = 2"))) {
        if (errorMessage) {
            *errorMessage = versionUpdate.lastError().text();
        }
        return false;
    }
    return true;
}

} // namespace qtllm::toolsinside
