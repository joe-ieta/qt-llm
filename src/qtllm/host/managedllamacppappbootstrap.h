#pragma once

#include <QString>

class QObject;

namespace qtllm::runtime {
class ManagedLlamaCppRuntime;
}

namespace qtllm::host {

runtime::ManagedLlamaCppRuntime *startManagedLlamaCppRuntimeForApp(QObject *parent,
                                                                   QString *statusMessage = nullptr);

} // namespace qtllm::host
