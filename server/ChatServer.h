#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QHash>
#include <QString>
#include <QJsonObject>
#include <QSqlDatabase>

class ChatServer : public QObject
{
    Q_OBJECT

public:
    explicit ChatServer(QObject *parent = nullptr);

    bool startServer(quint16 port);

private slots:
    void handleNewConnection();
    void handleReadyRead();

private:
    bool initDatabase();
    bool initDefaultData();
    QString privateConversationIdForUsers(const QString& userId1,const QString& userId2) const;

    void handleJsonMessage(QTcpSocket *clientSocket, const QJsonObject& json);
    
    void handleLogin(QTcpSocket *clientSocket, const QJsonObject& json);
    void sendLoginResult(QTcpSocket *clientSocket,bool success,const QString& userId,const QString& userName,const QString& errorText = QString());
    
    void handleClientRegister(QTcpSocket *clientSocket, const QJsonObject& json);
    
    void handleGetContacts(QTcpSocket *clientSocket, const QJsonObject& json);
    void sendContactsResult(QTcpSocket *clientSocket,const QString& userId);
    void handleChatMessage(const QJsonObject& json);

    bool saveChatMessage(QJsonObject& json);
    bool forwardChatMessage(const QJsonObject& json);
    void deliverOfflineMessages(const QString& userId);
    void markMessageDelivered(const QString& messageId);

    QTcpServer *m_server;
    QList<QTcpSocket*> m_clients;
    QHash<QString, QTcpSocket*> m_userSockets;

    QSqlDatabase m_db;
};

#endif // CHATSERVER_H