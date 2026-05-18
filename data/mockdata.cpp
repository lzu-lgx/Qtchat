#include "MockData.h"

QList<User> MockData::createUsers()
{
    QList<User> users;

    users.append(User("user_001",
                      "zhangsan",
                      "张三",
                      ":/avatars/zhangsan.png"));

    users.append(User("user_002",
                      "lisi",
                      "李四",
                      ":/avatars/lisi.png"));

    users.append(User("ai_assistant",
                      "ai",
                      "AI 助手",
                      ":/avatars/ai.png"));

    return users;
}

QList<Conversation> MockData::createConversations()
{
    QList<Conversation> conversations;

    QDateTime now = QDateTime::currentDateTime();

    Conversation conv1("conv_001",
                       "张三",
                       ":/avatars/zhangsan.png",
                       Conversation::Type::PrivateChat,
                       "明天几点见？",
                       now);

    Conversation conv2("conv_002",
                       "李四",
                       ":/avatars/lisi.png",
                       Conversation::Type::PrivateChat,
                       "文件我已经收到了",
                       now);

    Conversation conv3("conv_ai",
                       "AI 助手",
                       ":/avatars/ai.png",
                       Conversation::Type::AiAssistant,
                       "你好，我可以帮你学习 C++ 和 Qt",
                       now);

    conversations.append(conv1);
    conversations.append(conv2);
    conversations.append(conv3);

    return conversations;
}

QList<Message> MockData::createMessages()
{
    QList<Message> messages;

    messages.append(Message("msg_001",
                            "user_001",
                            "conv_001",
                            "你好，最近忙吗？",
                            Message::Type::Text));

    messages.append(Message("msg_002",
                            "me",
                            "conv_001",
                            "还可以，在写 Qt 聊天项目。",
                            Message::Type::Text));

    messages.append(Message("msg_003",
                            "user_001",
                            "conv_001",
                            "明天几点见？",
                            Message::Type::Text));

    messages.append(Message("msg_004",
                            "user_002",
                            "conv_002",
                            "文件我已经收到了",
                            Message::Type::Text));

    messages.append(Message("msg_005",
                            "ai_assistant",
                            "conv_ai",
                            "你好，我可以帮你学习 C++ 和 Qt",
                            Message::Type::Text));

    return messages;
}