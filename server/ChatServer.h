#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QHash>
#include <QString>
#include <QJsonObject>

class ChatServer : public QObject
{
    Q_OBJECT

public:
    explicit ChatServer(QObject *parent = nullptr);

    bool startServer(quint16 port);

private slots:
    void broadcastMessage(QTcpSocket *senderSocket, const QByteArray& data);
    void handleNewConnection();
    void handleReadyRead();
    void handleJsonMessage(QTcpSocket *clientSocket, const QJsonObject& json);
    void handleClientRegister(QTcpSocket *clientSocket, const QJsonObject& json);
    void forwardChatMessage(const QJsonObject& json);

private:
    QTcpServer *m_server;
    QList<QTcpSocket*> m_clients;
    QHash<QString, QTcpSocket*> m_userSockets;
};

#endif // CHATSERVER_H