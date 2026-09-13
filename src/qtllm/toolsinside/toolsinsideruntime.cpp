#include "toolsinsideruntime.h"

#include "toolsinsidetracerecorder.h"
#include "../events/llmeventdispatcher.h"

#include <QDir>

namespace qtllm::toolsinside {

ToolsInsideRuntime &ToolsInsideRuntime::instance()
{
    static ToolsInsideRuntime runtime;
    return runtime;
}

ToolsInsideRuntime::ToolsInsideRuntime()
    : m_workspaceRoot(QDir::currentPath())
    , m_redactionPolicy(std::make_shared<NoOpToolsInsideRedactionPolicy>())
{
    rebuildPaths();
}

void ToolsInsideRuntime::configureWorkspaceRoot(const QString &workspaceRoot)
{
    m_workspaceRoot = workspaceRoot.trimmed().isEmpty() ? QDir::currentPath() : workspaceRoot.trimmed();
    rebuildPaths();
}

QString ToolsInsideRuntime::workspaceRoot() const
{
    return m_workspaceRoot;
}

void ToolsInsideRuntime::setStoragePolicy(const ToolsInsideStoragePolicy &policy)
{
    m_storagePolicy = policy;
    rebuildPaths();
}

ToolsInsideStoragePolicy ToolsInsideRuntime::storagePolicy() const
{
    return m_storagePolicy;
}

void ToolsInsideRuntime::setRedactionPolicy(std::shared_ptr<IToolsInsideRedactionPolicy> redactionPolicy)
{
    m_redactionPolicy = redactionPolicy ? std::move(redactionPolicy) : std::make_shared<NoOpToolsInsideRedactionPolicy>();
    rebuildPaths();
}

const IToolsInsideRedactionPolicy &ToolsInsideRuntime::redactionPolicy() const
{
    return *m_redactionPolicy;
}

std::shared_ptr<ToolsInsideRepository> ToolsInsideRuntime::repository() const
{
    return m_repository;
}

std::shared_ptr<ToolsInsideArtifactStore> ToolsInsideRuntime::artifactStore() const
{
    return m_artifactStore;
}

std::shared_ptr<ToolsInsideQueryService> ToolsInsideRuntime::queryService() const
{
    return m_queryService;
}

std::shared_ptr<ToolsInsideAdminService> ToolsInsideRuntime::adminService() const
{
    return m_adminService;
}

std::shared_ptr<ToolsInsideTraceRecorder> ToolsInsideRuntime::recorder() const
{
    return m_recorder;
}

void ToolsInsideRuntime::rebuildPaths()
{
    if (m_recorder) {
        events::LlmEventDispatcher::instance().removeSink(m_recorder);
    }
    const QString root = QDir(m_workspaceRoot).filePath(QStringLiteral(".qtllm/tools_inside"));
    m_repository = std::make_shared<ToolsInsideRepository>(QDir(root).filePath(QStringLiteral("index.db")));
    m_artifactStore = std::make_shared<ToolsInsideArtifactStore>(QDir(root).filePath(QStringLiteral("artifacts")));
    m_queryService = std::make_shared<ToolsInsideQueryService>(m_repository);
    m_adminService = std::make_shared<ToolsInsideAdminService>(root, m_repository, m_artifactStore);
    m_recorder = std::make_shared<ToolsInsideTraceRecorder>(m_repository, m_artifactStore, m_storagePolicy, m_redactionPolicy);
    events::LlmEventDispatcher::instance().addSink(m_recorder);
    m_repository->ensureInitialized(nullptr);
}

} // namespace qtllm::toolsinside
