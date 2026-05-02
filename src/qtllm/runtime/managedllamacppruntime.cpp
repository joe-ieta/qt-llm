#include "managedllamacppruntime.h"

#include "../logging/qtllmlogger.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QStringList>
#include <QTcpSocket>
#include <QThread>

namespace qtllm::runtime {

namespace {

constexpr const char *kRuntimeDirName = "llama-cpp-runtime";

QString appDirPath()
{
    return QCoreApplication::instance()
        ? QCoreApplication::applicationDirPath()
        : QDir::currentPath();
}

QString runtimeChildIfPresent(const QString &basePath)
{
    const QString trimmed = basePath.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    const QFileInfo directInfo(trimmed);
    if (directInfo.exists()
        && directInfo.isDir()
        && directInfo.fileName().compare(QString::fromLatin1(kRuntimeDirName), Qt::CaseInsensitive) == 0) {
        return directInfo.absoluteFilePath();
    }

    const QFileInfo childInfo(QDir(trimmed).filePath(QString::fromLatin1(kRuntimeDirName)));
    if (childInfo.exists() && childInfo.isDir()) {
        return childInfo.absoluteFilePath();
    }
    return QString();
}

void appendCandidate(QStringList *candidates, const QString &candidate)
{
    if (!candidates) {
        return;
    }

    const QString trimmed = candidate.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    const QString normalized = QDir::fromNativeSeparators(trimmed);
    if (!candidates->contains(normalized, Qt::CaseInsensitive)) {
        candidates->append(normalized);
    }
}

QStringList splitEnvironmentPath(const QByteArray &value)
{
    QString text = QString::fromLocal8Bit(value).trimmed();
    if (text.isEmpty()) {
        return {};
    }

    QStringList parts;
#ifdef Q_OS_WIN
    parts = text.split(';', Qt::SkipEmptyParts);
#else
    parts = text.split(':', Qt::SkipEmptyParts);
#endif
    if (parts.isEmpty()) {
        parts.append(text);
    }
    return parts;
}

QStringList runtimeSearchCandidates()
{
    QStringList candidates;

    appendCandidate(&candidates, appDirPath());

    const QStringList envNames = {
        QStringLiteral("ZNZ_HOME"),
        QStringLiteral("ZNZ_BLACKBOARD"),
        QStringLiteral("YIDA_HOME"),
        QStringLiteral("YIDA_BLACKBOARD"),
        QStringLiteral("IETA_HOME"),
        QStringLiteral("IETA_BLACKBOARD"),
    };
    for (const QString &envName : envNames) {
        const QByteArray value = qgetenv(envName.toLocal8Bit().constData());
        for (const QString &path : splitEnvironmentPath(value)) {
            appendCandidate(&candidates, path);
        }
    }

#ifdef Q_OS_WIN
    const QFileInfoList drives = QDir::drives();
    for (const QFileInfo &drive : drives) {
        appendCandidate(&candidates, QDir(drive.absoluteFilePath()).filePath(QStringLiteral("LLMs")));
    }
#else
    appendCandidate(&candidates, QStringLiteral("/home/ieta/LLMs"));
    appendCandidate(&candidates, QStringLiteral("/home/IETA/LLMs"));
#endif

    return candidates;
}

QString resolveRuntimeRoot(const LlmConfig &config, QString *source, QStringList *searchedLocations)
{
    if (source) {
        source->clear();
    }
    if (searchedLocations) {
        searchedLocations->clear();
    }

    const QString explicitRoot = config.llamaCppRuntimeRoot.trimmed();
    if (!explicitRoot.isEmpty()) {
        if (source) {
            *source = QStringLiteral("config.llamaCppRuntimeRoot");
        }
        return QFileInfo(explicitRoot).absoluteFilePath();
    }

    const QStringList candidates = runtimeSearchCandidates();
    for (const QString &candidate : candidates) {
        const QString runtimeRoot = runtimeChildIfPresent(candidate);
        const QString searched = runtimeRoot.isEmpty()
            ? QDir(candidate).filePath(QString::fromLatin1(kRuntimeDirName))
            : runtimeRoot;
        appendCandidate(searchedLocations, searched);
        if (!runtimeRoot.isEmpty()) {
            if (source) {
                *source = candidate == appDirPath()
                    ? QStringLiteral("applicationDirPath")
                    : QStringLiteral("runtimeSearchPath");
            }
            return runtimeRoot;
        }
    }

    return QString();
}

QStringList localModelFiles(const QString &modelsDir)
{
    return QDir(modelsDir).entryList(QStringList({QStringLiteral("*.gguf")}),
                                     QDir::Files,
                                     QDir::Name | QDir::IgnoreCase);
}

QString findConfiguredModelPath(const LlmConfig &config, const QString &modelsDir)
{
    const QString explicitPath = config.llamaCppModelPath.trimmed();
    if (!explicitPath.isEmpty()) {
        return QFileInfo(explicitPath).absoluteFilePath();
    }

    const QString modelName = config.model.trimmed();
    const QDir dir(modelsDir);
    const QStringList files = localModelFiles(modelsDir);
    if (!modelName.isEmpty()) {
        for (const QString &fileName : files) {
            const QFileInfo info(dir.absoluteFilePath(fileName));
            if (fileName.compare(modelName, Qt::CaseInsensitive) == 0
                || info.completeBaseName().compare(modelName, Qt::CaseInsensitive) == 0) {
                return info.absoluteFilePath();
            }
        }
        return dir.absoluteFilePath(modelName);
    }

    if (files.size() == 1) {
        return dir.absoluteFilePath(files.first());
    }
    return QString();
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
    layout.rootDir = resolveRuntimeRoot(config, &layout.discoverySource, nullptr);
    layout.runtimeRootFound = !layout.rootDir.trimmed().isEmpty() && QFileInfo(layout.rootDir).isDir();
    if (layout.rootDir.trimmed().isEmpty()) {
        layout.availabilityStatus = QStringLiteral("runtime_not_found");
        layout.availabilityMessage = QStringLiteral("llama.cpp runtime root not found");
        return layout;
    }

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
    layout.executableAvailable = QFileInfo(layout.executablePath).isFile();
    layout.modelCount = localModelFiles(layout.modelsDir).size();
    layout.resolvedModelPath = findConfiguredModelPath(config, layout.modelsDir);
    layout.modelAvailable = !layout.resolvedModelPath.trimmed().isEmpty()
        ? QFileInfo(layout.resolvedModelPath).isFile()
        : layout.modelCount > 0;
    layout.available = layout.runtimeRootFound && layout.executableAvailable && layout.modelAvailable;
    if (layout.available) {
        layout.availabilityStatus = QStringLiteral("available");
        layout.availabilityMessage = QStringLiteral("llama.cpp runtime is available");
    } else if (!layout.executableAvailable) {
        layout.availabilityStatus = QStringLiteral("executable_not_found");
        layout.availabilityMessage = QStringLiteral("llama.cpp server executable not found: %1").arg(layout.executablePath);
    } else {
        layout.availabilityStatus = QStringLiteral("model_not_found");
        layout.availabilityMessage = layout.modelCount > 0 && !config.model.trimmed().isEmpty()
            ? QStringLiteral("Configured GGUF model not found: %1").arg(config.model.trimmed())
            : QStringLiteral("GGUF model not found in %1").arg(layout.modelsDir);
    }
    return layout;
}

bool ManagedLlamaCppRuntime::updateRuntimeAvailability(LlmConfig *config,
                                                       LlamaCppRuntimeLayout *layout,
                                                       QString *errorMessage)
{
    if (!config) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Missing LlmConfig for llama.cpp runtime availability check");
        }
        return false;
    }

    QStringList searchedLocations;
    QString source;
    const QString rootDir = resolveRuntimeRoot(*config, &source, &searchedLocations);
    LlamaCppRuntimeLayout resolved = defaultLayout(*config);
    if (resolved.rootDir.trimmed().isEmpty()) {
        resolved.discoverySource = source;
        resolved.availabilityStatus = QStringLiteral("runtime_not_found");
        resolved.availabilityMessage =
            QStringLiteral("llama.cpp runtime root not found. Searched: %1")
                .arg(searchedLocations.isEmpty()
                         ? QStringLiteral("<none>")
                         : searchedLocations.join(QStringLiteral("; ")));
    }

    config->runtimeName = QStringLiteral("llama-cpp-managed");
    config->providerName = QStringLiteral("llama-cpp");
    config->llamaCppRuntimeRoot = rootDir;
    config->resolvedRuntimeRoot = resolved.rootDir;
    config->llamaCppExecutablePath = config->llamaCppExecutablePath.trimmed().isEmpty()
        ? resolved.executablePath
        : config->llamaCppExecutablePath.trimmed();
    config->localModelCount = resolved.modelCount;
    config->providerAvailable = resolved.available;
    config->providerAvailabilityStatus = resolved.availabilityStatus;
    config->providerAvailabilityMessage = resolved.availabilityMessage;

    if (config->llamaCppModelPath.trimmed().isEmpty()
        && !resolved.resolvedModelPath.trimmed().isEmpty()
        && QFileInfo(resolved.resolvedModelPath).isFile()) {
        config->llamaCppModelPath = resolved.resolvedModelPath;
    } else if (config->llamaCppModelPath.trimmed().isEmpty() && resolved.modelCount == 1) {
        const QStringList files = localModelFiles(resolved.modelsDir);
        if (!files.isEmpty()) {
            config->llamaCppModelPath = QDir(resolved.modelsDir).absoluteFilePath(files.first());
        }
    }
    config->resolvedModelPath = !resolved.resolvedModelPath.trimmed().isEmpty()
        ? resolved.resolvedModelPath
        : config->llamaCppModelPath.trimmed();

    if (layout) {
        *layout = resolved;
    }

    if (!resolved.available) {
        if (errorMessage) {
            *errorMessage = resolved.availabilityMessage;
        }

        QJsonArray searched;
        for (const QString &location : searchedLocations) {
            searched.append(location);
        }
        logging::QtLlmLogger::instance().error(
            QStringLiteral("llm.runtime"),
            QStringLiteral("Managed llama.cpp runtime unavailable"),
            logging::LogContext(),
            QJsonObject{{QStringLiteral("status"), resolved.availabilityStatus},
                        {QStringLiteral("message"), resolved.availabilityMessage},
                        {QStringLiteral("runtimeRoot"), resolved.rootDir},
                        {QStringLiteral("executablePath"), resolved.executablePath},
                        {QStringLiteral("modelCount"), resolved.modelCount},
                        {QStringLiteral("searched"), searched}});
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool ManagedLlamaCppRuntime::ensureDefaultLayout(const LlmConfig &config,
                                                 LlamaCppRuntimeLayout *layout,
                                                 QString *errorMessage)
{
    const LlamaCppRuntimeLayout resolved = defaultLayout(config);
    if (resolved.rootDir.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = resolved.availabilityMessage;
        }
        logging::QtLlmLogger::instance().error(
            QStringLiteral("llm.runtime"),
            QStringLiteral("Managed llama.cpp runtime root not found"),
            logging::LogContext(),
            QJsonObject{{QStringLiteral("status"), resolved.availabilityStatus},
                        {QStringLiteral("message"), resolved.availabilityMessage}});
        return false;
    }

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
    const QStringList files = localModelFiles(resolved.modelsDir);

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
    config->resolvedRuntimeRoot = layout.rootDir;
    config->resolvedModelPath = modelPath;
    config->localModelCount = layout.modelCount;
    config->providerAvailable = layout.available;
    config->providerAvailabilityStatus = layout.availabilityStatus;
    config->providerAvailabilityMessage = layout.availabilityMessage;
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
