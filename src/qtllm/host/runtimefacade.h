#pragma once

#include "modelcatalogservice.h"
#include "runtimeprofile.h"
#include "../qtllm_global.h"

#include <QObject>
#include <memory>

namespace qtllm {
class QtLLMClient;
}

namespace qtllm::tools::runtime {
class ToolCallOrchestrator;
}

namespace qtllm::runtime {
class ManagedLlamaCppRuntimeService;
}

namespace qtllm::host {

class RuntimeRequestHandle;

class QTLLM_CONVERSATION_EXPORT RuntimeFacade : public QObject
{
    Q_OBJECT
public:
    explicit RuntimeFacade(QObject *parent = nullptr);
    ~RuntimeFacade() override;

    void setProfile(const RuntimeProfile &profile);
    RuntimeProfile profile() const;
    ModelCapabilitySnapshot modelCapabilities() const;

    QList<LocalModelInfo> listLocalModels(QString *errorMessage = nullptr) const;
    bool refreshRuntimeAvailability(QString *message = nullptr);
    void setManagedLlamaCppRuntimeService(
        const std::shared_ptr<runtime::ManagedLlamaCppRuntimeService> &service);
    std::shared_ptr<runtime::ManagedLlamaCppRuntimeService>
    managedLlamaCppRuntimeService() const;

    // Host-driven tool loops (External mode) can disable the library tool loop
    // so tool calls are returned to the caller instead of being executed
    // internally. Pass nullptr to disable; when never configured, the library
    // loop keeps its default behavior.
    void setToolCallOrchestrator(
        const std::shared_ptr<tools::runtime::ToolCallOrchestrator> &orchestrator);

    RuntimeRequestHandle *sendAsync(const ChatRequest &request);
    void send(const ChatRequest &request);
    ChatResult sendBlocking(const ChatRequest &request, int timeoutMs = 0);
    void cancel();

signals:
    void tokenReceived(const QString &token);
    void reasoningTokenReceived(const QString &token);
    void completed(const qtllm::host::ChatResult &result);
    void errorOccurred(const qtllm::host::ChatResult &result);
    void providerPayloadPrepared(const QString &url, const QString &payloadJson);
    void runtimeStatusChanged(const QString &status, const QString &detail);

private:
    ChatResult baseResult(const ChatRequest &request) const;
    bool configureForRequest(const ChatRequest &request);
    void finishWithError(const ChatRequest &request, const QString &code, const QString &message);

private:
    RuntimeProfile m_profile;
    std::shared_ptr<runtime::ManagedLlamaCppRuntimeService> m_runtimeService;
    QtLLMClient *m_client = nullptr;
    ModelCatalogService m_modelCatalog;
    ChatRequest m_activeRequest;
    QString m_configuredProviderName;
    bool m_requestActive = false;
    bool m_toolLoopConfigured = false;
    std::shared_ptr<tools::runtime::ToolCallOrchestrator> m_toolCallOrchestrator;
};

} // namespace qtllm::host
