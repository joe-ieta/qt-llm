# Conversation 组件

## 定位

QtLlm::Conversation 是面向上层应用的完整会话编排组件。它组合轻量核心、诊断、工具和本地运行时能力，同时保持业务界面、文档处理和具体产品流程在 qt-llm 之外。

## 包含范围

- 会话客户端、客户端工厂和快照。
- 会话持久化。
- 运行时宿主门面和模型目录服务。
- 工具化会话入口。
- Tools Inside 运行与追踪服务。
- Tools Studio 配置与目录服务。

## 依赖关系

Conversation 公开依赖 Core、Diagnostics、Tools、LocalRuntime 以及 Qt Core、Network、Sql。只需要基础请求或单项能力的项目应直接链接更小的组件。

## 兼容策略

原有 QtLlm::QtLlm 目标和 qtllm 库产物继续保留，并公开组合 Conversation。现有 CMake 使用方式不需要修改；新增项目可通过 find_package(QtLlm REQUIRED COMPONENTS Conversation) 明确选择完整会话能力。

聚合目标不再重复编译所有实现源码，公共 C++ 头文件、类名、命名空间和行为保持不变。

## 安装包集成与二进制边界

上层应用需要会话、运行时门面或工具调用入口时，应链接 `QtLlm::Conversation`：

```cmake
find_package(QtLlm CONFIG REQUIRED COMPONENTS Conversation)
target_link_libraries(my_application PRIVATE QtLlm::Conversation)
```

该目标负责传递 Conversation 所需的基础组件依赖。上层应用不需要了解内部源文件归属，也不应自行定义 DLL 导入导出宏。

- Qt5 与 Qt6 使用相同的 CMake 目标名和公开头文件路径。
- STATIC 与 SHARED 使用相同的上层集成代码。
- Windows 共享库中的 `ConversationClient`、`ConversationClientFactory`、`QtLLMClient`、`RuntimeFacade`、`RuntimeRequestHandle` 和 `ToolEnabledChatEntry` 均通过 Conversation 组件导出边界提供。
- 本次组件化不改变公开类、方法签名、信号槽和运行行为，既有工程可继续使用 `QtLlm::QtLlm` 聚合目标。
- 业务侧文档处理、工作流编排和界面逻辑仍由上层应用负责，不进入 Conversation 组件。

安装包消费验证已覆盖 Qt5/Qt6 与 STATIC/SHARED，并对 Conversation 独立目标完成实际可执行程序运行。
