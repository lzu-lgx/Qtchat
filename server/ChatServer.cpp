#include "ChatServer.h"

#include <QDebug>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>

ChatServer::ChatServer(QObject *parent)
    : QObject(parent),
      m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection,
            this, &ChatServer::handleNewConnection);
}

bool ChatServer::startServer(quint16 port)
{
    if (!m_server->listen(QHostAddress::Any, port)) {
        qDebug() << "Failed to start server:" << m_server->errorString();
        return false;
    }

    qDebug() << "ChatServer started on port" << port;
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

        for (auto it = m_userSockets.begin(); it != m_userSockets.end(); ++it) 
        {
            if (it.value() == clientSocket) {
                userIdToRemove = it.key();
                break;
            }
        }

        if (!userIdToRemove.isEmpty()) 
        {
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

        QJsonObject json = doc.object();

        handleJsonMessage(clientSocket, json);
    }
}

void ChatServer::broadcastMessage(QTcpSocket *senderSocket, const QByteArray& data)
{
    for (QTcpSocket *client : m_clients) {
        if (client == senderSocket) {
            continue;
        }

        if (client->state() == QAbstractSocket::ConnectedState) {
            client->write(data);
            client->flush();
        }
    }

    qDebug() << "Broadcast message to other clients";
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
        forwardChatMessage(json);
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
}

void ChatServer::forwardChatMessage(const QJsonObject& json)
{
    QString receiverId = json.value("receiver_id").toString();

    if (receiverId.isEmpty()) {
        qDebug() << "Cannot forward chat message: empty receiver_id";
        return;
    }

    QTcpSocket *targetSocket = m_userSockets.value(receiverId, nullptr);

    if (!targetSocket ||
        targetSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Receiver not online:" << receiverId;
        return;
    }

    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append('\n');

    targetSocket->write(data);
    targetSocket->flush();

    qDebug() << "Forwarded chat message to:" << receiverId;
}