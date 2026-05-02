#pragma once

#include "runtimeprofile.h"

#include <QList>

namespace qtllm::host {

class ModelCatalogService
{
public:
    QList<LocalModelInfo> listLocalModels(const RuntimeProfile &profile, QString *errorMessage = nullptr) const;
};

} // namespace qtllm::host
