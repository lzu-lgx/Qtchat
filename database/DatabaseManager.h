#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include "../model/Conversation.h"
#include "../model/Message.h"
#include <QList>

class DatabaseManager
{
public:
    DatabaseManager();

    bool openDatabase();
    bool initTables();

    bool saveConversation(const Conversation& conversation);
    QList<Conversation> loadConversations();

    bool saveMessage(const Message& message);
    QList<Message> loadMessages(const QString& conversationId);

    bool updateConversationLastMessage(const QString& conversationId,
                                   const QString& lastMessage,
                                   const QDateTime& updatedAt);
};

#endif // DATABASEMANAGER_H