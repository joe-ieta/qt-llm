#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace pdftranslator::skills {

enum class SkillExecutionType
{
    Builtin,
    Llm,
    Mcp,
    Hybrid
};

inline QString skillExecutionTypeToString(SkillExecutionType type)
{
    switch (type) {
    case SkillExecutionType::Builtin:
        return QStringLiteral("builtin");
    case SkillExecutionType::Llm:
        return QStringLiteral("llm");
    case SkillExecutionType::Mcp:
        return QStringLiteral("mcp");
    case SkillExecutionType::Hybrid:
        return QStringLiteral("hybrid");
    }
    return QStringLiteral("builtin");
}

struct SkillDescriptor
{
    QString id;
    QString displayName;
    QString description;
    QString category;
    QString version = QStringLiteral("0.1.0");
    SkillExecutionType executionType = SkillExecutionType::Builtin;
    bool supportsStreaming = false;
    bool defaultEnabled = true;
};

struct SkillArtifact
{
    QString kind;
    QString path;
    QString description;
};

struct SkillWarning
{
    QString code;
    QString message;
};

struct SkillContext
{
    QString taskId;
    QString documentPath;
    QString sourceText;
    QString sourceLanguageHint;
    QString targetLanguageHint;
    QJsonObject extra;
};

struct SkillResult
{
    bool success = false;
    QString status;
    QString errorMessage;
    bool retryable = false;
    QJsonObject output;
    QVector<SkillArtifact> artifacts;
    QVector<SkillWarning> warnings;
};

} // namespace pdftranslator::skills
