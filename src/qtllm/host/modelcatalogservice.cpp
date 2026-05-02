#include "modelcatalogservice.h"

#include "runtimeprofilemapper.h"
#include "../runtime/managedllamacppruntime.h"

#include <QFileInfo>

namespace qtllm::host {

QList<LocalModelInfo> ModelCatalogService::listLocalModels(const RuntimeProfile &profile, QString *errorMessage) const
{
    const LlmConfig config = RuntimeProfileMapper::toConfig(profile);

    runtime::LlamaCppRuntimeLayout layout;
    const QList<runtime::LlamaCppLocalModel> localModels =
        runtime::ManagedLlamaCppRuntime::listLocalModels(config, &layout, errorMessage);

    QList<LocalModelInfo> result;
    result.reserve(localModels.size());
    for (const runtime::LlamaCppLocalModel &model : localModels) {
        const QFileInfo fileInfo(model.filePath);
        LocalModelInfo info;
        info.id = model.id;
        info.displayName = model.displayName;
        info.filePath = model.filePath;
        info.providerName = QStringLiteral("llama-cpp");
        info.runtimeRoot = layout.rootDir;
        info.sizeBytes = fileInfo.exists() ? fileInfo.size() : 0;
        result.push_back(info);
    }
    return result;
}

} // namespace qtllm::host
