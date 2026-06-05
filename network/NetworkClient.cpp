#include "NetworkClient.h"

#include <QDebug>
#include <QAbstractSocket>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent),
      m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected,
        this, [this]()
    {
        qDebug() << "Connected to ChatServer";
        emit connected();
    });

    connect(m_socket, &QTcpSocket::disconnected,
            this, [this]()
    {
        qDebug() << "Disconnected from ChatServer";
        emit disconnected();
    });

    connect(m_socket, &QTcpSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError)
    {
        QString errorText = m_socket->errorString();

        qDebug() << "Network error:" << errorText;
        emit connectionError(errorText);
    });

    connect(m_socket, &QTcpSocket::readyRead,
        this, [this]()
    {
        while (m_socket->canReadLine())
        {
            QByteArray line = m_socket->readLine().trimmed();

            if (line.isEmpty()) {
                continue;
            }

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);

            if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            {
                QString text = QString::fromUtf8(line);
                emit messageReceived(text);
                continue;
            }

            QJsonObject obj = doc.object();
            QString type = obj.value("type").toString();

            if (type == "login_result")
            {
                bool success = obj.value("success").toBool();
                QString userId = obj.value("user_id").toString();
                QString userName = obj.value("user_name").toString();
                QString databaseName = obj.value("database_name").toString();
                QString errorText = obj.value("error").toString();

                emit loginResult(success, userId, userName, databaseName, errorText);
                continue;
            }

            if (type == "register_result") 
            {
                bool success = obj.value("success").toBool();
                QString userId = obj.value("user_id").toString();
                QString userName = obj.value("user_name").toString();
                QString errorText = obj.value("error").toString();

                emit registerResult(success, userId, userName, errorText);
                continue;
            }

            emit jsonMessageReceived(obj);
        }
    });
}

void NetworkClient::connectToServer(const QString& host, quint16 port)
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "Socket is already connecting or connected, state:"
                 << m_socket->state();
        return;
    }

    qDebug() << "Connecting to server:" << host << port;

    m_socket->connectToHost(host, port);
}

bool NetworkClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void NetworkClient::sendTextMessage(const QString& text)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Cannot send message: not connected to server";
        emit connectionError("未连接到服务器，无法发送消息");
        return;
    }

    QByteArray data = text.toUtf8();
    m_socket->write(data);
    m_socket->flush();

    qDebug() << "Sent message to server:" << text;
}

void NetworkClient::sendJsonMessage(const QJsonObject& message)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Cannot send JSON message: not connected to server";
        emit connectionError("未连接到服务器，无法发送消息");
        return;
    }

    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    m_socket->write(data);
    m_socket->write("\n");
    m_socket->flush();

}

void NetworkClient::registerClient(const QString& userId, const QString& userName)
{
    QJsonObject json;
    json["type"] = "client_register";
    json["user_id"] = userId;
    json["user_name"] = userName;

    sendJsonMessage(json);
}

void NetworkClient::requestContacts(const QString& userId)
{
    QJsonObject json;
    json["type"] = "get_contacts";
    json["user_id"] = userId;

    sendJsonMessage(json);
}

void NetworkClient::login(const QString& username, const QString& password)
{
    QJsonObject json;
    json["type"] = "login";
    json["username"] = username;
    json["password"] = password;

    sendJsonMessage(json);
}

void NetworkClient::registerUser(const QString& username,
                                 const QString& password)
{
    QJsonObject json;
    json["type"] = "register";
    json["username"] = username;
    json["password"] = password;

    sendJsonMessage(json);
}