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
        while (m_socket->canReadLine()) {
            QByteArray line = m_socket->readLine().trimmed();

            if (line.isEmpty()) {
                continue;
            }

            qDebug() << "Received raw message from server:" << line;

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);

            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                QString text = QString::fromUtf8(line);
                qDebug() << "Received non-json message from server:" << text;
                emit messageReceived(text);
                continue;
            }

            QJsonObject obj = doc.object();
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

    qDebug() << "Sent JSON message to server:" << data;
}

void NetworkClient::registerClient(const QString& userId, const QString& userName)
{
    QJsonObject json;
    json["type"] = "client_register";
    json["user_id"] = userId;
    json["user_name"] = userName;

    sendJsonMessage(json);
}