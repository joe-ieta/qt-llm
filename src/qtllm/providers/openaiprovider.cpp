#include "openaiprovider.h"

#include "../structuredoutput/structuredoutputservice.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QUrl>

namespace qtllm {

namespace {

struct ToolLoopResultItem
{
    QString callId;
    QString toolId;
    QString output;
    bool success = false;
    QString errorCode;
};

struct PendingFunctionCall
{
    QString callId;
    QString name;
    QString argumentsText;
};

struct SseEvent
{
    QString eventName;
    QByteArray data;
    QJsonObject object;
    bool isDoneMarker = false;
};

struct SseParseState
{
    QStringList textParts;
    QStringList refusalParts;
    QString finishReason;
    QString responseError;
    QHash<QString, PendingFunctionCall> calls;
    QJsonObject latestResponse;
    QJsonObject completedResponse;
};

QString compactJson(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QJsonObject buildMessageItem(const QString &role,
                             const QString &contentType,
                             const QString &text)
{
    QJsonObject item;
    item.insert(QStringLiteral("type"), QStringLiteral("message"));
    item.insert(QStringLiteral("role"), role);

    QJsonArray content;
    QJsonObject textItem;
    textItem.insert(QStringLiteral("type"), contentType);
    textItem.insert(QStringLiteral("text"), text);
    content.append(textItem);
    item.insert(QStringLiteral("content"), content);
    return item;
}

QJsonObject buildFunctionCallItem(const LlmToolCall &call)
{
    QJsonObject item;
    item.insert(QStringLiteral("type"), QStringLiteral("function_call"));
    if (!call.id.trimmed().isEmpty()) {
        item.insert(QStringLiteral("call_id"), call.id);
    }
    item.insert(QStringLiteral("name"), call.name);
    item.insert(QStringLiteral("arguments"), QString::fromUtf8(QJsonDocument(call.arguments).toJson(QJsonDocument::Compact)));
    return item;
}

QString extractToolResultsJson(const QString &content)
{
    if (!content.startsWith(QStringLiteral("[tool-loop]"))) {
        return QString();
    }

    const QStringList lines = content.split('\n');
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("tool_results="))) {
            return line.mid(QStringLiteral("tool_results=").size()).trimmed();
        }
    }

    return QString();
}

QList<ToolLoopResultItem> parseToolLoopResults(const QString &content)
{
    QList<ToolLoopResultItem> items;
    const QString payload = extractToolResultsJson(content);
    if (payload.isEmpty()) {
        return items;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
    if (!doc.isArray()) {
        return items;
    }

    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        ToolLoopResultItem item;
        item.callId = object.value(QStringLiteral("call_id")).toString();
        item.toolId = object.value(QStringLiteral("tool")).toString();
        item.output = object.value(QStringLiteral("output")).toString(object.value(QStringLiteral("content")).toString());
        item.success = object.value(QStringLiteral("success")).toBool(false);
        item.errorCode = object.value(QStringLiteral("error_code")).toString();
        if (!item.callId.trimmed().isEmpty()) {
            items.append(item);
        }
    }

    return items;
}

QJsonArray toResponseInput(const QVector<LlmMessage> &messages, QString *instructions)
{
    QJsonArray input;
    QStringList instructionBlocks;

    for (const LlmMessage &message : messages) {
        const QString role = message.role.trimmed().toLower();

        if (role == QStringLiteral("system") || role == QStringLiteral("developer")) {
            if (!message.content.trimmed().isEmpty()) {
                instructionBlocks.append(message.content);
            }
            continue;
        }

        if (role == QStringLiteral("tool")) {
            QJsonObject item;
            item.insert(QStringLiteral("type"), QStringLiteral("function_call_output"));
            item.insert(QStringLiteral("call_id"), message.toolCallId);
            item.insert(QStringLiteral("output"), message.content);
            input.append(item);
            continue;
        }

        if (role == QStringLiteral("user")) {
            const QList<ToolLoopResultItem> toolResults = parseToolLoopResults(message.content);
            if (!toolResults.isEmpty()) {
                for (const ToolLoopResultItem &result : toolResults) {
                    QJsonObject item;
                    item.insert(QStringLiteral("type"), QStringLiteral("function_call_output"));
                    item.insert(QStringLiteral("call_id"), result.callId);
                    item.insert(QStringLiteral("output"), result.output);
                    input.append(item);
                }
                continue;
            }
        }

        if (!message.content.trimmed().isEmpty()) {
            const QString contentType = role == QStringLiteral("assistant")
                ? QStringLiteral("output_text")
                : QStringLiteral("input_text");
            input.append(buildMessageItem(role, contentType, message.content));
        }

        if (role == QStringLiteral("assistant")) {
            for (const LlmToolCall &call : message.toolCalls) {
                if (!call.name.trimmed().isEmpty()) {
                    input.append(buildFunctionCallItem(call));
                }
            }
        }
    }

    if (instructions) {
        *instructions = instructionBlocks.join(QStringLiteral("\n\n"));
    }

    return input;
}

QJsonValue sanitizeSchemaValue(const QJsonValue &value);

QJsonArray sanitizeSchemaArray(const QJsonArray &array)
{
    QJsonArray sanitized;
    for (const QJsonValue &entry : array) {
        sanitized.append(sanitizeSchemaValue(entry));
    }
    return sanitized;
}

QJsonObject sanitizeSchemaObject(const QJsonObject &object)
{
    static const QSet<QString> blockedKeys = {
        QStringLiteral("$schema"),
        QStringLiteral("$id"),
        QStringLiteral("$comment"),
        QStringLiteral("definitions"),
        QStringLiteral("$defs"),
        QStringLiteral("default"),
        QStringLiteral("example"),
        QStringLiteral("examples"),
        QStringLiteral("title"),
        QStringLiteral("deprecated"),
        QStringLiteral("readOnly"),
        QStringLiteral("writeOnly"),
        QStringLiteral("nullable"),
        QStringLiteral("contentEncoding"),
        QStringLiteral("contentMediaType")
    };

    QJsonObject sanitized;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (blockedKeys.contains(it.key())) {
            continue;
        }
        sanitized.insert(it.key(), sanitizeSchemaValue(it.value()));
    }

    if (sanitized.contains(QStringLiteral("properties"))
        && !sanitized.contains(QStringLiteral("type"))) {
        sanitized.insert(QStringLiteral("type"), QStringLiteral("object"));
    }

    return sanitized;
}

QJsonValue sanitizeSchemaValue(const QJsonValue &value)
{
    if (value.isObject()) {
        return sanitizeSchemaObject(value.toObject());
    }
    if (value.isArray()) {
        return sanitizeSchemaArray(value.toArray());
    }
    return value;
}
QJsonArray toResponseTools(const QJsonArray &openAiTools)
{
    QJsonArray tools;
    for (const QJsonValue &value : openAiTools) {
        const QJsonObject descriptor = value.toObject();
        const QJsonObject function = descriptor.value(QStringLiteral("function")).toObject();
        const QString type = descriptor.value(QStringLiteral("type")).toString();
        if (type != QStringLiteral("function") || function.isEmpty()) {
            continue;
        }

        const QString name = function.value(QStringLiteral("name")).toString();
        if (name.trimmed().isEmpty()) {
            continue;
        }

        QJsonObject tool;
        tool.insert(QStringLiteral("type"), QStringLiteral("function"));
        tool.insert(QStringLiteral("name"), name);
        tool.insert(QStringLiteral("description"), function.value(QStringLiteral("description")).toString());
        tool.insert(QStringLiteral("parameters"), sanitizeSchemaObject(function.value(QStringLiteral("parameters")).toObject()));
        tool.insert(QStringLiteral("strict"), false);
        tools.append(tool);
    }
    return tools;
}

QString collectMessageText(const QJsonArray &content)
{
    QStringList texts;
    for (const QJsonValue &value : content) {
        const QJsonObject part = value.toObject();
        const QString type = part.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("output_text") || type == QStringLiteral("text") || type == QStringLiteral("input_text")) {
            const QString text = part.value(QStringLiteral("text")).toString();
            if (!text.isEmpty()) {
                texts.append(text);
            }
            continue;
        }

        if (type == QStringLiteral("refusal")) {
            const QString refusal = part.value(QStringLiteral("refusal")).toString();
            if (!refusal.isEmpty()) {
                texts.append(refusal);
            }
        }
    }
    return texts.join(QString());
}

LlmResponse parseResponseObject(const QJsonObject &root)
{
    LlmResponse response;
    response.finishReason = root.value(QStringLiteral("status")).toString();

    LlmMessage assistant;
    assistant.role = QStringLiteral("assistant");

    const QJsonArray output = root.value(QStringLiteral("output")).toArray();
    for (const QJsonValue &value : output) {
        const QJsonObject item = value.toObject();
        const QString type = item.value(QStringLiteral("type")).toString();

        if (type == QStringLiteral("message")) {
            const QString text = collectMessageText(item.value(QStringLiteral("content")).toArray());
            if (!text.isEmpty()) {
                assistant.content += text;
            }
            continue;
        }

        if (type == QStringLiteral("function_call")) {
            LlmToolCall call;
            call.id = item.value(QStringLiteral("call_id")).toString(item.value(QStringLiteral("id")).toString());
            call.name = item.value(QStringLiteral("name")).toString();
            const QString argumentsText = item.value(QStringLiteral("arguments")).toString();
            const QJsonDocument argumentsDoc = QJsonDocument::fromJson(argumentsText.toUtf8());
            if (argumentsDoc.isObject()) {
                call.arguments = argumentsDoc.object();
            }
            if (!call.name.trimmed().isEmpty()) {
                assistant.toolCalls.append(call);
            }
        }
    }

    response.assistantMessage = assistant;
    response.text = assistant.content;
    response.success = !assistant.content.isEmpty() || !assistant.toolCalls.isEmpty();
    if (!response.success) {
        response.errorMessage = QStringLiteral("Empty assistant message in OpenAI responses payload");
    }
    return response;
}

QJsonObject parseSseEvent(const QByteArray &chunk)
{
    const QByteArray trimmed = chunk.trimmed();
    if (!trimmed.startsWith("data:")) {
        return QJsonObject();
    }

    const QByteArray payload = trimmed.mid(5).trimmed();
    if (payload.isEmpty() || payload == "[DONE]") {
        return QJsonObject();
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    return doc.isObject() ? doc.object() : QJsonObject();
}

bool looksLikeSsePayload(const QByteArray &data)
{
    const QByteArray trimmed = data.trimmed();
    return trimmed.startsWith("data:")
        || trimmed.startsWith("event:")
        || trimmed.contains("\ndata:")
        || trimmed.contains("\nevent:");
}

QList<SseEvent> parseSseEvents(const QByteArray &payload)
{
    QList<SseEvent> events;
    QString eventName;
    QList<QByteArray> dataLines;

    const auto flushEvent = [&events, &eventName, &dataLines]() {
        const QByteArray joined = dataLines.join("\n").trimmed();
        if (joined.isEmpty()) {
            eventName.clear();
            dataLines.clear();
            return;
        }

        SseEvent event;
        event.eventName = eventName;
        event.data = joined;
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

QString extractResponseError(const QJsonObject &root)
{
    const QJsonObject error = root.value(QStringLiteral("error")).toObject();
    if (!error.isEmpty()) {
        const QString message = error.value(QStringLiteral("message")).toString();
        if (!message.isEmpty()) {
            return message;
        }
    }
    return QString();
}

QString functionCallKey(const QJsonObject &object)
{
    return object.value(QStringLiteral("call_id")).toString(
        object.value(QStringLiteral("item_id")).toString(
            object.value(QStringLiteral("id")).toString()));
}

bool isTerminalResponseEvent(const QString &type)
{
    return type == QStringLiteral("response.completed")
        || type == QStringLiteral("response.incomplete")
        || type == QStringLiteral("response.failed")
        || type == QStringLiteral("response.cancelled");
}

QString extractPartText(const QJsonObject &part)
{
    const QString type = part.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("output_text") || type == QStringLiteral("text") || type == QStringLiteral("input_text")) {
        return part.value(QStringLiteral("text")).toString();
    }

    if (type == QStringLiteral("refusal")) {
        return part.value(QStringLiteral("refusal")).toString();
    }

    if (type == QStringLiteral("summary_text")) {
        return part.value(QStringLiteral("text")).toString();
    }

    return QString();
}

void updateResponseStateFromObject(const QJsonObject &responseObject, SseParseState &state, bool isTerminal)
{
    if (responseObject.isEmpty()) {
        return;
    }

    state.latestResponse = responseObject;
    state.finishReason = responseObject.value(QStringLiteral("status")).toString(state.finishReason);
    if (state.responseError.isEmpty()) {
        state.responseError = extractResponseError(responseObject);
    }
    if (isTerminal) {
        state.completedResponse = responseObject;
    }
}

void mergeOutputItemIntoState(const QJsonObject &item, SseParseState &state)
{
    const QString itemType = item.value(QStringLiteral("type")).toString();
    if (itemType == QStringLiteral("message")) {
        const QString text = collectMessageText(item.value(QStringLiteral("content")).toArray());
        if (!text.isEmpty()) {
            state.textParts.append(text);
        }
        return;
    }

    if (itemType == QStringLiteral("function_call")) {
        const QString callId = functionCallKey(item);
        PendingFunctionCall &call = state.calls[callId];
        call.callId = callId;
        call.name = item.value(QStringLiteral("name")).toString(call.name);
        const QString argumentsText = item.value(QStringLiteral("arguments")).toString();
        if (!argumentsText.isEmpty()) {
            call.argumentsText = argumentsText;
        }
        return;
    }
}

void mergeContentPartIntoState(const QJsonObject &part, SseParseState &state)
{
    const QString type = part.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("refusal")) {
        const QString refusal = part.value(QStringLiteral("refusal")).toString();
        if (!refusal.isEmpty()) {
            state.refusalParts.append(refusal);
        }
        return;
    }

    const QString text = extractPartText(part);
    if (!text.isEmpty()) {
        state.textParts.append(text);
    }
}

bool handleSseEvent(const SseEvent &sseEvent, SseParseState &state)
{
    if (sseEvent.isDoneMarker || sseEvent.object.isEmpty()) {
        return true;
    }

    const QJsonObject event = sseEvent.object;
    const QString type = event.value(QStringLiteral("type")).toString(sseEvent.eventName);

    // OpenAI Responses SSE events currently documented by the streaming guide/API reference:
    // response.created, response.in_progress, response.completed, response.incomplete,
    // response.failed, response.cancelled, response.output_item.added,
    // response.output_item.done, response.content_part.added, response.content_part.done,
    // response.output_text.delta, response.output_text.done,
    // response.output_text.annotation.added, response.refusal.delta, response.refusal.done,
    // response.function_call_arguments.delta, response.function_call_arguments.done,
    // response.file_search_call.in_progress, response.file_search_call.searching,
    // response.file_search_call.completed, response.web_search_call.in_progress,
    // response.web_search_call.searching, response.web_search_call.completed,
    // response.image_generation_call.in_progress,
    // response.image_generation_call.partial_image, response.image_generation_call.completed,
    // response.code_interpreter_call.in_progress,
    // response.code_interpreter_call_code.delta, response.code_interpreter_call_code.done,
    // response.code_interpreter_call.interpreting, response.code_interpreter_call.completed,
    // response.reasoning_summary_part.added, response.reasoning_summary_part.done,
    // response.custom_tool_call_input.delta, response.custom_tool_call_input.done, error.

    if (type == QStringLiteral("response.created") || type == QStringLiteral("response.in_progress")) {
        updateResponseStateFromObject(event.value(QStringLiteral("response")).toObject(), state, false);
        return true;
    }

    if (isTerminalResponseEvent(type)) {
        updateResponseStateFromObject(event.value(QStringLiteral("response")).toObject(), state, true);
        return true;
    }

    if (type == QStringLiteral("response.output_item.added") || type == QStringLiteral("response.output_item.done")) {
        mergeOutputItemIntoState(event.value(QStringLiteral("item")).toObject(), state);
        return true;
    }

    if (type == QStringLiteral("response.content_part.added") || type == QStringLiteral("response.content_part.done")) {
        mergeContentPartIntoState(event.value(QStringLiteral("part")).toObject(), state);
        return true;
    }

    if (type == QStringLiteral("response.output_text.delta")) {
        const QString delta = event.value(QStringLiteral("delta")).toString();
        if (!delta.isEmpty()) {
            state.textParts.append(delta);
        }
        return true;
    }

    if (type == QStringLiteral("response.output_text.done")) {
        const QString text = event.value(QStringLiteral("text")).toString();
        if (!text.isEmpty()) {
            state.textParts.append(text);
        }
        return true;
    }

    if (type == QStringLiteral("response.output_text.annotation.added")) {
        return true;
    }

    if (type == QStringLiteral("response.refusal.delta")) {
        const QString delta = event.value(QStringLiteral("delta")).toString();
        if (!delta.isEmpty()) {
            state.refusalParts.append(delta);
        }
        return true;
    }

    if (type == QStringLiteral("response.refusal.done")) {
        const QString refusal = event.value(QStringLiteral("refusal")).toString();
        if (!refusal.isEmpty()) {
            state.refusalParts.append(refusal);
        }
        return true;
    }

    if (type == QStringLiteral("response.function_call_arguments.delta")) {
        const QString callId = functionCallKey(event);
        PendingFunctionCall &call = state.calls[callId];
        call.callId = callId;
        call.name = event.value(QStringLiteral("name")).toString(call.name);
        call.argumentsText += event.value(QStringLiteral("delta")).toString();
        return true;
    }

    if (type == QStringLiteral("response.function_call_arguments.done")) {
        const QString callId = functionCallKey(event);
        PendingFunctionCall &call = state.calls[callId];
        call.callId = callId;
        call.name = event.value(QStringLiteral("name")).toString(call.name);
        const QString argumentsText = event.value(QStringLiteral("arguments")).toString();
        if (!argumentsText.isEmpty()) {
            call.argumentsText = argumentsText;
        }
        return true;
    }

    if (type == QStringLiteral("response.file_search_call.in_progress")
        || type == QStringLiteral("response.file_search_call.searching")
        || type == QStringLiteral("response.file_search_call.completed")
        || type == QStringLiteral("response.web_search_call.in_progress")
        || type == QStringLiteral("response.web_search_call.searching")
        || type == QStringLiteral("response.web_search_call.completed")
        || type == QStringLiteral("response.image_generation_call.in_progress")
        || type == QStringLiteral("response.image_generation_call.partial_image")
        || type == QStringLiteral("response.image_generation_call.completed")
        || type == QStringLiteral("response.code_interpreter_call.in_progress")
        || type == QStringLiteral("response.code_interpreter_call.interpreting")
        || type == QStringLiteral("response.code_interpreter_call.completed")
        || type == QStringLiteral("response.code_interpreter_call_code.delta")
        || type == QStringLiteral("response.code_interpreter_call_code.done")
        || type == QStringLiteral("response.reasoning_summary_part.added")
        || type == QStringLiteral("response.reasoning_summary_part.done")
        || type == QStringLiteral("response.custom_tool_call_input.delta")
        || type == QStringLiteral("response.custom_tool_call_input.done")) {
        return true;
    }

    if (type == QStringLiteral("error")) {
        if (state.responseError.isEmpty()) {
            state.responseError = event.value(QStringLiteral("message")).toString();
        }
        return true;
    }

    return false;
}

} // namespace

QString OpenAIProvider::name() const
{
    return QStringLiteral("openai");
}

void OpenAIProvider::setConfig(const LlmConfig &config)
{
    m_config = config;
}

QNetworkRequest OpenAIProvider::buildRequest(const LlmRequest &request) const
{
    Q_UNUSED(request)

    QUrl url(m_config.baseUrl);
    if (!url.path().endsWith(QStringLiteral("/responses"))) {
        QString path = url.path();
        if (!path.endsWith('/')) {
            path += '/';
        }
        path += QStringLiteral("responses");
        url.setPath(path);
    }

    QNetworkRequest networkRequest(url);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_config.apiKey.isEmpty()) {
        networkRequest.setRawHeader("Authorization", QByteArray("Bearer ") + m_config.apiKey.toUtf8());
    }
    return networkRequest;
}

QByteArray OpenAIProvider::buildPayload(const LlmRequest &request) const
{
    QJsonObject root;
    root.insert(QStringLiteral("model"), request.model.trimmed().isEmpty() ? m_config.model : request.model);
    root.insert(QStringLiteral("stream"), request.stream);
    root.insert(QStringLiteral("store"), false);

    const QVector<LlmMessage> messages =
        StructuredOutputService::constrainedMessages(request.messages, request.output);
    QString instructions;
    const QJsonArray input = toResponseInput(messages, &instructions);
    root.insert(QStringLiteral("input"), input);
    if (!instructions.trimmed().isEmpty()) {
        root.insert(QStringLiteral("instructions"), instructions);
    }

    const QJsonArray tools = toResponseTools(request.tools);
    if (!tools.isEmpty()) {
        root.insert(QStringLiteral("tools"), tools);
    }

    const QJsonObject text = StructuredOutputService::openAiResponsesText(request.output);
    if (!text.isEmpty()) {
        root.insert(QStringLiteral("text"), text);
    }

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

LlmResponse OpenAIProvider::parseResponse(const QByteArray &data) const
{
    const QByteArray trimmed = data.trimmed();
    if (looksLikeSsePayload(trimmed)) {
        SseParseState state;

        const QList<SseEvent> events = parseSseEvents(trimmed);
        for (const SseEvent &sseEvent : events) {
            handleSseEvent(sseEvent, state);
        }

        LlmResponse response;
        if (!state.completedResponse.isEmpty()) {
            response = parseResponseObject(state.completedResponse);
        } else if (!state.latestResponse.isEmpty()) {
            response = parseResponseObject(state.latestResponse);
        }

        const QString mergedText = !state.textParts.isEmpty()
            ? state.textParts.join(QString())
            : state.refusalParts.join(QString());
        if (response.assistantMessage.content.isEmpty() && !mergedText.isEmpty()) {
            response.assistantMessage.role = QStringLiteral("assistant");
            response.assistantMessage.content = mergedText;
            response.text = response.assistantMessage.content;
            response.success = true;
        }

        for (auto it = state.calls.constBegin(); it != state.calls.constEnd(); ++it) {
            if (it->name.trimmed().isEmpty()) {
                continue;
            }
            bool exists = false;
            for (const LlmToolCall &call : response.assistantMessage.toolCalls) {
                if (call.id == it->callId || call.name == it->name) {
                    exists = true;
                    break;
                }
            }
            if (exists) {
                continue;
            }
            LlmToolCall call;
            call.id = it->callId;
            call.name = it->name;
            const QJsonDocument argsDoc = QJsonDocument::fromJson(it->argumentsText.toUtf8());
            if (argsDoc.isObject()) {
                call.arguments = argsDoc.object();
            }
            response.assistantMessage.toolCalls.append(call);
        }

        if (!state.finishReason.isEmpty() && response.finishReason.isEmpty()) {
            response.finishReason = state.finishReason;
        }
        if (!response.success) {
            response.success = !response.assistantMessage.content.isEmpty() || !response.assistantMessage.toolCalls.isEmpty();
        }
        if (!response.success) {
            response.errorMessage = state.responseError.isEmpty()
                ? QStringLiteral("Empty assistant message in OpenAI SSE response")
                : state.responseError;
        }
        return response;
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (!doc.isObject()) {
        LlmResponse response;
        response.errorMessage = QStringLiteral("Invalid JSON response: ") + error.errorString();
        return response;
    }

    return parseResponseObject(doc.object());
}

QList<QString> OpenAIProvider::parseStreamTokens(const QByteArray &chunk) const
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

QList<LlmStreamDelta> OpenAIProvider::parseStreamDeltas(const QByteArray &chunk) const
{
    QList<LlmStreamDelta> deltas;
    const QJsonObject event = parseSseEvent(chunk);
    if (event.isEmpty()) {
        return deltas;
    }

    const QString type = event.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("response.output_text.delta")) {
        LlmStreamDelta delta;
        delta.channel = QStringLiteral("content");
        delta.text = event.value(QStringLiteral("delta")).toString();
        if (!delta.text.isEmpty()) {
            deltas.append(delta);
        }
        return deltas;
    }

    if (type.contains(QStringLiteral("reasoning"), Qt::CaseInsensitive)) {
        const QString text = event.value(QStringLiteral("delta")).toString(event.value(QStringLiteral("text")).toString());
        if (!text.isEmpty()) {
            LlmStreamDelta delta;
            delta.channel = QStringLiteral("reasoning");
            delta.text = text;
            deltas.append(delta);
        }
    }

    return deltas;
}

bool OpenAIProvider::supportsStructuredToolCalls() const
{
    return true;
}

} // namespace qtllm





