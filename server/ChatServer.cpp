#include "ChatServer.h"

#include <QDebug>
#include <QHostAddress>

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
        QByteArray line = clientSocket->readLine();

        if (line.trimmed().isEmpty()) {
            continue;
        }

        qDebug() << "Received message from client:"
                 << clientSocket->peerAddress().toString()
                 << clientSocket->peerPort()
                 << line.trimmed();

        broadcastMessage(clientSocket, line);
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