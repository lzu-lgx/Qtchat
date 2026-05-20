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
    enum class Type
    {
        PrivateChat,
        AiAssistant,
        GroupChat
    };

    Conversation();

    Conversation(const QString& id,
                 const QString& title,
                 const QString& avatarPath,
                 Type type,
                 const QString& lastMessage,
                 const QDateTime& updatedAt);
    
    
    QString id() const;
    QString title() const;
    QString avatarPath() const;
    Type type() const;
    QString lastMessage() const;
    QDateTime updatedAt() const;


    void setLastMessage(const QString& message);
    void setUpdatedAt(const QDateTime& time);

private:
    QString m_id;
    QString m_title;
    QString m_avatarPath;
    Type m_type;
    QString m_lastMessage;
    QDateTime m_updatedAt;
    
};

#endif // CONVERSATION_H