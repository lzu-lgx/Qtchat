#include "databasemanager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QDateTime>

DatabaseManager::DatabaseManager()
{
}

bool DatabaseManager::openDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("chat.db");

    if (!db.open()) {
        qDebug() << "Failed to open database:" << db.lastError().text();
        return false;
    }

    qDebug() << "Database opened successfully";
    return true;
}

bool DatabaseManager::initTables()
{
    QSqlQuery query;

    QString createConversationsTable = R"(
        CREATE TABLE IF NOT EXISTS conversations (
            id TEXT PRIMARY KEY,
            title TEXT NOT NULL,
            avatar_path TEXT,
            type INTEGER NOT NULL,
            last_message TEXT,
            updated_at TEXT NOT NULL
        )
    )";

    if (!query.exec(createConversationsTable)) {
        qDebug() << "Failed to create conversations table:"
                 << query.lastError().text();
        return false;
    }

    QString createMessagesTable = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id TEXT PRIMARY KEY,
            conversation_id TEXT NOT NULL,
            sender_id TEXT NOT NULL,
            content TEXT NOT NULL,
            type INTEGER NOT NULL,
            timestamp TEXT NOT NULL,
            FOREIGN KEY (conversation_id) REFERENCES conversations(id)
        )
    )";

    if (!query.exec(createMessagesTable)) {
        qDebug() << "Failed to create messages table:"
                 << query.lastError().text();
        return false;
    }

    qDebug() << "Tables initialized successfully";
    return true;
}

bool DatabaseManager::saveConversation(const Conversation& conversation)
{
    QSqlQuery query;

    query.prepare(R"(
        INSERT OR REPLACE INTO conversations
        (id, title, avatar_path, type, last_message, updated_at)
        VALUES
        (:id, :title, :avatar_path, :type, :last_message, :updated_at)
    )");

    query.bindValue(":id", conversation.id());
    query.bindValue(":title", conversation.title());
    query.bindValue(":avatar_path", conversation.avatarPath());
    query.bindValue(":type", static_cast<int>(conversation.type()));
    query.bindValue(":last_message", conversation.lastMessage());
    query.bindValue(":updated_at", conversation.updatedAt().toString(Qt::ISODate));

    if (!query.exec()) {
        qDebug() << "Failed to save conversation:"
                 << query.lastError().text();
        return false;
    }

    return true;
}

QList<Conversation> DatabaseManager::loadConversations()
{
    QList<Conversation> conversations;

    QSqlQuery query;

    QString sql = R"(
        SELECT id, title, avatar_path, type, last_message, updated_at
        FROM conversations
        ORDER BY updated_at DESC
    )";

    if (!query.exec(sql)) {
        qDebug() << "Failed to load conversations:"
                 << query.lastError().text();
        return conversations;
    }

    while (query.next()) {
        QString id = query.value("id").toString();
        QString title = query.value("title").toString();
        QString avatarPath = query.value("avatar_path").toString();

        int typeValue = query.value("type").toInt();
        Conversation::Type type = static_cast<Conversation::Type>(typeValue);

        QString lastMessage = query.value("last_message").toString();
        QDateTime updatedAt = QDateTime::fromString(
            query.value("updated_at").toString(),
            Qt::ISODate
        );

        Conversation conversation(
            id,
            title,
            avatarPath,
            type,
            lastMessage,
            updatedAt
        );

        conversations.append(conversation);
    }

    return conversations;
}

bool DatabaseManager::saveMessage(const Message& message)
{
    QSqlQuery query;

    query.prepare(R"(
        INSERT OR REPLACE INTO messages
        (id, conversation_id, sender_id, content, type, timestamp)
        VALUES
        (:id, :conversation_id, :sender_id, :content, :type, :timestamp)
    )");

    query.bindValue(":id", message.id());
    query.bindValue(":conversation_id", message.conversationId());
    query.bindValue(":sender_id", message.senderId());
    query.bindValue(":content", message.content());
    query.bindValue(":type", static_cast<int>(message.type()));
    query.bindValue(":timestamp", message.timestamp().toString(Qt::ISODate));

    if (!query.exec()) {
        qDebug() << "Failed to save message:"
                 << query.lastError().text();
        return false;
    }

    return true;
}

QList<Message> DatabaseManager::loadMessages(const QString& conversationId)
{
    QList<Message> messages;

    QSqlQuery query;

    query.prepare(R"(
        SELECT id, sender_id, conversation_id, content, type, timestamp
        FROM messages
        WHERE conversation_id = :conversation_id
        ORDER BY timestamp ASC
    )");

    query.bindValue(":conversation_id", conversationId);

    if (!query.exec()) {
        qDebug() << "Failed to load messages:"
                 << query.lastError().text();
        return messages;
    }

    while (query.next()) {
        QString id = query.value("id").toString();
        QString senderId = query.value("sender_id").toString();
        QString convId = query.value("conversation_id").toString();
        QString content = query.value("content").toString();
        QDateTime timestamp = QDateTime::fromString(query.value("timestamp").toString(),Qt::ISODate);

        int typeValue = query.value("type").toInt();
        Message::Type type = static_cast<Message::Type>(typeValue);

        Message message(id,
                senderId,
                convId,
                content,
                type,
                timestamp);

        messages.append(message);
    }

    return messages;
}

bool DatabaseManager::updateConversationLastMessage(const QString& conversationId,
                                                    const QString& lastMessage,
                                                    const QDateTime& updatedAt)
{
    QSqlQuery query;

    query.prepare(R"(
        UPDATE conversations
        SET last_message = :last_message,
            updated_at = :updated_at
        WHERE id = :id
    )");

    query.bindValue(":last_message", lastMessage);
    query.bindValue(":updated_at", updatedAt.toString(Qt::ISODate));
    query.bindValue(":id", conversationId);

    if (!query.exec()) {
        qDebug() << "Failed to update conversation last message:"
                 << query.lastError().text();
        return false;
    }

    return true;
}