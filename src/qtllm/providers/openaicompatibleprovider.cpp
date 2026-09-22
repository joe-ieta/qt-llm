#include "openaicompatibleprovider.h"

#include "../structuredoutput/structuredoutputservice.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QUrl>
#include <QUrlQuery>

namespace qtllm {

namespace {

enum class VendorSchema
{
    OpenAI,
    Anthropic,
    Google
};

struct StreamEvent
{
    QString eventName;
    QJsonObject object;
    bool isDoneMarker = false;
};

struct PendingToolCall
{
    QString key;
    QString id;
    QString type = QStringLiteral("function");
    QString name;
    QString argumentsText;
};

struct OpenAiStreamState
{
    QJsonObject lastRoot;
    QStringList contentParts;
    QStringList reasoningParts;
    QHash<QString, PendingToolCall> toolCalls;
    QString finishReason;
    QString errorMessage;
};

bool containsAny(const QString &text, const QStringList &terms)
{
    const QString lowered = text.trimmed().toLower();
    for (const QString &term : terms) {
        if (lowered.contains(term)) {
            return true;
        }
    }
    return false;
}

QString inferVendor(const QString &explicitVendor, const QString &modelName)
{
    const QString vendor = explicitVendor.trimmed().toLower();
    if (!vendor.isEmpty()) {
        return vendor;
    }

    const QString model = modelName.trimmed().toLower();
    if (model.isEmpty()) {
        return QStringLiteral("openai");
    }

    if (containsAny(model, QStringList({QStringLiteral("claude"), QStringLiteral("anthropic")}))) {
        return QStringLiteral("anthropic");
    }

    if (containsAny(model, QStringList({QStringLiteral("gemini"), QStringLiteral("palm"), QStringLiteral("google")}))) {
        return QStringLiteral("google");
    }

    if (containsAny(model, QStringList({QStringLiteral("deepseek")}))) {
        return QStringLiteral("deepseek");
    }

    if (containsAny(model, QStringList({QStringLiteral("qwen"), QStringLiteral("tongyi")}))) {
        return QStringLiteral("qwen");
    }

    if (containsAny(model, QStringList({QStringLiteral("glm"), QStringLiteral("chatglm"), QStringLiteral("zhipu")}))) {
        return QStringLiteral("glm");
    }

    return QStringLiteral("openai");
}

VendorSchema vendorSchema(const QString &vendor)
{
    if (vendor == QStringLiteral("anthropic")) {
        return VendorSchema::Anthropic;
    }

    if (vendor == QStringLiteral("google") || vendor == QStringLiteral("gemini")) {
        return VendorSchema::Google;
    }

    return VendorSchema::OpenAI;
}

QString resolveVendor(const LlmConfig &config, const LlmRequest &request)
{
    const QString model = request.model.trimmed().isEmpty() ? config.model : request.model;
    return inferVendor(config.modelVendor, model);
}

bool looksLikeSsePayload(const QByteArray &data)
{
    const QByteArray trimmed = data.trimmed();
    return trimmed.startsWith("data:")
        || trimmed.startsWith("event:")
        || trimmed.contains("\ndata:")
        || trimmed.contains("\nevent:");
}

QList<StreamEvent> parseSseEvents(const QByteArray &payload)
{
    QList<StreamEvent> events;
    QString eventName;
    QList<QByteArray> dataLines;

    const auto flushEvent = [&events, &eventName, &dataLines]() {
        const QByteArray joined = dataLines.join("\n").trimmed();
        if (joined.isEmpty()) {
            eventName.clear();
            dataLines.clear();
            return;
        }

        StreamEvent event;
        event.eventName = eventName;
        event.isDoneMarker = joined == "[DONE]";
        if (!event.isDoneMarker) {
            const QJsonDocument doc = QJsonDocument::fromJson(joined);
            if (doc.isObject()) {
                event.object = doc.object();
            }
        }

        events.append(event);
        eventName.clear();
        dataLines.clear();
    };

    const QList<QByteArray> lines = payload.split('\n');
    for (QByteArray line : lines) {
        if (line.endsWith('\r')) {
            line.chop(1);
        }

        if (line.isEmpty()) {
            flushEvent();
            continue;
        }

        if (line.startsWith(':')) {
            continue;
        }

        if (line.startsWith("event:")) {
            eventName = QString::fromUtf8(line.mid(6).trimmed());
            continue;
        }

        if (line.startsWith("data:")) {
            QByteArray dataLine = line.mid(5);
            if (dataLine.startsWith(' ')) {
                dataLine.remove(0, 1);
            }
            dataLines.append(dataLine);
        }
    }

    flushEvent();
    return events;
}

QList<QJsonObject> parseJsonLines(const QByteArray &payload)
{
    QList<QJsonObject> objects;
    const QList<QByteArray> lines = payload.split('\n');
    for (QByteArray line : lines) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(trimmed);
        if (doc.isObject()) {
            objects.append(doc.object());
        }
    }
    return objects;
}

QString collectTextFromValue(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString();
    }

    if (!value.isArray()) {
        return QString();
    }

    QStringList parts;
    const QJsonArray array = value.toArray();
    for (const QJsonValue &entry : array) {
        const QJsonObject obj = entry.toObject();
        const QString type = obj.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("text")
            || type == QStringLiteral("output_text")
            || type == QStringLiteral("input_text")) {
            const QString text = obj.value(QStringLiteral("text")).toString();
            if (!text.isEmpty()) {
                parts.append(text);
            }
            continue;
        }

        if (type == QStringLiteral("refusal")) {
            const QString refusal = obj.value(QStringLiteral("refusal")).toString();
            if (!refusal.isEmpty()) {
                parts.append(refusal);
            }
        }
    }

    return parts.join(QString());
}

QString collectReasoningFromValue(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString();
    }

    if (!value.isArray()) {
        return QString();
    }

    QStringList parts;
    const QJsonArray array = value.toArray();
    for (const QJsonValue &entry : array) {
        const QJsonObject obj = entry.toObject();
        const QString text = obj.value(QStringLiteral("text")).toString(
            obj.value(QStringLiteral("content")).toString());
        if (!text.isEmpty()) {
            parts.append(text);
        }
    }

    return parts.join(QString());
}

QString extractOpenAiReasoning(const QJsonObject &object)
{
    QStringList parts;

    const QString thinking = collectReasoningFromValue(object.value(QStringLiteral("thinking")));
    if (!thinking.isEmpty()) {
        parts.append(thinking);
    }

    const QString reasoning = collectReasoningFromValue(object.value(QStringLiteral("reasoning")));
    if (!reasoning.isEmpty()) {
        parts.append(reasoning);
    }

    const QString reasoningContent = collectReasoningFromValue(object.value(QStringLiteral("reasoning_content")));
    if (!reasoningContent.isEmpty()) {
        parts.append(reasoningContent);
    }

    return parts.join(QString());
}

QString toolCallKey(const QJsonObject &callObj, int fallbackIndex)
{
    if (callObj.contains(QStringLiteral("index"))) {
        return QStringLiteral("index:%1").arg(callObj.value(QStringLiteral("index")).toInt(fallbackIndex));
    }

    const QString id = callObj.value(QStringLiteral("id")).toString();
    if (!id.isEmpty()) {
        return id;
    }

    return QStringLiteral("index:%1").arg(fallbackIndex);
}

void mergePendingToolCall(const QJsonObject &callObj, QHash<QString, PendingToolCall> &pendingCalls, bool appendArguments, int fallbackIndex)
{
    const QString key = toolCallKey(callObj, fallbackIndex);
    PendingToolCall &pending = pendingCalls[key];
    pending.key = key;

    const QString id = callObj.value(QStringLiteral("id")).toString();
    if (!id.isEmpty()) {
        pending.id = id;
    }

    pending.type = callObj.value(QStringLiteral("type")).toString(pending.type);

    const QJsonObject function = callObj.value(QStringLiteral("function")).toObject();
    pending.name = function.value(QStringLiteral("name")).toString(pending.name);

    const QJsonValue argumentsValue = function.value(QStringLiteral("arguments"));
    QString argumentsText;
    if (argumentsValue.isString()) {
        argumentsText = argumentsValue.toString();
    } else if (argumentsValue.isObject()) {
        argumentsText = QString::fromUtf8(QJsonDocument(argumentsValue.toObject()).toJson(QJsonDocument::Compact));
    }

    if (argumentsText.isEmpty()) {
        return;
    }

    if (appendArguments) {
        pending.argumentsText += argumentsText;
    } else {
        pending.argumentsText = argumentsText;
    }
}

void appendToolCallsToAssistant(const QHash<QString, PendingToolCall> &pendingCalls, LlmMessage &assistant)
{
    for (auto it = pendingCalls.constBegin(); it != pendingCalls.constEnd(); ++it) {
        if (it->name.trimmed().isEmpty()) {
            continue;
        }

        bool exists = false;
        for (const LlmToolCall &existing : assistant.toolCalls) {
            if ((!it->id.isEmpty() && existing.id == it->id)
                || existing.name == it->name) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        LlmToolCall call;
        call.id = !it->id.isEmpty() ? it->id : it->key;
        call.type = it->type;
        call.name = it->name;
        const QJsonDocument argsDoc = QJsonDocument::fromJson(it->argumentsText.toUtf8());
        if (argsDoc.isObject()) {
            call.arguments = argsDoc.object();
        }
        assistant.toolCalls.append(call);
    }
}

QJsonObject toOpenAiMessage(const LlmMessage &message)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("role"), message.role);

    const QString role = message.role.trimmed().toLower();
    const bool assistantToolCallOnly = role == QStringLiteral("assistant")
        && !message.toolCalls.isEmpty()
        && message.content.isEmpty();
    if (!assistantToolCallOnly) {
        obj.insert(QStringLiteral("content"), message.content);
    }

    // Thinking-mode models (DeepSeek V4/R1, Qwen, ...) require the assistant
    // reasoning channel to be passed back, especially on tool-call turns.
    if (role == QStringLiteral("assistant") && !message.reasoningContent.isEmpty()) {
        obj.insert(QStringLiteral("reasoning_content"), message.reasoningContent);
    }

    if (!message.name.trimmed().isEmpty()) {
        obj.insert(QStringLiteral("name"), message.name);
    }
    if (!message.toolCallId.trimmed().isEmpty()) {
        obj.insert(QStringLiteral("tool_call_id"), message.toolCallId);
    }

    if (!message.toolCalls.isEmpty()) {
        QJsonArray toolCalls;
        for (const LlmToolCall &call : message.toolCalls) {
            QJsonObject function;
            function.insert(QStringLiteral("name"), call.name);
            function.insert(QStringLiteral("arguments"),
                            QString::fromUtf8(QJsonDocument(call.arguments).toJson(QJsonDocument::Compact)));

            QJsonObject callObj;
            callObj.insert(QStringLiteral("id"), call.id);
            callObj.insert(QStringLiteral("type"), call.type);
            callObj.insert(QStringLiteral("function"), function);
            toolCalls.append(callObj);
        }
        obj.insert(QStringLiteral("tool_calls"), toolCalls);
    }

    return obj;
}

QJsonArray toOpenAiMessages(const QVector<LlmMessage> &messages)
{
    QJsonArray array;
    for (const LlmMessage &message : messages) {
        array.append(toOpenAiMessage(message));
    }
    return array;
}

QJsonArray toAnthropicTools(const QJsonArray &openAiTools)
{
    QJsonArray array;
    for (const QJsonValue &toolValue : openAiTools) {
        const QJsonObject toolObj = toolValue.toObject();
        const QJsonObject functionObj = toolObj.value(QStringLiteral("function")).toObject();
        const QString toolName = functionObj.value(QStringLiteral("name")).toString();
        if (toolName.trimmed().isEmpty()) {
            continue;
        }

        QJsonObject mapped;
        mapped.insert(QStringLiteral("name"), toolName);
        const QString desc = functionObj.value(QStringLiteral("description")).toString();
        if (!desc.trimmed().isEmpty()) {
            mapped.insert(QStringLiteral("description"), desc);
        }
        mapped.insert(QStringLiteral("input_schema"), functionObj.value(QStringLiteral("parameters")).toObject());
        array.append(mapped);
    }
    return array;
}

QJsonArray toAnthropicMessages(const QVector<LlmMessage> &messages, QString *systemPrompt)
{
    QJsonArray array;
    QStringList systemBlocks;
    int generatedToolId = 0;

    for (const LlmMessage &message : messages) {
        const QString role = message.role.trimmed().toLower();

        if (role == QStringLiteral("system")) {
            if (!message.content.trimmed().isEmpty()) {
                systemBlocks.append(message.content);
            }
            continue;
        }

        if (role == QStringLiteral("tool")) {
            QJsonArray content;
            QJsonObject toolResult;
            toolResult.insert(QStringLiteral("type"), QStringLiteral("tool_result"));
            if (!message.toolCallId.trimmed().isEmpty()) {
                toolResult.insert(QStringLiteral("tool_use_id"), message.toolCallId);
            }

            const QJsonDocument parsed = QJsonDocument::fromJson(message.content.toUtf8());
            if (parsed.isObject()) {
                toolResult.insert(QStringLiteral("content"), parsed.object());
            } else {
                toolResult.insert(QStringLiteral("content"), message.content);
            }
            content.append(toolResult);

            QJsonObject obj;
            obj.insert(QStringLiteral("role"), QStringLiteral("user"));
            obj.insert(QStringLiteral("content"), content);
            array.append(obj);
            continue;
        }

        const QString mappedRole = role == QStringLiteral("assistant") ? QStringLiteral("assistant") : QStringLiteral("user");
        QJsonArray content;

        if (!message.content.trimmed().isEmpty()) {
            QJsonObject textBlock;
            textBlock.insert(QStringLiteral("type"), QStringLiteral("text"));
            textBlock.insert(QStringLiteral("text"), message.content);
            content.append(textBlock);
        }

        if (mappedRole == QStringLiteral("assistant")) {
            for (const LlmToolCall &call : message.toolCalls) {
                if (call.name.trimmed().isEmpty()) {
                    continue;
                }

                QJsonObject toolUse;
                toolUse.insert(QStringLiteral("type"), QStringLiteral("tool_use"));
                const QString callId = call.id.trimmed().isEmpty()
                    ? QStringLiteral("call_%1").arg(++generatedToolId)
                    : call.id;
                toolUse.insert(QStringLiteral("id"), callId);
                toolUse.insert(QStringLiteral("name"), call.name);
                toolUse.insert(QStringLiteral("input"), call.arguments);
                content.append(toolUse);
            }
        }

        if (content.isEmpty()) {
            continue;
        }

        QJsonObject obj;
        obj.insert(QStringLiteral("role"), mappedRole);
        obj.insert(QStringLiteral("content"), content);
        array.append(obj);
    }

    if (systemPrompt) {
        *systemPrompt = systemBlocks.join(QStringLiteral("\n\n"));
    }

    return array;
}

QJsonArray toGoogleTools(const QJsonArray &openAiTools)
{
    QJsonArray declarations;
    for (const QJsonValue &toolValue : openAiTools) {
        const QJsonObject toolObj = toolValue.toObject();
        const QJsonObject functionObj = toolObj.value(QStringLiteral("function")).toObject();
        const QString toolName = functionObj.value(QStringLiteral("name")).toString();
        if (toolName.trimmed().isEmpty()) {
            continue;
        }

        QJsonObject decl;
        decl.insert(QStringLiteral("name"), toolName);
        const QString desc = functionObj.value(QStringLiteral("description")).toString();
        if (!desc.trimmed().isEmpty()) {
            decl.insert(QStringLiteral("description"), desc);
        }
        decl.insert(QStringLiteral("parameters"), functionObj.value(QStringLiteral("parameters")).toObject());
        declarations.append(decl);
    }

    if (declarations.isEmpty()) {
        return QJsonArray();
    }

    QJsonObject wrapper;
    wrapper.insert(QStringLiteral("functionDeclarations"), declarations);

    QJsonArray tools;
    tools.append(wrapper);
    return tools;
}

QJsonArray toGoogleContents(const QVector<LlmMessage> &messages, QString *systemPrompt)
{
    QJsonArray contents;
    QStringList systemBlocks;

    for (const LlmMessage &message : messages) {
        const QString role = message.role.trimmed().toLower();

        if (role == QStringLiteral("system")) {
            if (!message.content.trimmed().isEmpty()) {
                systemBlocks.append(message.content);
            }
            continue;
        }

        QString mappedRole = QStringLiteral("user");
        if (role == QStringLiteral("assistant")) {
            mappedRole = QStringLiteral("model");
        } else if (role == QStringLiteral("tool")) {
            mappedRole = QStringLiteral("function");
        }

        QJsonArray parts;

        if (role == QStringLiteral("tool")) {
            QJsonObject part;
            QJsonObject functionResponse;
            functionResponse.insert(QStringLiteral("name"), message.name.trimmed().isEmpty() ? QStringLiteral("tool") : message.name);
            const QJsonDocument parsed = QJsonDocument::fromJson(message.content.toUtf8());
            if (parsed.isObject()) {
                functionResponse.insert(QStringLiteral("response"), parsed.object());
            } else {
                QJsonObject fallback;
                fallback.insert(QStringLiteral("result"), message.content);
                functionResponse.insert(QStringLiteral("response"), fallback);
            }
            part.insert(QStringLiteral("functionResponse"), functionResponse);
            parts.append(part);
        } else {
            if (!message.content.trimmed().isEmpty()) {
                QJsonObject textPart;
                textPart.insert(QStringLiteral("text"), message.content);
                parts.append(textPart);
            }

            if (role == QStringLiteral("assistant")) {
                for (const LlmToolCall &call : message.toolCalls) {
                    if (call.name.trimmed().isEmpty()) {
                        continue;
                    }

                    QJsonObject part;
                    QJsonObject functionCall;
                    functionCall.insert(QStringLiteral("name"), call.name);
                    functionCall.insert(QStringLiteral("args"), call.arguments);
                    part.insert(QStringLiteral("functionCall"), functionCall);
                    parts.append(part);
                }
            }
        }

        if (parts.isEmpty()) {
            continue;
        }

        QJsonObject item;
        item.insert(QStringLiteral("role"), mappedRole);
        item.insert(QStringLiteral("parts"), parts);
        contents.append(item);
    }

    if (systemPrompt) {
        *systemPrompt = systemBlocks.join(QStringLiteral("\n\n"));
    }

    return contents;
}

LlmResponse parseOpenAiResponse(const QJsonObject &root)
{
    LlmResponse response;
    LlmMessage assistant;
    assistant.role = QStringLiteral("assistant");
    QHash<QString, PendingToolCall> pendingCalls;
    QString reasoningText;

    if (root.contains(QStringLiteral("choices"))) {
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) {
            response.errorMessage = QStringLiteral("No choices found in response");
            return response;
        }

        const QJsonObject firstChoice = choices.first().toObject();
        response.finishReason = firstChoice.value(QStringLiteral("finish_reason")).toString();

        const QJsonObject message = firstChoice.value(QStringLiteral("message")).toObject();
        assistant.role = message.value(QStringLiteral("role")).toString(assistant.role);
        assistant.content = collectTextFromValue(message.value(QStringLiteral("content")));
        reasoningText = extractOpenAiReasoning(message);

        const QJsonArray toolCalls = message.value(QStringLiteral("tool_calls")).toArray();
        for (int index = 0; index < toolCalls.size(); ++index) {
            mergePendingToolCall(toolCalls.at(index).toObject(), pendingCalls, false, index);
        }
    } else if (root.contains(QStringLiteral("message"))) {
        const QJsonObject message = root.value(QStringLiteral("message")).toObject();
        assistant.role = message.value(QStringLiteral("role")).toString(assistant.role);
        assistant.content = collectTextFromValue(message.value(QStringLiteral("content")));
        reasoningText = extractOpenAiReasoning(message);

        const QJsonArray toolCalls = message.value(QStringLiteral("tool_calls")).toArray();
        for (int index = 0; index < toolCalls.size(); ++index) {
            mergePendingToolCall(toolCalls.at(index).toObject(), pendingCalls, false, index);
        }

        response.finishReason = root.value(QStringLiteral("done_reason")).toString();
    } else {
        response.errorMessage = QStringLiteral("No choices or message found in response");
        return response;
    }

    appendToolCallsToAssistant(pendingCalls, assistant);

    assistant.reasoningContent = reasoningText;
    response.assistantMessage = assistant;
    response.text = !assistant.content.isEmpty() ? assistant.content : reasoningText;

    if (!response.text.isEmpty() || !assistant.toolCalls.isEmpty()) {
        response.success = true;
    } else {
        response.errorMessage = QStringLiteral("Empty assistant message in response");
    }

    return response;
}

LlmResponse parseAnthropicResponse(const QJsonObject &root)
{
    LlmResponse response;
    response.finishReason = root.value(QStringLiteral("stop_reason")).toString();

    LlmMessage assistant;
    assistant.role = QStringLiteral("assistant");

    QStringList texts;
    const QJsonArray content = root.value(QStringLiteral("content")).toArray();
    for (const QJsonValue &value : content) {
        const QJsonObject block = value.toObject();
        const QString type = block.value(QStringLiteral("type")).toString();

        if (type == QStringLiteral("text")) {
            const QString text = block.value(QStringLiteral("text")).toString();
            if (!text.isEmpty()) {
                texts.append(text);
            }
            continue;
        }

        if (type == QStringLiteral("tool_use")) {
            LlmToolCall call;
            call.id = block.value(QStringLiteral("id")).toString();
            call.type = QStringLiteral("function");
            call.name = block.value(QStringLiteral("name")).toString();
            call.arguments = block.value(QStringLiteral("input")).toObject();
            if (!call.name.trimmed().isEmpty()) {
                assistant.toolCalls.append(call);
            }
        }
    }

    assistant.content = texts.join(QStringLiteral("\n"));
    response.assistantMessage = assistant;
    response.text = assistant.content;

    if (!assistant.content.isEmpty() || !assistant.toolCalls.isEmpty()) {
        response.success = true;
    } else {
        response.errorMessage = QStringLiteral("Empty assistant message in anthropic response");
    }

    return response;
}

LlmResponse parseGoogleResponse(const QJsonObject &root)
{
    LlmResponse response;

    const QJsonArray candidates = root.value(QStringLiteral("candidates")).toArray();
    if (candidates.isEmpty()) {
        response.errorMessage = QStringLiteral("No candidates found in google response");
        return response;
    }

    const QJsonObject candidate = candidates.first().toObject();
    response.finishReason = candidate.value(QStringLiteral("finishReason")).toString();

    const QJsonObject contentObj = candidate.value(QStringLiteral("content")).toObject();
    const QJsonArray parts = contentObj.value(QStringLiteral("parts")).toArray();

    LlmMessage assistant;
    assistant.role = QStringLiteral("assistant");

    QStringList texts;
    for (const QJsonValue &value : parts) {
        const QJsonObject part = value.toObject();

        const QString text = part.value(QStringLiteral("text")).toString();
        if (!text.isEmpty()) {
            texts.append(text);
        }

        const QJsonObject functionCall = part.value(QStringLiteral("functionCall")).toObject();
        const QString callName = functionCall.value(QStringLiteral("name")).toString();
        if (!callName.trimmed().isEmpty()) {
            LlmToolCall call;
            call.type = QStringLiteral("function");
            call.name = callName;
            call.arguments = functionCall.value(QStringLiteral("args")).toObject();
            assistant.toolCalls.append(call);
        }
    }

    assistant.content = texts.join(QStringLiteral("\n"));
    response.assistantMessage = assistant;
    response.text = assistant.content;

    if (!assistant.content.isEmpty() || !assistant.toolCalls.isEmpty()) {
        response.success = true;
    } else {
        response.errorMessage = QStringLiteral("Empty assistant message in google response");
    }

    return response;
}

} // namespace

QString OpenAICompatibleProvider::name() const
{
    return QStringLiteral("openai-compatible");
}

void OpenAICompatibleProvider::setConfig(const LlmConfig &config)
{
    m_config = config;
}

QNetworkRequest OpenAICompatibleProvider::buildRequest(const LlmRequest &request) const
{
    const QString vendor = resolveVendor(m_config, request);
    const VendorSchema schema = vendorSchema(vendor);

    QUrl url(m_config.baseUrl);

    if (schema == VendorSchema::Anthropic) {
        if (!url.path().endsWith(QStringLiteral("/messages"))) {
            QString path = url.path();
            if (!path.endsWith('/')) {
                path += '/';
            }
            path += QStringLiteral("messages");
            url.setPath(path);
        }
    } else if (schema == VendorSchema::Google) {
        const QString model = request.model.trimmed().isEmpty() ? m_config.model : request.model;
        QString path = url.path();

        if (!path.contains(QStringLiteral(":generateContent"))) {
            if (path.contains(QStringLiteral("/models/"))) {
                if (!path.endsWith(QStringLiteral(":generateContent"))) {
                    path += QStringLiteral(":generateContent");
                }
            } else {
                if (!path.endsWith('/')) {
                    path += '/';
                }
                path += QStringLiteral("v1beta/models/") + model + QStringLiteral(":generateContent");
            }
            url.setPath(path);
        }

        if (!m_config.apiKey.isEmpty()) {
            QUrlQuery query(url);
            if (!query.hasQueryItem(QStringLiteral("key"))) {
                query.addQueryItem(QStringLiteral("key"), m_config.apiKey);
                url.setQuery(query);
            }
        }
    } else {
        if (!url.path().endsWith(QStringLiteral("/chat/completions"))) {
            QString path = url.path();
            if (!path.endsWith('/')) {
                path += '/';
            }
            path += QStringLiteral("chat/completions");
            url.setPath(path);
        }
    }

    QNetworkRequest networkRequest(url);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    if (schema == VendorSchema::Anthropic) {
        if (!m_config.apiKey.isEmpty()) {
            networkRequest.setRawHeader("x-api-key", m_config.apiKey.toUtf8());
        }
        networkRequest.setRawHeader("anthropic-version", QByteArray("2023-06-01"));
    } else if (schema == VendorSchema::Google) {
        if (!m_config.apiKey.isEmpty()) {
            networkRequest.setRawHeader("x-goog-api-key", m_config.apiKey.toUtf8());
        }
    } else {
        if (!m_config.apiKey.isEmpty()) {
            const QByteArray bearer = "Bearer " + m_config.apiKey.toUtf8();
            networkRequest.setRawHeader("Authorization", bearer);
        }
    }

    return networkRequest;
}

QByteArray OpenAICompatibleProvider::buildPayload(const LlmRequest &request) const
{
    const QString vendor = resolveVendor(m_config, request);
    const VendorSchema schema = vendorSchema(vendor);
    const QString model = request.model.trimmed().isEmpty() ? m_config.model : request.model;

    QJsonObject root;
    const QVector<LlmMessage> messages =
        StructuredOutputService::constrainedMessages(request.messages, request.output);

    if (schema == VendorSchema::Anthropic) {
        root.insert(QStringLiteral("model"), model);
        root.insert(QStringLiteral("stream"), request.stream);

        QString systemPrompt;
        root.insert(QStringLiteral("messages"), toAnthropicMessages(messages, &systemPrompt));
        if (!systemPrompt.trimmed().isEmpty()) {
            root.insert(QStringLiteral("system"), systemPrompt);
        }

        const QJsonArray tools = toAnthropicTools(request.tools);
        if (!tools.isEmpty()) {
            root.insert(QStringLiteral("tools"), tools);
        }

        return QJsonDocument(root).toJson(QJsonDocument::Compact);
    }

    if (schema == VendorSchema::Google) {
        QString systemPrompt;
        root.insert(QStringLiteral("contents"), toGoogleContents(messages, &systemPrompt));

        if (!systemPrompt.trimmed().isEmpty()) {
            QJsonObject systemInstruction;
            QJsonArray parts;
            QJsonObject textPart;
            textPart.insert(QStringLiteral("text"), systemPrompt);
            parts.append(textPart);
            systemInstruction.insert(QStringLiteral("parts"), parts);
            root.insert(QStringLiteral("systemInstruction"), systemInstruction);
        }

        const QJsonArray tools = toGoogleTools(request.tools);
        if (!tools.isEmpty()) {
            root.insert(QStringLiteral("tools"), tools);
        }

        const QJsonObject generationConfig =
            StructuredOutputService::googleGenerationConfig(request.output);
        if (!generationConfig.isEmpty()) {
            root.insert(QStringLiteral("generationConfig"), generationConfig);
        }

        return QJsonDocument(root).toJson(QJsonDocument::Compact);
    }

    root.insert(QStringLiteral("model"), model);
    root.insert(QStringLiteral("stream"), request.stream);
    root.insert(QStringLiteral("messages"), toOpenAiMessages(messages));
    if (!request.tools.isEmpty()) {
        root.insert(QStringLiteral("tools"), request.tools);
    }

    const QJsonObject responseFormat =
        StructuredOutputService::openAiChatResponseFormat(request.output);
    if (!responseFormat.isEmpty()) {
        root.insert(QStringLiteral("response_format"), responseFormat);
    }

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

LlmResponse OpenAICompatibleProvider::parseResponse(const QByteArray &data) const
{
    const QByteArray trimmedData = data.trimmed();
    const QString vendor = inferVendor(m_config.modelVendor, m_config.model);
    const VendorSchema schema = vendorSchema(vendor);

    if (looksLikeSsePayload(trimmedData)) {
        const QList<StreamEvent> events = parseSseEvents(trimmedData);

        if (schema == VendorSchema::OpenAI) {
            OpenAiStreamState state;
            for (const StreamEvent &event : events) {
                if (event.isDoneMarker || event.object.isEmpty()) {
                    continue;
                }

                const QJsonObject root = event.object;
                state.lastRoot = root;

                if (root.contains(QStringLiteral("error")) && state.errorMessage.isEmpty()) {
                    state.errorMessage = root.value(QStringLiteral("error")).toObject()
                                           .value(QStringLiteral("message")).toString();
                }

                const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
                if (choices.isEmpty()) {
                    continue;
                }

                const QJsonObject choice = choices.first().toObject();
                state.finishReason = choice.value(QStringLiteral("finish_reason")).toString(state.finishReason);

                const QJsonObject delta = choice.value(QStringLiteral("delta")).toObject();
                const QString content = collectTextFromValue(delta.value(QStringLiteral("content")));
                if (!content.isEmpty()) {
                    state.contentParts.append(content);
                }

                const QString reasoning = extractOpenAiReasoning(delta);
                if (!reasoning.isEmpty()) {
                    state.reasoningParts.append(reasoning);
                }

                const QJsonArray deltaToolCalls = delta.value(QStringLiteral("tool_calls")).toArray();
                for (int index = 0; index < deltaToolCalls.size(); ++index) {
                    mergePendingToolCall(deltaToolCalls.at(index).toObject(), state.toolCalls, true, index);
                }

                const QJsonObject message = choice.value(QStringLiteral("message")).toObject();
                if (!message.isEmpty()) {
                    const QJsonArray messageToolCalls = message.value(QStringLiteral("tool_calls")).toArray();
                    for (int index = 0; index < messageToolCalls.size(); ++index) {
                        mergePendingToolCall(messageToolCalls.at(index).toObject(), state.toolCalls, false, index);
                    }
                }
            }

            if (state.lastRoot.isEmpty()) {
                LlmResponse response;
                response.errorMessage = QStringLiteral("Invalid SSE response: no JSON events found");
                return response;
            }

            LlmResponse response;
            response.finishReason = state.finishReason;
            response.assistantMessage.role = QStringLiteral("assistant");
            response.assistantMessage.content = !state.contentParts.isEmpty()
                ? state.contentParts.join(QString())
                : state.reasoningParts.join(QString());
            response.assistantMessage.reasoningContent = state.reasoningParts.join(QString());
            response.text = response.assistantMessage.content;
            appendToolCallsToAssistant(state.toolCalls, response.assistantMessage);
            response.success = !response.text.isEmpty() || !response.assistantMessage.toolCalls.isEmpty();

            if (!response.success) {
                response = parseOpenAiResponse(state.lastRoot);
                if (!response.success && !state.errorMessage.isEmpty()) {
                    response.errorMessage = state.errorMessage;
                }
                if (!response.success && response.errorMessage.isEmpty()) {
                    response.errorMessage = QStringLiteral("Empty assistant message in SSE response");
                }
            }

            return response;
        }

        QJsonObject lastRoot;
        for (const StreamEvent &event : events) {
            if (!event.isDoneMarker && !event.object.isEmpty()) {
                lastRoot = event.object;
            }
        }

        if (lastRoot.isEmpty()) {
            LlmResponse response;
            response.errorMessage = QStringLiteral("Invalid SSE response: no JSON events found");
            return response;
        }

        if (schema == VendorSchema::Anthropic) {
            return parseAnthropicResponse(lastRoot);
        }

        return parseGoogleResponse(lastRoot);
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmedData, &error);
    if (doc.isObject()) {
        const QJsonObject root = doc.object();
        if (schema == VendorSchema::Anthropic || root.contains(QStringLiteral("stop_reason"))) {
            return parseAnthropicResponse(root);
        }

        if (schema == VendorSchema::Google || root.contains(QStringLiteral("candidates"))) {
            return parseGoogleResponse(root);
        }

        return parseOpenAiResponse(root);
    }

    if (schema == VendorSchema::OpenAI) {
        const QList<QJsonObject> jsonLines = parseJsonLines(trimmedData);
        if (!jsonLines.isEmpty()) {
            OpenAiStreamState state;
            for (const QJsonObject &root : jsonLines) {
                state.lastRoot = root;
                if (root.contains(QStringLiteral("error")) && state.errorMessage.isEmpty()) {
                    state.errorMessage = root.value(QStringLiteral("error")).toObject()
                                           .value(QStringLiteral("message")).toString();
                }

                if (root.contains(QStringLiteral("choices"))) {
                    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
                    if (!choices.isEmpty()) {
                        const QJsonObject choice = choices.first().toObject();
                        state.finishReason = choice.value(QStringLiteral("finish_reason")).toString(state.finishReason);

                        const QJsonObject delta = choice.value(QStringLiteral("delta")).toObject();
                        const QString content = collectTextFromValue(delta.value(QStringLiteral("content")));
                        if (!content.isEmpty()) {
                            state.contentParts.append(content);
                        }

                        const QString reasoning = extractOpenAiReasoning(delta);
                        if (!reasoning.isEmpty()) {
                            state.reasoningParts.append(reasoning);
                        }

                        const QJsonArray deltaToolCalls = delta.value(QStringLiteral("tool_calls")).toArray();
                        for (int index = 0; index < deltaToolCalls.size(); ++index) {
                            mergePendingToolCall(deltaToolCalls.at(index).toObject(), state.toolCalls, true, index);
                        }
                    }
                    continue;
                }

                if (root.contains(QStringLiteral("message"))) {
                    const QJsonObject message = root.value(QStringLiteral("message")).toObject();
                    const QString content = collectTextFromValue(message.value(QStringLiteral("content")));
                    if (!content.isEmpty()) {
                        state.contentParts.append(content);
                    }

                    const QString reasoning = extractOpenAiReasoning(message);
                    if (!reasoning.isEmpty()) {
                        state.reasoningParts.append(reasoning);
                    }

                    const QJsonArray toolCalls = message.value(QStringLiteral("tool_calls")).toArray();
                    for (int index = 0; index < toolCalls.size(); ++index) {
                        mergePendingToolCall(toolCalls.at(index).toObject(), state.toolCalls, false, index);
                    }

                    state.finishReason = root.value(QStringLiteral("done_reason")).toString(state.finishReason);
                }
            }

            LlmResponse response;
            response.finishReason = state.finishReason;
            response.assistantMessage.role = QStringLiteral("assistant");
            response.assistantMessage.content = !state.contentParts.isEmpty()
                ? state.contentParts.join(QString())
                : state.reasoningParts.join(QString());
            response.assistantMessage.reasoningContent = state.reasoningParts.join(QString());
            response.text = response.assistantMessage.content;
            appendToolCallsToAssistant(state.toolCalls, response.assistantMessage);
            response.success = !response.text.isEmpty() || !response.assistantMessage.toolCalls.isEmpty();

            if (!response.success && !state.lastRoot.isEmpty()) {
                response = parseOpenAiResponse(state.lastRoot);
            }
            if (!response.success) {
                response.errorMessage = !state.errorMessage.isEmpty()
                    ? state.errorMessage
                    : QStringLiteral("Invalid JSON response: ") + error.errorString();
            }
            return response;
        }
    }

    {
        LlmResponse response;
        response.errorMessage = QStringLiteral("Invalid JSON response: ")
            + error.errorString()
            + QStringLiteral("\n")
            + QString::fromUtf8(trimmedData.left(2048));
        return response;
    }
}

QList<LlmStreamDelta> OpenAICompatibleProvider::parseStreamDeltas(const QByteArray &chunk) const
{
    QList<LlmStreamDelta> deltas;

    const QString vendor = inferVendor(m_config.modelVendor, m_config.model);
    const VendorSchema schema = vendorSchema(vendor);

    const QList<QByteArray> lines = chunk.split('\n');
    for (const QByteArray &line : lines) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith("event:") || trimmed.startsWith(':')) {
            continue;
        }

        QByteArray payload = trimmed;
        if (trimmed.startsWith("data:")) {
            payload = trimmed.mid(5).trimmed();
        }
        if (payload == "[DONE]" || payload.isEmpty()) {
            continue;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (!doc.isObject()) {
            continue;
        }

        const QJsonObject root = doc.object();

        if (schema == VendorSchema::Anthropic || root.contains(QStringLiteral("delta"))) {
            const QJsonObject delta = root.value(QStringLiteral("delta")).toObject();
            const QString text = delta.value(QStringLiteral("text")).toString();
            if (!text.isEmpty()) {
                LlmStreamDelta streamDelta;
                streamDelta.channel = QStringLiteral("content");
                streamDelta.text = text;
                deltas.append(streamDelta);
            }
            continue;
        }

        if (schema == VendorSchema::Google || root.contains(QStringLiteral("candidates"))) {
            const QJsonArray candidates = root.value(QStringLiteral("candidates")).toArray();
            if (!candidates.isEmpty()) {
                const QJsonArray parts = candidates.first().toObject()
                                             .value(QStringLiteral("content")).toObject()
                                             .value(QStringLiteral("parts")).toArray();
                for (const QJsonValue &partValue : parts) {
                    const QString text = partValue.toObject().value(QStringLiteral("text")).toString();
                    if (!text.isEmpty()) {
                        LlmStreamDelta streamDelta;
                        streamDelta.channel = QStringLiteral("content");
                        streamDelta.text = text;
                        deltas.append(streamDelta);
                    }
                }
            }
            continue;
        }

        if (root.contains(QStringLiteral("message"))) {
            const QJsonObject message = root.value(QStringLiteral("message")).toObject();
            const QString content = collectTextFromValue(message.value(QStringLiteral("content")));
            if (!content.isEmpty()) {
                LlmStreamDelta streamDelta;
                streamDelta.channel = QStringLiteral("content");
                streamDelta.text = content;
                deltas.append(streamDelta);
            }

            const QString reasoning = extractOpenAiReasoning(message);
            if (!reasoning.isEmpty()) {
                LlmStreamDelta streamDelta;
                streamDelta.channel = QStringLiteral("reasoning");
                streamDelta.text = reasoning;
                deltas.append(streamDelta);
            }
            continue;
        }

        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) {
            continue;
        }

        const QJsonObject delta = choices.first().toObject().value(QStringLiteral("delta")).toObject();
        const QString content = collectTextFromValue(delta.value(QStringLiteral("content")));
        if (!content.isEmpty()) {
            LlmStreamDelta streamDelta;
            streamDelta.channel = QStringLiteral("content");
            streamDelta.text = content;
            deltas.append(streamDelta);
        }

        const QString reasoning = extractOpenAiReasoning(delta);
        if (!reasoning.isEmpty()) {
            LlmStreamDelta streamDelta;
            streamDelta.channel = QStringLiteral("reasoning");
            streamDelta.text = reasoning;
            deltas.append(streamDelta);
        }
    }

    return deltas;
}
QList<QString> OpenAICompatibleProvider::parseStreamTokens(const QByteArray &chunk) const
{
    QList<QString> tokens;
    const QList<LlmStreamDelta> deltas = parseStreamDeltas(chunk);
    for (const LlmStreamDelta &delta : deltas) {
        if (delta.channel == QStringLiteral("content")) {
            tokens.append(delta.text);
        }
    }

    return tokens;
}

bool OpenAICompatibleProvider::supportsStructuredToolCalls() const
{
    return true;
}

} // namespace qtllm



