#ifndef CONTACT_H
#define CONTACT_H

#include <QString>

class Contact
{
public:
    Contact();

    Contact(const QString& userId,
            const QString& userName,
            const QString& avatarPath = QString());

    QString userId() const;
    QString userName() const;
    QString avatarPath() const;

    void setUserId(const QString& userId);
    void setUserName(const QString& userName);
    void setAvatarPath(const QString& avatarPath);

private:
    QString m_userId;
    QString m_userName;
    QString m_avatarPath;
};

#endif // CONTACT_H