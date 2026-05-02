# `ConversationClientFactory` 与会话存储

## 1. 定位

如果 `ConversationClient` 负责单个 client 的会话编排，那么 `ConversationClientFactory` 负责：

- 统一创建和复用 `ConversationClient`
- 根据 `clientId` 恢复已有会话
- 管理多个 client
- 统一触发快照保存

与之对应的持久化类是 `ConversationRepository`。

## 2. 主要类

### `ConversationClientFactory`

头文件：

- `src/qtllm/chat/conversationclientfactory.h`

核心方法：

- `setRepository(...)`
- `acquire(const QString &uid = QString())`
- `find(const QString &uid) const`
- `clientIds() const`
- `persistedClientIds() const`
- `saveClient(...)`
- `saveAll(...)`

### `ConversationRepository`

头文件：

- `src/qtllm/storage/conversationrepository.h`

核心作用：

- 负责把 `ConversationSnapshot` 写入磁盘
- 负责按 `clientId` 从磁盘恢复快照

## 3. `acquire(...)` 的行为

`acquire(uid)` 的行为可以概括为：

```text
如果内存中已有 client
  -> 直接返回
否则
  -> 创建新的 ConversationClient
  -> 如果 repository 已配置，则尝试 loadSnapshot(uid)
  -> 建立自动保存信号连接
  -> 返回 client
```

如果 `uid` 为空：

- `ConversationClientFactory` 会自动生成一个新的 UUID

## 4. 自动保存机制

工厂在成功创建 client 后，会把以下信号连接到保存动作：

- `historyChanged`
- `sessionsChanged`
- `activeSessionChanged`
- `configChanged`
- `profileChanged`

这意味着：

- 只要会话历史、session、配置或 profile 有变化，就会自动触发快照保存

## 5. 快照里包含什么

`ConversationSnapshot` 当前包含：

- `uid`
- `config`
- `profile`
- `activeSessionId`
- `sessions`

每个 session 又包含：

- `sessionId`
- `title`
- `createdAt`
- `updatedAt`
- `history`

## 6. 默认存储位置

从当前工程用法看，常见持久化路径是：

- `.qtllm/clients/`

每个 client 对应一个 JSON 文件。

典型形式：

```text
.qtllm/clients/
  alice.json
  worker-bot.json
  1b3c...uuid.json
```

## 7. 适合的应用组织方式

### 单窗口、多角色

可以这样组织：

- 一个角色一个 `clientId`
- 角色下多个 session

### 多工作区、多账号

可以这样组织：

- 工作区或账号映射到不同 `ConversationRepository` 根目录
- 每个根目录下再通过 `clientId` 区分不同会话实体

## 8. 推荐开发流程

```cpp
auto repository = std::make_shared<qtllm::storage::ConversationRepository>(
    QStringLiteral(".qtllm/clients"));

auto *factory = new qtllm::chat::ConversationClientFactory(this);
factory->setRepository(repository);

QSharedPointer<qtllm::chat::ConversationClient> client =
    factory->acquire(QStringLiteral("assistant-main"));
```

然后继续：

- 给 `client` 设置 config
- 设置 profile
- 根据需要创建或切换 session
- 发送消息

## 9. 使用建议

- UI 层不要自己维护“同一个 `clientId` 对应哪个对象”的缓存，交给 `ConversationClientFactory`
- 统一通过工厂获取 `ConversationClient`
- 应用关闭前如需强制落盘，可调用 `saveAll(...)`
- 多 client 场景下，`persistedClientIds()` 很适合做启动时的恢复列表
