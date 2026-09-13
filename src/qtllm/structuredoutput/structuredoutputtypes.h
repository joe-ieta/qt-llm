#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace qtllm {

enum class StructuredOutputFormat
{
    Text,
    Json,
    JsonSchema
};

enum class OutputConstraintMode
{
    Native,
    Prompt
};

struct OutputConstraint
{
    StructuredOutputFormat format = StructuredOutputFormat::Text;
    OutputConstraintMode mode = OutputConstraintMode::Native;
    QJsonObject schema;
    QString schemaName;
    bool strict = true;
};

struct StructuredOutputResult
{
    bool requested = false;
    bool syntaxValid = false;
    bool schemaValid = false;
    QString rawText;
    QJsonValue value;
    QString errorCode;
    QString errorMessage;
    QStringList violations;
};

enum class CapabilitySupport
{
    Unknown,
    Supported,
    Unsupported
};

enum class CapabilitySource
{
    Unknown,
    ProviderDefault,
    ModelConfiguration,
    RuntimeProbe
};

struct CapabilityEvidence
{
    CapabilitySupport support = CapabilitySupport::Unknown;
    CapabilitySource source = CapabilitySource::Unknown;
    QString detail;
};

struct ModelCapabilityOverrides
{
    CapabilityEvidence streaming;
    CapabilityEvidence toolCalling;
    CapabilityEvidence jsonOutput;
    CapabilityEvidence jsonSchemaOutput;
};

struct CapabilityDescriptor
{
    CapabilitySupport adapterSupport = CapabilitySupport::Unknown;
    CapabilitySource adapterSource = CapabilitySource::Unknown;
    CapabilitySupport modelSupport = CapabilitySupport::Unknown;
    CapabilitySource modelSource = CapabilitySource::Unknown;
    QString detail;

    CapabilitySupport effectiveSupport() const;
};

struct ModelCapabilitySnapshot
{
    QString providerName;
    QString model;
    CapabilityDescriptor streaming;
    CapabilityDescriptor toolCalling;
    CapabilityDescriptor jsonOutput;
    CapabilityDescriptor jsonSchemaOutput;
};

} // namespace qtllm

Q_DECLARE_METATYPE(qtllm::StructuredOutputFormat)
Q_DECLARE_METATYPE(qtllm::OutputConstraintMode)
Q_DECLARE_METATYPE(qtllm::StructuredOutputResult)
Q_DECLARE_METATYPE(qtllm::CapabilitySupport)
Q_DECLARE_METATYPE(qtllm::CapabilitySource)
Q_DECLARE_METATYPE(qtllm::ModelCapabilitySnapshot)
