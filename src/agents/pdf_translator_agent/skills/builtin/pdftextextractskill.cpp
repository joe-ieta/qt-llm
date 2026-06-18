#include "pdftextextractskill.h"

#include "../core/modelrouter.h"
#include "../mcp/mcpgateway.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

namespace pdftranslator::skills {

namespace {

const QString kSkillId = QStringLiteral("pdf-text-extract");

struct ToolInvocation
{
    QString program;
    QStringList arguments;
    QString method;
};

SkillResult runExtractionCommand(const ToolInvocation &invocation, const QString &pdfPath)
{
    SkillResult result;

    QProcess process;
    process.start(invocation.program, invocation.arguments);
    if (!process.waitForStarted(3000)) {
        result.status = QStringLiteral("tool_start_failed");
        result.errorMessage = QStringLiteral("Failed to start extraction tool: %1").arg(invocation.program);
        return result;
    }

    if (!process.waitForFinished(60000)) {
        process.kill();
        process.waitForFinished(3000);
        result.status = QStringLiteral("tool_timeout");
        result.errorMessage = QStringLiteral("PDF extraction timed out for %1").arg(QFileInfo(pdfPath).fileName());
        result.retryable = true;
        return result;
    }

    const QByteArray stdoutData = process.readAllStandardOutput();
    const QByteArray stderrData = process.readAllStandardError();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        result.status = QStringLiteral("tool_failed");
        result.errorMessage = QStringLiteral("PDF extraction failed with %1").arg(QString::fromUtf8(stderrData).trimmed());
        return result;
    }

    const QString extractedText = QString::fromUtf8(stdoutData).trimmed();
    if (extractedText.isEmpty()) {
        result.status = QStringLiteral("empty_output");
        result.errorMessage = QStringLiteral("PDF extraction produced no text");
        return result;
    }

    result.success = true;
    result.status = QStringLiteral("completed");
    result.output.insert(QStringLiteral("sourcePdfPath"), pdfPath);
    result.output.insert(QStringLiteral("text"), extractedText);
    result.output.insert(QStringLiteral("method"), invocation.method);
    result.output.insert(QStringLiteral("tool"), invocation.program);
    return result;
}

QString extractTextFromMcpOutput(const QJsonObject &output)
{
    const QJsonValue contentValue = output.value(QStringLiteral("content"));
    if (contentValue.isString()) {
        return contentValue.toString().trimmed();
    }

    if (contentValue.isArray()) {
        QStringList parts;
        const QJsonArray array = contentValue.toArray();
        for (const QJsonValue &value : array) {
            if (value.isString()) {
                parts.append(value.toString());
            } else if (value.isObject()) {
                const QJsonObject object = value.toObject();
                const QString text = object.value(QStringLiteral("text")).toString();
                if (!text.isEmpty()) {
                    parts.append(text);
                }
            }
        }
        return parts.join(QStringLiteral("\n")).trimmed();
    }

    if (contentValue.isObject()) {
        const QJsonObject object = contentValue.toObject();
        const QString text = object.value(QStringLiteral("text")).toString().trimmed();
        if (!text.isEmpty()) {
            return text;
        }
    }

    const QString text = output.value(QStringLiteral("text")).toString().trimmed();
    if (!text.isEmpty()) {
        return text;
    }

    return QString();
}

} // namespace

PdfTextExtractSkill::PdfTextExtractSkill(std::shared_ptr<ModelRouter> modelRouter,
                                         std::shared_ptr<mcp::McpGateway> mcpGateway)
    : m_modelRouter(std::move(modelRouter))
    , m_mcpGateway(std::move(mcpGateway))
{
}

SkillDescriptor PdfTextExtractSkill::descriptor() const
{
    SkillDescriptor descriptor;
    descriptor.id = kSkillId;
    descriptor.displayName = QStringLiteral("PDF Text Extract");
    descriptor.description = QStringLiteral("Extracts text from a PDF via local tools or MCP fallback.");
    descriptor.category = QStringLiteral("preprocess");
    descriptor.executionType = SkillExecutionType::Hybrid;
    return descriptor;
}

bool PdfTextExtractSkill::canHandle(const SkillContext &context) const
{
    const QFileInfo info(context.documentPath);
    return info.exists() && info.isFile() && info.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0;
}

SkillResult PdfTextExtractSkill::execute(const SkillContext &context)
{
    SkillResult result;
    const QFileInfo pdfInfo(context.documentPath);
    if (!pdfInfo.exists() || !pdfInfo.isFile()) {
        result.status = QStringLiteral("invalid_input");
        result.errorMessage = QStringLiteral("PDF file does not exist: %1").arg(context.documentPath);
        return result;
    }

    const QString pdftotext = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
    if (!pdftotext.isEmpty()) {
        ToolInvocation invocation;
        invocation.program = pdftotext;
        invocation.arguments = QStringList{
            QStringLiteral("-layout"),
            QStringLiteral("-enc"),
            QStringLiteral("UTF-8"),
            pdfInfo.absoluteFilePath(),
            QStringLiteral("-")
        };
        invocation.method = QStringLiteral("pdftotext");
        return runExtractionCommand(invocation, pdfInfo.absoluteFilePath());
    }

    const QString mutool = QStandardPaths::findExecutable(QStringLiteral("mutool"));
    if (!mutool.isEmpty()) {
        ToolInvocation invocation;
        invocation.program = mutool;
        invocation.arguments = QStringList{
            QStringLiteral("draw"),
            QStringLiteral("-F"),
            QStringLiteral("txt"),
            QStringLiteral("-o"),
            QStringLiteral("-"),
            pdfInfo.absoluteFilePath()
        };
        invocation.method = QStringLiteral("mutool");
        return runExtractionCommand(invocation, pdfInfo.absoluteFilePath());
    }

    if (m_modelRouter && m_mcpGateway) {
        const std::optional<ResolvedSkillRoute> route = m_modelRouter->resolve(kSkillId);
        if (route.has_value() && !route->binding.mcpServerId.trimmed().isEmpty() && !route->binding.mcpToolName.trimmed().isEmpty()) {
            QString errorMessage;
            const qtllm::tools::mcp::McpToolCallResult mcpResult =
                m_mcpGateway->callTool(route->binding.mcpServerId,
                                       route->binding.mcpToolName,
                                       QJsonObject{{QStringLiteral("path"), pdfInfo.absoluteFilePath()}},
                                       &errorMessage);
            if (mcpResult.success) {
                const QString extractedText = extractTextFromMcpOutput(mcpResult.output);
                if (!extractedText.isEmpty()) {
                    result.success = true;
                    result.status = QStringLiteral("completed");
                    result.output.insert(QStringLiteral("sourcePdfPath"), pdfInfo.absoluteFilePath());
                    result.output.insert(QStringLiteral("text"), extractedText);
                    result.output.insert(QStringLiteral("method"), QStringLiteral("mcp"));
                    result.output.insert(QStringLiteral("mcpServerId"), route->binding.mcpServerId);
                    result.output.insert(QStringLiteral("mcpToolName"), route->binding.mcpToolName);
                    return result;
                }
            }

            result.status = QStringLiteral("mcp_failed");
            result.errorMessage = errorMessage.isEmpty()
                ? QStringLiteral("MCP PDF extraction failed")
                : errorMessage;
            return result;
        }
    }

    result.status = QStringLiteral("tool_unavailable");
    result.errorMessage = QStringLiteral("No supported PDF text extractor was found. Install pdftotext or mutool, or bind this skill to an MCP document parser.");
    result.output.insert(QStringLiteral("sourcePdfPath"), pdfInfo.absoluteFilePath());
    return result;
}

} // namespace pdftranslator::skills
