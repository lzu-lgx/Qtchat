#include "message.h"

Message::Message()
    : m_type(Type::Text),
      m_timestamp(QDateTime::currentDateTime())
{
}

Message::Message(const QString& id,
                 const QString& senderId,
                 const QString& conversationId,
                 const QString& content,
                 Type type)
    : m_id(id),
      m_senderId(senderId),
      m_conversationId(conversationId),
      m_content(content),
      m_type(type),
      m_timestamp(QDateTime::currentDateTime())
{
}
Message::Message(const QString& id,
                 const QString& senderId,
                 const QString& conversationId,
                 const QString& content,
                 Type type,
                 const QDateTime& timestamp)
    : m_id(id),
      m_senderId(senderId),
      m_conversationId(conversationId),
      m_content(content),
      m_type(type),
      m_timestamp(timestamp)
{
}

QString Message::id() const
{
    return m_id;
}

QString Message::senderId() const
{
    return m_senderId;
}

QString Message::conversationId() const
{
    return m_conversationId;
}

QString Message::content() const
{
    return m_content;
}

Message::Type Message::type() const
{
    return m_type;
}

QDateTime Message::timestamp() const
{
    return m_timestamp;
}