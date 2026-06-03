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

    QString sql =
        "CREATE TABLE IF NOT EXISTS server_messages ("
        "id TEXT PRIMARY KEY,"
        "sender_id TEXT NOT NULL,"
        "sender_name TEXT,"
        "receiver_id TEXT NOT NULL,"
        "receiver_name TEXT,"
        "content TEXT NOT NULL,"
        "timestamp TEXT NOT NULL,"
        "delivered INTEGER NOT NULL DEFAULT 0"
        ")";

    if (!query.exec(sql)) {
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

    if (type == "client_register") {
        handleClientRegister(clientSocket, json);
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

bool ChatServer::saveChatMessage(QJsonObject& json)
{
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
        "(id, sender_id, sender_name, receiver_id, receiver_name, content, timestamp, delivered) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
    );

    query.addBindValue(messageId);
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
             << "from" << senderId
             << "to" << receiverId;

    return true;
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

void ChatServer::deliverOfflineMessages(const QString& userId)
{
    QTcpSocket *targetSocket = m_userSockets.value(userId, nullptr);

    if (!targetSocket ||
        targetSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QSqlQuery query(m_db);

    query.prepare(
        "SELECT id, sender_id, sender_name, receiver_id, receiver_name, content, timestamp "
        "FROM server_messages "
        "WHERE receiver_id = ? AND delivered = 0 "
        "ORDER BY timestamp ASC"
    );

    query.addBindValue(userId);

    if (!query.exec()) {
        qDebug() << "Failed to load offline messages:" << query.lastError().text();
        return;
    }

    int count = 0;

    while (query.next()) {
        QString messageId = query.value(0).toString();

        QJsonObject json;
        json["type"] = "chat_message";
        json["message_id"] = messageId;
        json["sender_id"] = query.value(1).toString();
        json["sender_name"] = query.value(2).toString();
        json["receiver_id"] = query.value(3).toString();
        json["receiver_name"] = query.value(4).toString();
        json["content"] = query.value(5).toString();
        json["timestamp"] = query.value(6).toString();

        QJsonDocument doc(json);
        QByteArray data = doc.toJson(QJsonDocument::Compact);
        data.append('\n');

        targetSocket->write(data);
        targetSocket->flush();

        markMessageDelivered(messageId);
        count++;
    }

    if (count > 0) {
        qDebug() << "Delivered offline messages to"
                 << userId
                 << "count:" << count;
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