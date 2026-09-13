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
