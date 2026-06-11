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
    void handleRegister(QTcpSocket *clientSocket, const QJsonObject& json);

    void sendRegisterResult(QTcpSocket *clientSocket,bool success,const QString& userId,const QString& userName,const QString& errorText = QString());

    QString generateNextUserId();
    void sendLoginResult(QTcpSocket *clientSocket,bool success,const QString& userId,const QString& userName,const QString& errorText = QString());
    
    void handleClientRegister(QTcpSocket *clientSocket, const QJsonObject& json);
    
    void handleGetContacts(QTcpSocket *clientSocket, const QJsonObject& json);
    void sendContactsResult(QTcpSocket *clientSocket,const QString& userId);
    void handleChatMessage(const QJsonObject& json);

    bool saveChatMessage(QJsonObject& json);
    bool forwardChatMessage(const QJsonObject& json);
    void deliverOfflineMessages(const QString& userId);
    void markMessageDelivered(const QString& messageId);
    void handleAddFriend(QTcpSocket *clientSocket, const QJsonObject& json);

    void sendAddFriendResult(QTcpSocket *clientSocket,bool success,const QString& friendId,const QString& friendName,const QString& avatarPath,const QString& conversationId,
        const QString& errorText = QString());

    bool ensureFriendship(const QString& userId,const QString& friendId);

    bool ensurePrivateConversation(const QString& userId1,const QString& userId2);
    void notifyContactsUpdated(const QString& userId);
    void handleSendFriendRequest(QTcpSocket *clientSocket,const QJsonObject& json);

    void sendFriendRequestResult(QTcpSocket *clientSocket,bool success,const QString& message,const QString& errorText = QString(),
            const QString& requestId = QString(),const QString& toUserId = QString(),
            const QString& toUserName = QString());
        
    void notifyFriendRequestReceived(const QString& toUserId,const QString& requestId,const QString& fromUserId,const QString& fromUserName,const QString& message);

    void handleRespondFriendRequest(QTcpSocket *clientSocket,const QJsonObject& json);

    void sendRespondFriendRequestResult(QTcpSocket *clientSocket,bool success,const QString& action,const QString& friendId = QString(),const QString& friendName = QString(),
        const QString& avatarPath = QString(),const QString& conversationId = QString(),const QString& errorText = QString());
    
    void handleGetFriendRequests(QTcpSocket *clientSocket,const QJsonObject& json);

    void sendFriendRequestsResult(QTcpSocket *clientSocket,const QString& userId);

    void notifyFriendRequestStatusUpdated(const QString& userId,const QString& requestId,int status,
            const QString& action,const QString& friendId,const QString& friendName,const QString& conversationId);
    QTcpServer *m_server;
    QList<QTcpSocket*> m_clients;
    QHash<QString, QTcpSocket*> m_userSockets;

    QSqlDatabase m_db;
};

#endif // CHATSERVER_H