#include "conversation.h"

Conversation::Conversation()
    : m_type(Type::PrivateChat),
      m_updatedAt(QDateTime::currentDateTime())
{
}

Conversation::Conversation(const QString& id,
                           const QString& title,
                           const QString& avatarPath,
                           Type type,
                           const QString& lastMessage,
                           const QDateTime& updatedAt)
    : m_id(id),
      m_title(title),
      m_avatarPath(avatarPath),
      m_type(type),
      m_lastMessage(lastMessage),
      m_updatedAt(updatedAt)
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

Conversation::Type Conversation::type() const
{
    return m_type;
}

QString Conversation::lastMessage() const
{
    return m_lastMessage;
}

QDateTime Conversation::updatedAt() const
{
    return m_updatedAt;
}

void Conversation::setLastMessage(const QString& message)
{
    m_lastMessage = message;
}

void Conversation::setUpdatedAt(const QDateTime& time)
{
    m_updatedAt = time;
}

