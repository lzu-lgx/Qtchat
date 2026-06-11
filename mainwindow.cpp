#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QListWidgetItem>
#include <QDateTime>
#include <QDebug>
#include "database/DatabaseManager.h"
#include "model/Conversation.h"
#include "model/Message.h"
#include <QTextCursor>
#include <QInputDialog>
#include <QJsonObject>
#include <QTimer>
#include <QMessageBox>
#include <QEventLoop>
#include "config/AppConfig.h"
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_conversationList(nullptr)
    , m_chatTitleLabel(nullptr)
    , m_messageDisplay(nullptr)
    , m_messageInput(nullptr)
    , m_sendButton(nullptr)
    , m_currentConversationId()
    , m_dbManager()
    , m_aiService()
    , m_networkClient()
    , m_userId()
    , m_userName()
    , m_peerId()
    , m_peerName()
    , m_conversationModeButton(nullptr)
    , m_contactsModeButton(nullptr)
    , m_leftListMode(LeftListMode::Conversations)
    , m_addFriendButton(nullptr)
{
    ui->setupUi(this);

    setupNetwork();

    if (!showLoginDialog()) {
        QTimer::singleShot(0, this, &QWidget::close);
        return;
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupChatUi()
{
    // Central widget
QWidget *central = new QWidget(this);
setCentralWidget(central);

// 主布局
QHBoxLayout *mainLayout = new QHBoxLayout(central);

// 左侧面板
QWidget *leftPanel = new QWidget(central);
leftPanel->setFixedWidth(220);
QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
leftLayout->setContentsMargins(8, 8, 8, 8);
leftLayout->setSpacing(8);

// 左侧顶部按钮
m_conversationModeButton = new QPushButton("会话列表", leftPanel);
m_contactsModeButton = new QPushButton("通讯录", leftPanel);
m_addFriendButton = new QPushButton("添加好友", leftPanel);
m_addFriendButton->setVisible(false); // 默认隐藏

// 会话/联系人列表
m_conversationList = new QListWidget(leftPanel);

// 横向按钮布局
QHBoxLayout *modeButtonLayout = new QHBoxLayout;
modeButtonLayout->setSpacing(8);
modeButtonLayout->addWidget(m_conversationModeButton);
modeButtonLayout->addWidget(m_contactsModeButton);

// 将左侧按钮和列表加入左侧垂直布局
leftLayout->addLayout(modeButtonLayout);
leftLayout->addWidget(m_addFriendButton);
leftLayout->addWidget(m_conversationList);

// 聊天右侧面板
QWidget *chatWidget = new QWidget(central);
QVBoxLayout *chatLayout = new QVBoxLayout(chatWidget);
chatLayout->setContentsMargins(0, 0, 0, 0);
chatLayout->setSpacing(8);

m_chatTitleLabel = new QLabel("请选择一个会话", chatWidget);
m_chatTitleLabel->setMinimumHeight(40);

m_messageDisplay = new QTextEdit(chatWidget);
m_messageDisplay->setReadOnly(true);

QWidget *inputWidget = new QWidget(chatWidget);
QHBoxLayout *inputLayout = new QHBoxLayout(inputWidget);
inputLayout->setContentsMargins(0, 0, 0, 0);
inputLayout->setSpacing(8);

m_messageInput = new QLineEdit(inputWidget);
m_messageInput->setPlaceholderText("请输入消息...");
m_sendButton = new QPushButton("发送", inputWidget);

inputLayout->addWidget(m_messageInput);
inputLayout->addWidget(m_sendButton);

chatLayout->addWidget(m_chatTitleLabel);
chatLayout->addWidget(m_messageDisplay);
chatLayout->addWidget(inputWidget);

// 添加到主布局
mainLayout->addWidget(leftPanel);
mainLayout->addWidget(chatWidget);

    connect(m_conversationList, &QListWidget::itemClicked,
        this, [this](QListWidgetItem *item)
    {
        if (!item)
        {
            return;
        }

        QString id = item->data(Qt::UserRole).toString();

        if (m_leftListMode == LeftListMode::Conversations)
        {
            QString conversationId = id;

            m_currentConversationId = conversationId;
            m_chatTitleLabel->setText(item->text());
            showMessagesForConversation(conversationId);
            return;
        }

        if (m_leftListMode == LeftListMode::Contacts)
        {
            QString contactId = id;
            Contact contact = contactByUserId(contactId);

            if (contact.userId().isEmpty())
            {
                return;
            }

            openConversationWithContact(contact);
            return;
        }
    });
    connect(m_sendButton, &QPushButton::clicked,
            this, [this]()
            {
                sendCurrentMessage();
            });

    connect(m_messageInput, &QLineEdit::returnPressed,
            this, [this]()
            {
                 sendCurrentMessage();
            });
    connect(m_conversationModeButton, &QPushButton::clicked,
        this, [this]()
    {
        showConversationListMode();
    });

    connect(m_contactsModeButton, &QPushButton::clicked,
        this, [this]()
    {
        showContactListMode();
    });

    connect(&m_aiService, &AiService::replyReady,
        this, [this](const QString& conversationId,
                     const QString& replyText)
    {
        Message aiMessage(
        QString::number(QDateTime::currentMSecsSinceEpoch()),
        "ai",
        conversationId,
        replyText,
        Message::Type::Text
        );

        if (!m_dbManager.saveMessage(aiMessage)) {
        return;
        }

        m_dbManager.updateConversationLastMessage(
        conversationId,
        replyText,
        aiMessage.timestamp()
        );

        loadConversations();

        if (m_currentConversationId == conversationId) {
        showMessagesForConversation(conversationId);
        }
    });
    connect(&m_aiService, &AiService::replyFailed,
        this, [this](const QString& conversationId,
                     const QString& errorText)
    {
        Message aiMessage(
        QString::number(QDateTime::currentMSecsSinceEpoch()),
        "ai",
        conversationId,
        errorText,
        Message::Type::Text
        );

        m_dbManager.saveMessage(aiMessage);

        m_dbManager.updateConversationLastMessage(
        conversationId,
        errorText,
        aiMessage.timestamp()
        );

        loadConversations();

        if (m_currentConversationId == conversationId) {
        showMessagesForConversation(conversationId);
        }
    });
    connect(m_addFriendButton, &QPushButton::clicked,
        this, [this]()
{
    handleAddFriend();
});

}



void MainWindow::loadConversations()
{
    m_conversations = m_dbManager.loadConversations();

    m_conversationList->clear();

    QListWidgetItem *itemToSelect = nullptr;

    for (const Conversation& conversation : m_conversations)
    {
        QString itemText = conversation.title()
                           + "\n"
                           + conversation.lastMessage();

        QListWidgetItem *item = new QListWidgetItem(itemText);
        item->setData(Qt::UserRole, conversation.id());

        m_conversationList->addItem(item);

        if (conversation.id() == m_currentConversationId) {
            itemToSelect = item;
        }
    }

    if (itemToSelect) {
        m_conversationList->setCurrentItem(itemToSelect);
    } else if (m_conversationList->count() > 0 && m_currentConversationId.isEmpty()) {
        QListWidgetItem *firstItem = m_conversationList->item(0);
        m_conversationList->setCurrentItem(firstItem);

        m_currentConversationId = firstItem->data(Qt::UserRole).toString();

        QString title = firstItem->text().split('\n').first();
        m_chatTitleLabel->setText(title);

        showMessagesForConversation(m_currentConversationId);
    }
}

void MainWindow::showMessagesForConversation(const QString& conversationId)
{
    m_messageDisplay->clear();

    QList<Message> messages =
        m_dbManager.loadMessages(conversationId);

    QString text;

    for (const Message& message : messages) {
        QString senderName = displayNameForSender(message.senderId());

        QString timeText =
            message.timestamp().toString("yyyy-MM-dd hh:mm:ss");

        text += senderName + "  " + timeText + "\n";
        text += message.content() + "\n\n";
    }

    m_messageDisplay->setText(text);

    m_messageDisplay->moveCursor(QTextCursor::End);
}

void MainWindow::sendCurrentMessage()
{
    QString content = m_messageInput->text().trimmed();

    if (content.isEmpty()) {
        return;
    }

    if (m_currentConversationId.isEmpty()) {
        return;
    }

    QString conversationId = m_currentConversationId;

    Message message(
        QString::number(QDateTime::currentMSecsSinceEpoch()),
        "me",
        conversationId,
        content,
        Message::Type::Text
    );

    if (!m_dbManager.saveMessage(message)) {
        return;
    }

    if (!m_dbManager.updateConversationLastMessage(
            conversationId,
            content,
            message.timestamp())) {
        return;
    }

    showMessagesForConversation(conversationId);
    loadConversations();

    m_messageInput->clear();

    if (conversationId == "conv_ai") {
        showAiThinkingMessage();
        handleAiAssistantReply(conversationId, content);
    } else {
        sendNetworkChatMessage(content);
    }
}

void MainWindow::handleAiAssistantReply(const QString& conversationId,
                                        const QString& userMessage)
{
    if (conversationId != "conv_ai") {
        return;
    }

    m_aiService.requestReply(conversationId, userMessage);
}

void MainWindow::showAiThinkingMessage()
{
    QString currentText = m_messageDisplay->toPlainText();

    if (!currentText.isEmpty()) {
        currentText += "\n";
    }

    QString timeText = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    currentText += "AI 助手  " + timeText + "\n";
    currentText += "正在思考中...\n";

    m_messageDisplay->setText(currentText);
    m_messageDisplay->moveCursor(QTextCursor::End);
}

void MainWindow::setupNetwork()
{
    connect(&m_networkClient, &NetworkClient::connected,
            this, [this]()
    {
        qDebug() << "MainWindow received connected signal";
    });

    connect(&m_networkClient, &NetworkClient::jsonMessageReceived,
            this, [this](const QJsonObject& message)
    {
        QString type = message.value("type").toString();

        if (type == "contacts_result") {
            handleContactsResult(message);
            return;
        }

        if (type == "contacts_updated") {
            qDebug() << "Contacts updated notification received";
            m_networkClient.requestContacts(m_userId);
            return;
        }

        if (type == "chat_message") {
            handleJsonNetworkMessage(message);
            return;
        }

        if (type == "friend_request_received") {
            QString requestId = message.value("request_id").toString();
            QString fromUserName = message.value("from_user_name").toString();
            QString requestMessage = message.value("message").toString();

            QMessageBox::StandardButton reply = QMessageBox::question(this,"好友申请",fromUserName + " 请求添加你为好友。\n"+ requestMessage + "\n\n是否同意？",
            QMessageBox::Yes | QMessageBox::No);

            QString action = (reply == QMessageBox::Yes) ? "accept" : "reject";

            qDebug()<< "Respond friend request:"
                    << "requestId =" << requestId
                    << "userId =" << m_userId
                    << "action =" << action;

            m_networkClient.respondFriendRequest(requestId, m_userId, action);

    // 关键修复：
    // 同意后，当前客户端主动延迟刷新通讯录。
    // 延迟是为了等服务端先完成 friendships 写入。
            if (action == "accept") {
                QTimer::singleShot(300, this, [this]() {
                    qDebug() << "Force refresh contacts after accepting friend request:"
                            << m_userId;

                    m_networkClient.requestContacts(m_userId);
                });

                QTimer::singleShot(1000, this, [this]() {
                qDebug() << "Force refresh contacts again after accepting friend request:"
                        << m_userId;

                m_networkClient.requestContacts(m_userId);
                });
            }

            return;
        }

        qDebug() << "Unsupported JSON message type in MainWindow:" << type;
    });

    connect(&m_networkClient, &NetworkClient::connectionError,
            this, [this](const QString& errorText)
    {
        qDebug() << "Network error:" << errorText;
    });

    connect(&m_networkClient, &NetworkClient::respondFriendRequestResult,
        this,
        [this](bool success,
               const QString& action,
               const QString& friendId,
               const QString& friendName,
               const QString& avatarPath,
               const QString& conversationId,
               const QString& errorText)
{
    Q_UNUSED(conversationId);

    if (!success) {
        QMessageBox::warning(this,
                             "好友申请",
                             errorText.isEmpty() ? "处理好友申请失败" : errorText);
        return;
    }

    if (action == "accept") {
        if (!friendId.isEmpty()) {
            Contact newContact(friendId, friendName, avatarPath);
            addOrUpdateContact(newContact);
        }

        QMessageBox::information(this, "好友申请", "已同意好友申请");

        // 关键：先立刻刷新当前本地列表
        if (m_leftListMode == LeftListMode::Contacts) {
            showContactListMode();
        }

        // 再向服务端拉一次权威通讯录
        m_networkClient.requestContacts(m_userId);
        return;
    }

    if (action == "reject") {
        QMessageBox::information(this, "好友申请", "已拒绝好友申请");
        return;
    }
});

    m_networkClient.connectToServer("127.0.0.1", 8888);
}


void MainWindow::loadClientConfig()
{
    m_userId = AppConfig::value("Client/UserId", "user_001");
    m_userName = AppConfig::value("Client/UserName", "用户1");
    m_peerId = AppConfig::value("Client/PeerId", "user_002");
    m_peerName = AppConfig::value("Client/PeerName", "用户2");

    setWindowTitle("Qtchat - " + m_userName + " (" + m_userId + ")");
    
    qDebug() << "Current client:"
             << m_userId
             << m_userName
             << "peer:"
             << m_peerId
             << m_peerName;
}


void MainWindow::handleJsonNetworkMessage(const QJsonObject& json)
{
    QString type = json.value("type").toString();

    if (type != "chat_message") {
        qDebug() << "Unsupported network message type:" << type;
        return;
    }

    QString senderId = json.value("sender_id").toString();
    QString senderName = json.value("sender_name").toString();
    QString receiverId = json.value("receiver_id").toString();
    QString content = json.value("content").toString();
    QString timestampText = json.value("timestamp").toString();

    if (receiverId != m_userId) {
        qDebug() << "Ignore message not for current user:";
        return;
    }

    if (senderId.isEmpty() || content.isEmpty()) {
        qDebug() << "Invalid chat message json:" << json;
        return;
    }

    if (senderName.isEmpty()) {
        senderName = senderId;
    }

    QString conversationId = json.value("conversation_id").toString();

    if (conversationId.isEmpty()) 
    {
        conversationId = conversationIdForPeer(senderId);
    }

    Conversation conversation(
        conversationId,
        senderName,
        "",
        Conversation::Type::PrivateChat,
        content,
        QDateTime::currentDateTime()
    );

    m_dbManager.saveConversation(conversation);

    QDateTime timestamp = QDateTime::fromString(timestampText, Qt::ISODate);

    if (!timestamp.isValid()) {
        timestamp = QDateTime::currentDateTime();
    }

    Message receivedMessage(
        QString::number(QDateTime::currentMSecsSinceEpoch()),
        senderId,
        conversationId,
        content,
        Message::Type::Text,
        timestamp
    );

    if (!m_dbManager.saveMessage(receivedMessage)) {
        return;
    }

    if (!m_dbManager.updateConversationLastMessage(
            conversationId,
            content,
            receivedMessage.timestamp())) {
        return;
    }

    loadConversations();

    if (m_currentConversationId == conversationId) {
        showMessagesForConversation(conversationId);
        m_chatTitleLabel->setText(senderName);
    }
}

void MainWindow::sendNetworkChatMessage(const QString& content)
{
    Contact contact = contactByConversationId(m_currentConversationId);

    if (contact.userId().isEmpty()) {
        qDebug() << "Cannot send network message: no contact for conversation"
                 << m_currentConversationId;
        return;
    }

    QJsonObject json;
    json["type"] = "chat_message";
    json["conversation_id"] = m_currentConversationId;
    json["sender_id"] = m_userId;
    json["sender_name"] = m_userName;
    json["receiver_id"] = contact.userId();
    json["receiver_name"] = contact.userName();
    json["content"] = content;
    json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    m_networkClient.sendJsonMessage(json);
}

void MainWindow::ensureAiConversation()
{
    QList<Conversation> conversations = m_dbManager.loadConversations();

    for (const Conversation& conversation : conversations) {
        if (conversation.id() == "conv_ai") {
            return;
        }
    }

    Conversation aiConversation(
        "conv_ai",
        "AI 助手",
        "",
        Conversation::Type::AiAssistant,
        "你好，我是 AI 助手",
        QDateTime::currentDateTime()
    );

    m_dbManager.saveConversation(aiConversation);
}

void MainWindow::handleContactsResult(const QJsonObject& json)
{
    QString userId = json.value("user_id").toString();

    if (userId != m_userId) {
        qDebug() << "Ignore contacts result for another user:"
                 << userId
                 << "current:" << m_userId;
        return;
    }

    QJsonArray contactsArray = json.value("contacts").toArray();

    m_contacts.clear();

    for (const QJsonValue& value : contactsArray) {
        if (!value.isObject()) {
            continue;
        }

        QJsonObject obj = value.toObject();

        QString contactId = obj.value("user_id").toString();
        QString contactName = obj.value("user_name").toString();
        QString avatarPath = obj.value("avatar_path").toString();

        if (contactId.isEmpty()) {
            continue;
        }

        if (contactName.isEmpty()) {
            contactName = contactId;
        }

        m_contacts.append(Contact(contactId, contactName, avatarPath));
    }

    qDebug() << "Contacts loaded from server:" << m_contacts.size();

    for (const Contact& contact : m_contacts) {
        qDebug() << "Contact:"
                 << contact.userId()
                 << contact.userName();
    }

    if (m_leftListMode == LeftListMode::Contacts)
    {
        showContactListMode();
    }
}

bool MainWindow::showLoginDialog()
{
    while (true) {
        LoginDialog dialog(this);

        connect(&dialog, &LoginDialog::registerRequested,
                this, [this]()
        {
            handleRegister();
        });

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        QString username = dialog.username().trimmed();
        QString password = dialog.password();

        if (username.isEmpty()) {
            QMessageBox::warning(this, "登录失败", "用户名不能为空");
            continue;
        }

        if (password.isEmpty()) {
            QMessageBox::warning(this, "登录失败", "密码不能为空");
            continue;
        }

        if (!m_networkClient.isConnected()) {
            QMessageBox::warning(this, "登录失败", "尚未连接到服务器");
            continue;
        }

        QEventLoop loop;

        bool finished = false;
        bool loginSuccess = false;
        QString loginUserId;
        QString loginUserName;
        QString loginDatabaseName;
        QString loginErrorText;

        QMetaObject::Connection connection =
            connect(&m_networkClient, &NetworkClient::loginResult,
                    &loop,
                    [&](bool success,
                        const QString& userId,
                        const QString& userName,
                        const QString& databaseName,
                        const QString& errorText)
        {
            finished = true;
            loginSuccess = success;
            loginUserId = userId;
            loginUserName = userName;
            loginDatabaseName = databaseName;
            loginErrorText = errorText;

            loop.quit();
        });

        m_networkClient.login(username, password);

        loop.exec();

        disconnect(connection);

        if (!finished) {
            QMessageBox::warning(this, "登录失败", "登录请求未完成");
            continue;
        }

        if (!loginSuccess) {
            QMessageBox::warning(this, "登录失败", loginErrorText);
            continue;
        }

        m_userId = loginUserId;
        m_userName = loginUserName;

        initializeAfterLogin(loginDatabaseName);

        return true;
    }
}

void MainWindow::initializeAfterLogin(const QString& databaseName)
{
    setWindowTitle("Qtchat - " + m_userName + " (" + m_userId + ")");

    AppConfig::setRuntimeValue("Client/DatabaseName", databaseName);

    if (m_dbManager.openDatabase()) {
        m_dbManager.initTables();
        ensureAiConversation();
    }

    setupChatUi();
    loadConversations();

    m_networkClient.registerClient(m_userId, m_userName);
    m_networkClient.requestContacts(m_userId);

    if (!m_conversations.isEmpty()) {
        m_conversationList->setCurrentRow(0);
        showMessagesForConversation(m_conversations.first().id());
        m_chatTitleLabel->setText(m_conversations.first().title());
    }
}

QString MainWindow::conversationIdForUsers(const QString& userId1,
                                           const QString& userId2) const
{
    QString first = userId1;
    QString second = userId2;

    if (first > second) {
        std::swap(first, second);
    }

    return "conv_" + first + "_" + second;
}

QString MainWindow::conversationIdForPeer(const QString& peerId) const
{
    return conversationIdForUsers(m_userId, peerId);
}

Contact MainWindow::contactByConversationId(const QString& conversationId) const
{
    for (const Contact& contact : m_contacts) {
        if (conversationIdForPeer(contact.userId()) == conversationId) {
            return contact;
        }
    }

    return Contact();
}

QString MainWindow::displayNameForSender(const QString& senderId) const
{
    if (senderId == "me") {
        return "我";
    }

    if (senderId == "ai") {
        return "AI 助手";
    }

    if (senderId == m_userId) {
        return m_userName;
    }

    for (const Contact& contact : m_contacts) {
        if (contact.userId() == senderId) {
            return contact.userName();
        }
    }

    return senderId;
}

bool MainWindow::handleRegister()
{
    RegisterDialog dialog(this);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    QString username = dialog.username();
    QString password = dialog.password();
    QString confirmPassword = dialog.confirmPassword();
    QString captcha = dialog.captcha();

    if (username.isEmpty()) {
        QMessageBox::warning(this, "注册失败", "用户名不能为空");
        return false;
    }

    if (password.isEmpty()) {
        QMessageBox::warning(this, "注册失败", "密码不能为空");
        return false;
    }

    if (password.length() < 6) {
        QMessageBox::warning(this, "注册失败", "密码长度不能少于 6 位");
        return false;
    }

    if (password != confirmPassword) {
        QMessageBox::warning(this, "注册失败", "两次输入的密码不一致");
        return false;
    }

    if (captcha != "1234") {
        QMessageBox::warning(this, "注册失败", "验证码错误");
        return false;
    }

    if (!m_networkClient.isConnected()) {
        QMessageBox::warning(this, "注册失败", "尚未连接到服务器");
        return false;
    }

    QEventLoop loop;

    bool finished = false;
    bool registerSuccess = false;
    QString registerUserId;
    QString registerUserName;
    QString registerErrorText;

    QMetaObject::Connection connection =
        connect(&m_networkClient, &NetworkClient::registerResult,
                &loop,
                [&](bool success,
                    const QString& userId,
                    const QString& userName,
                    const QString& errorText)
    {
        finished = true;
        registerSuccess = success;
        registerUserId = userId;
        registerUserName = userName;
        registerErrorText = errorText;

        loop.quit();
    });

    m_networkClient.registerUser(username, password);

    loop.exec();

    disconnect(connection);

    if (!finished) {
        QMessageBox::warning(this, "注册失败", "注册请求未完成");
        return false;
    }

    if (!registerSuccess) {
        QMessageBox::warning(this, "注册失败", registerErrorText);
        return false;
    }

    QMessageBox::information(
        this,
        "注册成功",
        "注册成功！\n"
        "用户ID：" + registerUserId + "\n"
        "用户名：" + registerUserName + "\n\n"
        "请返回登录窗口进行登录。"
    );

    return true;
}

void MainWindow::showConversationListMode()
{
    m_leftListMode = LeftListMode::Conversations;

    if (m_addFriendButton) {
        m_addFriendButton->setVisible(false);
    }

    loadConversations();
}

void MainWindow::showContactListMode()
{
    m_leftListMode = LeftListMode::Contacts;

    if (m_addFriendButton) {
        m_addFriendButton->setVisible(true);
    }

    m_conversationList->clear();

    for (const Contact& contact : m_contacts) {
        QListWidgetItem *item = new QListWidgetItem(contact.userName());
        item->setData(Qt::UserRole, contact.userId());

        m_conversationList->addItem(item);
    }

    if (m_contacts.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem("暂无联系人");
        item->setData(Qt::UserRole, "");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        m_conversationList->addItem(item);
    }
}

Contact MainWindow::contactByUserId(const QString& userId) const
{
    for (const Contact& contact : m_contacts) {
        if (contact.userId() == userId) {
            return contact;
        }
    }

    return Contact();
}

void MainWindow::openConversationWithContact(const Contact& contact)
{
    if (contact.userId().isEmpty()) {
        return;
    }

    QString conversationId = conversationIdForPeer(contact.userId());

    Conversation conversation(
        conversationId,
        contact.userName(),
        contact.avatarPath(),
        Conversation::Type::PrivateChat,
        "",
        QDateTime::currentDateTime()
    );

    m_dbManager.saveConversation(conversation);

    m_currentConversationId = conversationId;

    m_leftListMode = LeftListMode::Conversations;
    

    loadConversations();

    m_chatTitleLabel->setText(contact.userName());
    showMessagesForConversation(conversationId);
}

void MainWindow::handleAddFriend()
{
    AddFriendDialog dialog(this);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString friendUsername = dialog.friendUsername();

    if (friendUsername.isEmpty()) {
        QMessageBox::warning(this, "添加好友失败", "好友用户名不能为空");
        return;
    }

    if (friendUsername == m_userName) {
        QMessageBox::warning(this, "添加好友失败", "不能添加自己为好友");
        return;
    }

    if (!m_networkClient.isConnected()) {
        QMessageBox::warning(this, "添加好友失败", "尚未连接到服务器");
        return;
    }

    QEventLoop loop;

    bool finished = false;
    bool addSuccess = false;
    QString friendId;
    QString friendName;
    QString avatarPath;
    QString conversationId;
    QString errorText;

    QMetaObject::Connection connection =
        connect(&m_networkClient, &NetworkClient::addFriendResult,
                &loop,
                [&](bool success,
                    const QString& resultFriendId,
                    const QString& resultFriendName,
                    const QString& resultAvatarPath,
                    const QString& resultConversationId,
                    const QString& resultErrorText)
    {
        finished = true;
        addSuccess = success;
        friendId = resultFriendId;
        friendName = resultFriendName;
        avatarPath = resultAvatarPath;
        conversationId = resultConversationId;
        errorText = resultErrorText;

        loop.quit();
    });

    m_networkClient.addFriend(m_userId, friendUsername);

    loop.exec();

    disconnect(connection);

    if (!finished) {
        QMessageBox::warning(this, "添加好友失败", "添加好友请求未完成");
        return;
    }

    if (!addSuccess) {
        QMessageBox::warning(this, "添加好友失败", errorText);
        return;
    }

    bool exists = false;

    for (const Contact& contact : m_contacts) {
        if (contact.userId() == friendId) {
            exists = true;
            break;
        }
    }

    QMessageBox::information(this,"好友申请","好友申请已发送，等待对方同意。");
}

void MainWindow::addOrUpdateContact(const Contact& contact)
{
    if (contact.userId().isEmpty()) {
        return;
    }

    for (Contact& existingContact : m_contacts) {
        if (existingContact.userId() == contact.userId()) {
            existingContact.setUserName(contact.userName());
            existingContact.setAvatarPath(contact.avatarPath());
            return;
        }
    }

    m_contacts.append(contact);
}