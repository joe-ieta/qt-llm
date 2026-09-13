#pragma once

#include "illmeventsink.h"

#include <QReadWriteLock>
#include <QVector>

#include <memory>

namespace qtllm::events {

class LlmEventDispatcher final : public ILlmEventSink
{
public:
    static LlmEventDispatcher &instance();

    void addSink(const std::shared_ptr<ILlmEventSink> &sink);
    void removeSink(const std::shared_ptr<ILlmEventSink> &sink);
    void clearSinks();
    int sinkCount() const;

    QString startTrace(const QString &, const QString &, const QString &, const QString &,
                       const QString &, const QString &, const QString &) override;
    void recordToolSelection(const QString &, const QStringList &, const QString &) override;
    void recordRequestPrepared(const QString &, const QString &, const QString &, const QString &) override;
    void recordRequestDispatched(const QString &, const QString &, const QString &, const QString &,
                                 const QString &, const QString &, const QString &, const QString &,
                                 int, int) override;
    void recordRequestAttemptStarted(const QString &, const QString &, const QString &,
                                     const QString &, int) override;
    void recordRequestAttemptReset(const QString &, const QString &, const QString &, const QString &,
                                   int, int) override;
    void recordStreamDelta(const QString &, const QString &, const QString &, int) override;
    void recordFirstStreamToken(const QString &, const QString &, const QString &) override;
    void recordResponseParsed(const QString &, const QString &, const qtllm::LlmResponse &,
                              const QString &) override;
    void recordToolCallsParsed(const qtllm::tools::runtime::ToolExecutionContext &, const QString &,
                               const QString &, int,
                               const QList<qtllm::tools::runtime::ToolCallRequest> &) override;
    void recordToolBatchStarted(const qtllm::tools::runtime::ToolExecutionContext &, const QString &,
                                int, int) override;
    void recordToolCallStarted(const qtllm::tools::runtime::ToolExecutionContext &, const QString &,
                               int, const qtllm::tools::runtime::ToolCallRequest &) override;
    void recordToolCallFinished(const qtllm::tools::runtime::ToolExecutionContext &, const QString &,
                                int, const qtllm::tools::runtime::ToolCallRequest &,
                                const qtllm::tools::runtime::ToolExecutionResult &) override;
    void recordFollowUpPrompt(const qtllm::tools::runtime::ToolExecutionContext &, const QString &,
                              int, const QString &,
                              const QList<qtllm::tools::runtime::ToolExecutionResult> &) override;
    void recordFailureGuard(const qtllm::tools::runtime::ToolExecutionContext &, const QString &,
                            int, int, const QString &) override;
    void recordCancellationRequested(const QString &, const QString &, const QString &,
                                     const QString &) override;
    void recordTraceCompleted(const QString &, const QString &, const QString &, const QString &,
                              const QString &, const QString &) override;
    void recordTraceError(const QString &, const QString &, const QString &, const QString &,
                          const QString &, const QString &) override;

private:
    LlmEventDispatcher() = default;
    QVector<std::shared_ptr<ILlmEventSink>> sinksSnapshot() const;

    mutable QReadWriteLock m_lock;
    QVector<std::shared_ptr<ILlmEventSink>> m_sinks;
};

} // namespace qtllm::events
