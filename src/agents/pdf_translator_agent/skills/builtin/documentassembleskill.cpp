#include "documentassembleskill.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace pdftranslator::skills {

namespace {

const QString kSkillId = QStringLiteral("document-assemble");

bool writeUtf8TextFile(const QString &path, const QString &text, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QTextStream stream(&file);
#if QT_VERSION<QT_VERSION_CHECK(6,0,0)
    stream.setCodec("UTF-8");
#else
    stream.setEncoding(QStringConverter::Utf8);
#endif
    stream << text;
    file.close();
    return true;
}

QString toHtmlEscaped(const QString &text)
{
    QString html = text.toHtmlEscaped();
    html.replace(QStringLiteral("\n\n"), QStringLiteral("</p><p>"));
    html.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));
    return html;
}

} // namespace

SkillDescriptor DocumentAssembleSkill::descriptor() const
{
    SkillDescriptor descriptor;
    descriptor.id = kSkillId;
    descriptor.displayName = QStringLiteral("Document Assemble");
    descriptor.description = QStringLiteral("Assembles translated markdown and HTML outputs for a document.");
    descriptor.category = QStringLiteral("assembly");
    descriptor.executionType = SkillExecutionType::Builtin;
    return descriptor;
}

bool DocumentAssembleSkill::canHandle(const SkillContext &context) const
{
    return !context.sourceText.trimmed().isEmpty();
}

SkillResult DocumentAssembleSkill::execute(const SkillContext &context)
{
    SkillResult result;
    const QString markdownPath = context.extra.value(QStringLiteral("translatedMarkdownPath")).toString().trimmed();
    const QString htmlPath = context.extra.value(QStringLiteral("translatedHtmlPath")).toString().trimmed();
    const QString originalText = context.extra.value(QStringLiteral("originalText")).toString();
    const QString translatedText = context.sourceText;

    if (markdownPath.isEmpty() || htmlPath.isEmpty()) {
        result.status = QStringLiteral("invalid_input");
        result.errorMessage = QStringLiteral("document-assemble requires output paths");
        return result;
    }

    const QString markdownText =
        QStringLiteral("# Translation Output\n\n"
                       "- Source language: %1\n"
                       "- Target language: %2\n\n"
                       "## Translated Text\n\n%3\n")
            .arg(context.sourceLanguageHint, context.targetLanguageHint, translatedText);

    QString errorMessage;
    if (!writeUtf8TextFile(markdownPath, markdownText, &errorMessage)) {
        result.status = QStringLiteral("write_failed");
        result.errorMessage = QStringLiteral("Failed to write markdown output: %1").arg(errorMessage);
        return result;
    }

    const QString htmlText =
        QStringLiteral("<!DOCTYPE html><html><head><meta charset=\"utf-8\"/>"
                       "<title>Translation Output</title>"
                       "<style>body{font-family:Segoe UI,Arial,sans-serif;margin:24px;line-height:1.6;} "
                       ".grid{display:grid;grid-template-columns:1fr 1fr;gap:24px;} "
                       "section{border:1px solid #d8dde6;border-radius:10px;padding:16px;background:#ffffff;} "
                       "h1,h2{margin-top:0;} pre{white-space:pre-wrap;word-break:break-word;}</style>"
                       "</head><body><h1>Translation Output</h1><div class=\"grid\">"
                       "<section><h2>Original</h2><pre>%1</pre></section>"
                       "<section><h2>Translated</h2><pre>%2</pre></section>"
                       "</div></body></html>")
            .arg(originalText.toHtmlEscaped(), translatedText.toHtmlEscaped());

    if (!writeUtf8TextFile(htmlPath, htmlText, &errorMessage)) {
        result.status = QStringLiteral("write_failed");
        result.errorMessage = QStringLiteral("Failed to write HTML output: %1").arg(errorMessage);
        return result;
    }

    result.success = true;
    result.status = QStringLiteral("completed");
    result.output.insert(QStringLiteral("translatedMarkdownPath"), markdownPath);
    result.output.insert(QStringLiteral("translatedHtmlPath"), htmlPath);
    result.output.insert(QStringLiteral("previewHtml"), toHtmlEscaped(translatedText));
    return result;
}

} // namespace pdftranslator::skills
