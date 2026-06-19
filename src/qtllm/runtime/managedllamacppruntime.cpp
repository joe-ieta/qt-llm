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
constexpr int kAutoGpuLayerCount = 999;
constexpr int kDefaultStartupTimeoutMs = 180000;

QStringList gpuBackendNameFilters()
{
#ifdef Q_OS_WIN
    return {QStringLiteral("ggml-cuda*.dll"),
            QStringLiteral("ggml-vulkan*.dll"),
            QStringLiteral("ggml-hip*.dll"),
            QStringLiteral("ggml-kompute*.dll"),
            QStringLiteral("ggml-sycl*.dll"),
            QStringLiteral("ggml-metal*.dll")};
#elif defined(Q_OS_MACOS)
    return {QStringLiteral("libggml-metal*.dylib"),
            QStringLiteral("libggml-vulkan*.dylib"),
            QStringLiteral("libggml-cuda*.dylib"),
            QStringLiteral("libggml-hip*.dylib"),
            QStringLiteral("libggml-kompute*.dylib"),
            QStringLiteral("libggml-sycl*.dylib")};
#else
    return {QStringLiteral("libggml-cuda*.so"),
            QStringLiteral("libggml-vulkan*.so"),
            QStringLiteral("libggml-hip*.so"),
            QStringLiteral("libggml-kompute*.so"),
            QStringLiteral("libggml-sycl*.so")};
#endif
}

QStringList availableGpuBackends(const QString &binDir)
{
    const QDir dir(binDir);
    QStringList names;
    for (const QString &fileName : dir.entryList(gpuBackendNameFilters(), QDir::Files, QDir::Name)) {
        names.append(QFileInfo(fileName).completeBaseName());
    }
    names.removeDuplicates();
    return names;
}

QString appDirPath()
{
    return QCoreApplication::instance()
        ? QCoreApplication::applicationDirPath()
        : QDir::currentPath();
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

QStringList searchBaseCandidates()
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
        const QString resolved = QFileInfo(explicitRoot).absoluteFilePath();
        appendCandidate(searchedLocations, resolved);
        return resolved;
    }
    if (source) {
        *source = QStringLiteral("applicationDirPath");
    }
    const QString runtimeRoot = QDir(appDirPath()).filePath(QString::fromLatin1(kRuntimeDirName));
    appendCandidate(searchedLocations, runtimeRoot);
    return runtimeRoot;
}

QString bundledModelsDir(const QString &runtimeRoot)
{
    return QDir(runtimeRoot).filePath(QStringLiteral("models"));
}

QString supplementalModelsDirIfPresent(const QString &basePath)
{
    const QString trimmed = basePath.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QStringList candidates = {
        QFileInfo(trimmed).absoluteFilePath(),
        QDir(trimmed).filePath(QStringLiteral("qtllm/models")),
        QDir(trimmed).filePath(QStringLiteral("qtllm"))
    };

    for (const QString &candidate : candidates) {
        const QFileInfo candidateInfo(candidate);
        if (!candidateInfo.exists() || !candidateInfo.isDir()) {
            continue;
        }
        const QString absolutePath = candidateInfo.absoluteFilePath();
        const QString normalizedPath = QDir::fromNativeSeparators(absolutePath);
        if (normalizedPath.endsWith(QStringLiteral("/qtllm/models"), Qt::CaseInsensitive)) {
            return absolutePath;
        }
        if (normalizedPath.endsWith(QStringLiteral("/qtllm"), Qt::CaseInsensitive)) {
            const QString childModels = QDir(absolutePath).filePath(QStringLiteral("models"));
            if (QFileInfo(childModels).isDir()) {
                return QFileInfo(childModels).absoluteFilePath();
            }
        }
    }
    return {};
}

QStringList localModelFiles(const QString &modelsDir)
{
    return QDir(modelsDir).entryList(QStringList({QStringLiteral("*.gguf")}),
                                     QDir::Files,
                                     QDir::Name | QDir::IgnoreCase);
}

QList<QFileInfo> collectLocalModelFiles(const QStringList &modelDirs)
{
    QList<QFileInfo> files;
    QStringList seenPaths;
    for (const QString &modelsDir : modelDirs) {
        const QDir dir(modelsDir);
        for (const QString &fileName : localModelFiles(modelsDir)) {
            const QFileInfo info(dir.absoluteFilePath(fileName));
            const QString absolutePath = info.absoluteFilePath();
            if (seenPaths.contains(absolutePath, Qt::CaseInsensitive)) {
                continue;
            }
            seenPaths.append(absolutePath);
            files.append(info);
        }
    }
    return files;
}

QStringList resolveModelSearchDirs(const QString &runtimeRoot)
{
    QStringList dirs;
    appendCandidate(&dirs, bundledModelsDir(runtimeRoot));

    const QStringList candidates = searchBaseCandidates();
    for (const QString &candidate : candidates) {
        const QString supplementalDir = supplementalModelsDirIfPresent(candidate);
        if (!supplementalDir.isEmpty()) {
            appendCandidate(&dirs, supplementalDir);
        }
    }
    return dirs;
}

QString findConfiguredModelPath(const LlmConfig &config, const QStringList &modelDirs)
{
    const QString explicitPath = config.llamaCppModelPath.trimmed();
    if (!explicitPath.isEmpty()) {
        return QFileInfo(explicitPath).absoluteFilePath();
    }

    const QString modelName = config.model.trimmed();
    const QList<QFileInfo> files = collectLocalModelFiles(modelDirs);
    if (!modelName.isEmpty()) {
        for (const QFileInfo &info : files) {
            if (info.fileName().compare(modelName, Qt::CaseInsensitive) == 0
                || info.completeBaseName().compare(modelName, Qt::CaseInsensitive) == 0) {
                return info.absoluteFilePath();
            }
        }
        if (!modelDirs.isEmpty()) {
            return QDir(modelDirs.first()).absoluteFilePath(modelName);
        }
    }

    if (files.size() == 1) {
        return files.first().absoluteFilePath();
    }
    return {};
}

QString normalizedProvider(const QString &providerName)
{
    return providerName.trimmed().toLower();
}

bool containsGpuLayerArgument(const QStringList &args)
{
    for (const QString &arg : args) {
        const QString normalized = arg.trimmed().toLower();
        if (normalized == QStringLiteral("--n-gpu-layers")
            || normalized.startsWith(QStringLiteral("--n-gpu-layers="))
            || normalized == QStringLiteral("--gpu-layers")
            || normalized.startsWith(QStringLiteral("--gpu-layers="))
            || normalized == QStringLiteral("-ngl")) {
            return true;
        }
    }
    return false;
}

bool containsAnyArgument(const QStringList &args, const QStringList &names)
{
    for (const QString &arg : args) {
        const QString normalized = arg.trimmed().toLower();
        for (const QString &name : names) {
            const QString normalizedName = name.trimmed().toLower();
            if (normalized == normalizedName || normalized.startsWith(normalizedName + QStringLiteral("="))) {
                return true;
            }
        }
    }
    return false;
}

QString normalizedPolicy(QString value, const QString &fallback, const QStringList &allowed)
{
    value = value.trimmed().toLower();
    if (allowed.contains(value)) {
        return value;
    }
    return fallback;
}

QString modelSizeClass(qint64 sizeBytes)
{
    constexpr qint64 gib = 1024LL * 1024LL * 1024LL;
    if (sizeBytes <= 0) {
        return QStringLiteral("unknown");
    }
    if (sizeBytes < 5 * gib) {
        return QStringLiteral("small");
    }
    if (sizeBytes < 10 * gib) {
        return QStringLiteral("medium");
    }
    return QStringLiteral("large");
}

struct LlamaCppLaunchPlan
{
    QString gpuMode;
    QString performanceProfile;
    QString contextMode;
    QString modelSizeClass;
    QStringList warnings;
    int gpuLayers = 0;
    int threadCount = 0;
    int contextSize = 4096;
    qint64 modelSizeBytes = 0;
    bool gpuLayersFromExtraArgs = false;
    bool threadCountFromExtraArgs = false;
    bool contextSizeFromExtraArgs = false;
};

int autoContextSize(const QString &performanceProfile, const QString &sizeClass)
{
    if (performanceProfile == QStringLiteral("conservative")) {
        return 4096;
    }
    if (performanceProfile == QStringLiteral("aggressive")) {
        if (sizeClass == QStringLiteral("small")) {
            return 16384;
        }
        if (sizeClass == QStringLiteral("medium")) {
            return 8192;
        }
        return 4096;
    }
    if (sizeClass == QStringLiteral("small")) {
        return 8192;
    }
    return 4096;
}

int autoThreadCount(const QString &performanceProfile)
{
    const int idealThreads = QThread::idealThreadCount();
    if (idealThreads <= 0) {
        return 0;
    }
    if (performanceProfile == QStringLiteral("conservative")) {
        return qMax(1, idealThreads / 2);
    }
    if (performanceProfile == QStringLiteral("aggressive")) {
        return idealThreads;
    }
    return qMax(1, idealThreads - 1);
}

LlamaCppLaunchPlan buildLaunchPlan(const LlmConfig &config,
                                   const LlamaCppRuntimeLayout &layout,
                                   const QString &modelPath)
{
    LlamaCppLaunchPlan plan;
    plan.gpuMode = normalizedPolicy(config.llamaCppGpuMode,
                                    QStringLiteral("auto"),
                                    {QStringLiteral("auto"),
                                     QStringLiteral("cpu-only"),
                                     QStringLiteral("prefer-gpu"),
                                     QStringLiteral("explicit")});
    plan.performanceProfile = normalizedPolicy(config.llamaCppPerformanceProfile,
                                               QStringLiteral("balanced"),
                                               {QStringLiteral("conservative"),
                                                QStringLiteral("balanced"),
                                                QStringLiteral("aggressive")});
    plan.contextMode = normalizedPolicy(config.llamaCppContextMode,
                                        QStringLiteral("auto"),
                                        {QStringLiteral("auto"), QStringLiteral("explicit")});
    plan.modelSizeBytes = QFileInfo(modelPath).size();
    plan.modelSizeClass = modelSizeClass(plan.modelSizeBytes);

    plan.gpuLayersFromExtraArgs = containsGpuLayerArgument(config.llamaCppExtraArgs);
    plan.threadCountFromExtraArgs =
        containsAnyArgument(config.llamaCppExtraArgs, {QStringLiteral("--threads"), QStringLiteral("-t")});
    plan.contextSizeFromExtraArgs =
        containsAnyArgument(config.llamaCppExtraArgs, {QStringLiteral("--ctx-size"), QStringLiteral("-c")});

    if (plan.gpuLayersFromExtraArgs) {
        plan.gpuMode = QStringLiteral("extra-args");
        plan.gpuLayers = config.llamaCppGpuLayers;
    } else if (plan.gpuMode == QStringLiteral("cpu-only")) {
        plan.gpuLayers = 0;
    } else if (plan.gpuMode == QStringLiteral("explicit") || config.llamaCppGpuLayers >= 0) {
        plan.gpuMode = config.llamaCppGpuLayers == 0 ? QStringLiteral("cpu-only") : QStringLiteral("explicit");
        plan.gpuLayers = qMax(0, config.llamaCppGpuLayers);
    } else if (layout.gpuBackendAvailable) {
        plan.gpuLayers = kAutoGpuLayerCount;
    } else {
        plan.gpuLayers = 0;
        plan.warnings.append(QStringLiteral("No llama.cpp GPU backend library was found; CPU-only launch plan selected."));
    }

    if (plan.contextSizeFromExtraArgs) {
        plan.contextSize = config.llamaCppContextSize > 0 ? config.llamaCppContextSize : 4096;
    } else if (plan.contextMode == QStringLiteral("explicit")) {
        plan.contextSize = config.llamaCppContextSize > 0 ? config.llamaCppContextSize : 4096;
    } else {
        plan.contextSize = autoContextSize(plan.performanceProfile, plan.modelSizeClass);
    }

    if (plan.threadCountFromExtraArgs) {
        plan.threadCount = config.llamaCppThreadCount;
    } else if (config.llamaCppThreadCount > 0) {
        plan.threadCount = config.llamaCppThreadCount;
    } else {
        plan.threadCount = autoThreadCount(plan.performanceProfile);
    }

    return plan;
}

QString launchPlanSummary(const LlamaCppLaunchPlan &plan)
{
    return QStringLiteral("gpuMode=%1; gpuLayers=%2; threads=%3; ctxSize=%4; performance=%5; modelSize=%6")
        .arg(plan.gpuMode)
        .arg(plan.gpuLayers)
        .arg(plan.threadCount)
        .arg(plan.contextSize)
        .arg(plan.performanceProfile)
        .arg(plan.modelSizeClass);
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
    layout.bundledModelsDir = bundledModelsDir(layout.rootDir);
    layout.modelSearchDirs = resolveModelSearchDirs(layout.rootDir);
    layout.modelsDir = layout.bundledModelsDir;
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
    layout.gpuBackendNames = availableGpuBackends(layout.binDir);
    layout.gpuBackendAvailable = !layout.gpuBackendNames.isEmpty();
    const QList<QFileInfo> models = collectLocalModelFiles(layout.modelSearchDirs);
    layout.modelCount = models.size();
    layout.resolvedModelPath = findConfiguredModelPath(config, layout.modelSearchDirs);
    layout.modelAvailable = !layout.resolvedModelPath.trimmed().isEmpty()
        ? QFileInfo(layout.resolvedModelPath).isFile()
        : layout.modelCount > 0;
    layout.available = layout.runtimeRootFound && layout.executableAvailable && layout.modelAvailable;
    if (layout.available) {
        layout.availabilityStatus = QStringLiteral("available");
        layout.availabilityMessage = layout.gpuBackendAvailable
            ? QStringLiteral("llama.cpp runtime is available with GPU backend: %1").arg(layout.gpuBackendNames.join(QStringLiteral(", ")))
            : QStringLiteral("llama.cpp runtime is available but no GPU backend library was found; it will run CPU-only");
    } else if (!layout.executableAvailable) {
        layout.availabilityStatus = QStringLiteral("executable_not_found");
        layout.availabilityMessage = QStringLiteral("llama.cpp server executable not found: %1").arg(layout.executablePath);
    } else {
        layout.availabilityStatus = QStringLiteral("model_not_found");
        layout.availabilityMessage = layout.modelCount > 0 && !config.model.trimmed().isEmpty()
            ? QStringLiteral("Configured GGUF model not found: %1").arg(config.model.trimmed())
            : QStringLiteral("GGUF model not found in bundled or supplemental model search directories.");
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
        const QList<QFileInfo> files = collectLocalModelFiles(resolved.modelSearchDirs);
        if (!files.isEmpty()) {
            config->llamaCppModelPath = files.first().absoluteFilePath();
        }
    }
    config->resolvedModelPath = !resolved.resolvedModelPath.trimmed().isEmpty()
        ? resolved.resolvedModelPath
        : config->llamaCppModelPath.trimmed();

    if (!config->resolvedModelPath.trimmed().isEmpty()
        && QFileInfo(config->resolvedModelPath).isFile()) {
        const LlamaCppLaunchPlan launchPlan = buildLaunchPlan(*config, resolved, config->resolvedModelPath);
        config->resolvedLlamaCppGpuMode = launchPlan.gpuMode;
        config->resolvedLlamaCppGpuLayers = launchPlan.gpuLayers;
        config->resolvedLlamaCppThreadCount = launchPlan.threadCount;
        config->resolvedLlamaCppContextSize = launchPlan.contextSize;
        config->runtimePlanSummary = launchPlanSummary(launchPlan);
        config->runtimePlanWarnings = launchPlan.warnings;
    } else {
        config->resolvedLlamaCppGpuMode.clear();
        config->resolvedLlamaCppGpuLayers = -1;
        config->resolvedLlamaCppThreadCount = 0;
        config->resolvedLlamaCppContextSize = 0;
        config->runtimePlanSummary.clear();
        config->runtimePlanWarnings.clear();
    }

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
                        {QStringLiteral("gpuBackendAvailable"), resolved.gpuBackendAvailable},
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

    const QStringList dirs = {resolved.rootDir, resolved.binDir, resolved.bundledModelsDir, resolved.logsDir};
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

    const QList<QFileInfo> files = collectLocalModelFiles(resolved.modelSearchDirs);

    QList<LlamaCppLocalModel> models;
    models.reserve(files.size());
    QHash<QString, int> displayNameCounts;
    for (const QFileInfo &info : files) {
        const QString baseName = info.completeBaseName().isEmpty() ? info.fileName() : info.completeBaseName();
        displayNameCounts.insert(baseName, displayNameCounts.value(baseName) + 1);
    }
    for (const QFileInfo &info : files) {
        const QString filePath = info.absoluteFilePath();
        const QString modelId = info.completeBaseName().isEmpty() ? info.fileName() : info.completeBaseName();
        QString displayName = modelId;
        if (displayNameCounts.value(displayName) > 1) {
            displayName = QStringLiteral("%1 [%2]").arg(displayName, info.absolutePath());
        }
        models.push_back(LlamaCppLocalModel{
            modelId,
            filePath,
            displayName
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

    const LlamaCppLaunchPlan launchPlan = buildLaunchPlan(*config, layout, modelPath);
    config->resolvedLlamaCppGpuMode = launchPlan.gpuMode;
    config->resolvedLlamaCppGpuLayers = launchPlan.gpuLayers;
    config->resolvedLlamaCppThreadCount = launchPlan.threadCount;
    config->resolvedLlamaCppContextSize = launchPlan.contextSize;
    config->runtimePlanSummary = launchPlanSummary(launchPlan);
    config->runtimePlanWarnings = launchPlan.warnings;

    if (m_process->state() != QProcess::NotRunning
        && m_activeExecutablePath == executablePath
        && m_activeModelPath == modelPath
        && m_activePort == port) {
        const int timeoutMs = config->llamaCppStartupTimeoutMs > 0
            ? config->llamaCppStartupTimeoutMs
            : kDefaultStartupTimeoutMs;
        return waitForServerReady(port, timeoutMs, errorMessage);
    }

    stop();
    if (isPortOpen(port)) {
        const int timeoutMs = config->llamaCppStartupTimeoutMs > 0
            ? config->llamaCppStartupTimeoutMs
            : kDefaultStartupTimeoutMs;
        if (!waitForServerReady(port, timeoutMs, errorMessage)) {
            logging::QtLlmLogger::instance().warn(
                QStringLiteral("llm.runtime"),
                QStringLiteral("Existing llama.cpp server on configured port is not ready"),
                logging::LogContext(),
                QJsonObject{{QStringLiteral("baseUrl"), defaultBaseUrl(port)},
                            {QStringLiteral("runtimeRoot"), layout.rootDir},
                            {QStringLiteral("modelPath"), modelPath},
                            {QStringLiteral("port"), port},
                            {QStringLiteral("error"), errorMessage ? *errorMessage : QString()}});
            return false;
        }

        logging::QtLlmLogger::instance().info(
            QStringLiteral("llm.runtime"),
            QStringLiteral("Reusing existing llama.cpp server on configured port"),
            logging::LogContext(),
            QJsonObject{{QStringLiteral("baseUrl"), defaultBaseUrl(port)},
                        {QStringLiteral("runtimeRoot"), layout.rootDir},
                        {QStringLiteral("modelPath"), modelPath},
                        {QStringLiteral("port"), port},
                        {QStringLiteral("runtimePlan"), config->runtimePlanSummary},
                        {QStringLiteral("runtimePlanWarnings"), QJsonArray::fromStringList(config->runtimePlanWarnings)},
                        {QStringLiteral("ownedByThisRuntime"), false}});
        if (errorMessage) {
            errorMessage->clear();
        }
        return true;
    }

    QStringList args;
    args << QStringLiteral("--host") << QStringLiteral("127.0.0.1")
         << QStringLiteral("--port") << QString::number(port)
         << QStringLiteral("-m") << modelPath;
    if (!launchPlan.contextSizeFromExtraArgs) {
        args << QStringLiteral("--ctx-size") << QString::number(launchPlan.contextSize);
    }
    if (!launchPlan.gpuLayersFromExtraArgs) {
        args << QStringLiteral("--gpu-layers") << QString::number(launchPlan.gpuLayers);
    }
    if (!launchPlan.threadCountFromExtraArgs && launchPlan.threadCount > 0) {
        args << QStringLiteral("--threads") << QString::number(launchPlan.threadCount);
    }
    args.append(config->llamaCppExtraArgs);

    logging::QtLlmLogger::instance().info(
        QStringLiteral("llm.runtime"),
        QStringLiteral("Starting managed llama.cpp server"),
        logging::LogContext(),
        QJsonObject{{QStringLiteral("executablePath"), executablePath},
                    {QStringLiteral("runtimeRoot"), layout.rootDir},
                    {QStringLiteral("modelPath"), modelPath},
                    {QStringLiteral("port"), port},
                    {QStringLiteral("arguments"), QJsonArray::fromStringList(args)},
                    {QStringLiteral("gpuBackendAvailable"), layout.gpuBackendAvailable},
                    {QStringLiteral("gpuBackends"), QJsonArray::fromStringList(layout.gpuBackendNames)},
                    {QStringLiteral("runtimePlan"), config->runtimePlanSummary},
                    {QStringLiteral("runtimePlanWarnings"), QJsonArray::fromStringList(config->runtimePlanWarnings)},
                    {QStringLiteral("resolvedGpuMode"), config->resolvedLlamaCppGpuMode},
                    {QStringLiteral("resolvedGpuLayers"), config->resolvedLlamaCppGpuLayers},
                    {QStringLiteral("resolvedThreadCount"), config->resolvedLlamaCppThreadCount},
                    {QStringLiteral("resolvedContextSize"), config->resolvedLlamaCppContextSize}});

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

    const int timeoutMs = config->llamaCppStartupTimeoutMs > 0
        ? config->llamaCppStartupTimeoutMs
        : kDefaultStartupTimeoutMs;
    if (!waitForServerReady(port, timeoutMs, errorMessage)) {
        const QByteArray output = m_process->readAll();
        stop();
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 %2")
                                .arg(*errorMessage, QString::fromUtf8(output.left(2048)));
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

bool ManagedLlamaCppRuntime::waitForServerReady(int port, int timeoutMs, QString *errorMessage) const
{
    QElapsedTimer timer;
    timer.start();
    QString lastError;
    while (timer.elapsed() < timeoutMs) {
        int statusCode = 0;
        if (probeServerReady(port, QStringLiteral("/health"), &statusCode, &lastError)) {
            if (errorMessage) {
                errorMessage->clear();
            }
            return true;
        }

        if (statusCode == 404 || statusCode == 405) {
            if (probeServerReady(port, QStringLiteral("/v1/models"), &statusCode, &lastError)) {
                if (errorMessage) {
                    errorMessage->clear();
                }
                return true;
            }
        }

        QThread::msleep(250);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("llama.cpp server did not become HTTP-ready on port %1 within %2 ms. Last readiness probe: %3")
                            .arg(port)
                            .arg(timeoutMs)
                            .arg(lastError.isEmpty() ? QStringLiteral("<no response>") : lastError);
    }
    return false;
}

bool ManagedLlamaCppRuntime::probeServerReady(int port,
                                              const QString &path,
                                              int *statusCode,
                                              QString *errorMessage) const
{
    if (statusCode) {
        *statusCode = 0;
    }

    QTcpSocket socket;
    socket.connectToHost(QStringLiteral("127.0.0.1"), static_cast<quint16>(port));
    if (!socket.waitForConnected(250)) {
        if (errorMessage) {
            *errorMessage = socket.errorString();
        }
        return false;
    }

    const QByteArray request = QByteArrayLiteral("GET ") + path.toUtf8()
        + QByteArrayLiteral(" HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
    socket.write(request);
    if (!socket.waitForBytesWritten(250)) {
        if (errorMessage) {
            *errorMessage = socket.errorString();
        }
        socket.disconnectFromHost();
        return false;
    }

    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    QByteArray response;
    QElapsedTimer readTimer;
    readTimer.start();
    while (readTimer.elapsed() < 1000) {
        if (socket.waitForReadyRead(50)) {
            response.append(socket.readAll());
            if (response.contains("\r\n")) {
                break;
            }
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    socket.disconnectFromHost();

    const int lineEnd = response.indexOf('\n');
    const QByteArray statusLine = (lineEnd >= 0 ? response.left(lineEnd) : response).trimmed();
    const QList<QByteArray> parts = statusLine.split(' ');
    bool ok = false;
    const int code = parts.size() >= 2 ? parts.at(1).toInt(&ok) : 0;
    if (!ok) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid HTTP readiness response from %1: %2")
                                .arg(path, QString::fromUtf8(statusLine.left(160)));
        }
        return false;
    }

    if (statusCode) {
        *statusCode = code;
    }
    if (code >= 200 && code < 300) {
        if (errorMessage) {
            errorMessage->clear();
        }
        return true;
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("HTTP %1 from %2").arg(code).arg(path);
    }
    return false;
}

bool ManagedLlamaCppRuntime::isPortOpen(int port) const
{
    if (port <= 0) {
        return false;
    }

    QTcpSocket socket;
    socket.connectToHost(QStringLiteral("127.0.0.1"), static_cast<quint16>(port));
    const bool connected = socket.waitForConnected(250);
    if (connected) {
        socket.disconnectFromHost();
    }
    return connected;
}

QString ManagedLlamaCppRuntime::resolveModelPath(const LlmConfig &config,
                                                 const LlamaCppRuntimeLayout &layout) const
{
    if (!config.llamaCppModelPath.trimmed().isEmpty()) {
        return QFileInfo(config.llamaCppModelPath.trimmed()).absoluteFilePath();
    }
    return findConfiguredModelPath(config, layout.modelSearchDirs);
}

} // namespace qtllm::runtime
