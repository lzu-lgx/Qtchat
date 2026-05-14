#ifndef USER_H
#define USER_H

#include<Qstring>

class User
{
public:
    User();
    User(const QString& id,
         const QString& username,
         const QString& nickname,
         const QString& avatarPath);

    QString id() const;
    QString username() const;
    QString nickname() const;
    QString avatarPath() const;

    void setNickname(const QString& nickname);
    void setAvatarPath(const QString& avatarPath);

private:
    QString m_id;
    QString m_username;
    QString m_nickname;
    QString m_avatarPath;
};

#endif