#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>

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

private:
    QTcpServer *m_server;
    QList<QTcpSocket*> m_clients;
};

#endif // CHATSERVER_H