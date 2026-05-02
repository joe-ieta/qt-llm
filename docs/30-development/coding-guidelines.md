# 编码规范

## 基本原则

- 保持 Qt 原生风格。
- 网络和 LLM 请求保持异步。
- 避免阻塞 UI 线程。
- provider 逻辑不进入 UI。
- Host App facade 不复制底层 provider 细节。
- 修改要聚焦，避免无关重构。

## C++/Qt

- 使用 C++17。
- 使用 `QString`、`QByteArray`、Qt 容器和 Qt JSON API。
- HTTP 使用 `QNetworkAccessManager` 封装在网络层。
- 信号槽用于异步通知。
- 公共头文件保持简洁。

## 文档同步

新增公共 API、provider、runtime 行为、工具能力或运行数据布局时，必须更新对应中文文档。
