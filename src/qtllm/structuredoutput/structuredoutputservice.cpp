#include "structuredoutputservice.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

#include <cmath>

namespace qtllm {

namespace {

const QSet<QString> &supportedSchemaKeywords()
{
    static const QSet<QString> keywords = {
        QStringLiteral("$schema"),
        QStringLiteral("type"),
        QStringLiteral("properties"),
        QStringLiteral("required"),
        QStringLiteral("items"),
        QStringLiteral("enum"),
        QStringLiteral("additionalProperties"),
        QStringLiteral("description"),
        QStringLiteral("title")
    };
    return keywords;
}

bool isSupportedType(const QString &type)
{
    static const QSet<QString> types = {
        QStringLiteral("object"),
        QStringLiteral("array"),
        QStringLiteral("string"),
        QStringLiteral("number"),
        QStringLiteral("integer"),
        QStringLiteral("boolean"),
        QStringLiteral("null")
    };
    return types.contains(type);
}

void appendDefinitionErrors(const QJsonObject &schema,
                            const QString &path,
                            QStringList *errors)
{
    for (auto it = schema.constBegin(); it != schema.constEnd(); ++it) {
        if (!supportedSchemaKeywords().contains(it.key())) {
            errors->append(path + QStringLiteral(": unsupported keyword: ") + it.key());
        }
    }

    const QJsonValue typeValue = schema.value(QStringLiteral("type"));
    if (!typeValue.isUndefined()
        && (!typeValue.isString() || !isSupportedType(typeValue.toString()))) {
        errors->append(path + QStringLiteral(": type must be one supported string"));
    }

    const QJsonValue requiredValue = schema.value(QStringLiteral("required"));
    if (!requiredValue.isUndefined()) {
        if (!requiredValue.isArray()) {
            errors->append(path + QStringLiteral(": required must be an array"));
        } else {
            for (const QJsonValue &entry : requiredValue.toArray()) {
                if (!entry.isString()) {
                    errors->append(path + QStringLiteral(": required entries must be strings"));
                    break;
                }
            }
        }
    }

    const QJsonValue enumValue = schema.value(QStringLiteral("enum"));
    if (!enumValue.isUndefined() && (!enumValue.isArray() || enumValue.toArray().isEmpty())) {
        errors->append(path + QStringLiteral(": enum must be a non-empty array"));
    }

    const QJsonValue additionalValue = schema.value(QStringLiteral("additionalProperties"));
    if (!additionalValue.isUndefined() && !additionalValue.isBool()) {
        errors->append(path + QStringLiteral(": additionalProperties must be boolean"));
    }

    const QJsonValue propertiesValue = schema.value(QStringLiteral("properties"));
    if (!propertiesValue.isUndefined()) {
        if (!propertiesValue.isObject()) {
            errors->append(path + QStringLiteral(": properties must be an object"));
        } else {
            const QJsonObject properties = propertiesValue.toObject();
            for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
                if (!it.value().isObject()) {
                    errors->append(path + QStringLiteral(".properties.") + it.key()
                                   + QStringLiteral(": property schema must be an object"));
                    continue;
                }
                appendDefinitionErrors(it.value().toObject(),
                                       path + QStringLiteral(".properties.") + it.key(),
                                       errors);
            }
        }
    }

    const QJsonValue itemsValue = schema.value(QStringLiteral("items"));
    if (!itemsValue.isUndefined()) {
        if (!itemsValue.isObject()) {
            errors->append(path + QStringLiteral(": items must be an object"));
        } else {
            appendDefinitionErrors(itemsValue.toObject(),
                                   path + QStringLiteral(".items"),
                                   errors);
        }
    }
}

bool valueMatchesType(const QJsonValue &value, const QString &type)
{
    if (type.isEmpty()) {
        return true;
    }
    if (type == QStringLiteral("object")) {
        return value.isObject();
    }
    if (type == QStringLiteral("array")) {
        return value.isArray();
    }
    if (type == QStringLiteral("string")) {
        return value.isString();
    }
    if (type == QStringLiteral("number")) {
        return value.isDouble();
    }
    if (type == QStringLiteral("integer")) {
        return value.isDouble() && std::floor(value.toDouble()) == value.toDouble();
    }
    if (type == QStringLiteral("boolean")) {
        return value.isBool();
    }
    if (type == QStringLiteral("null")) {
        return value.isNull();
    }
    return false;
}

void appendValidationErrors(const QJsonValue &value,
                            const QJsonObject &schema,
                            const QString &path,
                            QStringList *errors)
{
    const QString type = schema.value(QStringLiteral("type")).toString();
    if (!valueMatchesType(value, type)) {
        errors->append(path + QStringLiteral(": expected type ") + type);
        return;
    }

    const QJsonArray enumValues = schema.value(QStringLiteral("enum")).toArray();
    if (!enumValues.isEmpty()) {
        bool found = false;
        for (const QJsonValue &candidate : enumValues) {
            if (candidate == value) {
                found = true;
                break;
            }
        }
        if (!found) {
            errors->append(path + QStringLiteral(": value is not in enum"));
        }
    }

    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        const QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();
        const QJsonArray required = schema.value(QStringLiteral("required")).toArray();
        for (const QJsonValue &requiredValue : required) {
            const QString name = requiredValue.toString();
            if (!object.contains(name)) {
                errors->append(path + QStringLiteral(".") + name
                               + QStringLiteral(": required property is missing"));
            }
        }

        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
            if (object.contains(it.key())) {
                appendValidationErrors(object.value(it.key()),
                                       it.value().toObject(),
                                       path + QStringLiteral(".") + it.key(),
                                       errors);
            }
        }

        if (schema.value(QStringLiteral("additionalProperties")).isBool()
            && !schema.value(QStringLiteral("additionalProperties")).toBool()) {
            for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
                if (!properties.contains(it.key())) {
                    errors->append(path + QStringLiteral(".") + it.key()
                                   + QStringLiteral(": additional property is not allowed"));
                }
            }
        }
    }

    if (value.isArray() && schema.value(QStringLiteral("items")).isObject()) {
        const QJsonArray array = value.toArray();
        const QJsonObject itemSchema = schema.value(QStringLiteral("items")).toObject();
        for (int index = 0; index < array.size(); ++index) {
            appendValidationErrors(array.at(index),
                                   itemSchema,
                                   path + QStringLiteral("[%1]").arg(index),
                                   errors);
        }
    }
}

bool parseJsonValue(const QString &text, QJsonValue *value, QString *message)
{
    QJsonParseError parseError;
    const QByteArray bytes = text.trimmed().toUtf8();
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error == QJsonParseError::NoError && !document.isNull()) {
        *value = document.isObject() ? QJsonValue(document.object())
                                     : QJsonValue(document.array());
        return true;
    }

    const QJsonDocument wrapped = QJsonDocument::fromJson(
        QByteArrayLiteral("[") + bytes + QByteArrayLiteral("]"), &parseError);
    if (parseError.error == QJsonParseError::NoError
        && wrapped.isArray()
        && wrapped.array().size() == 1) {
        *value = wrapped.array().first();
        return true;
    }

    if (message) {
        *message = parseError.errorString();
    }
    return false;
}

QString resolvedVendor(const QString &modelVendor, const QString &model)
{
    const QString explicitVendor = modelVendor.trimmed().toLower();
    if (!explicitVendor.isEmpty()) {
        return explicitVendor;
    }

    const QString normalizedModel = model.trimmed().toLower();
    if (normalizedModel.contains(QStringLiteral("claude"))) {
        return QStringLiteral("anthropic");
    }
    if (normalizedModel.contains(QStringLiteral("gemini"))) {
        return QStringLiteral("google");
    }
    return QStringLiteral("openai");
}

CapabilityDescriptor descriptor(CapabilitySupport adapterSupport,
                                const CapabilityEvidence &modelEvidence,
                                const QString &detail)
{
    CapabilityDescriptor result;
    result.adapterSupport = adapterSupport;
    result.adapterSource = adapterSupport == CapabilitySupport::Unknown
        ? CapabilitySource::Unknown
        : CapabilitySource::ProviderDefault;
    result.modelSupport = modelEvidence.support;
    result.modelSource = modelEvidence.source;
    result.detail = !modelEvidence.detail.isEmpty() ? modelEvidence.detail : detail;
    return result;
}

QJsonObject nativeFormat(const OutputConstraint &constraint)
{
    QJsonObject format;
    if (constraint.format == StructuredOutputFormat::Json) {
        format.insert(QStringLiteral("type"), QStringLiteral("json_object"));
    } else if (constraint.format == StructuredOutputFormat::JsonSchema) {
        format.insert(QStringLiteral("type"), QStringLiteral("json_schema"));
        format.insert(QStringLiteral("name"),
                      constraint.schemaName.trimmed().isEmpty()
                          ? QStringLiteral("response")
                          : constraint.schemaName.trimmed());
        format.insert(QStringLiteral("schema"), constraint.schema);
        format.insert(QStringLiteral("strict"), constraint.strict);
    }
    return format;
}

} // namespace

CapabilitySupport CapabilityDescriptor::effectiveSupport() const
{
    if (adapterSupport == CapabilitySupport::Unsupported
        || modelSupport == CapabilitySupport::Unsupported) {
        return CapabilitySupport::Unsupported;
    }
    if (adapterSupport == CapabilitySupport::Supported
        && modelSupport == CapabilitySupport::Supported) {
        return CapabilitySupport::Supported;
    }
    return CapabilitySupport::Unknown;
}

bool StructuredOutputService::validateConstraint(const OutputConstraint &constraint,
                                                 QString *errorCode,
                                                 QString *errorMessage)
{
    if (constraint.format != StructuredOutputFormat::JsonSchema) {
        return true;
    }

    if (constraint.schema.isEmpty()) {
        if (errorCode) {
            *errorCode = QStringLiteral("structured_output_schema_missing");
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("JSON Schema output requires a non-empty schema");
        }
        return false;
    }

    QStringList errors;
    appendDefinitionErrors(constraint.schema, QStringLiteral("$"), &errors);
    if (errors.isEmpty()) {
        return true;
    }

    if (errorCode) {
        *errorCode = QStringLiteral("structured_output_schema_unsupported");
    }
    if (errorMessage) {
        *errorMessage = errors.join(QStringLiteral("; "));
    }
    return false;
}

StructuredOutputResult StructuredOutputService::validate(
    const QString &text,
    const OutputConstraint &constraint)
{
    StructuredOutputResult result;
    result.rawText = text;
    result.requested = constraint.format != StructuredOutputFormat::Text;
    if (!result.requested) {
        result.syntaxValid = true;
        result.schemaValid = true;
        return result;
    }

    QString parseMessage;
    if (!parseJsonValue(text, &result.value, &parseMessage)) {
        result.errorCode = QStringLiteral("structured_output_invalid_json");
        result.errorMessage = QStringLiteral("Model output is not valid JSON: ") + parseMessage;
        return result;
    }
    result.syntaxValid = true;

    if (constraint.format == StructuredOutputFormat::Json) {
        result.schemaValid = true;
        return result;
    }

    QString constraintCode;
    QString constraintMessage;
    if (!validateConstraint(constraint, &constraintCode, &constraintMessage)) {
        result.errorCode = constraintCode;
        result.errorMessage = constraintMessage;
        return result;
    }

    appendValidationErrors(result.value, constraint.schema, QStringLiteral("$"),
                           &result.violations);
    result.schemaValid = result.violations.isEmpty();
    if (!result.schemaValid) {
        result.errorCode = QStringLiteral("structured_output_schema_mismatch");
        result.errorMessage = result.violations.join(QStringLiteral("; "));
    }
    return result;
}

QVector<LlmMessage> StructuredOutputService::constrainedMessages(
    const QVector<LlmMessage> &messages,
    const OutputConstraint &constraint)
{
    if (constraint.format == StructuredOutputFormat::Text
        || constraint.mode != OutputConstraintMode::Prompt) {
        return messages;
    }

    QString instruction = QStringLiteral(
        "Return only one valid JSON value without Markdown fences or explanatory text.");
    if (constraint.format == StructuredOutputFormat::JsonSchema) {
        instruction += QStringLiteral(" The JSON value must satisfy this schema: ");
        instruction += QString::fromUtf8(
            QJsonDocument(constraint.schema).toJson(QJsonDocument::Compact));
    }

    QVector<LlmMessage> result = messages;
    LlmMessage message;
    message.role = QStringLiteral("system");
    message.content = instruction;

    int insertionIndex = 0;
    while (insertionIndex < result.size()) {
        const QString role = result.at(insertionIndex).role.trimmed().toLower();
        if (role != QStringLiteral("system") && role != QStringLiteral("developer")) {
            break;
        }
        ++insertionIndex;
    }
    result.insert(insertionIndex, message);
    return result;
}

ModelCapabilitySnapshot StructuredOutputService::capabilities(
    const QString &providerName,
    const QString &model,
    const QString &modelVendor,
    const ModelCapabilityOverrides &overrides)
{
    ModelCapabilitySnapshot snapshot;
    snapshot.providerName = providerName;
    snapshot.model = model;

    const QString provider = providerName.trimmed().toLower();
    const bool openAiResponses = provider == QStringLiteral("openai");
    const bool compatible = provider == QStringLiteral("openai-compatible")
        || provider == QStringLiteral("vllm")
        || provider == QStringLiteral("ollama")
        || provider == QStringLiteral("llama.cpp")
        || provider == QStringLiteral("llamacpp");
    const bool knownAdapter = openAiResponses || compatible;
    const QString vendor = resolvedVendor(modelVendor, model);

    const CapabilitySupport commonAdapter = knownAdapter
        ? CapabilitySupport::Supported
        : CapabilitySupport::Unknown;
    CapabilitySupport structuredAdapter = CapabilitySupport::Unknown;
    if (openAiResponses) {
        structuredAdapter = CapabilitySupport::Supported;
    } else if (compatible) {
        structuredAdapter = vendor == QStringLiteral("anthropic")
            ? CapabilitySupport::Unsupported
            : CapabilitySupport::Supported;
    }

    snapshot.streaming = descriptor(commonAdapter, overrides.streaming,
                                    QStringLiteral("Adapter streaming contract"));
    snapshot.toolCalling = descriptor(commonAdapter, overrides.toolCalling,
                                      QStringLiteral("Adapter tool-calling contract"));
    snapshot.jsonOutput = descriptor(structuredAdapter, overrides.jsonOutput,
                                     QStringLiteral("Native JSON request mapping"));
    snapshot.jsonSchemaOutput = descriptor(structuredAdapter, overrides.jsonSchemaOutput,
                                           QStringLiteral("Native JSON Schema request mapping"));
    return snapshot;
}

QJsonObject StructuredOutputService::openAiResponsesText(
    const OutputConstraint &constraint)
{
    if (constraint.mode != OutputConstraintMode::Native
        || constraint.format == StructuredOutputFormat::Text) {
        return QJsonObject();
    }
    return QJsonObject{{QStringLiteral("format"), nativeFormat(constraint)}};
}

QJsonObject StructuredOutputService::openAiChatResponseFormat(
    const OutputConstraint &constraint)
{
    if (constraint.mode != OutputConstraintMode::Native
        || constraint.format == StructuredOutputFormat::Text) {
        return QJsonObject();
    }

    QJsonObject format = nativeFormat(constraint);
    if (constraint.format == StructuredOutputFormat::JsonSchema) {
        QJsonObject schema;
        schema.insert(QStringLiteral("name"), format.take(QStringLiteral("name")));
        schema.insert(QStringLiteral("schema"), format.take(QStringLiteral("schema")));
        schema.insert(QStringLiteral("strict"), format.take(QStringLiteral("strict")));
        format.insert(QStringLiteral("json_schema"), schema);
    }
    return format;
}

QJsonObject StructuredOutputService::googleGenerationConfig(
    const OutputConstraint &constraint)
{
    if (constraint.mode != OutputConstraintMode::Native
        || constraint.format == StructuredOutputFormat::Text) {
        return QJsonObject();
    }

    QJsonObject config;
    config.insert(QStringLiteral("responseMimeType"), QStringLiteral("application/json"));
    if (constraint.format == StructuredOutputFormat::JsonSchema) {
        config.insert(QStringLiteral("responseSchema"), constraint.schema);
    }
    return config;
}

} // namespace qtllm
