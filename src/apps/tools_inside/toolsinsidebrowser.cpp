#include "toolsinsidebrowser.h"

#include "../../qtllm/toolsinside/toolsinsideadminservice.h"
#include "../../qtllm/toolsinside/toolsinsideartifactstore.h"
#include "../../qtllm/toolsinside/toolsinsidei18n.h"
#include "../../qtllm/toolsinside/toolsinsidequeryservice.h"
#include "../../qtllm/toolsinside/toolsinsideruntime.h"
#include "../../qtllm/identity/compactid.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QSettings>

#include <algorithm>
#include <limits>
#include <optional>

using namespace qtllm::toolsinside;

namespace {

QString toIso(const QDateTime &dateTime)
{
    return dateTime.isValid() ? dateTime.toUTC().toString(Qt::ISODateWithMs) : QString();
}

QString jsonText(const QJsonObject &object)
{
    return object.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QString prettyJsonText(const QByteArray &bytes)
{
    if (bytes.trimmed().isEmpty()) {
        return QString();
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || document.isNull()) {
        return QString();
    }
    return QString::fromUtf8(document.toJson(QJsonDocument::Indented));
}

QString joinSections(const QStringList &sections)
{
    QStringList filtered;
    for (const QString &section : sections) {
        if (!section.trimmed().isEmpty()) {
            filtered.append(section.trimmed());
        }
    }
    return filtered.join(QStringLiteral("\n\n"));
}

QString shortId(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (qtllm::identity::isValidId(trimmed)) {
        return trimmed;
    }
    if (trimmed.size() <= 12) {
        return trimmed;
    }
    return trimmed.left(8) + QStringLiteral("...") + trimmed.right(4);
}

qint64 relativeMs(const QDateTime &baseUtc, const QDateTime &valueUtc)
{
    if (!baseUtc.isValid() || !valueUtc.isValid()) {
        return 0;
    }
    return std::max<qint64>(0, baseUtc.msecsTo(valueUtc));
}

QString colorFor(const QString &entryType, const QString &status, const QString &laneKind)
{
    const QString normalizedStatus = status.trimmed().toLower();
    if (normalizedStatus == QStringLiteral("failed") || normalizedStatus == QStringLiteral("error")) {
        return QStringLiteral("#c65c4c");
    }
    if (normalizedStatus == QStringLiteral("archived")) {
        return QStringLiteral("#7c7f86");
    }
    if (normalizedStatus == QStringLiteral("completed") || normalizedStatus == QStringLiteral("success")) {
        return QStringLiteral("#3d7f5c");
    }
    if (laneKind == QStringLiteral("request")) {
        return entryType == QStringLiteral("span") ? QStringLiteral("#b67632") : QStringLiteral("#d59a45");
    }
    if (laneKind == QStringLiteral("batch")) {
        return entryType == QStringLiteral("span") ? QStringLiteral("#557ea0") : QStringLiteral("#7aa2c4");
    }
    if (laneKind == QStringLiteral("tool")) {
        return entryType == QStringLiteral("span") ? QStringLiteral("#2f7d73") : QStringLiteral("#54a79b");
    }
    return entryType == QStringLiteral("span") ? QStringLiteral("#7d6b57") : QStringLiteral("#8d7e70");
}

QVariantMap toMap(const ToolsInsideClientSummary &client)
{
    return {
        {QStringLiteral("clientId"), client.clientId},
        {QStringLiteral("sessionCount"), client.sessionCount},
        {QStringLiteral("traceCount"), client.traceCount},
        {QStringLiteral("lastActivityUtc"), toIso(client.lastActivityUtc)}
    };
}

QVariantMap toMap(const ToolsInsideSessionSummary &session)
{
    return {
        {QStringLiteral("clientId"), session.clientId},
        {QStringLiteral("sessionId"), session.sessionId},
        {QStringLiteral("latestTraceId"), session.latestTraceId},
        {QStringLiteral("traceCount"), session.traceCount},
        {QStringLiteral("lastActivityUtc"), toIso(session.lastActivityUtc)}
    };
}

QVariantMap toMap(const ToolsInsideTraceSummary &trace)
{
    return {
        {QStringLiteral("traceId"), trace.traceId},
        {QStringLiteral("clientId"), trace.clientId},
        {QStringLiteral("sessionId"), trace.sessionId},
        {QStringLiteral("rootRequestId"), trace.rootRequestId},
        {QStringLiteral("status"), trace.status},
        {QStringLiteral("model"), trace.model},
        {QStringLiteral("provider"), trace.provider},
        {QStringLiteral("vendor"), trace.vendor},
        {QStringLiteral("finalAnswerArtifactId"), trace.finalAnswerArtifactId},
        {QStringLiteral("turnInputPreview"), trace.turnInputPreview},
        {QStringLiteral("startedAtUtc"), toIso(trace.startedAtUtc)},
        {QStringLiteral("endedAtUtc"), toIso(trace.endedAtUtc)},
        {QStringLiteral("summaryJson"), jsonText(trace.summary)}
    };
}

QVariantMap toMap(const ToolsInsideArtifactRef &artifact)
{
    return {
        {QStringLiteral("artifactId"), artifact.artifactId},
        {QStringLiteral("kind"), artifact.kind},
        {QStringLiteral("mimeType"), artifact.mimeType},
        {QStringLiteral("relativePath"), artifact.relativePath},
        {QStringLiteral("redactionState"), artifact.redactionState},
        {QStringLiteral("sizeBytes"), artifact.sizeBytes},
        {QStringLiteral("createdAtUtc"), toIso(artifact.createdAtUtc)},
        {QStringLiteral("metadataJson"), jsonText(artifact.metadata)}
    };
}

QVariantMap toMap(const ToolsInsideToolCallRecord &toolCall)
{
    return {
        {QStringLiteral("toolCallId"), toolCall.toolCallId},
        {QStringLiteral("externalCallId"), toolCall.externalCallId},
        {QStringLiteral("requestId"), toolCall.requestId},
        {QStringLiteral("toolId"), toolCall.toolId},
        {QStringLiteral("toolName"), toolCall.toolName},
        {QStringLiteral("toolCategory"), toolCall.toolCategory},
        {QStringLiteral("serverId"), toolCall.serverId},
        {QStringLiteral("invocationName"), toolCall.invocationName},
        {QStringLiteral("argumentsArtifactId"), toolCall.argumentsArtifactId},
        {QStringLiteral("outputArtifactId"), toolCall.outputArtifactId},
        {QStringLiteral("followupArtifactId"), toolCall.followupArtifactId},
        {QStringLiteral("status"), toolCall.status},
        {QStringLiteral("errorCode"), toolCall.errorCode},
        {QStringLiteral("errorMessage"), toolCall.errorMessage},
        {QStringLiteral("retryable"), toolCall.retryable},
        {QStringLiteral("roundIndex"), toolCall.roundIndex},
        {QStringLiteral("durationMs"), toolCall.durationMs},
        {QStringLiteral("startedAtUtc"), toIso(toolCall.startedAtUtc)},
        {QStringLiteral("endedAtUtc"), toIso(toolCall.endedAtUtc)},
        {QStringLiteral("summaryJson"), jsonText(toolCall.summary)}
    };
}

QVariantMap toMap(const ToolsInsideSupportLink &link)
{
    return {
        {QStringLiteral("supportLinkId"), link.supportLinkId},
        {QStringLiteral("toolCallId"), link.toolCallId},
        {QStringLiteral("targetKind"), link.targetKind},
        {QStringLiteral("targetId"), link.targetId},
        {QStringLiteral("supportType"), link.supportType},
        {QStringLiteral("source"), link.source},
        {QStringLiteral("evidenceArtifactId"), link.evidenceArtifactId},
        {QStringLiteral("note"), link.note},
        {QStringLiteral("confidence"), link.confidence},
        {QStringLiteral("createdAtUtc"), toIso(link.createdAtUtc)},
        {QStringLiteral("metadataJson"), jsonText(link.metadata)}
    };
}

struct TimelineLaneBuild
{
    QString laneId;
    QString laneKind;
    QString title;
    QString subtitle;
    int laneOrder = 0;
    int rowCount = 1;
    qint64 firstStartMs = std::numeric_limits<qint64>::max();
    QVariantList entries;
};

void assignLaneRows(TimelineLaneBuild &lane)
{
    constexpr qint64 kEventVisualSpanMs = 6;
    QList<qint64> rowEndMs;
    int rowCount = 0;

    for (QVariant &item : lane.entries) {
        QVariantMap entry = item.toMap();
        const qint64 startMs = entry.value(QStringLiteral("startMs")).toLongLong();
        qint64 endMs = entry.value(QStringLiteral("endMs")).toLongLong();
        if (entry.value(QStringLiteral("entryType")).toString() == QStringLiteral("event")) {
            endMs = std::max(endMs, startMs + kEventVisualSpanMs);
        } else if (endMs <= startMs) {
            endMs = startMs + 1;
        }

        int assignedRow = -1;
        for (int row = 0; row < rowEndMs.size(); ++row) {
            if (startMs >= rowEndMs[row]) {
                assignedRow = row;
                rowEndMs[row] = endMs;
                break;
            }
        }

        if (assignedRow < 0) {
            assignedRow = rowEndMs.size();
            rowEndMs.append(endMs);
        }

        rowCount = std::max(rowCount, assignedRow + 1);
        entry.insert(QStringLiteral("stackIndex"), assignedRow);
        item = entry;
    }

    lane.rowCount = std::max(1, rowCount);
}

void appendLaneEntry(QHash<QString, TimelineLaneBuild> &laneById,
                     QList<QString> &laneIds,
                     const QString &laneId,
                     const QString &laneKind,
                     const QString &title,
                     const QString &subtitle,
                     int laneOrder,
                     const QVariantMap &entry)
{
    if (!laneById.contains(laneId)) {
        TimelineLaneBuild lane;
        lane.laneId = laneId;
        lane.laneKind = laneKind;
        lane.title = title;
        lane.subtitle = subtitle;
        lane.laneOrder = laneOrder;
        laneById.insert(laneId, lane);
        laneIds.append(laneId);
    }

    TimelineLaneBuild &lane = laneById[laneId];
    lane.entries.append(entry);
    lane.firstStartMs = std::min<qint64>(lane.firstStartMs, entry.value(QStringLiteral("startMs")).toLongLong());
    if (lane.subtitle.isEmpty() && !subtitle.isEmpty()) {
        lane.subtitle = subtitle;
    }
}

QString defaultWorkspaceRoot()
{
    return QDir::currentPath();
}

} // namespace

ToolsInsideBrowser::ToolsInsideBrowser(QObject *parent)
    : QObject(parent)
    , m_workspaceRoot(defaultWorkspaceRoot())
{
    loadWorkspacePreferences();
    reload();
}

QString ToolsInsideBrowser::workspaceRoot() const { return m_workspaceRoot; }
QStringList ToolsInsideBrowser::workspaceHistory() const { return m_workspaceHistory; }
QStringList ToolsInsideBrowser::workspaceFavorites() const { return m_workspaceFavorites; }
QVariantList ToolsInsideBrowser::clients() const { return m_clients; }
QVariantList ToolsInsideBrowser::sessions() const { return m_sessions; }
QVariantList ToolsInsideBrowser::traces() const { return m_traces; }
QVariantList ToolsInsideBrowser::timeline() const { return m_timeline; }
QVariantList ToolsInsideBrowser::timelineLanes() const { return m_timelineLanes; }
qint64 ToolsInsideBrowser::timelineDurationMs() const { return m_timelineDurationMs; }
int ToolsInsideBrowser::timelineTickMs() const { return kTimelineTickMs; }
QVariantList ToolsInsideBrowser::toolCalls() const { return m_toolCalls; }
QVariantList ToolsInsideBrowser::supportLinks() const { return m_supportLinks; }
QVariantList ToolsInsideBrowser::artifacts() const { return m_artifacts; }
QString ToolsInsideBrowser::selectedClientId() const { return m_selectedClientId; }
QString ToolsInsideBrowser::selectedSessionId() const { return m_selectedSessionId; }
QString ToolsInsideBrowser::selectedTraceId() const { return m_selectedTraceId; }
QVariantMap ToolsInsideBrowser::traceSummary() const { return m_traceSummary; }
QVariantMap ToolsInsideBrowser::inspector() const { return m_inspector; }
QString ToolsInsideBrowser::statusText() const { return m_statusText; }

void ToolsInsideBrowser::setWorkspaceRoot(const QString &workspaceRoot)
{
    const QString resolved = workspaceRoot.trimmed().isEmpty() ? defaultWorkspaceRoot() : workspaceRoot.trimmed();
    const bool changed = resolved != m_workspaceRoot;
    if (changed) {
        m_workspaceRoot = resolved;
        emit workspaceRootChanged();
    }
    rememberWorkspaceRoot(resolved);
    reload();
}

void ToolsInsideBrowser::reload()
{
    ensureRuntimeConfigured();
    m_selectedClientId.clear();
    m_selectedSessionId.clear();
    m_selectedTraceId.clear();
    emit selectedClientIdChanged();
    emit selectedSessionIdChanged();
    emit selectedTraceIdChanged();
    resetTraceDetail();
    reloadClients();
    reloadSessions();
    reloadTraces();
}

void ToolsInsideBrowser::chooseWorkspaceRoot()
{
    const QString directory = QFileDialog::getExistingDirectory(nullptr,
                                                                ti18n(u"Select tools_inside Workspace", u"选择 tools_inside 工作区"),
                                                                m_workspaceRoot);
    if (!directory.trimmed().isEmpty()) {
        setWorkspaceRoot(directory);
    }
}

void ToolsInsideBrowser::selectWorkspaceHistory(const QString &workspaceRoot)
{
    if (!workspaceRoot.trimmed().isEmpty()) {
        setWorkspaceRoot(workspaceRoot);
    }
}

void ToolsInsideBrowser::toggleWorkspaceFavorite(const QString &workspaceRoot)
{
    const QString normalized = normalizeWorkspaceRoot(workspaceRoot);
    if (normalized.isEmpty()) {
        return;
    }

    QStringList favorites = m_workspaceFavorites;
    if (favorites.contains(normalized)) {
        favorites.removeAll(normalized);
        setStatusText(ti18n(u"Workspace removed from favorites", u"已从常用目录中移除工作区"));
    } else {
        favorites.prepend(normalized);
        while (favorites.size() > kMaxWorkspaceFavorites) {
            favorites.removeLast();
        }
        setStatusText(ti18n(u"Workspace added to favorites", u"已将工作区加入常用目录"));
    }
    favorites.removeDuplicates();

    if (favorites != m_workspaceFavorites) {
        m_workspaceFavorites = favorites;
        emit workspaceFavoritesChanged();
        persistWorkspacePreferences();
    }
}

void ToolsInsideBrowser::removeWorkspaceFavorite(const QString &workspaceRoot)
{
    const QString normalized = normalizeWorkspaceRoot(workspaceRoot);
    if (normalized.isEmpty()) {
        return;
    }

    QStringList favorites = m_workspaceFavorites;
    const int removed = favorites.removeAll(normalized);
    if (removed > 0) {
        m_workspaceFavorites = favorites;
        emit workspaceFavoritesChanged();
        persistWorkspacePreferences();
        setStatusText(ti18n(u"Favorite removed", u"已移除常用目录"));
    }
}

void ToolsInsideBrowser::clearWorkspaceHistory()
{
    m_workspaceHistory = QStringList{m_workspaceRoot};
    emit workspaceHistoryChanged();
    persistWorkspacePreferences();
    setStatusText(ti18n(u"Workspace history cleared", u"最近目录已清空"));
}

bool ToolsInsideBrowser::isFavoriteWorkspace(const QString &workspaceRoot) const
{
    const QString normalized = normalizeWorkspaceRoot(workspaceRoot);
    return !normalized.isEmpty() && m_workspaceFavorites.contains(normalized);
}

void ToolsInsideBrowser::selectClient(const QString &clientId)
{
    if (clientId == m_selectedClientId) {
        return;
    }
    m_selectedClientId = clientId;
    m_selectedSessionId.clear();
    m_selectedTraceId.clear();
    emit selectedClientIdChanged();
    emit selectedSessionIdChanged();
    emit selectedTraceIdChanged();
    resetTraceDetail();
    reloadSessions();
    reloadTraces();
}

void ToolsInsideBrowser::selectSession(const QString &sessionId)
{
    if (sessionId == m_selectedSessionId) {
        return;
    }
    m_selectedSessionId = sessionId;
    m_selectedTraceId.clear();
    emit selectedSessionIdChanged();
    emit selectedTraceIdChanged();
    resetTraceDetail();
    reloadTraces();
}

void ToolsInsideBrowser::selectTrace(const QString &traceId)
{
    if (traceId == m_selectedTraceId) {
        return;
    }
    m_selectedTraceId = traceId;
    emit selectedTraceIdChanged();
    reloadTraceDetail();
}

bool ToolsInsideBrowser::archiveSelectedTrace()
{
    if (m_selectedTraceId.isEmpty()) {
        setStatusText(ti18n(u"No trace selected", u"未选择 Trace"));
        return false;
    }

    ensureRuntimeConfigured();
    QString errorMessage;
    const bool ok = ToolsInsideRuntime::instance().adminService()->archiveTrace(m_selectedTraceId, &errorMessage);
    setStatusText(ok ? ti18n(u"Trace archived", u"Trace 已归档") : errorMessage);
    if (ok) {
        reloadTraces();
        reloadTraceDetail();
    }
    return ok;
}

bool ToolsInsideBrowser::purgeSelectedTrace()
{
    if (m_selectedTraceId.isEmpty()) {
        setStatusText(ti18n(u"No trace selected", u"未选择 Trace"));
        return false;
    }

    ensureRuntimeConfigured();
    const QString traceId = m_selectedTraceId;
    QString errorMessage;
    const bool ok = ToolsInsideRuntime::instance().adminService()->purgeTrace(traceId, &errorMessage);
    setStatusText(ok ? ti18n(u"Trace purged", u"Trace 已清除") : errorMessage);
    if (ok) {
        m_selectedTraceId.clear();
        emit selectedTraceIdChanged();
        resetTraceDetail();
        reloadTraces();
    }
    return ok;
}


void ToolsInsideBrowser::inspectTimelineEntry(const QVariantMap &entry)
{
    QVariantMap inspector;
    inspector.insert(QStringLiteral("title"), entry.value(QStringLiteral("label")).toString());
    inspector.insert(QStringLiteral("kind"), QStringLiteral("timeline-entry"));
    inspector.insert(QStringLiteral("summary"), ti18n(u"Lane: %1\nStart: T+%2ms\nEnd: T+%3ms\nStatus: %4",
                                                      u"泳道：%1\n开始：T+%2ms\n结束：T+%3ms\n状态：%4")
                     .arg(entry.value(QStringLiteral("laneId")).toString(),
                          QString::number(entry.value(QStringLiteral("startMs")).toLongLong()),
                          QString::number(entry.value(QStringLiteral("endMs")).toLongLong()),
                          entry.value(QStringLiteral("status")).toString()));
    QStringList sections;
    sections.append(entry.value(QStringLiteral("detail")).toString());
    const QString toolCallId = entry.value(QStringLiteral("toolCallId")).toString();
    if (!toolCallId.isEmpty()) {
        const QVariantMap toolCall = findToolCallById(toolCallId);
        if (!toolCall.isEmpty()) {
            sections.append(ti18n(u"Related Tool Call\n%1", u"关联工具调用\n%1").arg(toolCall.value(QStringLiteral("summaryJson")).toString()));
        }
    }
    inspector.insert(QStringLiteral("content"), joinSections(sections));
    inspector.insert(QStringLiteral("sourceId"), entry.value(QStringLiteral("laneId")).toString());
    inspector.insert(QStringLiteral("sourceType"), QStringLiteral("timeline"));
    setInspector(inspector);
}

void ToolsInsideBrowser::inspectToolCall(const QVariantMap &toolCall)
{
    QVariantMap inspector;
    inspector.insert(QStringLiteral("title"), toolCall.value(QStringLiteral("toolId")).toString());
    inspector.insert(QStringLiteral("kind"), QStringLiteral("tool-call"));
    inspector.insert(QStringLiteral("summary"), ti18n(u"Call: %1\nStatus: %2\nRound: %3\nDuration: %4 ms\nRequest: %5",
                                                      u"调用：%1\n状态：%2\n轮次：%3\n耗时：%4 ms\n请求：%5")
                     .arg(toolCall.value(QStringLiteral("toolCallId")).toString(),
                          toolCall.value(QStringLiteral("status")).toString(),
                          QString::number(toolCall.value(QStringLiteral("roundIndex")).toInt()),
                          QString::number(toolCall.value(QStringLiteral("durationMs")).toLongLong()),
                          toolCall.value(QStringLiteral("requestId")).toString()));
    QStringList sections;
    sections.append(ti18n(u"Tool Summary\n%1", u"工具摘要\n%1").arg(toolCall.value(QStringLiteral("summaryJson")).toString()));
    const QVariantMap argsArtifact = findArtifactById(toolCall.value(QStringLiteral("argumentsArtifactId")).toString());
    const QVariantMap outputArtifact = findArtifactById(toolCall.value(QStringLiteral("outputArtifactId")).toString());
    const QVariantMap followupArtifact = findArtifactById(toolCall.value(QStringLiteral("followupArtifactId")).toString());
    if (!argsArtifact.isEmpty()) {
        sections.append(ti18n(u"Arguments Artifact\n%1", u"参数工件\n%1").arg(loadArtifactContent(argsArtifact)));
    }
    if (!outputArtifact.isEmpty()) {
        sections.append(ti18n(u"Output Artifact\n%1", u"输出工件\n%1").arg(loadArtifactContent(outputArtifact)));
    }
    if (!followupArtifact.isEmpty()) {
        sections.append(ti18n(u"Follow-up Prompt Artifact\n%1", u"后续提示词工件\n%1").arg(loadArtifactContent(followupArtifact)));
    }
    inspector.insert(QStringLiteral("content"), joinSections(sections));
    inspector.insert(QStringLiteral("sourceId"), toolCall.value(QStringLiteral("toolCallId")).toString());
    inspector.insert(QStringLiteral("sourceType"), QStringLiteral("tool-call"));
    setInspector(inspector);
}

void ToolsInsideBrowser::inspectSupportLink(const QVariantMap &supportLink)
{
    QVariantMap inspector;
    inspector.insert(QStringLiteral("title"), supportLink.value(QStringLiteral("supportType")).toString());
    inspector.insert(QStringLiteral("kind"), QStringLiteral("support-link"));
    inspector.insert(QStringLiteral("summary"), ti18n(u"Tool Call: %1\nTarget: %2\nSource: %3\nConfidence: %4",
                                                      u"工具调用：%1\n目标：%2\n来源：%3\n置信度：%4")
                     .arg(supportLink.value(QStringLiteral("toolCallId")).toString(),
                          supportLink.value(QStringLiteral("targetId")).toString(),
                          supportLink.value(QStringLiteral("source")).toString(),
                          QString::number(supportLink.value(QStringLiteral("confidence")).toDouble(), 'f', 2)));
    QStringList sections;
    sections.append(ti18n(u"Support Metadata\n%1", u"支撑元数据\n%1").arg(supportLink.value(QStringLiteral("metadataJson")).toString()));
    const QVariantMap evidenceArtifact = findArtifactById(supportLink.value(QStringLiteral("evidenceArtifactId")).toString());
    if (!evidenceArtifact.isEmpty()) {
        sections.append(ti18n(u"Evidence Artifact\n%1", u"证据工件\n%1").arg(loadArtifactContent(evidenceArtifact)));
    }
    const QVariantMap toolCall = findToolCallById(supportLink.value(QStringLiteral("toolCallId")).toString());
    if (!toolCall.isEmpty()) {
        sections.append(ti18n(u"Related Tool Call\n%1", u"关联工具调用\n%1").arg(toolCall.value(QStringLiteral("summaryJson")).toString()));
    }
    inspector.insert(QStringLiteral("content"), joinSections(sections));
    inspector.insert(QStringLiteral("sourceId"), supportLink.value(QStringLiteral("supportLinkId")).toString());
    inspector.insert(QStringLiteral("sourceType"), QStringLiteral("support-link"));
    setInspector(inspector);
}

void ToolsInsideBrowser::inspectArtifact(const QVariantMap &artifact)
{
    QVariantMap inspector;
    inspector.insert(QStringLiteral("title"), artifact.value(QStringLiteral("kind")).toString());
    inspector.insert(QStringLiteral("kind"), QStringLiteral("artifact"));
    inspector.insert(QStringLiteral("summary"), ti18n(u"Artifact: %1\nMIME: %2\nSize: %3 bytes\nPath: %4",
                                                      u"工件：%1\nMIME：%2\n大小：%3 字节\n路径：%4")
                     .arg(artifact.value(QStringLiteral("artifactId")).toString(),
                          artifact.value(QStringLiteral("mimeType")).toString(),
                          QString::number(artifact.value(QStringLiteral("sizeBytes")).toLongLong()),
                          artifact.value(QStringLiteral("relativePath")).toString()));
    inspector.insert(QStringLiteral("content"), loadArtifactContent(artifact));
    inspector.insert(QStringLiteral("sourceId"), artifact.value(QStringLiteral("artifactId")).toString());
    inspector.insert(QStringLiteral("sourceType"), QStringLiteral("artifact"));
    setInspector(inspector);
}

void ToolsInsideBrowser::clearInspector()
{
    setInspector(QVariantMap());
}

void ToolsInsideBrowser::reloadClients()
{
    ensureRuntimeConfigured();
    QString errorMessage;
    const QList<ToolsInsideClientSummary> clients = ToolsInsideRuntime::instance().queryService()->listClients(&errorMessage);
    QVariantList values;
    for (const ToolsInsideClientSummary &client : clients) {
        values.append(toMap(client));
    }
    m_clients = values;
    emit clientsChanged();
    if (!errorMessage.isEmpty()) {
        setStatusText(errorMessage);
    }
}

void ToolsInsideBrowser::reloadSessions()
{
    ensureRuntimeConfigured();
    QVariantList values;
    if (!m_selectedClientId.isEmpty()) {
        QString errorMessage;
        const QList<ToolsInsideSessionSummary> sessions = ToolsInsideRuntime::instance().queryService()->listSessions(m_selectedClientId, &errorMessage);
        for (const ToolsInsideSessionSummary &session : sessions) {
            values.append(toMap(session));
        }
        if (!errorMessage.isEmpty()) {
            setStatusText(errorMessage);
        }
    }
    m_sessions = values;
    emit sessionsChanged();
}

void ToolsInsideBrowser::reloadTraces()
{
    ensureRuntimeConfigured();
    ToolsInsideTraceFilter filter;
    filter.clientId = m_selectedClientId;
    filter.sessionId = m_selectedSessionId;
    filter.limit = 200;

    QString errorMessage;
    const QList<ToolsInsideTraceSummary> traces = ToolsInsideRuntime::instance().queryService()->listTraces(filter, &errorMessage);
    QVariantList values;
    for (const ToolsInsideTraceSummary &trace : traces) {
        values.append(toMap(trace));
    }
    m_traces = values;
    emit tracesChanged();
    if (!errorMessage.isEmpty()) {
        setStatusText(errorMessage);
    }
}

void ToolsInsideBrowser::reloadTraceDetail()
{
    resetTraceDetail();
    if (m_selectedTraceId.isEmpty()) {
        return;
    }

    ensureRuntimeConfigured();
    auto query = ToolsInsideRuntime::instance().queryService();
    QString errorMessage;
    const std::optional<ToolsInsideTraceSummary> trace = query->getTraceSummary(m_selectedTraceId, &errorMessage);
    if (!trace.has_value()) {
        if (!errorMessage.isEmpty()) {
            setStatusText(errorMessage);
        }
        return;
    }

    const QList<ToolsInsideArtifactRef> artifacts = query->getTraceArtifacts(m_selectedTraceId, &errorMessage);
    QVariantList artifactValues;
    for (const ToolsInsideArtifactRef &artifact : artifacts) {
        artifactValues.append(toMap(artifact));
    }
    m_artifacts = artifactValues;
    emit artifactsChanged();

    const QList<ToolsInsideToolCallRecord> toolCalls = query->getTraceToolCalls(m_selectedTraceId, &errorMessage);
    QVariantList toolValues;
    for (const ToolsInsideToolCallRecord &toolCall : toolCalls) {
        toolValues.append(toMap(toolCall));
    }
    m_toolCalls = toolValues;
    emit toolCallsChanged();

    const QList<ToolsInsideSupportLink> supportLinks = query->getTraceSupportLinks(m_selectedTraceId, &errorMessage);
    QVariantList supportValues;
    for (const ToolsInsideSupportLink &link : supportLinks) {
        supportValues.append(toMap(link));
    }
    m_supportLinks = supportValues;
    emit supportLinksChanged();

    const QList<ToolsInsideEventRecord> events = query->getTraceTimeline(m_selectedTraceId, &errorMessage);
    const QList<ToolsInsideSpanRecord> spans = query->getTraceSpans(m_selectedTraceId, &errorMessage);

    QDateTime baseUtc = trace->startedAtUtc;
    if (!baseUtc.isValid()) {
        for (const ToolsInsideSpanRecord &span : spans) {
            if (span.startedAtUtc.isValid() && (!baseUtc.isValid() || span.startedAtUtc < baseUtc)) {
                baseUtc = span.startedAtUtc;
            }
        }
        for (const ToolsInsideEventRecord &event : events) {
            if (event.timestampUtc.isValid() && (!baseUtc.isValid() || event.timestampUtc < baseUtc)) {
                baseUtc = event.timestampUtc;
            }
        }
    }
    if (!baseUtc.isValid()) {
        baseUtc = QDateTime::currentDateTimeUtc();
    }

    QHash<QString, TimelineLaneBuild> laneById;
    QList<QString> laneIds;
    QVariantList flatTimeline;
    qint64 maxEndMs = 0;

    for (const ToolsInsideSpanRecord &span : spans) {
        const qint64 startMs = relativeMs(baseUtc, span.startedAtUtc.isValid() ? span.startedAtUtc : baseUtc);
        const qint64 endMs = std::max(startMs, relativeMs(baseUtc, span.endedAtUtc.isValid() ? span.endedAtUtc : span.startedAtUtc));
        maxEndMs = std::max(maxEndMs, endMs);

        QString laneId = QStringLiteral("trace");
        QString laneKind = QStringLiteral("trace");
        QString laneTitle = ti18n(u"Trace Lifecycle", u"Trace 生命周期");
        QString laneSubtitle = trace->traceId;
        int laneOrder = 0;

        if (span.kind == QStringLiteral("llm_request")) {
            laneId = QStringLiteral("request:") + span.requestId;
            laneKind = QStringLiteral("request");
            laneTitle = ti18n(u"Request ", u"请求 ") + shortId(span.requestId);
            laneSubtitle = span.name;
            laneOrder = 100;
        } else if (span.kind == QStringLiteral("tool_batch")) {
            const int roundIndex = span.summary.value(QStringLiteral("roundIndex")).toInt();
            laneId = QStringLiteral("batch:") + span.requestId + QStringLiteral(":") + QString::number(roundIndex);
            laneKind = QStringLiteral("batch");
            laneTitle = ti18n(u"Tool Batch R%1", u"工具批次 R%1").arg(roundIndex);
            laneSubtitle = ti18n(u"Request ", u"请求 ") + shortId(span.requestId);
            laneOrder = 200 + roundIndex;
        } else if (span.kind == QStringLiteral("tool_call")) {
            laneId = QStringLiteral("tool:") + span.toolCallId;
            laneKind = QStringLiteral("tool");
            laneTitle = span.name.isEmpty() ? ti18n(u"Tool ", u"工具 ") + shortId(span.toolCallId) : span.name;
            laneSubtitle = ti18n(u"Call ", u"调用 ") + shortId(span.toolCallId);
            laneOrder = 300;
        }

        const QVariantMap entry = {
            {QStringLiteral("entryType"), QStringLiteral("span")},
            {QStringLiteral("laneId"), laneId},
            {QStringLiteral("laneKind"), laneKind},
            {QStringLiteral("startMs"), startMs},
            {QStringLiteral("endMs"), endMs},
            {QStringLiteral("durationMs"), std::max<qint64>(0, endMs - startMs)},
            {QStringLiteral("label"), span.name},
            {QStringLiteral("subtitle"), span.status},
            {QStringLiteral("status"), span.status},
            {QStringLiteral("detail"), jsonText(span.summary)},
            {QStringLiteral("timestampUtc"), toIso(span.startedAtUtc)},
            {QStringLiteral("endedAtUtc"), toIso(span.endedAtUtc)},
            {QStringLiteral("requestId"), span.requestId},
            {QStringLiteral("toolCallId"), span.toolCallId},
            {QStringLiteral("color"), colorFor(QStringLiteral("span"), span.status, laneKind)}
        };
        appendLaneEntry(laneById, laneIds, laneId, laneKind, laneTitle, laneSubtitle, laneOrder, entry);
        flatTimeline.append(entry);
    }

    for (const ToolsInsideEventRecord &event : events) {
        const qint64 startMs = relativeMs(baseUtc, event.timestampUtc.isValid() ? event.timestampUtc : baseUtc);
        maxEndMs = std::max(maxEndMs, startMs);

        QString laneId = QStringLiteral("trace");
        QString laneKind = QStringLiteral("trace");
        QString laneTitle = ti18n(u"Trace Lifecycle", u"Trace 生命周期");
        QString laneSubtitle = trace->traceId;
        int laneOrder = 0;

        if (!event.toolCallId.isEmpty()) {
            laneId = QStringLiteral("tool:") + event.toolCallId;
            laneKind = QStringLiteral("tool");
            laneTitle = ti18n(u"Tool ", u"工具 ") + shortId(event.toolCallId);
            laneSubtitle = event.category;
            laneOrder = 300;
        } else if (event.category == QStringLiteral("tool.loop") && !event.requestId.isEmpty()) {
            const int roundIndex = event.payload.value(QStringLiteral("roundIndex")).toInt();
            laneId = QStringLiteral("batch:") + event.requestId + QStringLiteral(":") + QString::number(roundIndex);
            laneKind = QStringLiteral("batch");
            laneTitle = ti18n(u"Tool Batch R%1", u"工具批次 R%1").arg(roundIndex);
            laneSubtitle = event.category;
            laneOrder = 200 + roundIndex;
        } else if (event.category.startsWith(QStringLiteral("llm.")) || !event.requestId.isEmpty()) {
            laneId = QStringLiteral("request:") + event.requestId;
            laneKind = QStringLiteral("request");
            laneTitle = ti18n(u"Request ", u"请求 ") + shortId(event.requestId);
            laneSubtitle = event.category;
            laneOrder = 100;
        }

        const QVariantMap entry = {
            {QStringLiteral("entryType"), QStringLiteral("event")},
            {QStringLiteral("laneId"), laneId},
            {QStringLiteral("laneKind"), laneKind},
            {QStringLiteral("startMs"), startMs},
            {QStringLiteral("endMs"), startMs},
            {QStringLiteral("durationMs"), 0},
            {QStringLiteral("label"), event.name},
            {QStringLiteral("subtitle"), event.category},
            {QStringLiteral("status"), QString()},
            {QStringLiteral("detail"), jsonText(event.payload)},
            {QStringLiteral("timestampUtc"), toIso(event.timestampUtc)},
            {QStringLiteral("endedAtUtc"), QString()},
            {QStringLiteral("requestId"), event.requestId},
            {QStringLiteral("toolCallId"), event.toolCallId},
            {QStringLiteral("color"), colorFor(QStringLiteral("event"), QString(), laneKind)}
        };
        appendLaneEntry(laneById, laneIds, laneId, laneKind, laneTitle, laneSubtitle, laneOrder, entry);
        flatTimeline.append(entry);
    }

    std::sort(flatTimeline.begin(), flatTimeline.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap lhs = left.toMap();
        const QVariantMap rhs = right.toMap();
        if (lhs.value(QStringLiteral("startMs")).toLongLong() == rhs.value(QStringLiteral("startMs")).toLongLong()) {
            return lhs.value(QStringLiteral("entryType")).toString() < rhs.value(QStringLiteral("entryType")).toString();
        }
        return lhs.value(QStringLiteral("startMs")).toLongLong() < rhs.value(QStringLiteral("startMs")).toLongLong();
    });

    QList<TimelineLaneBuild> lanes;
    for (const QString &laneId : laneIds) {
        TimelineLaneBuild lane = laneById.value(laneId);
        std::sort(lane.entries.begin(), lane.entries.end(), [](const QVariant &left, const QVariant &right) {
            const QVariantMap lhs = left.toMap();
            const QVariantMap rhs = right.toMap();
            if (lhs.value(QStringLiteral("startMs")).toLongLong() == rhs.value(QStringLiteral("startMs")).toLongLong()) {
                return lhs.value(QStringLiteral("entryType")).toString() < rhs.value(QStringLiteral("entryType")).toString();
            }
            return lhs.value(QStringLiteral("startMs")).toLongLong() < rhs.value(QStringLiteral("startMs")).toLongLong();
        });
        assignLaneRows(lane);
        lanes.append(lane);
    }

    std::sort(lanes.begin(), lanes.end(), [](const TimelineLaneBuild &left, const TimelineLaneBuild &right) {
        if (left.laneOrder == right.laneOrder) {
            if (left.firstStartMs == right.firstStartMs) {
                return left.title < right.title;
            }
            return left.firstStartMs < right.firstStartMs;
        }
        return left.laneOrder < right.laneOrder;
    });

    QVariantList laneValues;
    for (const TimelineLaneBuild &lane : lanes) {
        laneValues.append(QVariantMap{
            {QStringLiteral("laneId"), lane.laneId},
            {QStringLiteral("laneKind"), lane.laneKind},
            {QStringLiteral("title"), lane.title},
            {QStringLiteral("subtitle"), lane.subtitle},
            {QStringLiteral("rowCount"), lane.rowCount},
            {QStringLiteral("entries"), lane.entries}
        });
    }

    m_timeline = flatTimeline;
    m_timelineLanes = laneValues;
    m_timelineDurationMs = std::max<qint64>(kTimelineTickMs, maxEndMs + kTimelineTickMs);

    m_traceSummary = toMap(*trace);
    m_traceSummary.insert(QStringLiteral("t0Utc"), toIso(baseUtc));
    m_traceSummary.insert(QStringLiteral("durationMs"), m_timelineDurationMs);
    m_traceSummary.insert(QStringLiteral("tickMs"), kTimelineTickMs);
    m_traceSummary.insert(QStringLiteral("laneCount"), m_timelineLanes.size());

    emit traceSummaryChanged();
    emit timelineChanged();
    emit timelineLanesChanged();
    emit timelineDurationChanged();

    setInspector(QVariantMap{{QStringLiteral("title"), ti18n(u"Trace Overview", u"Trace 总览")},
                             {QStringLiteral("kind"), QStringLiteral("trace")},
                             {QStringLiteral("summary"), ti18n(u"Trace: %1\nStatus: %2\nT0: %3\nDuration: %4 ms\nLanes: %5",
                                                               u"Trace：%1\n状态：%2\nT0：%3\n时长：%4 ms\n泳道数：%5")
                              .arg(trace->traceId, trace->status, toIso(baseUtc), QString::number(m_timelineDurationMs), QString::number(m_timelineLanes.size()))},
                             {QStringLiteral("content"), m_traceSummary.value(QStringLiteral("summaryJson")).toString()},
                             {QStringLiteral("sourceId"), trace->traceId},
                             {QStringLiteral("sourceType"), QStringLiteral("trace")}});

    if (!errorMessage.isEmpty()) {
        setStatusText(errorMessage);
    } else {
        setStatusText(ti18n(u"Loaded trace detail with millisecond swimlanes", u"已加载带毫秒泳道的 Trace 详情"));
    }
}

void ToolsInsideBrowser::resetTraceDetail()
{
    m_traceSummary.clear();
    m_timeline.clear();
    m_timelineLanes.clear();
    m_timelineDurationMs = kTimelineTickMs;
    m_toolCalls.clear();
    m_supportLinks.clear();
    m_artifacts.clear();
    m_inspector.clear();
    emit traceSummaryChanged();
    emit timelineChanged();
    emit timelineLanesChanged();
    emit timelineDurationChanged();
    emit toolCallsChanged();
    emit supportLinksChanged();
    emit artifactsChanged();
    emit inspectorChanged();
}

void ToolsInsideBrowser::ensureRuntimeConfigured()
{
    ToolsInsideRuntime::instance().configureWorkspaceRoot(m_workspaceRoot);
}

void ToolsInsideBrowser::loadWorkspacePreferences()
{
    QSettings settings(QStringLiteral("qtllm"), QStringLiteral("tools_inside"));
    QStringList history = settings.value(QStringLiteral("workspaceHistory")).toStringList();
    QStringList favorites = settings.value(QStringLiteral("workspaceFavorites")).toStringList();
    for (QString &path : history) {
        path = QDir::cleanPath(path);
    }
    for (QString &path : favorites) {
        path = QDir::cleanPath(path);
    }
    history.removeDuplicates();
    favorites.removeDuplicates();
    while (history.size() > kMaxWorkspaceHistory) {
        history.removeLast();
    }
    while (favorites.size() > kMaxWorkspaceFavorites) {
        favorites.removeLast();
    }
    m_workspaceHistory = history;
    m_workspaceFavorites = favorites;
    const QString storedRoot = QDir::cleanPath(settings.value(QStringLiteral("workspaceRoot"), m_workspaceRoot).toString());
    if (!storedRoot.trimmed().isEmpty()) {
        m_workspaceRoot = storedRoot;
    }
    rememberWorkspaceRoot(m_workspaceRoot);
}

void ToolsInsideBrowser::persistWorkspacePreferences() const
{
    QSettings settings(QStringLiteral("qtllm"), QStringLiteral("tools_inside"));
    settings.setValue(QStringLiteral("workspaceRoot"), m_workspaceRoot);
    settings.setValue(QStringLiteral("workspaceHistory"), m_workspaceHistory);
    settings.setValue(QStringLiteral("workspaceFavorites"), m_workspaceFavorites);
}

void ToolsInsideBrowser::rememberWorkspaceRoot(const QString &workspaceRoot)
{
    const QString normalized = normalizeWorkspaceRoot(workspaceRoot);
    if (normalized.isEmpty()) {
        return;
    }

    QStringList history = m_workspaceHistory;
    history.removeAll(normalized);
    history.prepend(normalized);
    while (history.size() > kMaxWorkspaceHistory) {
        history.removeLast();
    }

    const bool historyChanged = history != m_workspaceHistory;
    m_workspaceHistory = history;
    if (historyChanged) {
        emit workspaceHistoryChanged();
    }
    persistWorkspacePreferences();
}

QString ToolsInsideBrowser::normalizeWorkspaceRoot(const QString &workspaceRoot) const
{
    return QDir::cleanPath(workspaceRoot.trimmed());
}

void ToolsInsideBrowser::setStatusText(const QString &statusText)
{
    if (statusText == m_statusText) {
        return;
    }
    m_statusText = statusText;
    emit statusTextChanged();
}


void ToolsInsideBrowser::setInspector(const QVariantMap &inspector)
{
    if (inspector == m_inspector) {
        return;
    }
    m_inspector = inspector;
    emit inspectorChanged();
}

QVariantMap ToolsInsideBrowser::findArtifactById(const QString &artifactId) const
{
    if (artifactId.trimmed().isEmpty()) {
        return QVariantMap();
    }
    for (const QVariant &item : m_artifacts) {
        const QVariantMap artifact = item.toMap();
        if (artifact.value(QStringLiteral("artifactId")).toString() == artifactId) {
            return artifact;
        }
    }
    return QVariantMap();
}

QVariantMap ToolsInsideBrowser::findToolCallById(const QString &toolCallId) const
{
    if (toolCallId.trimmed().isEmpty()) {
        return QVariantMap();
    }
    for (const QVariant &item : m_toolCalls) {
        const QVariantMap toolCall = item.toMap();
        if (toolCall.value(QStringLiteral("toolCallId")).toString() == toolCallId) {
            return toolCall;
        }
    }
    return QVariantMap();
}

QString ToolsInsideBrowser::loadArtifactContent(const QVariantMap &artifact) const
{
    const QString relativePath = artifact.value(QStringLiteral("relativePath")).toString();
    const QString mimeType = artifact.value(QStringLiteral("mimeType")).toString();
    if (relativePath.isEmpty()) {
        return QString();
    }
    const auto store = ToolsInsideRuntime::instance().artifactStore();
    if (!store) {
        return ti18n(u"Artifact store unavailable", u"工件存储不可用");
    }
    const QString absolutePath = store->absolutePathForRelativePath(relativePath);
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return ti18n(u"Failed to open artifact: ", u"打开工件失败：") + absolutePath;
    }
    const QByteArray bytes = file.read(262144);
    const bool truncated = !file.atEnd();
    QString textPreview;
    if (mimeType == QStringLiteral("application/json")) {
        textPreview = prettyJsonText(bytes);
        if (textPreview.isEmpty()) {
            textPreview = QString::fromUtf8(bytes);
        }
    } else if (mimeType.startsWith(QStringLiteral("text/"))) {
        textPreview = QString::fromUtf8(bytes);
        const QString maybeJson = prettyJsonText(bytes);
        if (!maybeJson.isEmpty()) {
            textPreview = maybeJson;
        }
    } else {
        textPreview = ti18n(u"Binary artifact preview is not enabled in current build.\nPath: ",
                            u"当前构建未启用二进制工件预览。\n路径：") + absolutePath;
    }
    if (truncated) {
        textPreview += ti18n(u"\n\n[preview truncated at 256 KiB]", u"\n\n[预览已在 256 KiB 处截断]");
    }
    return textPreview;
}
