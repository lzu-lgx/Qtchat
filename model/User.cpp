#include "user.h"

User::User()
{
}

User::User(const QString& id,
           const QString& username,
           const QString& nickname,
           const QString& avatarPath)
    : m_id(id),
      m_username(username),
      m_nickname(nickname),
      m_avatarPath(avatarPath)
{
}

QString User::id() const
{
    return m_id;
}

QString User::username() const
{
    return m_username;
}

QString User::nickname() const
{
    return m_nickname;
}

QString User::avatarPath() const
{
    return m_avatarPath;
}

void User::setNickname(const QString& nickname)
{
    m_nickname = nickname;
}

void User::setAvatarPath(const QString& avatarPath)
{
    m_avatarPath = avatarPath;
}