#include "batchtranslationqueuecontroller.h"

#include "documentworkflowcontroller.h"
#include "../../../qtllm/identity/compactid.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QThread>

namespace pdftranslator::pipeline {

namespace {

class BatchTranslationWorker : public QObject
{
    Q_OBJECT
public:
    BatchTranslationWorker(QString queueId,
                           QString pdfPath,
                           std::shared_ptr<skills::SkillRegistry> skillRegistry,
                           std::shared_ptr<skills::ModelRouter> modelRouter)
        : m_queueId(std::move(queueId))
        , m_pdfPath(std::move(pdfPath))
        , m_skillRegistry(std::move(skillRegistry))
        , m_modelRouter(std::move(modelRouter))
    {
    }

signals:
    void progress(const QString &queueId,
                  const pdftranslator::domain::DocumentTranslationTask &task);
    void finished(const QString &queueId,
                  const pdftranslator::domain::DocumentTranslationTask &task,
                  const QString &extractedText,
                  const QString &translatedText,
                  const QString &errorMessage);

public slots:
    void run()
    {
        DocumentWorkflowController controller(m_skillRegistry, m_modelRouter);
        const DocumentWorkflowRunResult result = controller.runSingleDocumentTranslation(
            m_pdfPath,
            [this](const pdftranslator::domain::DocumentTranslationTask &task) {
                emit progress(m_queueId, task);
            });
        emit finished(m_queueId, result.task, result.extractedText, result.translatedText, result.errorMessage);
    }

private:
    QString m_queueId;
    QString m_pdfPath;
    std::shared_ptr<skills::SkillRegistry> m_skillRegistry;
    std::shared_ptr<skills::ModelRouter> m_modelRouter;
};

} // namespace

BatchTranslationQueueController::BatchTranslationQueueController(const std::shared_ptr<skills::SkillRegistry> &skillRegistry,
                                                                 const std::shared_ptr<skills::ModelRouter> &modelRouter,
                                                                 QObject *parent)
    : QObject(parent)
    , m_skillRegistry(skillRegistry)
    , m_modelRouter(modelRouter)
{
    qRegisterMetaType<pdftranslator::domain::DocumentTranslationTask>();
}

BatchTranslationQueueController::~BatchTranslationQueueController()
{
    for (auto it = m_activeRuns.begin(); it != m_activeRuns.end(); ++it) {
        if (it.value().thread) {
            it.value().thread->quit();
            it.value().thread->wait();
        }
    }
}

void BatchTranslationQueueController::enqueueDocuments(const QStringList &pdfPaths)
{
    for (const QString &path : pdfPaths) {
        if (path.trimmed().isEmpty()) {
            continue;
        }

        BatchQueueEntry entry;
        entry.queueId = qtllm::identity::generateId(qtllm::identity::IdKind::Queue);
        entry.pdfPath = path.trimmed();
        entry.task.taskId = qtllm::identity::generateId(qtllm::identity::IdKind::Task);
        entry.task.sourcePdfPath = entry.pdfPath;
        entry.task.transitionTo(domain::DocumentTaskStage::Queued);
        entry.task.progress = 0;
        m_entries.append(entry);
    }

    emit queueChanged();
    startQueuedJobs();
}

QVector<BatchQueueEntry> BatchTranslationQueueController::entries() const
{
    return m_entries;
}

bool BatchTranslationQueueController::isRunning() const
{
    return !m_activeRuns.isEmpty();
}

bool BatchTranslationQueueController::isPaused() const
{
    return m_paused;
}

int BatchTranslationQueueController::maxConcurrentJobs() const
{
    return m_maxConcurrentJobs;
}

int BatchTranslationQueueController::activeJobCount() const
{
    return m_activeRuns.size();
}

void BatchTranslationQueueController::setPaused(bool paused)
{
    if (m_paused == paused) {
        return;
    }
    m_paused = paused;
    emit pausedChanged(m_paused);
    emit queueChanged();
    if (!m_paused) {
        startQueuedJobs();
    }
}

void BatchTranslationQueueController::setMaxConcurrentJobs(int value)
{
    const int normalized = qMax(1, value);
    if (m_maxConcurrentJobs == normalized) {
        return;
    }
    m_maxConcurrentJobs = normalized;
    emit activeJobsChanged(m_activeRuns.size(), m_maxConcurrentJobs);
    startQueuedJobs();
}

bool BatchTranslationQueueController::retryEntry(const QString &queueId)
{
    const QString normalized = queueId.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    for (BatchQueueEntry &entry : m_entries) {
        if (entry.queueId != normalized) {
            continue;
        }

        if (m_activeRuns.contains(normalized)) {
            return false;
        }

        entry.task.lastError.clear();
        entry.task.progress = 0;
        entry.task.sourceLanguage.clear();
        entry.task.targetLanguage.clear();
        entry.task.translatedMarkdownPath.clear();
        entry.task.translatedHtmlPath.clear();
        entry.task.transitionTo(domain::DocumentTaskStage::Queued);
        entry.extractedText.clear();
        entry.translatedText.clear();
        emit queueChanged();
        startQueuedJobs();
        return true;
    }

    return false;
}

QJsonArray BatchTranslationQueueController::toJson() const
{
    QJsonArray array;
    for (const BatchQueueEntry &entry : m_entries) {
        QJsonObject object;
        object.insert(QStringLiteral("queueId"), entry.queueId);
        object.insert(QStringLiteral("pdfPath"), entry.pdfPath);
        object.insert(QStringLiteral("task"), entry.task.toJson());
        object.insert(QStringLiteral("extractedText"), entry.extractedText);
        object.insert(QStringLiteral("translatedText"), entry.translatedText);
        array.append(object);
    }
    return array;
}

void BatchTranslationQueueController::restoreFromJson(const QJsonArray &entriesJson)
{
    m_entries.clear();
    for (const QJsonValue &value : entriesJson) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        BatchQueueEntry entry;
        entry.queueId = object.value(QStringLiteral("queueId")).toString();
        entry.pdfPath = object.value(QStringLiteral("pdfPath")).toString();
        entry.task = domain::DocumentTranslationTask::fromJson(object.value(QStringLiteral("task")).toObject());
        entry.extractedText = object.value(QStringLiteral("extractedText")).toString();
        entry.translatedText = object.value(QStringLiteral("translatedText")).toString();
        if (entry.queueId.isEmpty()) {
            entry.queueId = entry.task.taskId;
        }
        if (entry.task.stage == domain::DocumentTaskStage::Extracting
            || entry.task.stage == domain::DocumentTaskStage::DetectingLanguage
            || entry.task.stage == domain::DocumentTaskStage::Translating
            || entry.task.stage == domain::DocumentTaskStage::Assembling) {
            entry.task.transitionTo(domain::DocumentTaskStage::Queued);
            entry.task.progress = qBound(0, entry.task.progress, 79);
        }
        if (!entry.queueId.isEmpty() && !entry.pdfPath.isEmpty()) {
            m_entries.append(entry);
        }
    }

    emit queueChanged();
    startQueuedJobs();
}

void BatchTranslationQueueController::startQueuedJobs()
{
    if (m_paused) {
        return;
    }

    while (m_activeRuns.size() < m_maxConcurrentJobs) {
        int nextIndex = -1;
        for (int i = 0; i < m_entries.size(); ++i) {
            if (m_entries.at(i).task.stage == domain::DocumentTaskStage::Queued) {
                nextIndex = i;
                break;
            }
        }

        if (nextIndex < 0) {
            if (m_activeRuns.isEmpty()) {
                m_lastStartedQueueId.clear();
                emit activeTaskChanged(QString());
            }
            return;
        }

        BatchQueueEntry &entry = m_entries[nextIndex];
        entry.task.transitionTo(domain::DocumentTaskStage::Extracting);
        entry.task.progress = 1;
        m_lastStartedQueueId = entry.queueId;
        emit activeTaskChanged(m_lastStartedQueueId);
        emit queueChanged();

        QThread *thread = new QThread(this);
        auto *worker = new BatchTranslationWorker(entry.queueId, entry.pdfPath, m_skillRegistry, m_modelRouter);
        worker->moveToThread(thread);

        connect(thread, &QThread::started, worker, &BatchTranslationWorker::run);
        connect(worker, &BatchTranslationWorker::progress, this,
                [this](const QString &queueId, const pdftranslator::domain::DocumentTranslationTask &task) {
                    for (BatchQueueEntry &entry : m_entries) {
                        if (entry.queueId == queueId) {
                            entry.task = task;
                            break;
                        }
                    }
                    emit taskProgressChanged(queueId, task);
                    emit queueChanged();
                });
        connect(worker, &BatchTranslationWorker::finished, this,
                [this, worker, thread](const QString &queueId,
                                       const pdftranslator::domain::DocumentTranslationTask &task,
                                       const QString &extractedText,
                                       const QString &translatedText,
                                       const QString &errorMessage) {
                    for (BatchQueueEntry &entry : m_entries) {
                        if (entry.queueId == queueId) {
                            entry.task = task;
                            entry.extractedText = extractedText;
                            entry.translatedText = translatedText;
                            if (!errorMessage.trimmed().isEmpty()) {
                                entry.task.lastError = errorMessage.trimmed();
                            }
                            break;
                        }
                    }

                    m_activeRuns.remove(queueId);
                    emit activeJobsChanged(m_activeRuns.size(), m_maxConcurrentJobs);
                    if (m_activeRuns.isEmpty()) {
                        emit activeTaskChanged(QString());
                    }
                    emit queueChanged();

                    worker->deleteLater();
                    thread->quit();
                    thread->wait();
                    thread->deleteLater();
                    startQueuedJobs();
                });

        ActiveRun run;
        run.worker = worker;
        run.thread = thread;
        m_activeRuns.insert(entry.queueId, run);
        emit activeJobsChanged(m_activeRuns.size(), m_maxConcurrentJobs);
        thread->start();
    }
}

} // namespace pdftranslator::pipeline

#include "batchtranslationqueuecontroller.moc"
