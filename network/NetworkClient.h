#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QJsonObject>

class NetworkClient : public QObject
{
    Q_OBJECT

public:
    explicit NetworkClient(QObject *parent = nullptr);

    void connectToServer(const QString& host, quint16 port);
    bool isConnected() const;
    void sendTextMessage(const QString& text);
    void sendJsonMessage(const QJsonObject& message);
    void registerClient(const QString& userId, const QString& userName);
    void requestContacts(const QString& userId);
    void login(const QString& username, const QString& password);

signals:
    void connected();
    void disconnected();
    void connectionError(const QString& errorText);
    void messageReceived(const QString& message);
    void jsonMessageReceived(const QJsonObject& message);
    void loginResult(bool success,const QString& userId,const QString& userName,const QString& databaseName,const QString& errorText);

private:
    QTcpSocket *m_socket;
};

#endif // NETWORKCLIENT_H