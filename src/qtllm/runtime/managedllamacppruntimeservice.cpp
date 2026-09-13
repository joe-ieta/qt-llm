#include "managedllamacppruntimeservice.h"

#include "managedllamacppruntime.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QSet>
#include <QThread>
#include <QUuid>
#include <QWaitCondition>

#include <climits>

namespace qtllm::runtime {

namespace {

QString normalizedIdentityPath(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }
    QString normalized = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
    normalized = normalized.toLower();
#endif
    return normalized;
}

QString runtimeKey(const LlmConfig &config)
{
    QStringList identity;
    identity << normalizedIdentityPath(config.llamaCppRuntimeRoot)
             << normalizedIdentityPath(config.llamaCppExecutablePath)
             << normalizedIdentityPath(config.llamaCppModelPath)
             << config.model.trimmed()
             << QString::number(config.llamaCppServerPort > 0
                                    ? config.llamaCppServerPort
                                    : 18080)
             << config.llamaCppGpuMode.trimmed().toLower()
             << config.llamaCppPerformanceProfile.trimmed().toLower()
             << config.llamaCppContextMode.trimmed().toLower()
             << QString::number(config.llamaCppGpuLayers)
             << QString::number(config.llamaCppThreadCount)
             << QString::number(config.llamaCppContextSize)
             << config.llamaCppExtraArgs.join(QChar(0x1f));
    return QString::fromLatin1(
        QCryptographicHash::hash(identity.join(QChar(0x1e)).toUtf8(),
                                 QCryptographicHash::Sha256)
            .toHex());
}

int normalizedTimeout(int value, int fallback)
{
    return value > 0 ? value : fallback;
}

} // namespace

struct ManagedLlamaCppRuntimeService::Impl
{
    struct Entry
    {
        QString key;
        QString instanceId;
        int port = 0;
        LlmConfig config;
        LlamaCppRuntimeOwnership ownership = LlamaCppRuntimeOwnership::Unknown;
        LlamaCppRuntimeAcquireStatus status = LlamaCppRuntimeAcquireStatus::Starting;
        QString errorCode;
        QString errorMessage;
        QSet<QString> pendingRequests;
        QSet<QString> leaseIds;
        ManagedLlamaCppRuntime *runtime = nullptr;
    };

    Impl()
        : worker(new QObject)
    {
        worker->moveToThread(&workerThread);
        workerThread.start();
    }

    void stopWorker()
    {
        if (!worker) {
            return;
        }
        QObject *workerObject = worker;
        QThread *ownerThread = QThread::currentThread();
        QMetaObject::invokeMethod(workerObject,
                                  [workerObject, ownerThread]() {
                                      workerObject->moveToThread(ownerThread);
                                  },
                                  Qt::BlockingQueuedConnection);
        workerThread.quit();
        workerThread.wait();
        delete workerObject;
        worker = nullptr;
    }

    LlamaCppRuntimeLease makeLease(const std::shared_ptr<Entry> &entry,
                                   const QString &requestId,
                                   const QString &callerId,
                                   int requestTimeoutMs)
    {
        LlamaCppRuntimeLease lease;
        lease.requestId = requestId;
        lease.leaseId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        lease.instanceId = entry->instanceId;
        lease.callerId = callerId;
        lease.config = entry->config;
        lease.ownership = entry->ownership;
        lease.status = LlamaCppRuntimeAcquireStatus::Ready;
        lease.requestTimeoutMs = requestTimeoutMs;
        entry->pendingRequests.remove(requestId);
        entry->leaseIds.insert(lease.leaseId);
        leaseEntries.insert(lease.leaseId, entry);
        canceledRequests.remove(requestId);
        return lease;
    }

    void destroyRuntime(const std::shared_ptr<Entry> &entry)
    {
        ManagedLlamaCppRuntime *runtime = nullptr;
        {
            QMutexLocker locker(&mutex);
            runtime = entry->runtime;
            entry->runtime = nullptr;
        }
        if (!runtime) {
            return;
        }
        QMetaObject::invokeMethod(worker,
                                  [runtime]() {
                                      runtime->stop();
                                      delete runtime;
                                  },
                                  Qt::BlockingQueuedConnection);
    }

    bool hasInterestedRequest(const std::shared_ptr<Entry> &entry) const
    {
        for (const QString &requestId : entry->pendingRequests) {
            if (!canceledRequests.contains(requestId)) {
                return true;
            }
        }
        return false;
    }

    mutable QMutex mutex;
    QWaitCondition changed;
    QHash<QString, std::shared_ptr<Entry>> entries;
    QHash<QString, std::shared_ptr<Entry>> leaseEntries;
    QSet<QString> canceledRequests;
    QThread workerThread;
    QObject *worker = nullptr;
};

ManagedLlamaCppRuntimeService::ManagedLlamaCppRuntimeService()
    : m_impl(std::make_unique<Impl>())
{
}

ManagedLlamaCppRuntimeService::~ManagedLlamaCppRuntimeService()
{
    shutdownOwned();
    m_impl->stopWorker();
}

LlamaCppRuntimeLease ManagedLlamaCppRuntimeService::acquire(
    LlmConfig config,
    const LlamaCppRuntimeAcquireOptions &options,
    QString *errorMessage)
{
    const QString requestId = options.requestId.trimmed().isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : options.requestId.trimmed();
    const int queueTimeoutMs = normalizedTimeout(options.queueTimeoutMs, 30000);
    const int startupTimeoutMs =
        normalizedTimeout(options.startupTimeoutMs, 180000);
    const int requestTimeoutMs =
        normalizedTimeout(options.requestTimeoutMs, config.timeoutMs);
    const int totalTimeoutMs =
        normalizedTimeout(options.totalTimeoutMs, queueTimeoutMs + startupTimeoutMs);
    const int port = config.llamaCppServerPort > 0
        ? config.llamaCppServerPort
        : 18080;
    config.llamaCppServerPort = port;
    const QString key = runtimeKey(config);

    QElapsedTimer totalTimer;
    QElapsedTimer queueTimer;
    totalTimer.start();
    queueTimer.start();

    auto failure = [&](LlamaCppRuntimeAcquireStatus status,
                       const QString &code,
                       const QString &message) {
        LlamaCppRuntimeLease lease;
        lease.requestId = requestId;
        lease.callerId = options.callerId;
        lease.config = config;
        lease.status = status;
        lease.errorCode = code;
        lease.errorMessage = message;
        lease.requestTimeoutMs = requestTimeoutMs;
        if (errorMessage) {
            *errorMessage = message;
        }
        return lease;
    };

    std::shared_ptr<Impl::Entry> entry;
    bool startsInstance = false;
    for (;;) {
        std::shared_ptr<Impl::Entry> cleanup;
        {
            QMutexLocker locker(&m_impl->mutex);
            entry = m_impl->entries.value(key);

            if (m_impl->canceledRequests.contains(requestId)) {
                m_impl->canceledRequests.remove(requestId);
                if (entry) {
                    entry->pendingRequests.remove(requestId);
                    if (entry->status == LlamaCppRuntimeAcquireStatus::Ready
                        && entry->pendingRequests.isEmpty()
                        && entry->leaseIds.isEmpty()) {
                        m_impl->entries.remove(entry->key);
                        cleanup = entry;
                    }
                }
                locker.unlock();
                if (cleanup) {
                    m_impl->destroyRuntime(cleanup);
                }
                return failure(LlamaCppRuntimeAcquireStatus::Canceled,
                               QStringLiteral("runtime_acquire_canceled"),
                               QStringLiteral("Runtime acquisition was canceled"));
            }

            if (totalTimer.elapsed() >= totalTimeoutMs) {
                if (entry) {
                    entry->pendingRequests.remove(requestId);
                }
                return failure(LlamaCppRuntimeAcquireStatus::TotalTimedOut,
                               QStringLiteral("runtime_total_timeout"),
                               QStringLiteral("Runtime acquisition exceeded total timeout"));
            }

            if (entry && entry->status == LlamaCppRuntimeAcquireStatus::Ready) {
                LlamaCppRuntimeLease lease = m_impl->makeLease(
                    entry, requestId, options.callerId, requestTimeoutMs);
                if (errorMessage) {
                    errorMessage->clear();
                }
                return lease;
            }

            if (entry && entry->status != LlamaCppRuntimeAcquireStatus::Starting) {
                const LlamaCppRuntimeAcquireStatus status = entry->status;
                const QString code = entry->errorCode;
                const QString message = entry->errorMessage;
                entry->pendingRequests.remove(requestId);
                if (entry->pendingRequests.isEmpty() && entry->leaseIds.isEmpty()) {
                    m_impl->entries.remove(entry->key);
                }
                return failure(status, code, message);
            }

            if (!entry) {
                bool portBusy = false;
                for (const std::shared_ptr<Impl::Entry> &candidate :
                     m_impl->entries) {
                    if (candidate->port == port
                        && (candidate->status == LlamaCppRuntimeAcquireStatus::Starting
                            || candidate->status == LlamaCppRuntimeAcquireStatus::Ready)) {
                        portBusy = true;
                        break;
                    }
                }

                if (!portBusy) {
                    entry = std::make_shared<Impl::Entry>();
                    entry->key = key;
                    entry->instanceId =
                        QStringLiteral("llama-cpp-") + key.left(16);
                    entry->port = port;
                    entry->config = config;
                    entry->pendingRequests.insert(requestId);
                    m_impl->entries.insert(key, entry);
                    startsInstance = true;
                    break;
                }
            } else {
                entry->pendingRequests.insert(requestId);
            }

            const int queueRemaining = queueTimeoutMs - queueTimer.elapsed();
            const int totalRemaining = totalTimeoutMs - totalTimer.elapsed();
            if (queueRemaining <= 0) {
                if (entry) {
                    entry->pendingRequests.remove(requestId);
                }
                return failure(LlamaCppRuntimeAcquireStatus::QueueTimedOut,
                               QStringLiteral("runtime_queue_timeout"),
                               QStringLiteral("Runtime acquisition exceeded queue timeout"));
            }
            if (totalRemaining <= 0) {
                if (entry) {
                    entry->pendingRequests.remove(requestId);
                }
                return failure(LlamaCppRuntimeAcquireStatus::TotalTimedOut,
                               QStringLiteral("runtime_total_timeout"),
                               QStringLiteral("Runtime acquisition exceeded total timeout"));
            }
            m_impl->changed.wait(
                &m_impl->mutex,
                static_cast<unsigned long>(qMin(queueRemaining, totalRemaining)));
        }
    }

    if (!startsInstance || !entry) {
        return failure(LlamaCppRuntimeAcquireStatus::Failed,
                       QStringLiteral("runtime_internal_error"),
                       QStringLiteral("Runtime acquisition could not create an instance"));
    }

    const int totalRemaining = totalTimeoutMs - totalTimer.elapsed();
    if (totalRemaining <= 0) {
        QMutexLocker locker(&m_impl->mutex);
        entry->status = LlamaCppRuntimeAcquireStatus::TotalTimedOut;
        entry->errorCode = QStringLiteral("runtime_total_timeout");
        entry->errorMessage = QStringLiteral("Runtime acquisition exceeded total timeout");
        m_impl->changed.wakeAll();
    } else {
        const int effectiveStartupTimeout = qMin(startupTimeoutMs, totalRemaining);
        LlmConfig resolvedConfig = config;
        resolvedConfig.llamaCppStartupTimeoutMs = effectiveStartupTimeout;
        QString startupError;
        bool started = false;
        LlamaCppRuntimeOwnership ownership = LlamaCppRuntimeOwnership::Unknown;
        QElapsedTimer startupTimer;
        startupTimer.start();

        const bool invoked = QMetaObject::invokeMethod(
            m_impl->worker,
            [this,
             entry,
             &resolvedConfig,
             &startupError,
             &started,
             &ownership]() {
                ManagedLlamaCppRuntime *runtime = new ManagedLlamaCppRuntime;
                bool cancelStartup = false;
                {
                    QMutexLocker locker(&m_impl->mutex);
                    entry->runtime = runtime;
                    cancelStartup = !m_impl->hasInterestedRequest(entry);
                }
                if (cancelStartup) {
                    runtime->requestStop();
                }
                started = runtime->ensureRunning(&resolvedConfig, &startupError);
                if (started) {
                    ownership = runtime->ownsProcess()
                        ? LlamaCppRuntimeOwnership::LibraryOwned
                        : LlamaCppRuntimeOwnership::ExternalService;
                } else {
                    runtime->stop();
                    delete runtime;
                    QMutexLocker locker(&m_impl->mutex);
                    entry->runtime = nullptr;
                }
            },
            Qt::BlockingQueuedConnection);

        QMutexLocker locker(&m_impl->mutex);
        entry->config = resolvedConfig;
        entry->ownership = ownership;
        if (invoked && started) {
            entry->status = LlamaCppRuntimeAcquireStatus::Ready;
            entry->errorCode.clear();
            entry->errorMessage.clear();
        } else if (startupError.contains(QStringLiteral("canceled"),
                                         Qt::CaseInsensitive)) {
            entry->status = LlamaCppRuntimeAcquireStatus::Canceled;
            entry->errorCode = QStringLiteral("runtime_acquire_canceled");
            entry->errorMessage = startupError;
        } else if (totalTimer.elapsed() >= totalTimeoutMs) {
            entry->status = LlamaCppRuntimeAcquireStatus::TotalTimedOut;
            entry->errorCode = QStringLiteral("runtime_total_timeout");
            entry->errorMessage = startupError;
        } else if (startupTimer.elapsed() >= effectiveStartupTimeout) {
            entry->status = LlamaCppRuntimeAcquireStatus::StartupTimedOut;
            entry->errorCode = QStringLiteral("runtime_startup_timeout");
            entry->errorMessage = startupError;
        } else {
            entry->status = LlamaCppRuntimeAcquireStatus::Failed;
            entry->errorCode = QStringLiteral("runtime_start_failed");
            entry->errorMessage = invoked
                ? startupError
                : QStringLiteral("Failed to invoke the runtime worker");
        }
        m_impl->changed.wakeAll();
    }

    std::shared_ptr<Impl::Entry> cleanup;
    LlamaCppRuntimeLease result;
    {
        QMutexLocker locker(&m_impl->mutex);
        if (entry->status == LlamaCppRuntimeAcquireStatus::Ready
            && !m_impl->canceledRequests.contains(requestId)
            && totalTimer.elapsed() < totalTimeoutMs) {
            result = m_impl->makeLease(
                entry, requestId, options.callerId, requestTimeoutMs);
            if (errorMessage) {
                errorMessage->clear();
            }
            return result;
        }

        LlamaCppRuntimeAcquireStatus status = entry->status;
        QString code = entry->errorCode;
        QString message = entry->errorMessage;
        if (m_impl->canceledRequests.remove(requestId)) {
            status = LlamaCppRuntimeAcquireStatus::Canceled;
            code = QStringLiteral("runtime_acquire_canceled");
            message = QStringLiteral("Runtime acquisition was canceled");
        } else if (totalTimer.elapsed() >= totalTimeoutMs
                   && status == LlamaCppRuntimeAcquireStatus::Ready) {
            status = LlamaCppRuntimeAcquireStatus::TotalTimedOut;
            code = QStringLiteral("runtime_total_timeout");
            message = QStringLiteral("Runtime acquisition exceeded total timeout");
        }
        entry->pendingRequests.remove(requestId);
        if (entry->pendingRequests.isEmpty() && entry->leaseIds.isEmpty()) {
            m_impl->entries.remove(entry->key);
            cleanup = entry;
        }
        result = failure(status, code, message);
    }
    if (cleanup) {
        m_impl->destroyRuntime(cleanup);
    }
    return result;
}

bool ManagedLlamaCppRuntimeService::cancelAcquire(const QString &requestId)
{
    const QString normalized = requestId.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    ManagedLlamaCppRuntime *runtime = nullptr;
    {
        QMutexLocker locker(&m_impl->mutex);
        m_impl->canceledRequests.insert(normalized);
        for (const std::shared_ptr<Impl::Entry> &entry : m_impl->entries) {
            if (entry->pendingRequests.contains(normalized)
                && !m_impl->hasInterestedRequest(entry)) {
                runtime = entry->runtime;
            }
        }
        m_impl->changed.wakeAll();
    }
    if (runtime) {
        runtime->requestStop();
    }
    return true;
}

bool ManagedLlamaCppRuntimeService::release(const QString &leaseId)
{
    std::shared_ptr<Impl::Entry> cleanup;
    {
        QMutexLocker locker(&m_impl->mutex);
        const auto leaseIt = m_impl->leaseEntries.find(leaseId);
        if (leaseIt == m_impl->leaseEntries.end()) {
            return false;
        }
        const std::shared_ptr<Impl::Entry> entry = leaseIt.value();
        m_impl->leaseEntries.erase(leaseIt);
        entry->leaseIds.remove(leaseId);
        if (entry->leaseIds.isEmpty() && entry->pendingRequests.isEmpty()) {
            m_impl->entries.remove(entry->key);
            cleanup = entry;
            m_impl->changed.wakeAll();
        }
    }
    if (cleanup) {
        m_impl->destroyRuntime(cleanup);
    }
    return true;
}

QList<LlamaCppRuntimeInstanceSnapshot>
ManagedLlamaCppRuntimeService::instances() const
{
    QList<LlamaCppRuntimeInstanceSnapshot> snapshots;
    QMutexLocker locker(&m_impl->mutex);
    snapshots.reserve(m_impl->entries.size());
    for (const std::shared_ptr<Impl::Entry> &entry : m_impl->entries) {
        LlamaCppRuntimeInstanceSnapshot snapshot;
        snapshot.instanceId = entry->instanceId;
        snapshot.modelPath = entry->config.llamaCppModelPath;
        snapshot.executablePath = entry->config.llamaCppExecutablePath;
        snapshot.launchArguments = entry->config.llamaCppExtraArgs;
        snapshot.port = entry->port;
        snapshot.activeLeaseCount = entry->leaseIds.size();
        snapshot.pendingAcquireCount = entry->pendingRequests.size();
        snapshot.ownership = entry->ownership;
        snapshot.status = entry->status;
        snapshots.append(snapshot);
    }
    return snapshots;
}

void ManagedLlamaCppRuntimeService::shutdownOwned()
{
    QList<std::shared_ptr<Impl::Entry>> entries;
    {
        QMutexLocker locker(&m_impl->mutex);
        entries = m_impl->entries.values();
        m_impl->entries.clear();
        m_impl->leaseEntries.clear();
        m_impl->canceledRequests.clear();
        for (const std::shared_ptr<Impl::Entry> &entry : entries) {
            if (entry->runtime) {
                entry->runtime->requestStop();
            }
        }
        m_impl->changed.wakeAll();
    }
    for (const std::shared_ptr<Impl::Entry> &entry : entries) {
        m_impl->destroyRuntime(entry);
    }
}

} // namespace qtllm::runtime
