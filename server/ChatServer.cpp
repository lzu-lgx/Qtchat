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

    if (type == "client_register") {
        handleClientRegister(clientSocket, json);
        return;
    }

    if (type == "get_contacts") {
        handleGetContacts(clientSocket, json);
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