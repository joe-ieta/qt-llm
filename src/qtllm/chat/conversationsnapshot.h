#pragma once

#include "../identity/compactid.h"
#include "../core/llmconfig.h"
#include "../core/llmtypes.h"
#include "../profile/clientprofile.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>

namespace qtllm::chat {

struct ConversationSessionSnapshot
{
    QString sessionId;
    QString title;
    QVector<LlmMessage> history;
    QDateTime createdAt = QDateTime::currentDateTimeUtc();
    QDateTime updatedAt = QDateTime::currentDateTimeUtc();

    QJsonObject toJson() const
    {
        QJsonObject root;
        root.insert(QStringLiteral("sessionId"), sessionId);
        root.insert(QStringLiteral("title"), title);
        root.insert(QStringLiteral("createdAt"), createdAt.toString(Qt::ISODateWithMs));
        root.insert(QStringLiteral("updatedAt"), updatedAt.toString(Qt::ISODateWithMs));

        QJsonArray historyArray;
        for (const LlmMessage &message : history) {
            QJsonObject item;
            item.insert(QStringLiteral("role"), message.role);
            item.insert(QStringLiteral("content"), message.content);
            if (!message.name.isEmpty()) {
                item.insert(QStringLiteral("name"), message.name);
            }
            if (!message.toolCallId.isEmpty()) {
                item.insert(QStringLiteral("toolCallId"), message.toolCallId);
            }

            if (!message.toolCalls.isEmpty()) {
                QJsonArray toolCalls;
                for (const LlmToolCall &call : message.toolCalls) {
                    QJsonObject callJson;
                    callJson.insert(QStringLiteral("id"), call.id);
                    callJson.insert(QStringLiteral("name"), call.name);
                    callJson.insert(QStringLiteral("type"), call.type);
                    callJson.insert(QStringLiteral("arguments"), call.arguments);
                    toolCalls.append(callJson);
                }
                item.insert(QStringLiteral("toolCalls"), toolCalls);
            }

            historyArray.append(item);
        }
        root.insert(QStringLiteral("history"), historyArray);

        return root;
    }

    static ConversationSessionSnapshot fromJson(const QJsonObject &root)
    {
        ConversationSessionSnapshot snapshot;
        snapshot.sessionId = root.value(QStringLiteral("sessionId")).toString();
        snapshot.title = root.value(QStringLiteral("title")).toString();

        const QString createdAtText = root.value(QStringLiteral("createdAt")).toString();
        const QString updatedAtText = root.value(QStringLiteral("updatedAt")).toString();
        const QDateTime createdAt = QDateTime::fromString(createdAtText, Qt::ISODateWithMs);
        const QDateTime updatedAt = QDateTime::fromString(updatedAtText, Qt::ISODateWithMs);
        if (createdAt.isValid()) {
            snapshot.createdAt = createdAt;
        }
        if (updatedAt.isValid()) {
            snapshot.updatedAt = updatedAt;
        }

        const QJsonArray historyArray = root.value(QStringLiteral("history")).toArray();
        for (const QJsonValue &value : historyArray) {
            const QJsonObject item = value.toObject();
            LlmMessage message;
            message.role = item.value(QStringLiteral("role")).toString();
            message.content = item.value(QStringLiteral("content")).toString();
            message.name = item.value(QStringLiteral("name")).toString();
            message.toolCallId = item.value(QStringLiteral("toolCallId")).toString();

            const QJsonArray toolCallsArray = item.value(QStringLiteral("toolCalls")).toArray();
            for (const QJsonValue &callValue : toolCallsArray) {
                const QJsonObject callJson = callValue.toObject();
                LlmToolCall call;
                call.id = callJson.value(QStringLiteral("id")).toString();
                call.name = callJson.value(QStringLiteral("name")).toString();
                call.type = callJson.value(QStringLiteral("type")).toString(call.type);
                call.arguments = callJson.value(QStringLiteral("arguments")).toObject();
                if (!call.name.isEmpty()) {
                    message.toolCalls.append(call);
                }
            }

            if (!message.role.isEmpty()) {
                snapshot.history.append(message);
            }
        }

        return snapshot;
    }
};

struct ConversationSnapshot
{
    QString uid;
    LlmConfig config;
    profile::ClientProfile profile;
    QString activeSessionId;
    QVector<ConversationSessionSnapshot> sessions;

    QJsonObject toJson() const
    {
        QJsonObject root;
        root.insert(QStringLiteral("uid"), uid);

        QJsonObject configObject;
        configObject.insert(QStringLiteral("providerName"), config.providerName);
        configObject.insert(QStringLiteral("baseUrl"), config.baseUrl);
        configObject.insert(QStringLiteral("apiKey"), config.apiKey);
        configObject.insert(QStringLiteral("model"), config.model);
        configObject.insert(QStringLiteral("modelVendor"), config.modelVendor);
        configObject.insert(QStringLiteral("runtimeName"), config.runtimeName);
        configObject.insert(QStringLiteral("llamaCppRuntimeRoot"), config.llamaCppRuntimeRoot);
        configObject.insert(QStringLiteral("llamaCppExecutablePath"), config.llamaCppExecutablePath);
        configObject.insert(QStringLiteral("llamaCppModelPath"), config.llamaCppModelPath);
        configObject.insert(QStringLiteral("llamaCppExtraArgs"), QJsonArray::fromStringList(config.llamaCppExtraArgs));
        configObject.insert(QStringLiteral("stream"), config.stream);
        configObject.insert(QStringLiteral("timeoutMs"), config.timeoutMs);
        configObject.insert(QStringLiteral("maxRetries"), config.maxRetries);
        configObject.insert(QStringLiteral("retryDelayMs"), config.retryDelayMs);
        configObject.insert(QStringLiteral("llamaCppServerPort"), config.llamaCppServerPort);
        configObject.insert(QStringLiteral("llamaCppContextSize"), config.llamaCppContextSize);
        configObject.insert(QStringLiteral("llamaCppGpuLayers"), config.llamaCppGpuLayers);
        configObject.insert(QStringLiteral("llamaCppThreadCount"), config.llamaCppThreadCount);
        configObject.insert(QStringLiteral("llamaCppStartupTimeoutMs"), config.llamaCppStartupTimeoutMs);
        root.insert(QStringLiteral("config"), configObject);

        root.insert(QStringLiteral("profile"), profile.toJson());
        root.insert(QStringLiteral("activeSessionId"), activeSessionId);

        QJsonArray sessionsArray;
        for (const ConversationSessionSnapshot &session : sessions) {
            sessionsArray.append(session.toJson());
        }
        root.insert(QStringLiteral("sessions"), sessionsArray);

        return root;
    }

    static ConversationSnapshot fromJson(const QJsonObject &root)
    {
        ConversationSnapshot snapshot;
        snapshot.uid = root.value(QStringLiteral("uid")).toString();

        const QJsonObject configObject = root.value(QStringLiteral("config")).toObject();
        snapshot.config.providerName = configObject.value(QStringLiteral("providerName")).toString();
        snapshot.config.baseUrl = configObject.value(QStringLiteral("baseUrl")).toString();
        snapshot.config.apiKey = configObject.value(QStringLiteral("apiKey")).toString();
        snapshot.config.model = configObject.value(QStringLiteral("model")).toString();
        snapshot.config.modelVendor = configObject.value(QStringLiteral("modelVendor")).toString();
        snapshot.config.runtimeName = configObject.value(QStringLiteral("runtimeName")).toString();
        snapshot.config.llamaCppRuntimeRoot = configObject.value(QStringLiteral("llamaCppRuntimeRoot")).toString();
        snapshot.config.llamaCppExecutablePath = configObject.value(QStringLiteral("llamaCppExecutablePath")).toString();
        snapshot.config.llamaCppModelPath = configObject.value(QStringLiteral("llamaCppModelPath")).toString();
        const QJsonArray llamaCppExtraArgs = configObject.value(QStringLiteral("llamaCppExtraArgs")).toArray();
        for (const QJsonValue &value : llamaCppExtraArgs) {
            const QString arg = value.toString();
            if (!arg.isEmpty()) {
                snapshot.config.llamaCppExtraArgs.append(arg);
            }
        }
        snapshot.config.stream = configObject.value(QStringLiteral("stream")).toBool(snapshot.config.stream);
        snapshot.config.timeoutMs = configObject.value(QStringLiteral("timeoutMs")).toInt(snapshot.config.timeoutMs);
        snapshot.config.maxRetries = configObject.value(QStringLiteral("maxRetries")).toInt(snapshot.config.maxRetries);
        snapshot.config.retryDelayMs = configObject.value(QStringLiteral("retryDelayMs")).toInt(snapshot.config.retryDelayMs);
        snapshot.config.llamaCppServerPort = configObject.value(QStringLiteral("llamaCppServerPort")).toInt(snapshot.config.llamaCppServerPort);
        snapshot.config.llamaCppContextSize = configObject.value(QStringLiteral("llamaCppContextSize")).toInt(snapshot.config.llamaCppContextSize);
        snapshot.config.llamaCppGpuLayers = configObject.value(QStringLiteral("llamaCppGpuLayers")).toInt(snapshot.config.llamaCppGpuLayers);
        snapshot.config.llamaCppThreadCount = configObject.value(QStringLiteral("llamaCppThreadCount")).toInt(snapshot.config.llamaCppThreadCount);
        snapshot.config.llamaCppStartupTimeoutMs = configObject.value(QStringLiteral("llamaCppStartupTimeoutMs")).toInt(snapshot.config.llamaCppStartupTimeoutMs);

        snapshot.profile = profile::ClientProfile::fromJson(root.value(QStringLiteral("profile")).toObject());
        snapshot.activeSessionId = root.value(QStringLiteral("activeSessionId")).toString();

        const QJsonArray sessionsArray = root.value(QStringLiteral("sessions")).toArray();
        for (const QJsonValue &value : sessionsArray) {
            const ConversationSessionSnapshot session = ConversationSessionSnapshot::fromJson(value.toObject());
            if (!session.sessionId.isEmpty()) {
                snapshot.sessions.append(session);
            }
        }

        // Backward-compatible fallback for old snapshots that only had a single history array.
        if (snapshot.sessions.isEmpty()) {
            ConversationSessionSnapshot fallback;
            fallback.sessionId = qtllm::identity::generateId(qtllm::identity::IdKind::Session);
            fallback.title = QStringLiteral("Session 1");
            const QJsonArray historyArray = root.value(QStringLiteral("history")).toArray();
            for (const QJsonValue &value : historyArray) {
                const QJsonObject item = value.toObject();
                LlmMessage message;
                message.role = item.value(QStringLiteral("role")).toString();
                message.content = item.value(QStringLiteral("content")).toString();
                if (!message.role.isEmpty()) {
                    fallback.history.append(message);
                }
            }
            snapshot.sessions.append(fallback);
            if (snapshot.activeSessionId.isEmpty()) {
                snapshot.activeSessionId = fallback.sessionId;
            }
        }

        if (snapshot.activeSessionId.isEmpty() && !snapshot.sessions.isEmpty()) {
            snapshot.activeSessionId = snapshot.sessions.first().sessionId;
        }

        return snapshot;
    }
};

} // namespace qtllm::chat
