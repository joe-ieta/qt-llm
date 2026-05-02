#include "managedllamacppruntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QTcpSocket>
#include <QThread>

namespace qtllm::runtime {

namespace {

QString appRuntimeRoot()
{
    const QString appDir = QCoreApplication::instance()
        ? QCoreApplication::applicationDirPath()
        : QDir::currentPath();
    return QDir(appDir).filePath(QStringLiteral("llama-cpp-runtime"));
}

QString normalizedProvider(const QString &providerName)
{
    return providerName.trimmed().toLower();
}

} // namespace

ManagedLlamaCppRuntime::ManagedLlamaCppRuntime(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
}

ManagedLlamaCppRuntime::~ManagedLlamaCppRuntime()
{
    stop();
}

bool ManagedLlamaCppRuntime::isManagedProvider(const QString &providerName)
{
    const QString provider = normalizedProvider(providerName);
    return provider == QStringLiteral("llama-cpp")
        || provider == QStringLiteral("llamacpp")
        || provider == QStringLiteral("llama-cpp-local");
}

QString ManagedLlamaCppRuntime::defaultBaseUrl(int port)
{
    const int resolvedPort = port > 0 ? port : 18080;
    return QStringLiteral("http://127.0.0.1:%1/v1").arg(resolvedPort);
}

LlamaCppRuntimeLayout ManagedLlamaCppRuntime::defaultLayout(const LlmConfig &config)
{
    LlamaCppRuntimeLayout layout;
    layout.rootDir = config.llamaCppRuntimeRoot.trimmed().isEmpty()
        ? appRuntimeRoot()
        : config.llamaCppRuntimeRoot.trimmed();
    QDir root(layout.rootDir);
    layout.binDir = root.filePath(QStringLiteral("bin"));
    layout.modelsDir = root.filePath(QStringLiteral("models"));
    layout.logsDir = root.filePath(QStringLiteral("logs"));

    if (!config.llamaCppExecutablePath.trimmed().isEmpty()) {
        layout.executablePath = config.llamaCppExecutablePath.trimmed();
    } else {
#ifdef Q_OS_WIN
        layout.executablePath = QDir(layout.binDir).filePath(QStringLiteral("llama-server.exe"));
#else
        layout.executablePath = QDir(layout.binDir).filePath(QStringLiteral("llama-server"));
#endif
    }
    return layout;
}

bool ManagedLlamaCppRuntime::ensureDefaultLayout(const LlmConfig &config,
                                                 LlamaCppRuntimeLayout *layout,
                                                 QString *errorMessage)
{
    const LlamaCppRuntimeLayout resolved = defaultLayout(config);
    const QStringList dirs = {resolved.rootDir, resolved.binDir, resolved.modelsDir, resolved.logsDir};
    for (const QString &path : dirs) {
        QDir dir(path);
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to create llama.cpp runtime directory: ") + path;
            }
            return false;
        }
    }
    if (layout) {
        *layout = resolved;
    }
    return true;
}

QList<LlamaCppLocalModel> ManagedLlamaCppRuntime::listLocalModels(const LlmConfig &config,
                                                                  LlamaCppRuntimeLayout *layout,
                                                                  QString *errorMessage)
{
    LlamaCppRuntimeLayout resolved;
    if (!ensureDefaultLayout(config, &resolved, errorMessage)) {
        return {};
    }

    const QDir modelsDir(resolved.modelsDir);
    const QStringList files = modelsDir.entryList(QStringList({QStringLiteral("*.gguf")}),
                                                  QDir::Files,
                                                  QDir::Name | QDir::IgnoreCase);

    QList<LlamaCppLocalModel> models;
    models.reserve(files.size());
    for (const QString &fileName : files) {
        const QString filePath = modelsDir.absoluteFilePath(fileName);
        const QString modelId = QFileInfo(filePath).completeBaseName();
        models.push_back(LlamaCppLocalModel{
            modelId.isEmpty() ? fileName : modelId,
            filePath,
            modelId.isEmpty() ? fileName : modelId
        });
    }

    if (layout) {
        *layout = resolved;
    }
    return models;
}

bool ManagedLlamaCppRuntime::ensureRunning(LlmConfig *config, QString *errorMessage)
{
    if (!config) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Missing LlmConfig for llama.cpp runtime");
        }
        return false;
    }

    LlamaCppRuntimeLayout layout;
    if (!ensureDefaultLayout(*config, &layout, errorMessage)) {
        return false;
    }

    const int port = config->llamaCppServerPort > 0 ? config->llamaCppServerPort : 18080;
    const QString executablePath = QFileInfo(layout.executablePath).absoluteFilePath();
    const QString modelPath = resolveModelPath(*config, layout);

    config->runtimeName = QStringLiteral("llama-cpp-managed");
    config->providerName = QStringLiteral("llama-cpp");
    config->baseUrl = defaultBaseUrl(port);
    config->llamaCppRuntimeRoot = layout.rootDir;
    config->llamaCppExecutablePath = executablePath;
    config->llamaCppModelPath = modelPath;
    const QString resolvedModelId = QFileInfo(modelPath).completeBaseName();
    if (config->model.trimmed().isEmpty() && !resolvedModelId.isEmpty()) {
        config->model = resolvedModelId;
    }

    if (!QFileInfo::exists(executablePath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("llama.cpp server executable not found: %1. Put llama-server in %2.")
                                .arg(executablePath, layout.binDir);
        }
        return false;
    }

    if (modelPath.trimmed().isEmpty() || !QFileInfo::exists(modelPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GGUF model not found. Put a .gguf model in %1 or set llamaCppModelPath.")
                                .arg(layout.modelsDir);
        }
        return false;
    }

    if (m_process->state() != QProcess::NotRunning
        && m_activeExecutablePath == executablePath
        && m_activeModelPath == modelPath
        && m_activePort == port) {
        return true;
    }

    stop();

    QStringList args;
    args << QStringLiteral("--host") << QStringLiteral("127.0.0.1")
         << QStringLiteral("--port") << QString::number(port)
         << QStringLiteral("-m") << modelPath
         << QStringLiteral("--ctx-size") << QString::number(config->llamaCppContextSize > 0 ? config->llamaCppContextSize : 4096);
    if (config->llamaCppGpuLayers >= 0) {
        args << QStringLiteral("--n-gpu-layers") << QString::number(config->llamaCppGpuLayers);
    }
    if (config->llamaCppThreadCount > 0) {
        args << QStringLiteral("--threads") << QString::number(config->llamaCppThreadCount);
    }
    args.append(config->llamaCppExtraArgs);

    m_process->setProgram(executablePath);
    m_process->setArguments(args);
    m_process->setWorkingDirectory(layout.rootDir);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->start();

    if (!m_process->waitForStarted(5000)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to start llama.cpp server: ") + m_process->errorString();
        }
        return false;
    }

    const int timeoutMs = config->llamaCppStartupTimeoutMs > 0 ? config->llamaCppStartupTimeoutMs : 30000;
    if (!waitForPort(port, timeoutMs)) {
        const QByteArray output = m_process->readAll();
        stop();
        if (errorMessage) {
            *errorMessage = QStringLiteral("llama.cpp server did not open port %1 within %2 ms. %3")
                                .arg(port)
                                .arg(timeoutMs)
                                .arg(QString::fromUtf8(output.left(2048)));
        }
        return false;
    }

    m_activeExecutablePath = executablePath;
    m_activeModelPath = modelPath;
    m_activePort = port;
    return true;
}

void ManagedLlamaCppRuntime::stop()
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        return;
    }
    m_process->terminate();
    if (!m_process->waitForFinished(3000)) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    m_activeExecutablePath.clear();
    m_activeModelPath.clear();
    m_activePort = 0;
}

bool ManagedLlamaCppRuntime::waitForPort(int port, int timeoutMs) const
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QTcpSocket socket;
        socket.connectToHost(QStringLiteral("127.0.0.1"), static_cast<quint16>(port));
        if (socket.waitForConnected(250)) {
            socket.disconnectFromHost();
            return true;
        }
        QThread::msleep(100);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return false;
}

QString ManagedLlamaCppRuntime::resolveModelPath(const LlmConfig &config,
                                                 const LlamaCppRuntimeLayout &layout) const
{
    if (!config.llamaCppModelPath.trimmed().isEmpty()) {
        return QFileInfo(config.llamaCppModelPath.trimmed()).absoluteFilePath();
    }

    const QDir modelsDir(layout.modelsDir);
    const QStringList files = modelsDir.entryList(QStringList({QStringLiteral("*.gguf")}),
                                                  QDir::Files,
                                                  QDir::Name);
    if (files.isEmpty()) {
        return QString();
    }
    return modelsDir.absoluteFilePath(files.first());
}

} // namespace qtllm::runtime
