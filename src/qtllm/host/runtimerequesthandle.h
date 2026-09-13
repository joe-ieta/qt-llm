#pragma once

#include "runtimeprofile.h"

#include <QObject>

namespace qtllm {
class QtLLMClient;
}

namespace qtllm::host {

class RuntimeRequestHandle final : public QObject
{
    Q_OBJECT
public:
    RuntimeRequestHandle(const RuntimeProfile &profile,
                         const ChatRequest &request,
                         QObject *parent = nullptr);
    ~RuntimeRequestHandle() override;

    QString requestId() const;
    RuntimeRequestState state() const;
    bool isFinished() const;
    ChatRequest request() const;
    ChatResult result() const;

public slots:
    void start();
    void cancel();

signals:
    void tokenReceived(const QString &requestId, const QString &token);
    void reasoningTokenReceived(const QString &requestId, const QString &token);
    void streamReset(const QString &requestId, int nextAttempt);
    void providerPayloadPrepared(const QString &requestId,
                                 const QString &url,
                                 const QString &payloadJson);
    void stateChanged(const QString &requestId, qtllm::host::RuntimeRequestState state);
    void finished(const qtllm::host::ChatResult &result);

private:
    void transitionTo(RuntimeRequestState state);
    void finishFromResponse(const qtllm::LlmResponse &response);
    void finishWithError(LlmErrorCategory category,
                         const QString &code,
                         const QString &message,
                         bool canceled = false);

private:
    RuntimeProfile m_profile;
    ChatRequest m_request;
    ChatResult m_result;
    QString m_requestId;
    QtLLMClient *m_client = nullptr;
    RuntimeRequestState m_state = RuntimeRequestState::Pending;
    bool m_started = false;
    bool m_finished = false;
};

} // namespace qtllm::host
