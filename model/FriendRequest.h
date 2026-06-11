#ifndef FRIENDREQUEST_H
#define FRIENDREQUEST_H

#include <QString>

class FriendRequest
{
public:
    enum Status
    {
        Pending = 0,
        Accepted = 1,
        Rejected = 2
    };

    FriendRequest()
        : status(Pending),
          outgoing(false)
    {
    }

    FriendRequest(const QString& requestId,
                  const QString& fromUserId,
                  const QString& fromUserName,
                  const QString& message,
                  int status,
                  const QString& createdAt,
                  bool outgoing = false,
                  const QString& toUserId = QString(),
                  const QString& toUserName = QString())
        : requestId(requestId),
          fromUserId(fromUserId),
          fromUserName(fromUserName),
          toUserId(toUserId),
          toUserName(toUserName),
          message(message),
          status(status),
          createdAt(createdAt),
          outgoing(outgoing)
    {
    }

    QString requestId;

    QString fromUserId;
    QString fromUserName;

    QString toUserId;
    QString toUserName;

    QString message;
    int status;
    QString createdAt;

    // true 表示“我发出去的好友申请”
    // false 表示“别人发给我的好友申请”
    bool outgoing;
};

#endif // FRIENDREQUEST_H