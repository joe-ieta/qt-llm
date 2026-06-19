#include <QtTest>
#include <QCoreApplication>

#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QHostAddress>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTextStream>
#include <QTcpSocket>
#include <QTimer>

#include "tst_qtllm.h"

#include "../../src/qtllm/chat/conversationclient.h"
#include "../../src/qtllm/chat/conversationclientfactory.h"
#include "../../src/qtllm/core/llmconfig.h"
#include "../../src/qtllm/core/llmtypes.h"
#include "../../src/qtllm/identity/compactid.h"
#include "../../src/qtllm/logging/filelogsink.h"
#include "../../src/qtllm/logging/logtypes.h"
#include "../../src/qtllm/providers/illmprovider.h"
#include "../../src/qtllm/providers/openaicompatibleprovider.h"
#include "../../src/qtllm/providers/openaiprovider.h"
#include "../../src/qtllm/providers/providerfactory.h"
#include "../../src/qtllm/runtime/managedllamacppruntime.h"
#include "../../src/qtllm/storage/conversationrepository.h"
#include "../../src/qtllm/streaming/streamchunkparser.h"
#include "../../src/qtllm/tools/llmtoolregistry.h"
#include "../../src/qtllm/tools/mcp/imcpclient.h"
#include "../../src/qtllm/tools/mcp/mcpserverregistry.h"
#include "../../src/qtllm/tools/mcp/mcptoolsyncservice.h"
#include "../../src/qtllm/tools/runtime/toolexecutionlayer.h"
#include "../../src/qtllm/toolsstudio/toolimportexportservice.h"
#include "../../src/qtllm/toolsstudio/toolworkspaceservice.h"

using namespace qtllm;

namespace {

class FakeMcpClient final : public qtllm::tools::mcp::IMcpClient
{
public:
    QVector<qtllm::tools::mcp::McpToolDescriptor> listedTools;
    qtllm::tools::mcp::McpToolCallResult callResult;
    QString lastToolName;
    QJsonObject lastArguments;
    QString lastServerId;

    QVector<qtllm::tools::mcp::McpToolDescriptor> listTools(const qtllm::tools::mcp::McpServerDefinition &server,
                                                            QString *errorMessage = nullptr) override
    {
        Q_UNUSED(errorMessage)
        lastServerId = server.serverId;
        return listedTools;
    }

    QVector<qtllm::tools::mcp::McpResourceDescriptor> listResources(const qtllm::tools::mcp::McpServerDefinition &server,
                                                                    QString *errorMessage = nullptr) override
    {
        Q_UNUSED(server)
        Q_UNUSED(errorMessage)
        return {};
    }

    QVector<qtllm::tools::mcp::McpPromptDescriptor> listPrompts(const qtllm::tools::mcp::McpServerDefinition &server,
                                                                QString *errorMessage = nullptr) override
    {
        Q_UNUSED(server)
        Q_UNUSED(errorMessage)
        return {};
    }

    qtllm::tools::mcp::McpToolCallResult callTool(const qtllm::tools::mcp::McpServerDefinition &server,
                                                  const QString &toolName,
                                                  const QJsonObject &arguments,
                                                  const qtllm::tools::runtime::ToolExecutionContext &context,
                                                  QString *errorMessage = nullptr) override
    {
        Q_UNUSED(context)
        Q_UNUSED(errorMessage)
        lastServerId = server.serverId;
        lastToolName = toolName;
        lastArguments = arguments;
        return callResult;
    }
};

QJsonObject makeSimpleSchema()
{
    return QJsonObject{{QStringLiteral("$schema"), QStringLiteral("http://json-schema.org/draft-07/schema#")},
                       {QStringLiteral("type"), QStringLiteral("object")},
                       {QStringLiteral("additionalProperties"), false},
                       {QStringLiteral("properties"),
                        QJsonObject{{QStringLiteral("path"),
                                     QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                                 {QStringLiteral("default"), QStringLiteral(".")}}}}}};
}

QByteArray makeHttpResponse(const QJsonObject &body)
{
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QByteArray response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += payload;
    return response;
}

}

void QtLlmCoreTests::compactIdGeneratesExpectedPrefixes()
{
    QCOMPARE(identity::prefixForKind(identity::IdKind::Client), QStringLiteral("cli"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::Session), QStringLiteral("ses"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::Trace), QStringLiteral("trc"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::Request), QStringLiteral("req"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::Span), QStringLiteral("spn"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::Event), QStringLiteral("evt"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::ToolCall), QStringLiteral("tcl"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::Artifact), QStringLiteral("art"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::SupportLink), QStringLiteral("lnk"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::Workspace), QStringLiteral("wsp"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::Node), QStringLiteral("nod"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::Placement), QStringLiteral("plc"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::Package), QStringLiteral("pkg"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::Task), QStringLiteral("tsk"));
    QCOMPARE(identity::prefixForKind(identity::IdKind::Queue), QStringLiteral("que"));

    const QString traceId = identity::generateId(identity::IdKind::Trace);
    QVERIFY(identity::isValidId(traceId));
    QVERIFY(identity::hasIdPrefix(traceId, QStringLiteral("trc")));

    const QString customId = identity::generateIdWithPrefix(QStringLiteral("abc"));
    QVERIFY(identity::isValidId(customId));
    QVERIFY(identity::hasIdPrefix(customId, QStringLiteral("abc")));
}

void QtLlmCoreTests::compactIdOrdersMonotonicallyWithinProcess()
{
    const QString first = identity::generateId(identity::IdKind::Event);
    const QString second = identity::generateId(identity::IdKind::Event);
    const QString third = identity::generateId(identity::IdKind::Event);

    QVERIFY(first < second);
    QVERIFY(second < third);

    bool firstOk = false;
    bool secondOk = false;
    bool thirdOk = false;
    const quint64 firstOrder = identity::decodeIdOrder(first, &firstOk);
    const quint64 secondOrder = identity::decodeIdOrder(second, &secondOk);
    const quint64 thirdOrder = identity::decodeIdOrder(third, &thirdOk);
    QVERIFY(firstOk);
    QVERIFY(secondOk);
    QVERIFY(thirdOk);
    QVERIFY(firstOrder < secondOrder);
    QVERIFY(secondOrder < thirdOrder);
}

void QtLlmCoreTests::compactIdValidatesAndDecodes()
{
    const quint64 orderValue = 0x123456789ABCDULL;
    const QString composed = identity::composeId(QStringLiteral("req"), orderValue);
    QCOMPARE(composed, QStringLiteral("req_000938nkrkayd"));
    QVERIFY(identity::isValidId(composed));
    QVERIFY(identity::hasIdPrefix(composed, QStringLiteral("req")));

    bool ok = false;
    QCOMPARE(identity::decodeIdOrder(composed, &ok), orderValue);
    QVERIFY(ok);

    const QString uppercase = QStringLiteral("req_000938NKRKAYD");
    QCOMPARE(identity::decodeIdOrder(uppercase, &ok), orderValue);
    QVERIFY(ok);

    const QString ambiguousLetters = QStringLiteral("req_ooooooooooooi");
    QCOMPARE(identity::decodeIdOrder(ambiguousLetters, &ok), 1ULL);
    QVERIFY(ok);
}

void QtLlmCoreTests::compactIdRejectsMalformedValues()
{
    bool ok = true;
    QVERIFY(identity::composeId(QStringLiteral("Bad"), 1).isEmpty());
    QVERIFY(identity::generateIdWithPrefix(QStringLiteral("bad-prefix")).isEmpty());
    QVERIFY(!identity::isValidId(QStringLiteral("")));
    QVERIFY(!identity::isValidId(QStringLiteral("req")));
    QVERIFY(!identity::isValidId(QStringLiteral("REQ_0123456789ABC")));
    QVERIFY(!identity::isValidId(QStringLiteral("req_0123456789AB")));
    QVERIFY(!identity::isValidId(QStringLiteral("req_0123456789ABC!")));
    QVERIFY(!identity::hasIdPrefix(QStringLiteral("req_0123456789ABU"), QStringLiteral("req")));
    QCOMPARE(identity::decodeIdOrder(QStringLiteral("req_0123456789ABC!"), &ok), 0ULL);
    QVERIFY(!ok);
}

void QtLlmCoreTests::conversationClientFactoryGeneratesCompactClientIds()
{
    chat::ConversationClientFactory factory;

    const QSharedPointer<chat::ConversationClient> client = factory.acquire();
    QVERIFY(client);
    QVERIFY(identity::isValidId(client->uid()));
    QVERIFY(identity::hasIdPrefix(client->uid(), QStringLiteral("cli")));
    QCOMPARE(factory.find(client->uid()), client);
}

void QtLlmCoreTests::conversationClientGeneratesCompactSessionIds()
{
    chat::ConversationClient client(QStringLiteral("cli_testclient0001"));

    QVERIFY(identity::isValidId(client.activeSessionId()));
    QVERIFY(identity::hasIdPrefix(client.activeSessionId(), QStringLiteral("ses")));

    const QString newSessionId = client.createSession(QStringLiteral("Follow-up"));
    QVERIFY(identity::isValidId(newSessionId));
    QVERIFY(identity::hasIdPrefix(newSessionId, QStringLiteral("ses")));
    QCOMPARE(client.activeSessionId(), newSessionId);
    QVERIFY(client.sessionIds().contains(newSessionId));
}

void QtLlmCoreTests::conversationRepositoryPersistsCompactConversationIds()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    auto repository = std::make_shared<storage::ConversationRepository>(tempDir.path());
    chat::ConversationClientFactory factory;
    factory.setRepository(repository);

    const QSharedPointer<chat::ConversationClient> client = factory.acquire();
    QVERIFY(client);
    const QString sessionId = client->activeSessionId();

    QString errorMessage;
    QVERIFY(factory.saveClient(client->uid(), &errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

    const QStringList persistedIds = repository->listClientIds();
    QVERIFY(persistedIds.contains(client->uid()));

    const std::optional<chat::ConversationSnapshot> snapshot = repository->loadSnapshot(client->uid(), &errorMessage);
    QVERIFY2(snapshot.has_value(), qPrintable(errorMessage));
    QCOMPARE(snapshot->uid, client->uid());
    QCOMPARE(snapshot->activeSessionId, sessionId);
    QVERIFY(identity::hasIdPrefix(snapshot->uid, QStringLiteral("cli")));
    QVERIFY(identity::hasIdPrefix(snapshot->activeSessionId, QStringLiteral("ses")));
}

void QtLlmCoreTests::toolStudioGeneratesCompactWorkspaceNodeAndPlacementIds()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    auto repository = std::make_shared<toolsstudio::ToolWorkspaceRepository>(tempDir.path());
    toolsstudio::ToolWorkspaceService service(repository);

    QString errorMessage;
    QVERIFY(service.initialize(QString(), &errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

    QString workspaceId;
    QVERIFY(service.createWorkspace(QStringLiteral("My Workspace"), &workspaceId, &errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QVERIFY(identity::isValidId(workspaceId));
    QVERIFY(identity::hasIdPrefix(workspaceId, QStringLiteral("wsp")));
    QCOMPARE(service.currentWorkspaceId(), workspaceId);

    const toolsstudio::ToolWorkspaceSnapshot workspace = service.currentWorkspace();
    QCOMPARE(workspace.rootNodeId, QStringLiteral("root"));

    QString nodeId;
    QVERIFY(service.createNode(workspace.rootNodeId, QStringLiteral("Utilities"), &nodeId, &errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QVERIFY(identity::isValidId(nodeId));
    QVERIFY(identity::hasIdPrefix(nodeId, QStringLiteral("nod")));

    QString placementId;
    QVERIFY(service.addToolToNode(nodeId, QStringLiteral("tool.echo"), &placementId, &errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QVERIFY(identity::isValidId(placementId));
    QVERIFY(identity::hasIdPrefix(placementId, QStringLiteral("plc")));
}

void QtLlmCoreTests::toolStudioExportPackageUsesCompactPackageId()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    toolsstudio::ToolWorkspaceSnapshot workspace;
    workspace.workspaceId = identity::generateId(identity::IdKind::Workspace);
    workspace.name = QStringLiteral("Workspace Export");

    toolsstudio::ToolCategoryNode node;
    node.nodeId = identity::generateId(identity::IdKind::Node);
    node.parentNodeId = workspace.rootNodeId;
    node.name = QStringLiteral("Imported");
    workspace.nodes.append(node);

    toolsstudio::ToolPlacement placement;
    placement.placementId = identity::generateId(identity::IdKind::Placement);
    placement.nodeId = node.nodeId;
    placement.toolId = QStringLiteral("tool.echo");
    workspace.placements.append(placement);

    const QString packagePath = tempDir.filePath(QStringLiteral("workspace-export.json"));
    toolsstudio::ToolImportExportService service;
    QString errorMessage;
    QVERIFY(service.exportWorkspace(workspace, {}, packagePath, false, &errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

    const std::optional<toolsstudio::ToolImportPackage> package = service.loadPackage(packagePath, &errorMessage);
    QVERIFY2(package.has_value(), qPrintable(errorMessage));
    QVERIFY(identity::isValidId(package->packageId));
    QVERIFY(identity::hasIdPrefix(package->packageId, QStringLiteral("pkg")));
    QCOMPARE(package->workspace.workspaceId, workspace.workspaceId);
}

void QtLlmCoreTests::providerFactoryCreatesKnownProviders()
{
    std::unique_ptr<ILLMProvider> ollama = ProviderFactory::create(QStringLiteral("ollama"));
    QVERIFY(ollama != nullptr);
    QCOMPARE(ollama->name(), QStringLiteral("ollama"));

    std::unique_ptr<ILLMProvider> vllm = ProviderFactory::create(QStringLiteral("vllm"));
    QVERIFY(vllm != nullptr);
    QCOMPARE(vllm->name(), QStringLiteral("vllm"));

    std::unique_ptr<ILLMProvider> llamaCpp = ProviderFactory::create(QStringLiteral("llama-cpp"));
    QVERIFY(llamaCpp != nullptr);
    QCOMPARE(llamaCpp->name(), QStringLiteral("llama-cpp"));

    std::unique_ptr<ILLMProvider> openai = ProviderFactory::create(QStringLiteral("openai"));
    QVERIFY(openai != nullptr);
    QCOMPARE(openai->name(), QStringLiteral("openai"));
}

void QtLlmCoreTests::providerFactoryCreatesVendorAliases()
{
    QVERIFY(ProviderFactory::create(QStringLiteral("anthropic")) != nullptr);
    QVERIFY(ProviderFactory::create(QStringLiteral("google")) != nullptr);
    QVERIFY(ProviderFactory::create(QStringLiteral("gemini")) != nullptr);
    QVERIFY(ProviderFactory::create(QStringLiteral("deepseek")) != nullptr);
    QVERIFY(ProviderFactory::create(QStringLiteral("qwen")) != nullptr);
    QVERIFY(ProviderFactory::create(QStringLiteral("glm")) != nullptr);
    QVERIFY(ProviderFactory::create(QStringLiteral("zhipu")) != nullptr);
}

void QtLlmCoreTests::providerFactoryRejectsUnknownProvider()
{
    const std::unique_ptr<ILLMProvider> unknown = ProviderFactory::create(QStringLiteral("unknown-provider"));
    QVERIFY(unknown == nullptr);
}

void QtLlmCoreTests::managedLlamaCppRuntimeListsSupplementalQtllmModels()
{
    const QByteArray oldZnzHome = qgetenv("ZNZ_HOME");
    const QByteArray oldIetaHome = qgetenv("IETA_HOME");
    const bool hadZnzHome = qEnvironmentVariableIsSet("ZNZ_HOME");
    const bool hadIetaHome = qEnvironmentVariableIsSet("IETA_HOME");

    QTemporaryDir runtimeRoot;
    QVERIFY(runtimeRoot.isValid());
    QDir runtimeDir(runtimeRoot.path());
    QVERIFY(runtimeDir.mkpath(QStringLiteral("bin")));
    QVERIFY(runtimeDir.mkpath(QStringLiteral("models")));

    QTemporaryDir emptyRoot;
    QVERIFY(emptyRoot.isValid());
    QVERIFY(QDir(emptyRoot.path()).mkpath(QStringLiteral("qtllm")));

    QTemporaryDir validRoot;
    QVERIFY(validRoot.isValid());
    QDir validDir(validRoot.path());
    QVERIFY(validDir.mkpath(QStringLiteral("qtllm/models")));

    const QString modelPath = validDir.filePath(QStringLiteral("qtllm/models/test-model.gguf"));
    QFile model(modelPath);
    QVERIFY(model.open(QIODevice::WriteOnly));
    model.write("gguf");
    model.close();

    qputenv("ZNZ_HOME", QFile::encodeName(emptyRoot.path()));
    qputenv("IETA_HOME", QFile::encodeName(validRoot.path()));

    LlmConfig config;
    config.providerName = QStringLiteral("llama-cpp");
    config.llamaCppRuntimeRoot = runtimeRoot.path();

    qtllm::runtime::LlamaCppRuntimeLayout layout;
    QString errorMessage;
    const QList<qtllm::runtime::LlamaCppLocalModel> models =
        qtllm::runtime::ManagedLlamaCppRuntime::listLocalModels(config, &layout, &errorMessage);

    if (hadZnzHome) {
        qputenv("ZNZ_HOME", oldZnzHome);
    } else {
        qunsetenv("ZNZ_HOME");
    }
    if (hadIetaHome) {
        qputenv("IETA_HOME", oldIetaHome);
    } else {
        qunsetenv("IETA_HOME");
    }

    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    bool foundModel = false;
    for (const qtllm::runtime::LlamaCppLocalModel &modelInfo : models) {
        if (QFileInfo(modelInfo.filePath).absoluteFilePath() == QFileInfo(modelPath).absoluteFilePath()) {
            foundModel = true;
            QVERIFY2(modelInfo.id == QStringLiteral("test-model"),
                     qPrintable(QStringLiteral("Unexpected model id: %1").arg(modelInfo.id)));
            break;
        }
    }
    QVERIFY(foundModel);
    const QString actualRootDir = QFileInfo(layout.rootDir).absoluteFilePath();
    const QString expectedRootDir = QFileInfo(runtimeRoot.path()).absoluteFilePath();
    QVERIFY2(actualRootDir == expectedRootDir,
             qPrintable(QStringLiteral("Unexpected runtime root: %1").arg(actualRootDir)));
    QVERIFY(layout.modelSearchDirs.contains(QFileInfo(validDir.filePath(QStringLiteral("qtllm/models"))).absoluteFilePath(),
                                            Qt::CaseInsensitive));
}

void QtLlmCoreTests::managedLlamaCppRuntimeDefaultsGpuLayersToAuto()
{
    QTemporaryDir runtimeRoot;
    QVERIFY(runtimeRoot.isValid());

    QDir runtimeDir(runtimeRoot.path());
    QVERIFY(runtimeDir.mkpath(QStringLiteral("bin")));
    QVERIFY(runtimeDir.mkpath(QStringLiteral("models")));

#ifdef Q_OS_WIN
    QFile fakeBackend(runtimeDir.filePath(QStringLiteral("bin/ggml-vulkan.dll")));
#elif defined(Q_OS_MACOS)
    QFile fakeBackend(runtimeDir.filePath(QStringLiteral("bin/libggml-metal.dylib")));
#else
    QFile fakeBackend(runtimeDir.filePath(QStringLiteral("bin/libggml-vulkan.so")));
#endif
    QVERIFY(fakeBackend.open(QIODevice::WriteOnly));
    fakeBackend.close();

    const QString modelPath = runtimeDir.filePath(QStringLiteral("models/test-model.gguf"));
    QFile model(modelPath);
    QVERIFY(model.open(QIODevice::WriteOnly));
    model.write("gguf");
    model.close();

    QTcpServer portProbe;
    QVERIFY(portProbe.listen(QHostAddress::LocalHost, 0));
    const int port = portProbe.serverPort();
    portProbe.close();

    const QString argsPath = runtimeDir.filePath(QStringLiteral("llama-args.txt"));
    qputenv("QTLLM_FAKE_LLAMA_ARGS_FILE", QFile::encodeName(argsPath));

    LlmConfig config;
    config.providerName = QStringLiteral("llama-cpp");
    config.llamaCppRuntimeRoot = runtimeRoot.path();
    config.llamaCppExecutablePath = QCoreApplication::applicationFilePath();
    config.llamaCppModelPath = modelPath;
    config.llamaCppServerPort = port;
    config.llamaCppStartupTimeoutMs = 5000;

    qtllm::runtime::ManagedLlamaCppRuntime runtime;
    QString errorMessage;
    QVERIFY2(runtime.ensureRunning(&config, &errorMessage), qPrintable(errorMessage));

    QFile argsFile(argsPath);
    QVERIFY(argsFile.open(QIODevice::ReadOnly));
    const QString argsText = QString::fromUtf8(argsFile.readAll());
    argsFile.close();

    runtime.stop();
    qunsetenv("QTLLM_FAKE_LLAMA_ARGS_FILE");

    const QStringList args = argsText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const int gpuFlagIndex = args.indexOf(QStringLiteral("--gpu-layers"));
    QVERIFY(gpuFlagIndex >= 0);
    QVERIFY(gpuFlagIndex + 1 < args.size());
    QCOMPARE(args.at(gpuFlagIndex + 1), QStringLiteral("999"));
    QCOMPARE(config.resolvedLlamaCppGpuMode, QStringLiteral("auto"));
    QCOMPARE(config.resolvedLlamaCppGpuLayers, 999);
    QVERIFY(config.resolvedLlamaCppThreadCount > 0);
    QVERIFY(config.resolvedLlamaCppContextSize > 0);
    QVERIFY(config.runtimePlanSummary.contains(QStringLiteral("gpuLayers=999")));
}

void QtLlmCoreTests::managedLlamaCppRuntimeCpuOnlyPolicyDisablesGpuLayers()
{
    QTemporaryDir runtimeRoot;
    QVERIFY(runtimeRoot.isValid());

    QDir runtimeDir(runtimeRoot.path());
    QVERIFY(runtimeDir.mkpath(QStringLiteral("bin")));
    QVERIFY(runtimeDir.mkpath(QStringLiteral("models")));

    const QString modelPath = runtimeDir.filePath(QStringLiteral("models/test-model.gguf"));
    QFile model(modelPath);
    QVERIFY(model.open(QIODevice::WriteOnly));
    model.write("gguf");
    model.close();

    QTcpServer portProbe;
    QVERIFY(portProbe.listen(QHostAddress::LocalHost, 0));
    const int port = portProbe.serverPort();
    portProbe.close();

    const QString argsPath = runtimeDir.filePath(QStringLiteral("llama-args.txt"));
    qputenv("QTLLM_FAKE_LLAMA_ARGS_FILE", QFile::encodeName(argsPath));

    LlmConfig config;
    config.providerName = QStringLiteral("llama-cpp");
    config.llamaCppRuntimeRoot = runtimeRoot.path();
    config.llamaCppExecutablePath = QCoreApplication::applicationFilePath();
    config.llamaCppModelPath = modelPath;
    config.llamaCppServerPort = port;
    config.llamaCppStartupTimeoutMs = 5000;
    config.llamaCppGpuMode = QStringLiteral("cpu-only");
    config.llamaCppPerformanceProfile = QStringLiteral("conservative");
    config.llamaCppContextMode = QStringLiteral("auto");

    qtllm::runtime::ManagedLlamaCppRuntime runtime;
    QString errorMessage;
    QVERIFY2(runtime.ensureRunning(&config, &errorMessage), qPrintable(errorMessage));

    QFile argsFile(argsPath);
    QVERIFY(argsFile.open(QIODevice::ReadOnly));
    const QString argsText = QString::fromUtf8(argsFile.readAll());
    argsFile.close();

    runtime.stop();
    qunsetenv("QTLLM_FAKE_LLAMA_ARGS_FILE");

    const QStringList args = argsText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const int gpuFlagIndex = args.indexOf(QStringLiteral("--gpu-layers"));
    QVERIFY(gpuFlagIndex >= 0);
    QVERIFY(gpuFlagIndex + 1 < args.size());
    QCOMPARE(args.at(gpuFlagIndex + 1), QStringLiteral("0"));
    const int contextFlagIndex = args.indexOf(QStringLiteral("--ctx-size"));
    QVERIFY(contextFlagIndex >= 0);
    QVERIFY(contextFlagIndex + 1 < args.size());
    QCOMPARE(args.at(contextFlagIndex + 1), QStringLiteral("4096"));
    QCOMPARE(config.resolvedLlamaCppGpuMode, QStringLiteral("cpu-only"));
    QCOMPARE(config.resolvedLlamaCppGpuLayers, 0);
    QCOMPARE(config.resolvedLlamaCppContextSize, 4096);
    QVERIFY(config.runtimePlanSummary.contains(QStringLiteral("performance=conservative")));
}

void QtLlmCoreTests::managedLlamaCppRuntimeReusesExistingServerPort()
{
    QTemporaryDir runtimeRoot;
    QVERIFY(runtimeRoot.isValid());

    QDir runtimeDir(runtimeRoot.path());
    QVERIFY(runtimeDir.mkpath(QStringLiteral("bin")));
    QVERIFY(runtimeDir.mkpath(QStringLiteral("models")));

    const QString modelPath = runtimeDir.filePath(QStringLiteral("models/test-model.gguf"));
    QFile model(modelPath);
    QVERIFY(model.open(QIODevice::WriteOnly));
    model.write("gguf");
    model.close();

    QTcpServer existingServer;
    QVERIFY(existingServer.listen(QHostAddress::LocalHost, 0));
    const int port = existingServer.serverPort();
    QObject::connect(&existingServer, &QTcpServer::newConnection, &existingServer, [&existingServer]() {
        while (existingServer.hasPendingConnections()) {
            QTcpSocket *socket = existingServer.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                socket->readAll();
                const QByteArray payload = R"({"status":"ok"})";
                QByteArray response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
                response += "Connection: close\r\n\r\n";
                response += payload;
                socket->write(response);
                socket->disconnectFromHost();
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });

    const QString argsPath = runtimeDir.filePath(QStringLiteral("should-not-start.txt"));
    qputenv("QTLLM_FAKE_LLAMA_ARGS_FILE", QFile::encodeName(argsPath));

    LlmConfig config;
    config.providerName = QStringLiteral("llama-cpp");
    config.llamaCppRuntimeRoot = runtimeRoot.path();
    config.llamaCppExecutablePath = QCoreApplication::applicationFilePath();
    config.llamaCppModelPath = modelPath;
    config.llamaCppServerPort = port;

    qtllm::runtime::ManagedLlamaCppRuntime runtime;
    QString errorMessage;
    QVERIFY2(runtime.ensureRunning(&config, &errorMessage), qPrintable(errorMessage));
    runtime.stop();
    qunsetenv("QTLLM_FAKE_LLAMA_ARGS_FILE");

    QVERIFY(!QFileInfo::exists(argsPath));
    QVERIFY(existingServer.isListening());
}

void QtLlmCoreTests::managedLlamaCppRuntimeWaitsForHttpReadiness()
{
    QTemporaryDir runtimeRoot;
    QVERIFY(runtimeRoot.isValid());

    QDir runtimeDir(runtimeRoot.path());
    QVERIFY(runtimeDir.mkpath(QStringLiteral("bin")));
    QVERIFY(runtimeDir.mkpath(QStringLiteral("models")));

    const QString modelPath = runtimeDir.filePath(QStringLiteral("models/test-model.gguf"));
    QFile model(modelPath);
    QVERIFY(model.open(QIODevice::WriteOnly));
    model.write("gguf");
    model.close();

    QTcpServer readinessServer;
    QVERIFY(readinessServer.listen(QHostAddress::LocalHost, 0));
    const int port = readinessServer.serverPort();
    int readinessProbeCount = 0;
    ::QObject::connect(&readinessServer, &QTcpServer::newConnection, &readinessServer, [&readinessServer, &readinessProbeCount]() {
        while (readinessServer.hasPendingConnections()) {
            QTcpSocket *socket = readinessServer.nextPendingConnection();
            ::QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, &readinessProbeCount]() {
                socket->readAll();
                ++readinessProbeCount;
                const bool ready = readinessProbeCount >= 3;
                const QByteArray payload = ready ? QByteArrayLiteral(R"({"status":"ok"})")
                                                 : QByteArrayLiteral(R"({"status":"loading"})");
                QByteArray response = ready ? QByteArrayLiteral("HTTP/1.1 200 OK\r\n")
                                            : QByteArrayLiteral("HTTP/1.1 503 Service Unavailable\r\n");
                response += QByteArrayLiteral("Content-Type: application/json\r\n");
                response += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
                response += QByteArrayLiteral("Connection: close\r\n\r\n");
                response += payload;
                socket->write(response);
                socket->disconnectFromHost();
            });
            ::QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });

    LlmConfig config;
    config.providerName = QStringLiteral("llama-cpp");
    config.llamaCppRuntimeRoot = runtimeRoot.path();
    config.llamaCppExecutablePath = QCoreApplication::applicationFilePath();
    config.llamaCppModelPath = modelPath;
    config.llamaCppServerPort = port;
    config.llamaCppStartupTimeoutMs = 5000;

    qtllm::runtime::ManagedLlamaCppRuntime runtime;
    QString errorMessage;
    QVERIFY2(runtime.ensureRunning(&config, &errorMessage), qPrintable(errorMessage));
    runtime.stop();
    QVERIFY(readinessProbeCount >= 3);
}

void QtLlmCoreTests::openAiCompatibleBuildRequestNormalizesPath()
{
    OpenAICompatibleProvider provider;

    LlmConfig config;
    config.baseUrl = QStringLiteral("http://127.0.0.1:11434/v1");
    config.apiKey = QStringLiteral("test-key");
    provider.setConfig(config);

    LlmRequest request;
    const QNetworkRequest networkRequest = provider.buildRequest(request);

    QCOMPARE(networkRequest.url().toString(), QStringLiteral("http://127.0.0.1:11434/v1/chat/completions"));
    QCOMPARE(networkRequest.rawHeader("Authorization"), QByteArray("Bearer test-key"));
}

void QtLlmCoreTests::openAiCompatibleBuildRequestAnthropic()
{
    OpenAICompatibleProvider provider;

    LlmConfig config;
    config.baseUrl = QStringLiteral("https://api.anthropic.com/v1");
    config.apiKey = QStringLiteral("anthropic-key");
    config.modelVendor = QStringLiteral("anthropic");
    provider.setConfig(config);

    LlmRequest request;
    request.model = QStringLiteral("claude-3-7-sonnet");

    const QNetworkRequest networkRequest = provider.buildRequest(request);

    QCOMPARE(networkRequest.url().toString(), QStringLiteral("https://api.anthropic.com/v1/messages"));
    QCOMPARE(networkRequest.rawHeader("x-api-key"), QByteArray("anthropic-key"));
    QCOMPARE(networkRequest.rawHeader("anthropic-version"), QByteArray("2023-06-01"));
}

void QtLlmCoreTests::openAiCompatibleBuildRequestGoogle()
{
    OpenAICompatibleProvider provider;

    LlmConfig config;
    config.baseUrl = QStringLiteral("https://generativelanguage.googleapis.com");
    config.apiKey = QStringLiteral("google-key");
    config.modelVendor = QStringLiteral("google");
    config.model = QStringLiteral("gemini-2.0-flash");
    provider.setConfig(config);

    LlmRequest request;

    const QNetworkRequest networkRequest = provider.buildRequest(request);
    const QUrl url = networkRequest.url();

    QCOMPARE(url.path(), QStringLiteral("/v1beta/models/gemini-2.0-flash:generateContent"));
    QCOMPARE(url.query(), QStringLiteral("key=google-key"));
    QCOMPARE(networkRequest.rawHeader("x-goog-api-key"), QByteArray("google-key"));
}

void QtLlmCoreTests::openAiCompatibleBuildPayloadProducesJson()
{
    OpenAICompatibleProvider provider;

    LlmConfig config;
    config.model = QStringLiteral("default-model");
    provider.setConfig(config);

    LlmRequest request;
    request.stream = true;
    request.messages.append({QStringLiteral("user"), QStringLiteral("hello")});

    const QByteArray payload = provider.buildPayload(request);
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    QVERIFY(doc.isObject());

    const QJsonObject obj = doc.object();
    QCOMPARE(obj.value(QStringLiteral("model")).toString(), QStringLiteral("default-model"));
    QCOMPARE(obj.value(QStringLiteral("stream")).toBool(), true);

    const QJsonArray messages = obj.value(QStringLiteral("messages")).toArray();
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.first().toObject().value(QStringLiteral("role")).toString(), QStringLiteral("user"));
    QCOMPARE(messages.first().toObject().value(QStringLiteral("content")).toString(), QStringLiteral("hello"));
}

void QtLlmCoreTests::openAiCompatibleBuildPayloadAnthropicTools()
{
    OpenAICompatibleProvider provider;

    LlmConfig config;
    config.modelVendor = QStringLiteral("anthropic");
    config.model = QStringLiteral("claude-3-7-sonnet");
    provider.setConfig(config);

    LlmRequest request;
    request.stream = false;
    request.messages.append({QStringLiteral("system"), QStringLiteral("You are an assistant")});
    request.messages.append({QStringLiteral("user"), QStringLiteral("What's weather?")});

    QJsonObject parameters;
    parameters.insert(QStringLiteral("type"), QStringLiteral("object"));
    parameters.insert(QStringLiteral("properties"), QJsonObject{{QStringLiteral("city"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}});

    QJsonObject function;
    function.insert(QStringLiteral("name"), QStringLiteral("current_weather"));
    function.insert(QStringLiteral("description"), QStringLiteral("Get weather"));
    function.insert(QStringLiteral("parameters"), parameters);

    QJsonObject tool;
    tool.insert(QStringLiteral("type"), QStringLiteral("function"));
    tool.insert(QStringLiteral("function"), function);

    request.tools.append(tool);

    const QJsonObject payload = QJsonDocument::fromJson(provider.buildPayload(request)).object();
    QCOMPARE(payload.value(QStringLiteral("model")).toString(), QStringLiteral("claude-3-7-sonnet"));
    QCOMPARE(payload.value(QStringLiteral("system")).toString(), QStringLiteral("You are an assistant"));

    const QJsonArray tools = payload.value(QStringLiteral("tools")).toArray();
    QCOMPARE(tools.size(), 1);
    QCOMPARE(tools.first().toObject().value(QStringLiteral("name")).toString(), QStringLiteral("current_weather"));
    QVERIFY(tools.first().toObject().contains(QStringLiteral("input_schema")));
}

void QtLlmCoreTests::openAiCompatibleParseResponse()
{
    OpenAICompatibleProvider provider;

    const QByteArray okJson = R"({"choices":[{"message":{"content":"ok"}}]})";
    const LlmResponse ok = provider.parseResponse(okJson);
    QVERIFY(ok.success);
    QCOMPARE(ok.text, QStringLiteral("ok"));

    const QByteArray badJson = R"({"choices":[]})";
    const LlmResponse bad = provider.parseResponse(badJson);
    QVERIFY(!bad.success);
    QVERIFY(!bad.errorMessage.isEmpty());
}

void QtLlmCoreTests::openAiCompatibleParseAnthropicResponse()
{
    OpenAICompatibleProvider provider;

    LlmConfig config;
    config.modelVendor = QStringLiteral("anthropic");
    provider.setConfig(config);

    const QByteArray json = R"({
      "content": [
        {"type":"text","text":"Let me check."},
        {"type":"tool_use","id":"toolu_1","name":"current_time","input":{"timezone":"Asia/Shanghai"}}
      ],
      "stop_reason":"tool_use"
    })";

    const LlmResponse response = provider.parseResponse(json);
    QVERIFY(response.success);
    QCOMPARE(response.finishReason, QStringLiteral("tool_use"));
    QCOMPARE(response.assistantMessage.content, QStringLiteral("Let me check."));
    QCOMPARE(response.assistantMessage.toolCalls.size(), 1);
    QCOMPARE(response.assistantMessage.toolCalls.first().name, QStringLiteral("current_time"));
}

void QtLlmCoreTests::openAiCompatibleParseGoogleResponse()
{
    OpenAICompatibleProvider provider;

    LlmConfig config;
    config.modelVendor = QStringLiteral("google");
    provider.setConfig(config);

    const QByteArray json = R"({
      "candidates": [
        {
          "finishReason": "STOP",
          "content": {
            "parts": [
              {"text": "Checking weather."},
              {"functionCall": {"name":"current_weather","args":{"city":"Shanghai"}}}
            ]
          }
        }
      ]
    })";

    const LlmResponse response = provider.parseResponse(json);
    QVERIFY(response.success);
    QCOMPARE(response.finishReason, QStringLiteral("STOP"));
    QCOMPARE(response.assistantMessage.content, QStringLiteral("Checking weather."));
    QCOMPARE(response.assistantMessage.toolCalls.size(), 1);
    QCOMPARE(response.assistantMessage.toolCalls.first().name, QStringLiteral("current_weather"));
}

void QtLlmCoreTests::openAiCompatibleParseStreamTokens()
{
    OpenAICompatibleProvider provider;

    const QByteArray chunk =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hel\"}}]}\n"
        "data: {\"choices\":[{\"delta\":{\"content\":\"lo\"}}]}\n"
        "data: [DONE]\n";

    const QList<QString> tokens = provider.parseStreamTokens(chunk);
    QCOMPARE(tokens.size(), 2);
    QCOMPARE(tokens.at(0), QStringLiteral("Hel"));
    QCOMPARE(tokens.at(1), QStringLiteral("lo"));
}

void QtLlmCoreTests::openAiCompatibleParseEventPrefixedSse()
{
    OpenAICompatibleProvider provider;

    const QByteArray sse = R"(event: message

data: {"choices":[{"delta":{"content":"Hel"}}]}

event: message

data: {"choices":[{"delta":{"content":"lo","reasoning_content":"think"},"finish_reason":"stop"}]}

event: done

data: [DONE]
)";

    const LlmResponse response = provider.parseResponse(sse);
    QVERIFY(response.success);
    QCOMPARE(response.finishReason, QStringLiteral("stop"));
    QCOMPARE(response.assistantMessage.content, QStringLiteral("Hello"));
    QCOMPARE(response.text, QStringLiteral("Hello"));
}

void QtLlmCoreTests::openAiCompatibleParseStreamDeltasReasoningAndToolCalls()
{
    OpenAICompatibleProvider provider;

    const QByteArray chunk =
        "event: message\n"
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hi\",\"reasoning_content\":\"think\",\"tool_calls\":[{\"index\":0,\"id\":\"call_1\",\"type\":\"function\",\"function\":{\"name\":\"lookup\",\"arguments\":\"{\\\"q\\\":\"}}]}}]}\n";

    const QList<LlmStreamDelta> deltas = provider.parseStreamDeltas(chunk);
    QCOMPARE(deltas.size(), 2);
    QCOMPARE(deltas.at(0).channel, QStringLiteral("content"));
    QCOMPARE(deltas.at(0).text, QStringLiteral("Hi"));
    QCOMPARE(deltas.at(1).channel, QStringLiteral("reasoning"));
    QCOMPARE(deltas.at(1).text, QStringLiteral("think"));
}

void QtLlmCoreTests::openAiCompatibleParseOllamaJsonLines()
{
    OpenAICompatibleProvider provider;

    const QByteArray jsonLines = R"({"model":"qwen3","message":{"role":"assistant","content":"Hel","thinking":"plan"},"done":false}
{"model":"qwen3","message":{"role":"assistant","content":"lo"},"done":false}
{"model":"qwen3","message":{"role":"assistant","tool_calls":[{"function":{"name":"lookup","arguments":{"q":"qt"}}}]},"done":true,"done_reason":"stop"}
)";

    const LlmResponse response = provider.parseResponse(jsonLines);
    QVERIFY(response.success);
    QCOMPARE(response.finishReason, QStringLiteral("stop"));
    QCOMPARE(response.assistantMessage.content, QStringLiteral("Hello"));
    QCOMPARE(response.assistantMessage.toolCalls.size(), 1);
    QCOMPARE(response.assistantMessage.toolCalls.first().name, QStringLiteral("lookup"));
    QCOMPARE(response.assistantMessage.toolCalls.first().arguments.value(QStringLiteral("q")).toString(), QStringLiteral("qt"));

    const QList<LlmStreamDelta> deltas = provider.parseStreamDeltas(
        QByteArray(R"({"model":"qwen3","message":{"role":"assistant","content":"Hel","thinking":"plan"},"done":false}
)"));
    QCOMPARE(deltas.size(), 2);
    QCOMPARE(deltas.at(0).channel, QStringLiteral("content"));
    QCOMPARE(deltas.at(0).text, QStringLiteral("Hel"));
    QCOMPARE(deltas.at(1).channel, QStringLiteral("reasoning"));
    QCOMPARE(deltas.at(1).text, QStringLiteral("plan"));
}

void QtLlmCoreTests::openAiBuildRequestNormalizesResponsesPath()
{
    OpenAIProvider provider;

    LlmConfig config;
    config.baseUrl = QStringLiteral("https://api.openai.com/v1");
    config.apiKey = QStringLiteral("openai-key");
    provider.setConfig(config);

    LlmRequest request;
    const QNetworkRequest networkRequest = provider.buildRequest(request);

    QCOMPARE(networkRequest.url().toString(), QStringLiteral("https://api.openai.com/v1/responses"));
    QCOMPARE(networkRequest.rawHeader("Authorization"), QByteArray("Bearer openai-key"));
}

void QtLlmCoreTests::openAiBuildPayloadSanitizesTools()
{
    OpenAIProvider provider;

    LlmConfig config;
    config.baseUrl = QStringLiteral("https://api.openai.com/v1");
    config.model = QStringLiteral("gpt-5.4");
    provider.setConfig(config);

    LlmRequest request;
    request.stream = true;
    request.messages.append({QStringLiteral("system"), QStringLiteral("You are helpful")});
    request.messages.append({QStringLiteral("user"), QStringLiteral("List files")});

    QJsonObject function;
    function.insert(QStringLiteral("name"), QStringLiteral("mcp_filesystem_list_directory"));
    function.insert(QStringLiteral("description"), QStringLiteral("List files in a directory"));
    function.insert(QStringLiteral("parameters"), makeSimpleSchema());

    QJsonObject tool;
    tool.insert(QStringLiteral("type"), QStringLiteral("function"));
    tool.insert(QStringLiteral("function"), function);
    request.tools.append(tool);

    const QJsonObject payload = QJsonDocument::fromJson(provider.buildPayload(request)).object();
    QCOMPARE(payload.value(QStringLiteral("model")).toString(), QStringLiteral("gpt-5.4"));
    QCOMPARE(payload.value(QStringLiteral("stream")).toBool(), true);
    QCOMPARE(payload.value(QStringLiteral("instructions")).toString(), QStringLiteral("You are helpful"));

    const QJsonArray input = payload.value(QStringLiteral("input")).toArray();
    QCOMPARE(input.size(), 1);
    QCOMPARE(input.first().toObject().value(QStringLiteral("role")).toString(), QStringLiteral("user"));

    const QJsonArray tools = payload.value(QStringLiteral("tools")).toArray();
    QCOMPARE(tools.size(), 1);
    const QJsonObject outTool = tools.first().toObject();
    QCOMPARE(outTool.value(QStringLiteral("type")).toString(), QStringLiteral("function"));
    QCOMPARE(outTool.value(QStringLiteral("name")).toString(), QStringLiteral("mcp_filesystem_list_directory"));
    QCOMPARE(outTool.value(QStringLiteral("strict")).toBool(), false);

    const QJsonObject parameters = outTool.value(QStringLiteral("parameters")).toObject();
    QVERIFY(!parameters.contains(QStringLiteral("$schema")));
    QVERIFY(!parameters.value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("path")).toObject()
                 .contains(QStringLiteral("default")));
    QCOMPARE(parameters.value(QStringLiteral("type")).toString(), QStringLiteral("object"));
}

void QtLlmCoreTests::openAiParseResponseParsesFunctionCalls()
{
    OpenAIProvider provider;

    const QByteArray json = R"({
      "status":"completed",
      "output":[
        {"type":"message","content":[{"type":"output_text","text":"Checking files."}]},
        {"type":"function_call","call_id":"call_1","name":"mcp_filesys2_list_directory","arguments":"{\"path\":\".\"}"}
      ]
    })";

    const LlmResponse response = provider.parseResponse(json);
    QVERIFY(response.success);
    QCOMPARE(response.finishReason, QStringLiteral("completed"));
    QCOMPARE(response.assistantMessage.content, QStringLiteral("Checking files."));
    QCOMPARE(response.assistantMessage.toolCalls.size(), 1);
    QCOMPARE(response.assistantMessage.toolCalls.first().id, QStringLiteral("call_1"));
    QCOMPARE(response.assistantMessage.toolCalls.first().name, QStringLiteral("mcp_filesys2_list_directory"));
    QCOMPARE(response.assistantMessage.toolCalls.first().arguments.value(QStringLiteral("path")).toString(), QStringLiteral("."));
}

void QtLlmCoreTests::openAiParseResponseParsesEventPrefixedSse()
{
    OpenAIProvider provider;

    const QByteArray sse = R"(event: response.created

data: {"type":"response.created","response":{"id":"resp_1","status":"in_progress"}}

event: response.in_progress

data: {"type":"response.in_progress","response":{"id":"resp_1","status":"in_progress"}}

event: response.output_item.added

data: {"type":"response.output_item.added","item":{"id":"msg_1","type":"message","role":"assistant","content":[]}}

event: response.content_part.added

data: {"type":"response.content_part.added","item_id":"msg_1","part":{"type":"output_text","text":"Hello"}}

event: response.content_part.done

data: {"type":"response.content_part.done","item_id":"msg_1","part":{"type":"output_text","text":"Hello"}}

event: response.output_text.delta

data: {"type":"response.output_text.delta","delta":"Hel"}

event: response.output_text.annotation.added

data: {"type":"response.output_text.annotation.added","annotation":{"type":"file_citation"}}

event: response.output_text.delta

data: {"type":"response.output_text.delta","delta":"lo"}

event: response.output_text.done

data: {"type":"response.output_text.done","text":"Hello"}

event: response.completed

data: {"type":"response.completed","response":{"id":"resp_1","status":"completed","output":[{"type":"message","content":[{"type":"output_text","text":"Hello"}]}]}}

data: [DONE]
)";

    const LlmResponse response = provider.parseResponse(sse);
    QVERIFY(response.success);
    QCOMPARE(response.finishReason, QStringLiteral("completed"));
    QCOMPARE(response.assistantMessage.content, QStringLiteral("Hello"));
    QCOMPARE(response.text, QStringLiteral("Hello"));
}

void QtLlmCoreTests::openAiParseResponseParsesStreamingFunctionCallEvents()
{
    OpenAIProvider provider;

    const QByteArray sse = R"(event: response.created

data: {"type":"response.created","response":{"id":"resp_2","status":"in_progress"}}

event: response.output_item.added

data: {"type":"response.output_item.added","item":{"type":"function_call","id":"fc_1","call_id":"call_1","name":"mcp_filesys2_list_directory"}}

event: response.function_call_arguments.delta

data: {"type":"response.function_call_arguments.delta","item_id":"fc_1","call_id":"call_1","delta":"{\"path\":"}

event: response.function_call_arguments.done

data: {"type":"response.function_call_arguments.done","item_id":"fc_1","call_id":"call_1","name":"mcp_filesys2_list_directory","arguments":"{\"path\":\".\"}"}

event: response.completed

data: {"type":"response.completed","response":{"id":"resp_2","status":"completed","output":[{"type":"function_call","id":"fc_1","call_id":"call_1","name":"mcp_filesys2_list_directory","arguments":"{\"path\":\".\"}"}]}}

data: [DONE]
)";

    const LlmResponse response = provider.parseResponse(sse);
    QVERIFY(response.success);
    QCOMPARE(response.finishReason, QStringLiteral("completed"));
    QCOMPARE(response.assistantMessage.toolCalls.size(), 1);
    QCOMPARE(response.assistantMessage.toolCalls.first().id, QStringLiteral("call_1"));
    QCOMPARE(response.assistantMessage.toolCalls.first().name, QStringLiteral("mcp_filesys2_list_directory"));
    QCOMPARE(response.assistantMessage.toolCalls.first().arguments.value(QStringLiteral("path")).toString(), QStringLiteral("."));
}

void QtLlmCoreTests::mcpToolSyncRegistersImportedTools()
{
    auto toolRegistry = std::make_shared<qtllm::tools::LlmToolRegistry>();
    auto serverRegistry = std::make_shared<qtllm::tools::mcp::McpServerRegistry>();
    auto mcpClient = std::make_shared<FakeMcpClient>();

    qtllm::tools::mcp::McpServerDefinition server;
    server.serverId = QStringLiteral("filesys2");
    server.name = QStringLiteral("Filesystem");
    server.transport = QStringLiteral("stdio");
    server.command = QStringLiteral("dummy");

    QString errorMessage;
    QVERIFY(serverRegistry->registerServer(server, &errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

    qtllm::tools::mcp::McpToolDescriptor descriptor;
    descriptor.name = QStringLiteral("list_directory");
    descriptor.description = QStringLiteral("List directory content");
    descriptor.inputSchema = makeSimpleSchema();
    mcpClient->listedTools.append(descriptor);

    qtllm::tools::mcp::McpToolSyncService service(toolRegistry, serverRegistry, mcpClient);
    QVERIFY(service.syncServerTools(QStringLiteral("filesys2"), &errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

    const QList<qtllm::tools::LlmToolDefinition> tools = toolRegistry->allTools();
    bool found = false;
    for (const qtllm::tools::LlmToolDefinition &tool : tools) {
        if (tool.toolId == QStringLiteral("mcp::filesys2::list_directory")) {
            found = true;
            QCOMPARE(tool.invocationName, QStringLiteral("mcp_filesys2_list_directory"));
            QCOMPARE(tool.category, QStringLiteral("mcp"));
            QVERIFY(tool.capabilityTags.contains(QStringLiteral("source:mcp")));
            QVERIFY(tool.capabilityTags.contains(QStringLiteral("mcp_server:filesys2")));
        }
    }
    QVERIFY(found);
}

void QtLlmCoreTests::toolExecutionLayerExecutesMcpToolByInvocationName()
{
    auto toolRegistry = std::make_shared<qtllm::tools::LlmToolRegistry>();
    auto serverRegistry = std::make_shared<qtllm::tools::mcp::McpServerRegistry>();
    auto mcpClient = std::make_shared<FakeMcpClient>();

    qtllm::tools::mcp::McpServerDefinition server;
    server.serverId = QStringLiteral("filesys2");
    server.name = QStringLiteral("Filesystem");
    server.transport = QStringLiteral("stdio");
    server.command = QStringLiteral("dummy");

    QString errorMessage;
    QVERIFY(serverRegistry->registerServer(server, &errorMessage));

    qtllm::tools::LlmToolDefinition tool;
    tool.toolId = QStringLiteral("mcp::filesys2::list_directory");
    tool.invocationName = QStringLiteral("mcp_filesys2_list_directory");
    tool.name = QStringLiteral("list_directory");
    tool.description = QStringLiteral("List directory content");
    tool.inputSchema = makeSimpleSchema();
    tool.category = QStringLiteral("mcp");
    QVERIFY(toolRegistry->registerTool(tool));

    mcpClient->callResult.success = true;
    mcpClient->callResult.output = QJsonObject{{QStringLiteral("entries"), QJsonArray{QStringLiteral("a.txt"), QStringLiteral("b.txt")}}};

    qtllm::tools::runtime::ToolExecutionLayer layer;
    layer.setToolRegistry(toolRegistry);
    layer.setMcpServerRegistry(serverRegistry);
    layer.setMcpClient(mcpClient);

    qtllm::tools::runtime::ToolCallRequest request;
    request.callId = QStringLiteral("call_1");
    request.toolId = QStringLiteral("mcp_filesys2_list_directory");
    request.arguments = QJsonObject{{QStringLiteral("path"), QStringLiteral(".")}};

    qtllm::tools::runtime::ToolExecutionContext context;
    context.clientId = QStringLiteral("client-a");
    context.sessionId = QStringLiteral("session-1");
    context.requestId = QStringLiteral("request-1");

    const QList<qtllm::tools::runtime::ToolExecutionResult> results = layer.executeBatch({request}, context);
    QCOMPARE(results.size(), 1);
    QVERIFY(results.first().success);
    QCOMPARE(results.first().toolId, QStringLiteral("mcp::filesys2::list_directory"));
    QCOMPARE(mcpClient->lastServerId, QStringLiteral("filesys2"));
    QCOMPARE(mcpClient->lastToolName, QStringLiteral("list_directory"));
    QCOMPARE(mcpClient->lastArguments.value(QStringLiteral("path")).toString(), QStringLiteral("."));
}

void QtLlmCoreTests::fileLogSinkRotatesPerClient()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qtllm::logging::FileLogSinkOptions options;
    options.workspaceRoot = tempDir.path();
    options.maxBytesPerFile = 180;
    options.maxFilesPerClient = 2;

    qtllm::logging::FileLogSink sink(options);

    qtllm::logging::LogEvent eventA;
    eventA.clientId = QStringLiteral("client-a");
    eventA.category = QStringLiteral("tool.execution");
    eventA.level = qtllm::logging::LogLevel::Info;
    eventA.message = QStringLiteral("rotation test entry with enough payload to trigger file rollover");
    eventA.fields = QJsonObject{{QStringLiteral("index"), 1}, {QStringLiteral("payload"), QString(80, QLatin1Char('x'))}};

    for (int i = 0; i < 8; ++i) {
        eventA.timestampUtc = QDateTime::currentDateTimeUtc();
        eventA.fields.insert(QStringLiteral("index"), i);
        sink.write(eventA);
    }

    qtllm::logging::LogEvent eventB;
    eventB.clientId = QStringLiteral("client-b");
    eventB.category = QStringLiteral("llm.request");
    eventB.level = qtllm::logging::LogLevel::Debug;
    eventB.message = QStringLiteral("secondary client log");
    eventB.timestampUtc = QDateTime::currentDateTimeUtc();
    sink.write(eventB);

    const QDir logsRoot(tempDir.filePath(QStringLiteral(".qtllm/logs")));
    QVERIFY(logsRoot.exists());
    QVERIFY(QDir(logsRoot.filePath(QStringLiteral("client-a"))).exists());
    QVERIFY(QDir(logsRoot.filePath(QStringLiteral("client-b"))).exists());

    const QFileInfoList clientAFiles = QDir(logsRoot.filePath(QStringLiteral("client-a"))).entryInfoList(QStringList() << QStringLiteral("*.jsonl"), QDir::Files, QDir::Name);
    QVERIFY(!clientAFiles.isEmpty());
    QVERIFY(clientAFiles.size() <= 4);

    const QFileInfoList clientBFiles = QDir(logsRoot.filePath(QStringLiteral("client-b"))).entryInfoList(QStringList() << QStringLiteral("*.jsonl"), QDir::Files, QDir::Name);
    QCOMPARE(clientBFiles.size(), 1);

    QFile file(clientAFiles.last().absoluteFilePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray content = file.readAll();
    QVERIFY(content.contains("tool.execution"));
    QVERIFY(content.contains("client-a"));
}

void QtLlmCoreTests::defaultMcpClientReadsToolsOverStdio()
{
    QSKIP("Requires unrestricted child-process launch for stdio transport smoke testing.");
}

void QtLlmCoreTests::defaultMcpClientCallsToolOverHttpLikeTransport()
{
    QSKIP("Requires stable loopback socket transport in the current test host.");
}

void QtLlmCoreTests::streamChunkParserHandlesFragmentedInput()
{
    StreamChunkParser parser;

    const QStringList first = parser.append("line1-part");
    QVERIFY(first.isEmpty());

    const QStringList second = parser.append("-end\nline2\n");
    QCOMPARE(second.size(), 2);
    QCOMPARE(second.at(0), QStringLiteral("line1-part-end"));
    QCOMPARE(second.at(1), QStringLiteral("line2"));
}

void QtLlmCoreTests::streamChunkParserTakePendingLine()
{
    StreamChunkParser parser;

    parser.append("partial");
    QCOMPARE(parser.takePendingLine(), QStringLiteral("partial"));
    QCOMPARE(parser.takePendingLine(), QString());
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.contains(QStringLiteral("--host")) && args.contains(QStringLiteral("--port"))) {
        const QByteArray argsFilePath = qgetenv("QTLLM_FAKE_LLAMA_ARGS_FILE");
        if (!argsFilePath.isEmpty()) {
            QFile argsFile(QString::fromLocal8Bit(argsFilePath));
            if (argsFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                for (int i = 1; i < args.size(); ++i) {
                    argsFile.write(args.at(i).toUtf8());
                    argsFile.write("\n");
                }
            }
        }

        const int portIndex = args.indexOf(QStringLiteral("--port"));
        bool ok = false;
        const int port = portIndex >= 0 && portIndex + 1 < args.size()
            ? args.at(portIndex + 1).toInt(&ok)
            : 0;
        QTcpServer server;
        if (!ok || !server.listen(QHostAddress::LocalHost, static_cast<quint16>(port))) {
            return 2;
        }
        bool readyAfterOk = false;
        int readyAfterProbes = QString::fromLocal8Bit(qgetenv("QTLLM_FAKE_LLAMA_READY_AFTER_PROBES")).toInt(&readyAfterOk);
        if (!readyAfterOk || readyAfterProbes <= 0) {
            readyAfterProbes = 1;
        }
        int readinessProbeCount = 0;
        const QString probeCountPath = QString::fromLocal8Bit(qgetenv("QTLLM_FAKE_LLAMA_PROBE_COUNT_FILE"));
        QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, readyAfterProbes, probeCountPath, &readinessProbeCount]() {
            while (server.hasPendingConnections()) {
                QTcpSocket *socket = server.nextPendingConnection();
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, readyAfterProbes, probeCountPath, &readinessProbeCount]() {
                    socket->readAll();
                    ++readinessProbeCount;
                    if (!probeCountPath.isEmpty()) {
                        QFile probeCountFile(probeCountPath);
                        if (probeCountFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                            probeCountFile.write(QByteArray::number(readinessProbeCount));
                        }
                    }

                    const bool ready = readinessProbeCount >= readyAfterProbes;
                    const QByteArray payload = ready ? QByteArrayLiteral(R"({"status":"ok"})")
                                                     : QByteArrayLiteral(R"({"status":"loading"})");
                    QByteArray response = ready ? QByteArrayLiteral("HTTP/1.1 200 OK\r\n")
                                                : QByteArrayLiteral("HTTP/1.1 503 Service Unavailable\r\n");
                    response += QByteArrayLiteral("Content-Type: application/json\r\n");
                    response += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
                    response += QByteArrayLiteral("Connection: close\r\n\r\n");
                    response += payload;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        QEventLoop loop;
        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        loop.exec();
        return 0;
    }
    if (args.size() >= 3 && args.at(1) == QStringLiteral("--emit-json")) {
        QTextStream(stdout) << args.at(2) << Qt::endl;
        return 0;
    }

    QtLlmCoreTests tc;
    return QTest::qExec(&tc, argc, argv);
}
