#include "Contact.h"

Contact::Contact()
{
}

Contact::Contact(const QString& userId,
                 const QString& userName,
                 const QString& avatarPath)
    : m_userId(userId),
      m_userName(userName),
      m_avatarPath(avatarPath)
{
}

QString Contact::userId() const
{
    return m_userId;
}

QString Contact::userName() const
{
    return m_userName;
}

QString Contact::avatarPath() const
{
    return m_avatarPath;
}

void Contact::setUserId(const QString& userId)
{
    m_userId = userId;
}

void Contact::setUserName(const QString& userName)
{
    m_userName = userName;
}

void Contact::setAvatarPath(const QString& avatarPath)
{
    m_avatarPath = avatarPath;
}