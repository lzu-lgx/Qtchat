#ifndef CONVERSATION_H
#define CONVERSATION_H

#include "User.h"
#include "Message.h"

#include <QString>
#include <QVector>
#include <QDateTime>

class Conversation
{
public:
    Conversation();

    Conversation(const QString& id,
                 const QString& title,
                 const QString& avatarPath);

    QString id() const;
    QString title() const;
    QString avatarPath() const;

    QString lastMessage() const;
    QDateTime lastMessageTime() const;

    void setLastMessage(const QString& message);
    void setLastMessageTime(const QDateTime& time);

private:
    QString m_id;
    QString m_title;
    QString m_avatarPath;
    QString m_lastMessage;
    QDateTime m_lastMessageTime;
};

#endif // CONVERSATION_H