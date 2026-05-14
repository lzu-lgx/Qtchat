#include "conversation.h"

Conversation::Conversation()
{
}

Conversation::Conversation(const QString& id,
                           const QString& title,
                           const QString& avatarPath)
    : m_id(id),
      m_title(title),
      m_avatarPath(avatarPath)
{
}

QString Conversation::id() const
{
    return m_id;
}

QString Conversation::title() const
{
    return m_title;
}

QString Conversation::avatarPath() const
{
    return m_avatarPath;
}

QString Conversation::lastMessage() const
{
    return m_lastMessage;
}

QDateTime Conversation::lastMessageTime() const
{
    return m_lastMessageTime;
}

void Conversation::setLastMessage(const QString& message)
{
    m_lastMessage = message;
}

void Conversation::setLastMessageTime(const QDateTime& time)
{
    m_lastMessageTime = time;
}