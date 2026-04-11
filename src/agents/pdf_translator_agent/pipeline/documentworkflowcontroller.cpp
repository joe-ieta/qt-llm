#include "documentworkflowcontroller.h"

#include "../skills/core/iskill.h"
#include "../skills/core/modelrouter.h"
#include "../skills/core/skillregistry.h"
#include "../storage/manifestrepository.h"
#include "../../../qtllm/identity/compactid.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

namespace pdftranslator::pipeline {

namespace {

QString manifestPathForPdf(const QString &pdfPath)
{
    const QFileInfo info(pdfPath);
    return info.absolutePath()
        + QDir::separator()
        + info.completeBaseName()
        + QStringLiteral(".translation.manifest.json");
}

QString extractedTextPathForPdf(const QString &pdfPath)
{
    const QFileInfo info(pdfPath);
    return info.absolutePath()
        + QDir::separator()
        + info.completeBaseName()
        + QStringLiteral(".extracted.txt");
}

QString normalizedLanguageSuffix(const QString &targetLanguage)
{
    if (targetLanguage.trimmed().isEmpty()) {
        return QStringLiteral("unknown");
    }
    return targetLanguage;
}

} // namespace

DocumentWorkflowController::DocumentWorkflowController(const std::shared_ptr<skills::SkillRegistry> &skillRegistry,
                                                       const std::shared_ptr<skills::ModelRouter> &modelRouter)
    : m_skillRegistry(skillRegistry)
    , m_modelRouter(modelRouter)
{
}

DocumentWorkflowRunResult DocumentWorkflowController::runSingleDocumentTranslation(const QString &pdfPath,
                                                                                   const ProgressCallback &progressCallback) const
{
    DocumentWorkflowRunResult runResult;
    const auto publishProgress = [&](const domain::DocumentTranslationTask &task) {
        if (progressCallback) {
            progressCallback(task);
        }
    };
    runResult.task.taskId = qtllm::identity::generateId(qtllm::identity::IdKind::Task);
    runResult.task.sourcePdfPath = pdfPath;
    runResult.task.manifestPath = manifestPathForPdf(pdfPath);
    runResult.task.extractedTextPath = extractedTextPathForPdf(pdfPath);
    runResult.task.transitionTo(domain::DocumentTaskStage::Queued);
    publishProgress(runResult.task);

    storage::ManifestRepository manifestRepository;
    manifestRepository.save(runResult.task, nullptr);

    const std::shared_ptr<skills::ISkill> extractSkill = m_skillRegistry ? m_skillRegistry->skill(QStringLiteral("pdf-text-extract")) : nullptr;
    const std::shared_ptr<skills::ISkill> detectSkill = m_skillRegistry ? m_skillRegistry->skill(QStringLiteral("language-detect")) : nullptr;
    const std::shared_ptr<skills::ISkill> termBaseSkill = m_skillRegistry ? m_skillRegistry->skill(QStringLiteral("term-base-resolve")) : nullptr;
    const std::shared_ptr<skills::ISkill> translateSkill = m_skillRegistry ? m_skillRegistry->skill(QStringLiteral("chunk-translate")) : nullptr;
    const std::shared_ptr<skills::ISkill> assembleSkill = m_skillRegistry ? m_skillRegistry->skill(QStringLiteral("document-assemble")) : nullptr;

    if (!extractSkill || !detectSkill || !translateSkill || !assembleSkill) {
        runResult.task.transitionTo(domain::DocumentTaskStage::Failed, QStringLiteral("Workflow skills are not fully registered"));
        runResult.errorMessage = runResult.task.lastError;
        publishProgress(runResult.task);
        manifestRepository.save(runResult.task, nullptr);
        return runResult;
    }

    runResult.task.transitionTo(domain::DocumentTaskStage::Extracting);
    runResult.task.progress = 10;
    publishProgress(runResult.task);
    manifestRepository.save(runResult.task, nullptr);

    skills::SkillContext extractContext;
    extractContext.taskId = runResult.task.taskId;
    extractContext.documentPath = pdfPath;
    const skills::SkillResult extraction = extractSkill->execute(extractContext);
    if (!extraction.success) {
        runResult.task.transitionTo(domain::DocumentTaskStage::Failed, extraction.errorMessage);
        runResult.task.progress = 0;
        runResult.errorMessage = extraction.errorMessage;
        publishProgress(runResult.task);
        manifestRepository.save(runResult.task, nullptr);
        return runResult;
    }

    runResult.extractedText = extraction.output.value(QStringLiteral("text")).toString();
    QFile extractedFile(runResult.task.extractedTextPath);
    if (extractedFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        extractedFile.write(runResult.extractedText.toUtf8());
        extractedFile.close();
    }

    runResult.task.transitionTo(domain::DocumentTaskStage::DetectingLanguage);
    runResult.task.progress = 35;
    publishProgress(runResult.task);
    manifestRepository.save(runResult.task, nullptr);

    skills::SkillContext detectContext;
    detectContext.taskId = runResult.task.taskId;
    detectContext.documentPath = pdfPath;
    detectContext.sourceText = runResult.extractedText.left(4000);
    const skills::SkillResult detection = detectSkill->execute(detectContext);
    if (!detection.success) {
        runResult.task.transitionTo(domain::DocumentTaskStage::Failed, detection.errorMessage);
        runResult.task.progress = 0;
        runResult.errorMessage = detection.errorMessage;
        publishProgress(runResult.task);
        manifestRepository.save(runResult.task, nullptr);
        return runResult;
    }

    runResult.task.sourceLanguage = detection.output.value(QStringLiteral("sourceLanguage")).toString();
    runResult.task.targetLanguage = detection.output.value(QStringLiteral("targetLanguage")).toString();

    const QFileInfo pdfInfo(pdfPath);
    const QString targetSuffix = normalizedLanguageSuffix(runResult.task.targetLanguage);
    runResult.task.translatedMarkdownPath = pdfInfo.absolutePath()
        + QDir::separator()
        + pdfInfo.completeBaseName()
        + QStringLiteral(".translated.")
        + targetSuffix
        + QStringLiteral(".md");
    runResult.task.translatedHtmlPath = pdfInfo.absolutePath()
        + QDir::separator()
        + pdfInfo.completeBaseName()
        + QStringLiteral(".translated.")
        + targetSuffix
        + QStringLiteral(".html");

    QString glossaryText;
    if (termBaseSkill) {
        skills::SkillContext termContext;
        termContext.taskId = runResult.task.taskId;
        termContext.documentPath = pdfPath;
        termContext.sourceText = runResult.extractedText.left(6000);
        termContext.sourceLanguageHint = runResult.task.sourceLanguage;
        termContext.targetLanguageHint = runResult.task.targetLanguage;
        const skills::SkillResult termResult = termBaseSkill->execute(termContext);
        if (termResult.success) {
            const QJsonArray terms = termResult.output.value(QStringLiteral("terms")).toArray();
            QStringList lines;
            for (const QJsonValue &value : terms) {
                if (value.isObject()) {
                    const QJsonObject item = value.toObject();
                    const QString source = item.value(QStringLiteral("source")).toString();
                    const QString target = item.value(QStringLiteral("target")).toString();
                    if (!source.isEmpty() && !target.isEmpty()) {
                        lines.append(QStringLiteral("- %1 => %2").arg(source, target));
                    }
                } else if (value.isString()) {
                    lines.append(QStringLiteral("- ") + value.toString());
                }
            }
            glossaryText = lines.join(QStringLiteral("\n"));
        }
    }

    runResult.task.transitionTo(domain::DocumentTaskStage::Translating);
    runResult.task.progress = 55;
    publishProgress(runResult.task);
    manifestRepository.save(runResult.task, nullptr);

    skills::SkillContext translateContext;
    translateContext.taskId = runResult.task.taskId;
    translateContext.documentPath = pdfPath;
    translateContext.sourceText = runResult.extractedText;
    translateContext.sourceLanguageHint = runResult.task.sourceLanguage;
    translateContext.targetLanguageHint = runResult.task.targetLanguage;
    if (!glossaryText.isEmpty()) {
        translateContext.extra.insert(QStringLiteral("glossaryText"), glossaryText);
    }
    const skills::SkillResult translation = translateSkill->execute(translateContext);
    if (!translation.success) {
        runResult.task.transitionTo(domain::DocumentTaskStage::Failed, translation.errorMessage);
        runResult.task.progress = 0;
        runResult.errorMessage = translation.errorMessage;
        publishProgress(runResult.task);
        manifestRepository.save(runResult.task, nullptr);
        return runResult;
    }

    runResult.translatedText = translation.output.value(QStringLiteral("translatedText")).toString();

    runResult.task.transitionTo(domain::DocumentTaskStage::Assembling);
    runResult.task.progress = 80;
    publishProgress(runResult.task);
    manifestRepository.save(runResult.task, nullptr);

    skills::SkillContext assembleContext;
    assembleContext.taskId = runResult.task.taskId;
    assembleContext.documentPath = pdfPath;
    assembleContext.sourceText = runResult.translatedText;
    assembleContext.sourceLanguageHint = runResult.task.sourceLanguage;
    assembleContext.targetLanguageHint = runResult.task.targetLanguage;
    assembleContext.extra.insert(QStringLiteral("sourcePdfPath"), runResult.task.sourcePdfPath);
    assembleContext.extra.insert(QStringLiteral("originalText"), runResult.extractedText);
    assembleContext.extra.insert(QStringLiteral("translatedMarkdownPath"), runResult.task.translatedMarkdownPath);
    assembleContext.extra.insert(QStringLiteral("translatedHtmlPath"), runResult.task.translatedHtmlPath);
    assembleContext.extra.insert(QStringLiteral("manifestPath"), runResult.task.manifestPath);
    const skills::SkillResult assembly = assembleSkill->execute(assembleContext);
    if (!assembly.success) {
        runResult.task.transitionTo(domain::DocumentTaskStage::Failed, assembly.errorMessage);
        runResult.task.progress = 0;
        runResult.errorMessage = assembly.errorMessage;
        publishProgress(runResult.task);
        manifestRepository.save(runResult.task, nullptr);
        return runResult;
    }

    runResult.task.transitionTo(domain::DocumentTaskStage::Completed);
    runResult.task.progress = 100;
    publishProgress(runResult.task);
    manifestRepository.save(runResult.task, nullptr);
    return runResult;
}

} // namespace pdftranslator::pipeline
