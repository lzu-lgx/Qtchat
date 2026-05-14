#ifndef MOCKDATA_H
#define MOCKDATA_H

#include <QList>

#include "../model/User.h"
#include "../model/Conversation.h"
#include "../model/Message.h"

class MockData
{
public:
    static QList<User> createUsers();
    static QList<Conversation> createConversations();
    static QList<Message> createMessages();
};

#endif // MOCKDATA_H