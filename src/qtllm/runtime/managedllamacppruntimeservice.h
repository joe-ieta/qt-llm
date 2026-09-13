#pragma once

#include "../core/llmconfig.h"

#include <QList>
#include <QString>

#include <memory>

namespace qtllm::runtime {

enum class LlamaCppRuntimeOwnership
{
    Unknown,
    ExternalService,
    LibraryOwned
};

enum class LlamaCppRuntimeAcquireStatus
{
    Invalid,
    Queued,
    Starting,
    Ready,
    Failed,
    Canceled,
    QueueTimedOut,
    StartupTimedOut,
    TotalTimedOut
};

struct LlamaCppRuntimeAcquireOptions
{
    QString requestId;
    QString callerId;
    int queueTimeoutMs = 30000;
    int startupTimeoutMs = 180000;
    int requestTimeoutMs = 60000;
    int totalTimeoutMs = 210000;
};

struct LlamaCppRuntimeLease
{
    QString requestId;
    QString leaseId;
    QString instanceId;
    QString callerId;
    LlmConfig config;
    LlamaCppRuntimeOwnership ownership = LlamaCppRuntimeOwnership::Unknown;
    LlamaCppRuntimeAcquireStatus status = LlamaCppRuntimeAcquireStatus::Invalid;
    QString errorCode;
    QString errorMessage;
    int requestTimeoutMs = 60000;

    bool isValid() const
    {
        return status == LlamaCppRuntimeAcquireStatus::Ready
            && !leaseId.isEmpty()
            && !instanceId.isEmpty();
    }
};

struct LlamaCppRuntimeInstanceSnapshot
{
    QString instanceId;
    QString modelPath;
    QString executablePath;
    QStringList launchArguments;
    int port = 0;
    int activeLeaseCount = 0;
    int pendingAcquireCount = 0;
    LlamaCppRuntimeOwnership ownership = LlamaCppRuntimeOwnership::Unknown;
    LlamaCppRuntimeAcquireStatus status = LlamaCppRuntimeAcquireStatus::Invalid;
};

class ManagedLlamaCppRuntimeService
{
public:
    ManagedLlamaCppRuntimeService();
    ~ManagedLlamaCppRuntimeService();

    ManagedLlamaCppRuntimeService(const ManagedLlamaCppRuntimeService &) = delete;
    ManagedLlamaCppRuntimeService &operator=(const ManagedLlamaCppRuntimeService &) = delete;

    LlamaCppRuntimeLease acquire(
        LlmConfig config,
        const LlamaCppRuntimeAcquireOptions &options = LlamaCppRuntimeAcquireOptions(),
        QString *errorMessage = nullptr);
    bool cancelAcquire(const QString &requestId);
    bool release(const QString &leaseId);
    QList<LlamaCppRuntimeInstanceSnapshot> instances() const;
    void shutdownOwned();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace qtllm::runtime
