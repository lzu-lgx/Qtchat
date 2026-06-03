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

signals:
    void connected();
    void disconnected();
    void connectionError(const QString& errorText);
    void messageReceived(const QString& message);
    void jsonMessageReceived(const QJsonObject& message);

private:
    QTcpSocket *m_socket;
};

#endif // NETWORKCLIENT_H