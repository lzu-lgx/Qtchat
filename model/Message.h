#ifndef MESSAGE_H
#define MESSAGE_H

#include <QString>
#include <QDateTime>

class Message
{
public:
    enum class Type
    {
        Text,
        Image,
        File
    };

    Message();

    Message(const QString& id,
            const QString& senderId,
            const QString& conversationId,
            const QString& content,
            Type type);

    QString id() const;
    QString senderId() const;
    QString conversationId() const;
    QString content() const;
    Type type() const;
    QDateTime timestamp() const;

private:
    QString m_id;
    QString m_senderId;
    QString m_conversationId;
    QString m_content;
    Type m_type;
    QDateTime m_timestamp;
};

#endif // MESSAGE_H