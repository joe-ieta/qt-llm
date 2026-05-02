TEMPLATE = lib
TARGET = qtllm
CONFIG += c++17 warn_on staticlib create_prl
QT += core network sql

msvc:QMAKE_CXXFLAGS += /utf-8

DEFINES += QTLLM_LIBRARY

# Keep library output predictable across platforms and shadow builds.
QTLLM_LIB_DIR = $$OUT_PWD/lib
CONFIG(debug, debug|release): QTLLM_LIB_DIR = $$OUT_PWD/lib/debug
CONFIG(release, debug|release): QTLLM_LIB_DIR = $$OUT_PWD/lib/release
DESTDIR = $$QTLLM_LIB_DIR

INCLUDEPATH += \
    $$PWD \
    $$PWD/identity \
    $$PWD/core \
    $$PWD/providers \
    $$PWD/runtime \
    $$PWD/network \
    $$PWD/streaming \
    $$PWD/chat \
    $$PWD/storage \
    $$PWD/profile \
    $$PWD/tools \
    $$PWD/tools/runtime \
    $$PWD/tools/protocol \
    $$PWD/tools/mcp \
    $$PWD/logging \
    $$PWD/toolsinside \
    $$PWD/toolsstudio

HEADERS += \
    identity/compactid.h \
    core/llmconfig.h \
    core/llmtypes.h \
    core/qtllmclient.h \
    providers/illmprovider.h \
    providers/llamacppprovider.h \
    providers/openaiprovider.h \
    providers/openaicompatibleprovider.h \
    providers/ollamaprovider.h \
    providers/providerfactory.h \
    providers/vllmprovider.h \
    runtime/managedllamacppruntime.h \
    network/httpexecutor.h \
    streaming/streamchunkparser.h \
    chat/conversationsnapshot.h \
    chat/conversationclient.h \
    chat/conversationclientfactory.h \
    storage/conversationrepository.h \
    profile/clientprofile.h \
    profile/memorypolicy.h \
    tools/llmtooldefinition.h \
    tools/builtintools.h \
    tools/llmtoolregistry.h \
    tools/llmtooladapter.h \
    tools/toolselectionlayer.h \
    tools/toolenabledchatentry.h \
    tools/runtime/toolruntime_types.h \
    tools/runtime/itoolexecutor.h \
    tools/runtime/builtinexecutors.h \
    tools/runtime/toolexecutorregistry.h \
    tools/runtime/toolruntimehooks.h \
    tools/runtime/toolexecutionlayer.h \
    tools/runtime/toolcatalogrepository.h \
    tools/runtime/clienttoolpolicyrepository.h \
    tools/runtime/toolcallorchestrator.h \
    tools/protocol/itoolcallprotocoladapter.h \
    tools/protocol/providerprotocoladapters.h \
    tools/protocol/toolcallprotocolrouter.h \
    tools/mcp/mcp_types.h \
    tools/mcp/mcpserverregistry.h \
    tools/mcp/mcpserverrepository.h \
    tools/mcp/imcpclient.h \
    tools/mcp/defaultmcpclient.h \
    tools/mcp/mcptoolsyncservice.h \
    tools/mcp/mcpservermanager.h \
    toolsstudio/toolsstudio_types.h \
    toolsstudio/toolcatalogservice.h \
    toolsstudio/toolmetadataoverriderepository.h \
    toolsstudio/toolimportexportservice.h \
    toolsstudio/toolmergeservice.h \
    toolsstudio/toolworkspacerepository.h \
    toolsstudio/toolworkspaceservice.h \
    toolsstudio/toolstudiosettings.h \
    toolsinside/toolsinside_types.h \
    toolsinside/toolsinsideartifactstore.h \
    toolsinside/toolsinsiderepository.h \
    toolsinside/toolsinsidequeryservice.h \
    toolsinside/toolsinsideadminservice.h \
    toolsinside/toolsinsidetracerecorder.h \
    toolsinside/toolsinsideruntime.h \
    logging/logtypes.h \
    logging/ilogsink.h \
    logging/filelogsink.h \
    logging/signallogsink.h \
    logging/qtllmlogger.h

SOURCES += \
    identity/compactid.cpp \
    core/qtllmclient.cpp \
    providers/llamacppprovider.cpp \
    providers/openaiprovider.cpp \
    providers/openaicompatibleprovider.cpp \
    providers/ollamaprovider.cpp \
    providers/providerfactory.cpp \
    providers/vllmprovider.cpp \
    runtime/managedllamacppruntime.cpp \
    network/httpexecutor.cpp \
    streaming/streamchunkparser.cpp \
    chat/conversationclient.cpp \
    chat/conversationclientfactory.cpp \
    storage/conversationrepository.cpp \
    tools/llmtoolregistry.cpp \
    tools/builtintools.cpp \
    tools/llmtooladapter.cpp \
    tools/toolselectionlayer.cpp \
    tools/toolenabledchatentry.cpp \
    tools/runtime/toolexecutorregistry.cpp \
    tools/runtime/builtinexecutors.cpp \
    tools/runtime/toolexecutionlayer.cpp \
    tools/runtime/toolcatalogrepository.cpp \
    tools/runtime/clienttoolpolicyrepository.cpp \
    tools/runtime/toolcallorchestrator.cpp \
    tools/protocol/providerprotocoladapters.cpp \
    tools/protocol/toolcallprotocolrouter.cpp \
    tools/mcp/mcpserverregistry.cpp \
    tools/mcp/mcpserverrepository.cpp \
    tools/mcp/defaultmcpclient.cpp \
    tools/mcp/mcptoolsyncservice.cpp \
    tools/mcp/mcpservermanager.cpp \
    toolsstudio/toolcatalogservice.cpp \
    toolsstudio/toolmetadataoverriderepository.cpp \
    toolsstudio/toolimportexportservice.cpp \
    toolsstudio/toolmergeservice.cpp \
    toolsstudio/toolworkspacerepository.cpp \
    toolsstudio/toolworkspaceservice.cpp \
    toolsstudio/toolstudiosettings.cpp \
    toolsinside/toolsinsideartifactstore.cpp \
    toolsinside/toolsinsiderepository.cpp \
    toolsinside/toolsinsidequeryservice.cpp \
    toolsinside/toolsinsideadminservice.cpp \
    toolsinside/toolsinsidetracerecorder.cpp \
    toolsinside/toolsinsideruntime.cpp \
    logging/logtypes.cpp \
    logging/filelogsink.cpp \
    logging/signallogsink.cpp \
    logging/qtllmlogger.cpp
