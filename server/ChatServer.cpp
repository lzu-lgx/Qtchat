#include "ChatServer.h"

#include <QDebug>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QUuid>
#include <QAbstractSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>

static QString passwordHash(const QString& password)
{
    QByteArray hash = QCryptographicHash::hash(
        password.toUtf8(),
        QCryptographicHash::Sha256
    );

    return QString(hash.toHex());
}

ChatServer::ChatServer(QObject *parent)
    : QObject(parent),
      m_server(new QTcpServer(this)),
      m_db(QSqlDatabase::addDatabase("QSQLITE", "chat_server_connection"))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &ChatServer::handleNewConnection);
}

bool ChatServer::startServer(quint16 port)
{
    if (!initDatabase()) {
        return false;
    }

    if (!initDefaultData()) {
        return false;
    }

    if (!m_server->listen(QHostAddress::Any, port)) {
        qDebug() << "Failed to start server:" << m_server->errorString();
        return false;
    }

    qDebug() << "ChatServer started on port" << port;
    return true;
}

bool ChatServer::initDatabase()
{
    m_db.setDatabaseName("chat_server.db");

    if (!m_db.open()) {
        qDebug() << "Failed to open server database:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);

    QString createUsersTable =
        "CREATE TABLE IF NOT EXISTS users ("
        "id TEXT PRIMARY KEY,"
        "username TEXT NOT NULL UNIQUE,"
        "password_hash TEXT,"
        "avatar_path TEXT,"
        "created_at TEXT NOT NULL"
        ")";

    if (!query.exec(createUsersTable)) {
        qDebug() << "Failed to create users table:"
                 << query.lastError().text();
        return false;
    }

    QString createFriendshipsTable =
        "CREATE TABLE IF NOT EXISTS friendships ("
        "id TEXT PRIMARY KEY,"
        "user_id TEXT NOT NULL,"
        "friend_id TEXT NOT NULL,"
        "created_at TEXT NOT NULL,"
        "UNIQUE(user_id, friend_id),"
        "FOREIGN KEY(user_id) REFERENCES users(id),"
        "FOREIGN KEY(friend_id) REFERENCES users(id)"
        ")";

    if (!query.exec(createFriendshipsTable)) {
        qDebug() << "Failed to create friendships table:"
                 << query.lastError().text();
        return false;
    }

    QString createFriendRequestsTable =
    "CREATE TABLE IF NOT EXISTS friend_requests ("
    "id TEXT PRIMARY KEY,"
    "from_user_id TEXT NOT NULL,"
    "from_user_name TEXT NOT NULL,"
    "to_user_id TEXT NOT NULL,"
    "to_user_name TEXT NOT NULL,"
    "message TEXT,"
    "status INTEGER NOT NULL DEFAULT 0,"
    "created_at TEXT NOT NULL,"
    "handled_at TEXT,"
    "UNIQUE(from_user_id, to_user_id),"
    "FOREIGN KEY(from_user_id) REFERENCES users(id),"
    "FOREIGN KEY(to_user_id) REFERENCES users(id)"
    ")";

    if (!query.exec(createFriendRequestsTable)) {
        qDebug() << "Failed to create friend_requests table:"
                 << query.lastError().text();
        return false;
    }

    QString createConversationsTable =
        "CREATE TABLE IF NOT EXISTS conversations ("
        "id TEXT PRIMARY KEY,"
        "type INTEGER NOT NULL,"
        "title TEXT,"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ")";

    if (!query.exec(createConversationsTable)) {
        qDebug() << "Failed to create conversations table:"
                 << query.lastError().text();
        return false;
    }

    QString createConversationMembersTable =
        "CREATE TABLE IF NOT EXISTS conversation_members ("
        "id TEXT PRIMARY KEY,"
        "conversation_id TEXT NOT NULL,"
        "user_id TEXT NOT NULL,"
        "joined_at TEXT NOT NULL,"
        "is_hidden INTEGER NOT NULL DEFAULT 0,"
        "UNIQUE(conversation_id, user_id),"
        "FOREIGN KEY(conversation_id) REFERENCES conversations(id),"
        "FOREIGN KEY(user_id) REFERENCES users(id)"
        ")";

    if (!query.exec(createConversationMembersTable)) {
        qDebug() << "Failed to create conversation_members table:"
                 << query.lastError().text();
        return false;
    }

    QString createMessagesTable =
        "CREATE TABLE IF NOT EXISTS server_messages ("
        "id TEXT PRIMARY KEY,"
        "conversation_id TEXT,"
        "sender_id TEXT NOT NULL,"
        "sender_name TEXT,"
        "receiver_id TEXT NOT NULL,"
        "receiver_name TEXT,"
        "content TEXT NOT NULL,"
        "timestamp TEXT NOT NULL,"
        "delivered INTEGER NOT NULL DEFAULT 0,"
        "FOREIGN KEY(conversation_id) REFERENCES conversations(id)"
        ")";

    if (!query.exec(createMessagesTable)) {
        qDebug() << "Failed to create server_messages table:"
                 << query.lastError().text();
        return false;
    }

    qDebug() << "Server database initialized successfully";
    return true;
}

void ChatServer::handleNewConnection()
{
    QTcpSocket *clientSocket = m_server->nextPendingConnection();

    if (!clientSocket) {
        return;
    }

    m_clients.append(clientSocket);

    qDebug() << "New client connected:"
             << clientSocket->peerAddress().toString()
             << clientSocket->peerPort();

    connect(clientSocket, &QTcpSocket::readyRead,
            this, &ChatServer::handleReadyRead);

    connect(clientSocket, &QTcpSocket::disconnected,
            this, [this, clientSocket]()
    {
        qDebug() << "Client disconnected:"
                 << clientSocket->peerAddress().toString()
                 << clientSocket->peerPort();

        m_clients.removeAll(clientSocket);

        QString userIdToRemove;

        for (auto it = m_userSockets.begin(); it != m_userSockets.end(); ++it) {
            if (it.value() == clientSocket) {
                userIdToRemove = it.key();
                break;
            }
        }

        if (!userIdToRemove.isEmpty()) {
            m_userSockets.remove(userIdToRemove);
            qDebug() << "Removed user socket mapping:" << userIdToRemove;
        }

        clientSocket->deleteLater();
    });
}

void ChatServer::handleReadyRead()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());

    if (!clientSocket) {
        return;
    }

    while (clientSocket->canReadLine()) {
        QByteArray line = clientSocket->readLine().trimmed();

        if (line.isEmpty()) {
            continue;
        }

        qDebug() << "Received message from client:"
                 << clientSocket->peerAddress().toString()
                 << clientSocket->peerPort()
                 << line;

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);

        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            qDebug() << "Invalid JSON from client:" << parseError.errorString();
            continue;
        }

        handleJsonMessage(clientSocket, doc.object());
    }
}

void ChatServer::handleJsonMessage(QTcpSocket *clientSocket,
                                   const QJsonObject& json)
{
    QString type = json.value("type").toString();

    if (type == "login") {
        handleLogin(clientSocket, json);
        return;
    }

    if (type == "register") {
        handleRegister(clientSocket, json);
        return;
    }

    if (type == "client_register") {
        handleClientRegister(clientSocket, json);
        return;
    }

    if (type == "get_contacts") {
        handleGetContacts(clientSocket, json);
        return;
    }

    if (type == "send_friend_request") 
    {
        handleSendFriendRequest(clientSocket, json);
        return;
    }

    if (type == "respond_friend_request") {
        handleRespondFriendRequest(clientSocket, json);
        return;
    }

    if (type == "chat_message") {
        handleChatMessage(json);
        return;
    }

    qDebug() << "Unsupported message type:" << type;
}

void ChatServer::handleClientRegister(QTcpSocket *clientSocket,
                                      const QJsonObject& json)
{
    QString userId = json.value("user_id").toString();
    QString userName = json.value("user_name").toString();

    if (userId.isEmpty()) {
        qDebug() << "Client register failed: empty user_id";
        return;
    }

    m_userSockets[userId] = clientSocket;

    qDebug() << "Client registered:"
             << userId
             << userName
             << clientSocket->peerAddress().toString()
             << clientSocket->peerPort();

    deliverOfflineMessages(userId);
}

void ChatServer::handleChatMessage(const QJsonObject& json)
{
    QJsonObject message = json;

    if (!saveChatMessage(message)) {
        return;
    }

    bool delivered = forwardChatMessage(message);

    if (delivered) {
        QString messageId = message.value("message_id").toString();
        markMessageDelivered(messageId);
    }
}



bool ChatServer::forwardChatMessage(const QJsonObject& json)
{
    QString receiverId = json.value("receiver_id").toString();

    if (receiverId.isEmpty()) {
        qDebug() << "Cannot forward chat message: empty receiver_id";
        return false;
    }

    QTcpSocket *targetSocket = m_userSockets.value(receiverId, nullptr);

    if (!targetSocket ||
        targetSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Receiver not online, message kept offline:" << receiverId;
        return false;
    }

    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');

    targetSocket->write(data);
    targetSocket->flush();

    qDebug() << "Forwarded chat message to:" << receiverId;
    return true;
}
bool ChatServer::saveChatMessage(QJsonObject& json)
{
    QString conversationId = json.value("conversation_id").toString();
    QString senderId = json.value("sender_id").toString();
    QString senderName = json.value("sender_name").toString();
    QString receiverId = json.value("receiver_id").toString();
    QString receiverName = json.value("receiver_name").toString();
    QString content = json.value("content").toString();
    QString timestamp = json.value("timestamp").toString();

    if (senderId.isEmpty() || receiverId.isEmpty() || content.isEmpty()) {
        qDebug() << "Invalid chat message, cannot save:" << json;
        return false;
    }

    if (conversationId.isEmpty()) {
        conversationId = privateConversationIdForUsers(senderId, receiverId);
        json["conversation_id"] = conversationId;
    }

    if (timestamp.isEmpty()) {
        timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
        json["timestamp"] = timestamp;
    }

    QString messageId = json.value("message_id").toString();

    if (messageId.isEmpty()) {
        messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        json["message_id"] = messageId;
    }

    QSqlQuery query(m_db);

    query.prepare(
        "INSERT OR IGNORE INTO server_messages "
        "(id, conversation_id, sender_id, sender_name, receiver_id, receiver_name, content, timestamp, delivered) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"
    );

    query.addBindValue(messageId);
    query.addBindValue(conversationId);
    query.addBindValue(senderId);
    query.addBindValue(senderName);
    query.addBindValue(receiverId);
    query.addBindValue(receiverName);
    query.addBindValue(content);
    query.addBindValue(timestamp);
    query.addBindValue(0);

    if (!query.exec()) {
        qDebug() << "Failed to save server message:" << query.lastError().text();
        return false;
    }

    qDebug() << "Server saved message:" << messageId
             << "conversation:" << conversationId
             << "from" << senderId
             << "to" << receiverId;

    return true;
}

void ChatServer::deliverOfflineMessages(const QString& userId)
{
    QTcpSocket *targetSocket = m_userSockets.value(userId, nullptr);

    if (!targetSocket ||
        targetSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Cannot deliver offline messages, user not connected:" << userId;
        return;
    }

    QSqlQuery query(m_db);

    query.prepare(
        "SELECT id, conversation_id, sender_id, sender_name, "
        "receiver_id, receiver_name, content, timestamp "
        "FROM server_messages "
        "WHERE receiver_id = ? AND delivered = 0 "
        "ORDER BY timestamp ASC"
    );

    query.addBindValue(userId);

    if (!query.exec()) {
        qDebug() << "Failed to load offline messages:"
                 << query.lastError().text();
        return;
    }

    int count = 0;

    while (query.next()) {
        QString messageId = query.value(0).toString();

        QJsonObject json;
        json["type"] = "chat_message";
        json["message_id"] = messageId;
        json["conversation_id"] = query.value(1).toString();
        json["sender_id"] = query.value(2).toString();
        json["sender_name"] = query.value(3).toString();
        json["receiver_id"] = query.value(4).toString();
        json["receiver_name"] = query.value(5).toString();
        json["content"] = query.value(6).toString();
        json["timestamp"] = query.value(7).toString();

        QJsonDocument doc(json);
        QByteArray data = doc.toJson(QJsonDocument::Compact);
        data.append('\n');

        qint64 bytesWritten = targetSocket->write(data);
        targetSocket->flush();

        qDebug() << "Sent offline message to"
                 << userId
                 << "messageId:" << messageId
                 << "bytes:" << bytesWritten
                 << "data:" << data;

        if (bytesWritten > 0) {
            markMessageDelivered(messageId);
            count++;
        } else {
            qDebug() << "Failed to write offline message to socket:"
                     << targetSocket->errorString();
        }
    }

    if (count > 0) {
        qDebug() << "Delivered offline messages to"
                 << userId
                 << "count:" << count;
    } else {
        qDebug() << "No offline messages delivered to" << userId;
    }
}

void ChatServer::markMessageDelivered(const QString& messageId)
{
    if (messageId.isEmpty()) {
        return;
    }

    QSqlQuery query(m_db);

    query.prepare(
        "UPDATE server_messages "
        "SET delivered = 1 "
        "WHERE id = ?"
    );

    query.addBindValue(messageId);

    if (!query.exec()) {
        qDebug() << "Failed to mark message delivered:"
                 << query.lastError().text();
        return;
    }

    qDebug() << "Marked message delivered:" << messageId;
}

QString ChatServer::privateConversationIdForUsers(const QString& userId1,
                                                  const QString& userId2) const
{
    QString first = userId1;
    QString second = userId2;

    if (first > second) {
        std::swap(first, second);
    }

    return "conv_" + first + "_" + second;
}

bool ChatServer::initDefaultData()
{
    QSqlQuery query(m_db);

    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    query.prepare(
        "INSERT OR IGNORE INTO users "
        "(id, username, password_hash, avatar_path, created_at) "
        "VALUES (?, ?, ?, ?, ?)"
    );

    query.addBindValue("user_001");
    query.addBindValue("张三");
    query.addBindValue(passwordHash("123456"));
    query.addBindValue("");
    query.addBindValue(now);

    if (!query.exec()) {
        qDebug() << "Failed to insert default user_001:"
                 << query.lastError().text();
        return false;
    }

    query.prepare(
        "INSERT OR IGNORE INTO users "
        "(id, username, password_hash, avatar_path, created_at) "
        "VALUES (?, ?, ?, ?, ?)"
    );

    query.addBindValue("user_002");
    query.addBindValue("李四");
    query.addBindValue(passwordHash("123456"));
    query.addBindValue("");
    query.addBindValue(now);

    if (!query.exec()) {
        qDebug() << "Failed to insert default user_002:"
                 << query.lastError().text();
        return false;
    }

    query.prepare(
        "INSERT OR IGNORE INTO friendships "
        "(id, user_id, friend_id, created_at) "
        "VALUES (?, ?, ?, ?)"
    );

    query.addBindValue("friend_user_001_user_002");
    query.addBindValue("user_001");
    query.addBindValue("user_002");
    query.addBindValue(now);

    if (!query.exec()) {
        qDebug() << "Failed to insert friendship user_001 -> user_002:"
                 << query.lastError().text();
        return false;
    }

    query.prepare(
        "INSERT OR IGNORE INTO friendships "
        "(id, user_id, friend_id, created_at) "
        "VALUES (?, ?, ?, ?)"
    );

    query.addBindValue("friend_user_002_user_001");
    query.addBindValue("user_002");
    query.addBindValue("user_001");
    query.addBindValue(now);

    if (!query.exec()) {
        qDebug() << "Failed to insert friendship user_002 -> user_001:"
                 << query.lastError().text();
        return false;
    }

    QString conversationId = privateConversationIdForUsers("user_001", "user_002");

    query.prepare(
        "INSERT OR IGNORE INTO conversations "
        "(id, type, title, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?)"
    );

    query.addBindValue(conversationId);
    query.addBindValue(1);          // 1 = 私聊
    query.addBindValue("");         // 私聊标题可以为空，客户端按对方名称展示
    query.addBindValue(now);
    query.addBindValue(now);

    if (!query.exec()) {
        qDebug() << "Failed to insert default conversation:"
                 << query.lastError().text();
        return false;
    }

    query.prepare(
        "INSERT OR IGNORE INTO conversation_members "
        "(id, conversation_id, user_id, joined_at, is_hidden) "
        "VALUES (?, ?, ?, ?, ?)"
    );

    query.addBindValue("member_" + conversationId + "_user_001");
    query.addBindValue(conversationId);
    query.addBindValue("user_001");
    query.addBindValue(now);
    query.addBindValue(0);

    if (!query.exec()) {
        qDebug() << "Failed to insert conversation member user_001:"
                 << query.lastError().text();
        return false;
    }

    query.prepare(
        "INSERT OR IGNORE INTO conversation_members "
        "(id, conversation_id, user_id, joined_at, is_hidden) "
        "VALUES (?, ?, ?, ?, ?)"
    );

    query.addBindValue("member_" + conversationId + "_user_002");
    query.addBindValue(conversationId);
    query.addBindValue("user_002");
    query.addBindValue(now);
    query.addBindValue(0);

    if (!query.exec()) {
        qDebug() << "Failed to insert conversation member user_002:"
                 << query.lastError().text();
        return false;
    }

    qDebug() << "Default users, friendships and conversations initialized";
    return true;
}

void ChatServer::handleGetContacts(QTcpSocket *clientSocket,
                                   const QJsonObject& json)
{
    QString userId = json.value("user_id").toString();

    if (userId.isEmpty()) {
        qDebug() << "Get contacts failed: empty user_id";
        return;
    }

    sendContactsResult(clientSocket, userId);
}

void ChatServer::sendContactsResult(QTcpSocket *clientSocket,
                                    const QString& userId)
{
    if (!clientSocket ||
        clientSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QSqlQuery query(m_db);

    query.prepare(
        "SELECT users.id, users.username, users.avatar_path "
        "FROM friendships "
        "JOIN users ON friendships.friend_id = users.id "
        "WHERE friendships.user_id = ? "
        "ORDER BY users.username ASC"
    );

    query.addBindValue(userId);

    if (!query.exec()) {
        qDebug() << "Failed to query contacts:"
                 << query.lastError().text();
        return;
    }

    QJsonArray contacts;

    while (query.next()) {
        QJsonObject contact;
        contact["user_id"] = query.value(0).toString();
        contact["user_name"] = query.value(1).toString();
        contact["avatar_path"] = query.value(2).toString();

        contacts.append(contact);
    }

    QJsonObject response;
    response["type"] = "contacts_result";
    response["user_id"] = userId;
    response["contacts"] = contacts;

    QJsonDocument doc(response);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');

    clientSocket->write(data);
    clientSocket->flush();

    qDebug() << "Sent contacts result to"
             << userId
             << "count:" << contacts.size();
}

void ChatServer::handleLogin(QTcpSocket *clientSocket,
                             const QJsonObject& json)
{
    QString username = json.value("username").toString().trimmed();
    QString password = json.value("password").toString();

    if (username.isEmpty()) {
        sendLoginResult(clientSocket,
                        false,
                        "",
                        "",
                        "用户名不能为空");
        return;
    }

    if (password.isEmpty()) {
        sendLoginResult(clientSocket,
                        false,
                        "",
                        "",
                        "密码不能为空");
        return;
    }

    QSqlQuery query(m_db);

    query.prepare(
        "SELECT id, username, password_hash "
        "FROM users "
        "WHERE username = ?"
    );

    query.addBindValue(username);

    if (!query.exec()) {
        qDebug() << "Failed to query user for login:"
                 << query.lastError().text();

        sendLoginResult(clientSocket,
                        false,
                        "",
                        "",
                        "服务器查询用户失败");
        return;
    }

    if (!query.next()) {
        sendLoginResult(clientSocket,
                        false,
                        "",
                        "",
                        "用户不存在");
        return;
    }

    QString userId = query.value(0).toString();
    QString userName = query.value(1).toString();
    QString savedPasswordHash = query.value(2).toString();

    QString inputPasswordHash = passwordHash(password);

    if (savedPasswordHash != inputPasswordHash) {
        sendLoginResult(clientSocket,
                        false,
                        "",
                        "",
                        "密码错误");
        return;
    }

    qDebug() << "Login success:"
             << userId
             << userName;

    sendLoginResult(clientSocket,
                    true,
                    userId,
                    userName);
}

void ChatServer::sendLoginResult(QTcpSocket *clientSocket,
                                 bool success,
                                 const QString& userId,
                                 const QString& userName,
                                 const QString& errorText)
{
    if (!clientSocket ||
        clientSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonObject response;
    response["type"] = "login_result";
    response["success"] = success;

    if (success) {
        response["user_id"] = userId;
        response["user_name"] = userName;
        response["database_name"] = "chat_" + userId + ".db";
    } else {
        response["error"] = errorText;
    }

    QJsonDocument doc(response);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');

    clientSocket->write(data);
    clientSocket->flush();

    qDebug() << "Sent login result:"
             << response;
}

QString ChatServer::generateNextUserId()
{
    QSqlQuery query(m_db);

    if (!query.exec("SELECT id FROM users")) {
        qDebug() << "Failed to query users for generating user id:"
                 << query.lastError().text();
        return QString();
    }

    int maxNumber = 0;

    while (query.next()) {
        QString userId = query.value(0).toString();

        if (!userId.startsWith("user_")) {
            continue;
        }

        QString numberText = userId.mid(QString("user_").length());
        bool ok = false;
        int number = numberText.toInt(&ok);

        if (ok && number > maxNumber) {
            maxNumber = number;
        }
    }

    int nextNumber = maxNumber + 1;

    return QString("user_%1").arg(nextNumber, 3, 10, QChar('0'));
}

void ChatServer::handleRegister(QTcpSocket *clientSocket,
                                const QJsonObject& json)
{
    QString username = json.value("username").toString().trimmed();
    QString password = json.value("password").toString();

    if (username.isEmpty()) {
        sendRegisterResult(clientSocket,
                           false,
                           "",
                           "",
                           "用户名不能为空");
        return;
    }

    if (password.isEmpty()) {
        sendRegisterResult(clientSocket,
                           false,
                           "",
                           "",
                           "密码不能为空");
        return;
    }

    QSqlQuery checkQuery(m_db);

    checkQuery.prepare(
        "SELECT id "
        "FROM users "
        "WHERE username = ?"
    );

    checkQuery.addBindValue(username);

    if (!checkQuery.exec()) {
        qDebug() << "Failed to check username:"
                 << checkQuery.lastError().text();

        sendRegisterResult(clientSocket,
                           false,
                           "",
                           "",
                           "服务器检查用户名失败");
        return;
    }

    if (checkQuery.next()) {
        sendRegisterResult(clientSocket,
                           false,
                           "",
                           "",
                           "用户名已存在");
        return;
    }

    QString userId = generateNextUserId();

    if (userId.isEmpty()) {
        sendRegisterResult(clientSocket,
                           false,
                           "",
                           "",
                           "生成用户 ID 失败");
        return;
    }

    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    QSqlQuery insertQuery(m_db);

    insertQuery.prepare(
        "INSERT INTO users "
        "(id, username, password_hash, avatar_path, created_at) "
        "VALUES (?, ?, ?, ?, ?)"
    );

    insertQuery.addBindValue(userId);
    insertQuery.addBindValue(username);
    insertQuery.addBindValue(passwordHash(password));
    insertQuery.addBindValue("");
    insertQuery.addBindValue(now);

    if (!insertQuery.exec()) {
        qDebug() << "Failed to register user:"
                 << insertQuery.lastError().text();

        sendRegisterResult(clientSocket,
                           false,
                           "",
                           "",
                           "注册用户失败");
        return;
    }

    qDebug() << "Register success:" << userId << username;

    sendRegisterResult(clientSocket,
                       true,
                       userId,
                       username);
}

void ChatServer::sendRegisterResult(QTcpSocket *clientSocket,
                                    bool success,
                                    const QString& userId,
                                    const QString& userName,
                                    const QString& errorText)
{
    if (!clientSocket ||
        clientSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonObject response;
    response["type"] = "register_result";
    response["success"] = success;

    if (success) {
        response["user_id"] = userId;
        response["user_name"] = userName;
    } else {
        response["error"] = errorText;
    }

    QJsonDocument doc(response);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');

    clientSocket->write(data);
    clientSocket->flush();

    qDebug() << "Sent register result:" << response;
}

void ChatServer::handleAddFriend(QTcpSocket *clientSocket,
                                 const QJsonObject& json)
{
    QString userId = json.value("user_id").toString().trimmed();
    QString friendUsername = json.value("friend_username").toString().trimmed();

    if (userId.isEmpty()) {
        sendAddFriendResult(clientSocket,
                            false,
                            "",
                            "",
                            "",
                            "",
                            "当前用户ID为空");
        return;
    }

    if (friendUsername.isEmpty()) {
        sendAddFriendResult(clientSocket,
                            false,
                            "",
                            "",
                            "",
                            "",
                            "好友用户名不能为空");
        return;
    }

    QSqlQuery userQuery(m_db);

    userQuery.prepare(
        "SELECT id, username, avatar_path "
        "FROM users "
        "WHERE username = ?"
    );

    userQuery.addBindValue(friendUsername);

    if (!userQuery.exec()) {
        qDebug() << "Failed to query friend user:"
                 << userQuery.lastError().text();

        sendAddFriendResult(clientSocket,
                            false,
                            "",
                            "",
                            "",
                            "",
                            "查询用户失败");
        return;
    }

    if (!userQuery.next()) {
        sendAddFriendResult(clientSocket,
                            false,
                            "",
                            "",
                            "",
                            "",
                            "用户不存在");
        return;
    }

    QString friendId = userQuery.value(0).toString();
    QString friendName = userQuery.value(1).toString();
    QString avatarPath = userQuery.value(2).toString();

    if (friendId == userId) {
        sendAddFriendResult(clientSocket,
                            false,
                            "",
                            "",
                            "",
                            "",
                            "不能添加自己为好友");
        return;
    }

    QSqlQuery checkQuery(m_db);

    checkQuery.prepare(
        "SELECT id "
        "FROM friendships "
        "WHERE user_id = ? AND friend_id = ?"
    );

    checkQuery.addBindValue(userId);
    checkQuery.addBindValue(friendId);

    if (!checkQuery.exec()) {
        qDebug() << "Failed to check friendship:"
                 << checkQuery.lastError().text();

        sendAddFriendResult(clientSocket,
                            false,
                            "",
                            "",
                            "",
                            "",
                            "检查好友关系失败");
        return;
    }

    if (checkQuery.next()) {
        sendAddFriendResult(clientSocket,
                            false,
                            friendId,
                            friendName,
                            avatarPath,
                            privateConversationIdForUsers(userId, friendId),
                            "已经是好友");
        return;
    }

    if (!ensureFriendship(userId, friendId)) {
        sendAddFriendResult(clientSocket,
                            false,
                            "",
                            "",
                            "",
                            "",
                            "添加好友失败");
        return;
    }

    if (!ensurePrivateConversation(userId, friendId)) {
        sendAddFriendResult(clientSocket,
                            false,
                            "",
                            "",
                            "",
                            "",
                            "创建私聊会话失败");
        return;
    }

    QString conversationId = privateConversationIdForUsers(userId, friendId);

    qDebug() << "Add friend success:"
             << userId
             << "->"
             << friendId
             << friendName;

    sendAddFriendResult(clientSocket,
                        true,
                        friendId,
                        friendName,
                        avatarPath,
                        conversationId);
    
    notifyContactsUpdated(userId);
    notifyContactsUpdated(friendId);
}

void ChatServer::sendAddFriendResult(QTcpSocket *clientSocket,
                                     bool success,
                                     const QString& friendId,
                                     const QString& friendName,
                                     const QString& avatarPath,
                                     const QString& conversationId,
                                     const QString& errorText)
{
    if (!clientSocket ||
        clientSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonObject response;
    response["type"] = "add_friend_result";
    response["success"] = success;

    if (success) {
        response["friend_id"] = friendId;
        response["friend_name"] = friendName;
        response["avatar_path"] = avatarPath;
        response["conversation_id"] = conversationId;
    } else {
        response["error"] = errorText;

        if (!friendId.isEmpty()) {
            response["friend_id"] = friendId;
        }

        if (!friendName.isEmpty()) {
            response["friend_name"] = friendName;
        }

        if (!conversationId.isEmpty()) {
            response["conversation_id"] = conversationId;
        }
    }

    QJsonDocument doc(response);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');

    clientSocket->write(data);
    clientSocket->flush();

    qDebug() << "Sent add friend result:" << response;
}

bool ChatServer::ensureFriendship(const QString& userId,
                                  const QString& friendId)
{
    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    QString firstFriendshipId =
        "friend_" + userId + "_" + friendId;

    QString secondFriendshipId =
        "friend_" + friendId + "_" + userId;

    QSqlQuery query(m_db);

    query.prepare(
        "INSERT OR IGNORE INTO friendships "
        "(id, user_id, friend_id, created_at) "
        "VALUES (?, ?, ?, ?)"
    );

    query.addBindValue(firstFriendshipId);
    query.addBindValue(userId);
    query.addBindValue(friendId);
    query.addBindValue(now);

    if (!query.exec()) {
        qDebug() << "Failed to insert friendship:"
                 << query.lastError().text();
        return false;
    }

    query.prepare(
        "INSERT OR IGNORE INTO friendships "
        "(id, user_id, friend_id, created_at) "
        "VALUES (?, ?, ?, ?)"
    );

    query.addBindValue(secondFriendshipId);
    query.addBindValue(friendId);
    query.addBindValue(userId);
    query.addBindValue(now);

    if (!query.exec()) {
        qDebug() << "Failed to insert reverse friendship:"
                 << query.lastError().text();
        return false;
    }

    return true;
}

bool ChatServer::ensurePrivateConversation(const QString& userId1,
                                           const QString& userId2)
{
    QString conversationId = privateConversationIdForUsers(userId1, userId2);
    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    QSqlQuery query(m_db);

    query.prepare(
        "INSERT OR IGNORE INTO conversations "
        "(id, type, title, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?)"
    );

    query.addBindValue(conversationId);
    query.addBindValue(1);
    query.addBindValue("");
    query.addBindValue(now);
    query.addBindValue(now);

    if (!query.exec()) {
        qDebug() << "Failed to ensure private conversation:"
                 << query.lastError().text();
        return false;
    }

    query.prepare(
        "INSERT OR IGNORE INTO conversation_members "
        "(id, conversation_id, user_id, joined_at, is_hidden) "
        "VALUES (?, ?, ?, ?, ?)"
    );

    query.addBindValue("member_" + conversationId + "_" + userId1);
    query.addBindValue(conversationId);
    query.addBindValue(userId1);
    query.addBindValue(now);
    query.addBindValue(0);

    if (!query.exec()) {
        qDebug() << "Failed to ensure conversation member:"
                 << query.lastError().text();
        return false;
    }

    query.prepare(
        "INSERT OR IGNORE INTO conversation_members "
        "(id, conversation_id, user_id, joined_at, is_hidden) "
        "VALUES (?, ?, ?, ?, ?)"
    );

    query.addBindValue("member_" + conversationId + "_" + userId2);
    query.addBindValue(conversationId);
    query.addBindValue(userId2);
    query.addBindValue(now);
    query.addBindValue(0);

    if (!query.exec()) {
        qDebug() << "Failed to ensure reverse conversation member:"
                 << query.lastError().text();
        return false;
    }

    return true;
}

void ChatServer::notifyContactsUpdated(const QString& userId)
{
    QTcpSocket *targetSocket = m_userSockets.value(userId, nullptr);

    if (!targetSocket ||
        targetSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonObject json;
    json["type"] = "contacts_updated";
    json["user_id"] = userId;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');

    targetSocket->write(data);
    targetSocket->flush();

    qDebug() << "Notified contacts updated:" << userId;
}

void ChatServer::handleSendFriendRequest(QTcpSocket *clientSocket,
                                         const QJsonObject& json)
{
    QString fromUserId = json.value("from_user_id").toString().trimmed();
    QString friendUsername = json.value("friend_username").toString().trimmed();
    QString requestMessage = json.value("message").toString().trimmed();

    if (fromUserId.isEmpty()) {
        sendFriendRequestResult(clientSocket,
                                false,
                                "",
                                "当前用户ID为空");
        return;
    }

    if (friendUsername.isEmpty()) {
        sendFriendRequestResult(clientSocket,
                                false,
                                "",
                                "好友用户名不能为空");
        return;
    }

    QSqlQuery fromUserQuery(m_db);
    fromUserQuery.prepare(
        "SELECT username FROM users WHERE id = ?"
    );
    fromUserQuery.addBindValue(fromUserId);

    if (!fromUserQuery.exec() || !fromUserQuery.next()) {
        sendFriendRequestResult(clientSocket,
                                false,
                                "",
                                "当前用户不存在");
        return;
    }

    QString fromUserName = fromUserQuery.value(0).toString();

    QSqlQuery toUserQuery(m_db);
    toUserQuery.prepare(
        "SELECT id, username FROM users WHERE username = ?"
    );
    toUserQuery.addBindValue(friendUsername);

    if (!toUserQuery.exec()) {
        qDebug() << "Failed to query target user:"
                 << toUserQuery.lastError().text();

        sendFriendRequestResult(clientSocket,
                                false,
                                "",
                                "查询用户失败");
        return;
    }

    if (!toUserQuery.next()) {
        sendFriendRequestResult(clientSocket,
                                false,
                                "",
                                "用户不存在");
        return;
    }

    QString toUserId = toUserQuery.value(0).toString();
    QString toUserName = toUserQuery.value(1).toString();

    if (toUserId == fromUserId) {
        sendFriendRequestResult(clientSocket,
                                false,
                                "",
                                "不能添加自己为好友");
        return;
    }

    QSqlQuery friendshipQuery(m_db);
    friendshipQuery.prepare(
        "SELECT id FROM friendships "
        "WHERE user_id = ? AND friend_id = ?"
    );
    friendshipQuery.addBindValue(fromUserId);
    friendshipQuery.addBindValue(toUserId);

    if (!friendshipQuery.exec()) {
        qDebug() << "Failed to check friendship:"
                 << friendshipQuery.lastError().text();

        sendFriendRequestResult(clientSocket,
                                false,
                                "",
                                "检查好友关系失败");
        return;
    }

    if (friendshipQuery.next()) {
        sendFriendRequestResult(clientSocket,
                                false,
                                "",
                                "已经是好友");
        return;
    }

    QSqlQuery requestQuery(m_db);
    requestQuery.prepare(
        "SELECT id, status FROM friend_requests "
        "WHERE from_user_id = ? AND to_user_id = ?"
    );
    requestQuery.addBindValue(fromUserId);
    requestQuery.addBindValue(toUserId);

    if (!requestQuery.exec()) {
        qDebug() << "Failed to check friend request:"
                 << requestQuery.lastError().text();

        sendFriendRequestResult(clientSocket,
                                false,
                                "",
                                "检查好友申请失败");
        return;
    }

    if (requestQuery.next()) {
        int status = requestQuery.value(1).toInt();

        if (status == 0) {
            sendFriendRequestResult(clientSocket,
                                    false,
                                    "",
                                    "好友申请已发送，等待对方处理");
            return;
        }

        // 如果之前被拒绝，可以重新发送，更新为 pending
        QString requestId = requestQuery.value(0).toString();
        QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

        QSqlQuery updateQuery(m_db);
        updateQuery.prepare(
            "UPDATE friend_requests "
            "SET status = 0, message = ?, created_at = ?, handled_at = NULL "
            "WHERE id = ?"
        );
        updateQuery.addBindValue(requestMessage);
        updateQuery.addBindValue(now);
        updateQuery.addBindValue(requestId);

        if (!updateQuery.exec()) {
            qDebug() << "Failed to update friend request:"
                     << updateQuery.lastError().text();

            sendFriendRequestResult(clientSocket,
                                    false,
                                    "",
                                    "重新发送好友申请失败");
            return;
        }

        sendFriendRequestResult(clientSocket,
                                true,
                                "好友申请已发送");

        notifyFriendRequestReceived(toUserId,
                                    requestId,
                                    fromUserId,
                                    fromUserName,
                                    requestMessage);
        return;
    }

    QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    QSqlQuery insertQuery(m_db);
    insertQuery.prepare(
        "INSERT INTO friend_requests "
        "(id, from_user_id, from_user_name, to_user_id, to_user_name, message, status, created_at, handled_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"
    );

    insertQuery.addBindValue(requestId);
    insertQuery.addBindValue(fromUserId);
    insertQuery.addBindValue(fromUserName);
    insertQuery.addBindValue(toUserId);
    insertQuery.addBindValue(toUserName);
    insertQuery.addBindValue(requestMessage);
    insertQuery.addBindValue(0);
    insertQuery.addBindValue(now);
    insertQuery.addBindValue("");

    if (!insertQuery.exec()) {
        qDebug() << "Failed to insert friend request:"
                 << insertQuery.lastError().text();

        sendFriendRequestResult(clientSocket,
                                false,
                                "",
                                "发送好友申请失败");
        return;
    }

    qDebug() << "Friend request created:"
             << requestId
             << fromUserId
             << "->"
             << toUserId;

    sendFriendRequestResult(clientSocket,
                            true,
                            "好友申请已发送");

    notifyFriendRequestReceived(toUserId,
                                requestId,
                                fromUserId,
                                fromUserName,
                                requestMessage);
}

void ChatServer::sendFriendRequestResult(QTcpSocket *clientSocket,
                                         bool success,
                                         const QString& message,
                                         const QString& errorText)
{
    if (!clientSocket ||
        clientSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonObject response;
    response["type"] = "send_friend_request_result";
    response["success"] = success;

    if (success) {
        response["message"] = message;
    } else {
        response["error"] = errorText;
    }

    QJsonDocument doc(response);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');

    clientSocket->write(data);
    clientSocket->flush();

    qDebug() << "Sent friend request result:" << response;
}

void ChatServer::notifyFriendRequestReceived(const QString& toUserId,
                                             const QString& requestId,
                                             const QString& fromUserId,
                                             const QString& fromUserName,
                                             const QString& message)
{
    QTcpSocket *targetSocket = m_userSockets.value(toUserId, nullptr);

    if (!targetSocket ||
        targetSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonObject json;
    json["type"] = "friend_request_received";
    json["request_id"] = requestId;
    json["from_user_id"] = fromUserId;
    json["from_user_name"] = fromUserName;
    json["message"] = message;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');

    targetSocket->write(data);
    targetSocket->flush();

    qDebug() << "Notified friend request received:"
             << toUserId
             << "from"
             << fromUserId;
}

void ChatServer::handleRespondFriendRequest(QTcpSocket *clientSocket,
                                            const QJsonObject& json)
{
    QString requestId = json.value("request_id").toString().trimmed();
    QString userId = json.value("user_id").toString().trimmed();
    QString action = json.value("action").toString().trimmed();

    if (requestId.isEmpty()) {
        sendRespondFriendRequestResult(clientSocket,
                                       false,
                                       action,
                                       "",
                                       "",
                                       "",
                                       "",
                                       "好友申请ID为空");
        return;
    }

    if (userId.isEmpty()) {
        sendRespondFriendRequestResult(clientSocket,
                                       false,
                                       action,
                                       "",
                                       "",
                                       "",
                                       "",
                                       "当前用户ID为空");
        return;
    }

    if (action != "accept" && action != "reject") {
        sendRespondFriendRequestResult(clientSocket,
                                       false,
                                       action,
                                       "",
                                       "",
                                       "",
                                       "",
                                       "无效的处理操作");
        return;
    }

    QSqlQuery query(m_db);

    query.prepare(
        "SELECT from_user_id, from_user_name, to_user_id, to_user_name, status "
        "FROM friend_requests "
        "WHERE id = ?"
    );

    query.addBindValue(requestId);

    if (!query.exec()) {
        qDebug() << "Failed to query friend request:"
                 << query.lastError().text();

        sendRespondFriendRequestResult(clientSocket,
                                       false,
                                       action,
                                       "",
                                       "",
                                       "",
                                       "",
                                       "查询好友申请失败");
        return;
    }

    if (!query.next()) {
        sendRespondFriendRequestResult(clientSocket,
                                       false,
                                       action,
                                       "",
                                       "",
                                       "",
                                       "",
                                       "好友申请不存在");
        return;
    }

    QString fromUserId = query.value(0).toString();
    QString fromUserName = query.value(1).toString();
    QString toUserId = query.value(2).toString();
    QString toUserName = query.value(3).toString();
    int status = query.value(4).toInt();

    if (toUserId != userId) {
        sendRespondFriendRequestResult(clientSocket,
                                       false,
                                       action,
                                       "",
                                       "",
                                       "",
                                       "",
                                       "你无权处理该好友申请");
        return;
    }

    if (status != 0) {
        sendRespondFriendRequestResult(clientSocket,
                                       false,
                                       action,
                                       "",
                                       "",
                                       "",
                                       "",
                                       "该好友申请已处理");
        return;
    }

    int newStatus = (action == "accept") ? 1 : 2;
    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    QSqlQuery updateQuery(m_db);

    updateQuery.prepare(
        "UPDATE friend_requests "
        "SET status = ?, handled_at = ? "
        "WHERE id = ?"
    );

    updateQuery.addBindValue(newStatus);
    updateQuery.addBindValue(now);
    updateQuery.addBindValue(requestId);

    if (!updateQuery.exec()) {
        qDebug() << "Failed to update friend request:"
                 << updateQuery.lastError().text();

        sendRespondFriendRequestResult(clientSocket,
                                       false,
                                       action,
                                       "",
                                       "",
                                       "",
                                       "",
                                       "更新好友申请状态失败");
        return;
    }

    QString conversationId;

    if (action == "accept") {
        if (!ensureFriendship(fromUserId, toUserId)) {
            sendRespondFriendRequestResult(clientSocket,
                                           false,
                                           action,
                                           "",
                                           "",
                                           "",
                                           "",
                                           "建立好友关系失败");
            return;
        }

        if (!ensurePrivateConversation(fromUserId, toUserId)) {
            sendRespondFriendRequestResult(clientSocket,
                                           false,
                                           action,
                                           "",
                                           "",
                                           "",
                                           "",
                                           "创建私聊会话失败");
            return;
        }

        conversationId = privateConversationIdForUsers(fromUserId, toUserId);
    }

    qDebug() << "Friend request responded:"
             << requestId
             << "action:"
             << action;

    if (action == "accept") {
        // 当前处理申请的人是 toUserId，所以它的新好友是 fromUserId
        sendRespondFriendRequestResult(clientSocket,
                                       true,
                                       action,
                                       fromUserId,
                                       fromUserName,
                                       "",
                                       conversationId,
                                       "");

        notifyContactsUpdated(fromUserId);
        notifyContactsUpdated(toUserId);
    } else {
        sendRespondFriendRequestResult(clientSocket,
                                       true,
                                       action,
                                       "",
                                       "",
                                       "",
                                       "",
                                       "");
    }
}

void ChatServer::sendRespondFriendRequestResult(QTcpSocket *clientSocket,
                                                bool success,
                                                const QString& action,
                                                const QString& friendId,
                                                const QString& friendName,
                                                const QString& avatarPath,
                                                const QString& conversationId,
                                                const QString& errorText)
{
    if (!clientSocket ||
        clientSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonObject response;
    response["type"] = "respond_friend_request_result";
    response["success"] = success;
    response["action"] = action;

    if (success) {
        response["friend_id"] = friendId;
        response["friend_name"] = friendName;
        response["avatar_path"] = avatarPath;
        response["conversation_id"] = conversationId;
    } else {
        response["error"] = errorText;
    }

    QJsonDocument doc(response);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');

    clientSocket->write(data);
    clientSocket->flush();

    qDebug() << "Sent respond friend request result:" << response;
}