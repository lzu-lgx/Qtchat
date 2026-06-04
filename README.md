# Qtchat - 基于 Qt/C++ 的桌面即时通讯系统

Qtchat 是一个使用 Qt/C++ 开发的桌面即时通讯系统，项目目标是实现一个类似微信/QQ的桌面聊天原型。项目目前包含桌面客户端、聊天服务端、本地消息缓存、联系人系统、服务端消息持久化、离线消息补发以及 AI 助手等功能。

项目当前包含两个可执行程序：

```text
Qtchat      桌面聊天客户端
ChatServer  聊天服务端
```

---

## 1. 项目简介

Qtchat 是一个 C++/Qt 桌面即时通讯项目，主要用于练习和展示客户端开发、网络通信、数据库持久化、异步 API 调用和服务端消息转发等能力。

项目当前已经实现了：

- Qt Widgets 桌面聊天界面
- 本地 SQLite 会话和消息缓存
- 多用户独立本地数据库
- AI 助手会话
- DeepSeek API 接入
- TCP 客户端和服务端通信
- JSON 消息协议
- 客户端身份注册
- 服务端用户表
- 服务端好友关系表
- 服务端会话表
- 服务端会话成员表
- 服务端消息表
- 联系人列表请求与返回
- 新建会话选择联系人
- 普通私聊在线转发
- 普通私聊离线消息保存
- 用户上线后离线消息补发

---

## 2. 功能说明

### 2.1 桌面聊天客户端

客户端基于 Qt Widgets 实现，主要界面包括：

- 左侧会话列表
- 右侧聊天标题
- 右侧消息显示区域
- 底部消息输入框
- 发送按钮
- 新建会话按钮

客户端支持：

- 加载本地会话列表
- 创建私聊会话
- 显示历史消息
- 发送文本消息
- 接收网络消息
- 保存聊天记录到本地 SQLite
- 根据联系人创建会话
- 不同用户使用不同本地数据库

---

### 2.2 AI 助手

项目内置一个特殊会话：

```text
AI 助手
```

用户在 AI 助手会话中发送消息时，不会走普通网络聊天逻辑，而是调用 DeepSeek API 获取回复。

AI 助手功能包括：

- 独立 AI 会话
- DeepSeek API 接入
- config.ini / config_client*.ini 配置 API Key
- 异步网络请求
- AI 回复保存到本地数据库
- “正在思考中...” 临时提示
- AI 消息不发送到 ChatServer

AI 助手流程：

```text
用户在 AI 助手会话输入问题
↓
客户端保存用户消息到本地 SQLite
↓
显示“正在思考中...”
↓
AiService 异步调用 DeepSeek API
↓
收到 AI 回复
↓
保存 AI 回复到本地 SQLite
↓
刷新聊天窗口
```

---

### 2.3 普通私聊

普通私聊消息会走服务端。

在线消息流程：

```text
张三客户端
↓
保存消息到张三本地 SQLite
↓
组装 JSON 消息
↓
发送给 ChatServer
↓
ChatServer 保存消息到服务端数据库
↓
ChatServer 根据 receiver_id 精准转发给李四
↓
李四客户端保存消息到李四本地 SQLite
↓
李四客户端刷新 UI
```

离线消息流程：

```text
张三客户端发送消息
↓
ChatServer 保存消息到 server_messages
↓
李四不在线
↓
消息保持 delivered = 0
↓
李四上线并注册 userId
↓
ChatServer 查询李四未投递消息
↓
ChatServer 补发离线消息
↓
李四客户端保存并显示
↓
ChatServer 标记 delivered = 1
```

---

### 2.4 联系人系统

服务端维护正式的用户表和好友关系表：

```text
users
friendships
```

客户端连接成功后，会向服务端请求联系人列表：

```json
{
  "type": "get_contacts",
  "user_id": "user_001"
}
```

服务端查询数据库后返回：

```json
{
  "type": "contacts_result",
  "user_id": "user_001",
  "contacts": [
    {
      "user_id": "user_002",
      "user_name": "李四",
      "avatar_path": ""
    }
  ]
}
```

客户端收到联系人列表后保存到内存中的 `m_contacts`，点击“新建会话”时从联系人列表中选择联系人。

---

## 3. 技术栈

### 3.1 客户端

- C++
- Qt 6
- Qt Widgets
- Qt Network
- Qt SQL
- SQLite
- JSON 消息协议
- DeepSeek API

### 3.2 服务端

- C++
- Qt Core
- Qt Network
- Qt SQL
- QTcpServer
- QTcpSocket
- SQLite

---

## 4. 项目结构

```text
Qtchat/
├── main.cpp
├── mainwindow.h
├── mainwindow.cpp
├── mainwindow.ui
│
├── model/
│   ├── User.h
│   ├── User.cpp
│   ├── Conversation.h
│   ├── Conversation.cpp
│   ├── Message.h
│   ├── Message.cpp
│   ├── Contact.h
│   └── Contact.cpp
│
├── database/
│   ├── DatabaseManager.h
│   └── DatabaseManager.cpp
│
├── network/
│   ├── NetworkClient.h
│   └── NetworkClient.cpp
│
├── service/
│   ├── AiService.h
│   └── AiService.cpp
│
├── config/
│   ├── AppConfig.h
│   └── AppConfig.cpp
│
├── server/
│   ├── main.cpp
│   ├── ChatServer.h
│   └── ChatServer.cpp
│
├── CMakeLists.txt
├── .gitignore
└── README.md
```

---

## 5. 核心模块说明

### 5.1 MainWindow

`MainWindow` 是客户端主窗口，负责客户端主要业务流程。

主要职责：

- 初始化聊天界面
- 加载本地会话
- 创建新会话
- 发送普通聊天消息
- 发送 AI 助手消息
- 接收网络聊天消息
- 接收服务端联系人列表
- 刷新会话列表
- 刷新消息显示区域

普通消息发送流程：

```text
用户输入消息
↓
MainWindow::sendCurrentMessage()
↓
保存消息到本地 SQLite
↓
刷新 UI
↓
判断当前会话是否为 AI 助手
↓
普通会话：sendNetworkChatMessage()
AI 会话：handleAiAssistantReply()
```

---

### 5.2 DatabaseManager

`DatabaseManager` 负责客户端本地 SQLite 数据库。

客户端本地主要表：

```text
conversations
messages
```

作用：

- 保存本地会话
- 保存本地聊天记录
- 支持客户端重启后加载历史消息
- 作为客户端本地缓存层

不同用户使用不同数据库文件：

```text
chat_user_001.db
chat_user_002.db
```

本地数据库不是服务端权威数据源，而是客户端缓存。服务端数据库负责保存正式用户、好友、会话和消息数据。

---

### 5.3 NetworkClient

`NetworkClient` 封装客户端 TCP 通信。

主要职责：

- 连接 ChatServer
- 发送 JSON 消息
- 接收服务端 JSON 消息
- 注册客户端身份
- 请求联系人列表
- 发出 `jsonMessageReceived` 信号给 MainWindow

客户端连接成功后会发送身份注册消息：

```json
{
  "type": "client_register",
  "user_id": "user_001",
  "user_name": "张三"
}
```

然后请求联系人列表：

```json
{
  "type": "get_contacts",
  "user_id": "user_001"
}
```

---

### 5.4 AiService

`AiService` 负责调用 DeepSeek API。

主要职责：

- 从配置文件读取 API Key
- 组装 DeepSeek 请求 JSON
- 发起异步 HTTP 请求
- 解析 DeepSeek 返回结果
- 通过 `replyReady` / `replyFailed` 信号通知 MainWindow

AI 助手会话和普通聊天会话分离：

```text
普通私聊：走 NetworkClient + ChatServer
AI 助手：走 AiService + DeepSeek API
```

---

### 5.5 ChatServer

`ChatServer` 是服务端核心模块。

主要职责：

- 监听 TCP 端口
- 接收客户端连接
- 维护在线用户映射
- 处理客户端身份注册
- 处理联系人请求
- 保存聊天消息
- 在线消息精准转发
- 离线消息保存
- 用户上线后补发离线消息

在线用户映射：

```text
userId -> QTcpSocket*
```

例如：

```text
user_001 -> socketA
user_002 -> socketB
```

这样服务端收到消息后，可以根据：

```json
{
  "receiver_id": "user_002"
}
```

找到对应 socket，并只转发给目标用户。

---

## 6. JSON 消息协议

### 6.1 客户端注册

客户端连接服务端成功后发送：

```json
{
  "type": "client_register",
  "user_id": "user_001",
  "user_name": "张三"
}
```

服务端收到后记录：

```text
user_001 -> 当前 QTcpSocket
```

---

### 6.2 请求联系人列表

客户端请求当前用户的联系人：

```json
{
  "type": "get_contacts",
  "user_id": "user_001"
}
```

---

### 6.3 联系人列表返回

服务端返回：

```json
{
  "type": "contacts_result",
  "user_id": "user_001",
  "contacts": [
    {
      "user_id": "user_002",
      "user_name": "李四",
      "avatar_path": ""
    }
  ]
}
```

---

### 6.4 普通聊天消息

客户端发送普通聊天消息：

```json
{
  "type": "chat_message",
  "conversation_id": "conv_user_001_user_002",
  "sender_id": "user_001",
  "sender_name": "张三",
  "receiver_id": "user_002",
  "receiver_name": "李四",
  "content": "你好",
  "timestamp": "2026-06-03T17:48:08"
}
```

服务端保存消息后，如果接收方在线，则立即转发；如果接收方不在线，则保存为离线消息。

---

## 7. 服务端数据库设计

服务端数据库文件：

```text
chat_server.db
```

目前包含以下表。

---

### 7.1 users

用户表。

```text
id
username
password_hash
avatar_path
created_at
```

作用：

```text
保存系统用户信息
```

示例：

```text
user_001 张三
user_002 李四
```

---

### 7.2 friendships

好友关系表。

```text
id
user_id
friend_id
created_at
```

张三和李四互为好友时，保存两条关系：

```text
user_001 -> user_002
user_002 -> user_001
```

这样查询某个用户的联系人时，只需要查：

```text
friendships.user_id = 当前用户
```

---

### 7.3 conversations

服务端会话表。

```text
id
type
title
created_at
updated_at
```

私聊会话 ID 示例：

```text
conv_user_001_user_002
```

私聊会话 ID 通过双方 userId 排序生成，保证双方客户端和服务端使用同一个会话 ID。

---

### 7.4 conversation_members

会话成员表。

```text
id
conversation_id
user_id
joined_at
is_hidden
```

作用：

```text
表示某个用户参与了某个会话
```

例如：

```text
conv_user_001_user_002 user_001
conv_user_001_user_002 user_002
```

这样张三登录后，服务端可以查询张三参与了哪些会话。

---

### 7.5 server_messages

服务端消息表。

```text
id
conversation_id
sender_id
sender_name
receiver_id
receiver_name
content
timestamp
delivered
```

其中：

```text
delivered = 0  表示未投递
delivered = 1  表示已投递
```

当接收方离线时，消息保存为 `delivered = 0`。接收方上线后，服务端补发离线消息，并将其标记为 `delivered = 1`。

---

## 8. 客户端本地数据库设计

客户端本地数据库文件由配置文件决定，例如：

```text
chat_user_001.db
chat_user_002.db
```

客户端本地主要表：

```text
conversations
messages
```

作用：

- 缓存当前用户的会话列表
- 缓存当前用户的历史消息
- 支持离线查看历史消息
- 支持客户端重启后恢复会话记录

本地数据库和服务端数据库的关系：

```text
服务端数据库：权威数据源
客户端数据库：本地缓存
```

---

## 9. 配置文件

客户端通过启动参数指定配置文件：

```text
--config config_client1.ini
```

示例：`config_client1.ini`

```ini
[Client]
UserId=user_001
UserName=张三
DatabaseName=chat_user_001.db

[DeepSeek]
ApiKey=your_api_key
Model=deepseek-v4-flash
BaseUrl=https://api.deepseek.com/chat/completions
```

示例：`config_client2.ini`

```ini
[Client]
UserId=user_002
UserName=李四
DatabaseName=chat_user_002.db

[DeepSeek]
ApiKey=your_api_key
Model=deepseek-v4-flash
BaseUrl=https://api.deepseek.com/chat/completions
```

当前阶段客户端仍通过配置文件确定当前用户身份。后续可以扩展为正式登录页面，由服务端验证账号密码并返回 userId。

---

## 10. 运行方式

### 10.1 启动服务端

在 Qt Creator 中选择运行目标：

```text
ChatServer
```

运行成功后应看到：

```text
ChatServer started on port 8888
```

---

### 10.2 启动客户端 1

运行目标选择：

```text
Qtchat
```

运行参数填写：

```text
--config config_client1.ini
```

客户端 1 身份示例：

```text
user_001 张三
```

---

### 10.3 启动客户端 2

可以在 Qt Creator 中切换参数，也可以在 build 目录通过命令行启动：

```powershell
.\Qtchat.exe --config config_client2.ini
```

客户端 2 身份示例：

```text
user_002 李四
```

---

## 11. 测试流程

### 11.1 联系人加载测试

启动：

```text
ChatServer
Qtchat --config config_client1.ini
Qtchat --config config_client2.ini
```

张三客户端应加载到：

```text
李四 user_002
```

李四客户端应加载到：

```text
张三 user_001
```

---

### 11.2 新建会话测试

在张三客户端点击：

```text
新建会话
```

应弹出联系人选择框，选择：

```text
李四 (user_002)
```

随后左侧会话列表出现：

```text
李四
```

李四客户端同理，选择：

```text
张三 (user_001)
```

---

### 11.3 在线私聊测试

```text
1. 启动 ChatServer
2. 启动张三客户端
3. 启动李四客户端
4. 张三创建和李四的会话
5. 张三发送消息
6. 李四客户端收到并显示消息
```

服务端会输出类似：

```text
Server saved message: ...
Forwarded chat message to: "user_002"
```

---

### 11.4 离线消息测试

```text
1. 启动 ChatServer
2. 只启动张三客户端
3. 张三创建和李四的会话
4. 张三给李四发送消息
5. 服务端保存为未投递消息
6. 启动李四客户端
7. 服务端自动补发离线消息
8. 李四客户端显示该消息
```

服务端会输出类似：

```text
Receiver not online, message kept offline: "user_002"
Delivered offline messages to "user_002" count: 1
```

---

### 11.5 AI 助手测试

```text
1. 打开 AI 助手会话
2. 输入问题
3. 客户端调用 DeepSeek API
4. AI 回复保存到本地数据库
```

AI 助手消息不会发送给 ChatServer。

---

## 12. 当前已完成内容

- Qt Widgets 客户端界面
- 本地 SQLite 会话和消息缓存
- 多用户独立本地数据库
- AI 助手会话
- DeepSeek API 接入
- TCP 客户端连接
- TCP 服务端监听
- JSON 消息协议
- 客户端身份注册
- 服务端在线用户映射
- 服务端用户表
- 服务端好友关系表
- 服务端会话表
- 服务端会话成员表
- 服务端消息表
- 联系人列表请求与返回
- 新建会话选择联系人
- 普通私聊在线转发
- 普通私聊离线消息保存
- 用户上线后离线消息补发

---

## 13. 后续计划

后续可以继续扩展：

- 注册 / 登录界面
- 密码哈希校验
- 登录态管理
- 发送失败状态
- 消息 ACK 机制
- 发送中 / 已发送 / 发送失败状态
- 已读 / 未读状态
- 群聊
- 图片消息
- 文件消息
- 好友申请与好友验证
- 服务端历史消息同步
- 会话分页加载
- 消息分页加载
- UI 美化
- 打包部署

---

## 14. Git 忽略文件建议

建议 `.gitignore` 中忽略以下文件：

```gitignore
config.ini
config_client*.ini

chat.db
chat_user_*.db
chat_server.db

*.user
build/
```

配置文件中包含 API Key，本地数据库中包含测试数据，不应提交到 Git 仓库。