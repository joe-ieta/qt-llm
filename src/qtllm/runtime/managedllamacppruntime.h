#pragma once

#include "../qtllm_global.h"

#include "../core/llmconfig.h"

#include <QList>
#include <QObject>
#include <QString>

#include <atomic>

class QProcess;

namespace qtllm::runtime {

struct LlamaCppRuntimeLayout
{
    QString rootDir;
    QString binDir;
    QString modelsDir;
    QString bundledModelsDir;
    QStringList modelSearchDirs;
    QString logsDir;
    QString executablePath;
    QString resolvedModelPath;
    QString discoverySource;
    QString availabilityStatus;
    QString availabilityMessage;
    QStringList gpuBackendNames;
    bool runtimeRootFound = false;
    bool executableAvailable = false;
    bool modelAvailable = false;
    bool gpuBackendAvailable = false;
    bool available = false;
    int modelCount = 0;
};

struct LlamaCppLocalModel
{
    QString id;
    QString filePath;
    QString displayName;
};

class QTLLM_LOCAL_RUNTIME_EXPORT ManagedLlamaCppRuntime : public QObject
{
    Q_OBJECT
public:
    explicit ManagedLlamaCppRuntime(QObject *parent = nullptr);
    ~ManagedLlamaCppRuntime() override;

    static bool isManagedProvider(const QString &providerName);
    static QString defaultBaseUrl(int port = 18080);
    static LlamaCppRuntimeLayout defaultLayout(const LlmConfig &config = {});
    static bool updateRuntimeAvailability(LlmConfig *config,
                                          LlamaCppRuntimeLayout *layout = nullptr,
                                          QString *errorMessage = nullptr);
    static bool ensureDefaultLayout(const LlmConfig &config, LlamaCppRuntimeLayout *layout, QString *errorMessage);
    static QList<LlamaCppLocalModel> listLocalModels(const LlmConfig &config = {},
                                                     LlamaCppRuntimeLayout *layout = nullptr,
                                                     QString *errorMessage = nullptr);

    bool ensureRunning(LlmConfig *config, QString *errorMessage);
    bool ownsProcess() const;
    bool isRunning() const;
    void requestStop();
    void stop();

private:
    bool waitForServerReady(int port, int timeoutMs, QString *errorMessage) const;
    bool probeServerReady(int port, const QString &path, int *statusCode, QString *errorMessage) const;
    bool isPortOpen(int port) const;
    QString resolveModelPath(const LlmConfig &config, const LlamaCppRuntimeLayout &layout) const;

private:
    QProcess *m_process = nullptr;
    QString m_activeExecutablePath;
    QString m_activeModelPath;
    int m_activePort = 0;
    bool m_externalService = false;
    mutable std::atomic_bool m_stopRequested {false};
};

} // namespace qtllm::runtime
