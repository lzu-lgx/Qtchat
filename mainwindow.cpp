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
#include <QTextBrowser>
#include <QUrl>
#include <QJsonArray>
#include <QRegularExpression>
#include "MessageBubbleWidget.h"

#include <QScrollArea>
#include <QScrollBar>
#include <QFrame>




namespace
{
    const QString kFriendRequestsConversationId = QStringLiteral("conv_friend_requests");
    const QString kFriendRequestsConversationTitle = QStringLiteral("好友通知");
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_conversationList(nullptr)
    , m_chatTitleLabel(nullptr)
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
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

// 左侧面板
    QWidget *leftPanel = new QWidget(central);
    leftPanel->setObjectName("leftPanel");
    leftPanel->setFixedWidth(240);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    leftLayout->setSpacing(10);

// 左侧顶部按钮
    m_conversationModeButton = new QPushButton("会话列表", leftPanel);
    m_contactsModeButton = new QPushButton("通讯录", leftPanel);
    m_addFriendButton = new QPushButton("添加好友", leftPanel);
    m_conversationModeButton->setObjectName("modeButton");
    m_contactsModeButton->setObjectName("modeButton");
    m_addFriendButton->setObjectName("addFriendButton");
    m_conversationModeButton->setFixedHeight(32);
    m_contactsModeButton->setFixedHeight(32);
    m_addFriendButton->setFixedHeight(32);
    m_addFriendButton->setVisible(false); // 默认隐藏

// 会话/联系人列表
    m_conversationList = new QListWidget(leftPanel);
    m_conversationList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_conversationList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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
    chatWidget->setObjectName("chatWidget");
    QVBoxLayout *chatLayout = new QVBoxLayout(chatWidget);
    chatLayout->setContentsMargins(16, 12, 16, 12);
    chatLayout->setSpacing(10);

    m_chatTitleLabel = new QLabel("请选择一个会话", chatWidget);
    m_chatTitleLabel->setObjectName("chatTitleLabel");
    m_chatTitleLabel->setMinimumHeight(44);

    m_messageScrollArea = new QScrollArea(chatWidget);
    m_messageScrollArea->setWidgetResizable(true);
    m_messageScrollArea->setFrameShape(QFrame::NoFrame);
    m_messageScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_messageScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_messageContainer = new QWidget(m_messageScrollArea);
    m_messageContainer->setObjectName("messageContainer");
    m_messageLayout = new QVBoxLayout(m_messageContainer);
    m_messageLayout->setContentsMargins(12, 12, 12, 12);
    m_messageLayout->setSpacing(6);
    m_messageLayout->addStretch();

    m_messageScrollArea->setWidget(m_messageContainer);

    QWidget *inputWidget = new QWidget(chatWidget);
    inputWidget->setObjectName("inputWidget");
    QHBoxLayout *inputLayout = new QHBoxLayout(inputWidget);
    inputLayout->setContentsMargins(0, 8, 0, 0);
    inputLayout->setSpacing(10);

    m_messageInput = new QLineEdit(inputWidget);
    m_messageInput->setPlaceholderText("请输入消息...");
    m_messageInput->setFixedHeight(34);
    m_sendButton = new QPushButton("发送", inputWidget);
    m_sendButton->setObjectName("sendButton");
    m_sendButton->setFixedHeight(34);
    m_sendButton->setMinimumWidth(76);

    inputLayout->addWidget(m_messageInput);
    inputLayout->addWidget(m_sendButton);

    chatLayout->addWidget(m_chatTitleLabel);
    chatLayout->addWidget(m_messageScrollArea);
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

            if (isFriendRequestsConversation(conversationId)) {
                clearUnreadCount(kFriendRequestsConversationId);
                showFriendRequestMessages();
                return;
            }
            clearUnreadCount(conversationId);
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

        QListWidgetItem *item = new QListWidgetItem;
        item->setData(Qt::UserRole, conversation.id());

        m_conversationList->addItem(item);

        setConversationListItemWidget(item,conversation.title(),conversation.lastMessage(),unreadCount(conversation.id()));

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
    
    clearMessageArea();
    m_messageInput->setEnabled(true);
    m_sendButton->setEnabled(true);

    QList<Message> messages =
        m_dbManager.loadMessages(conversationId);

    
    for (const Message& message : messages) {
    QString senderId = message.senderId();
    QString senderName = displayNameForSender(senderId);
    QString timeText = message.timestamp().toString("yyyy-MM-dd HH:mm");

    bool isMine =
        senderId == m_userId ||
        senderId == m_userName ||
        senderId == "me" ||
        senderId == "我";

    bool isAi =
        senderId == "AI 助手" ||
        senderId == "assistant" ||
        senderId == "ai" ||
        senderId == "conv_ai";

    MessageBubbleWidget::Role role = MessageBubbleWidget::Role::Other;

    if (isMine) {
        role = MessageBubbleWidget::Role::Mine;
    } else if (isAi) {
        role = MessageBubbleWidget::Role::Ai;
    }

    MessageBubbleWidget *bubble =
        new MessageBubbleWidget(senderName,
                                message.content(),
                                timeText,
                                role,
                                m_messageContainer);

    addMessageWidget(bubble);
}
    scrollMessagesToBottom();

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
    QString timeText = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");

    MessageBubbleWidget *bubble =
        new MessageBubbleWidget(
            "AI 助手",
            "正在思考中...",
            timeText,
            MessageBubbleWidget::Role::Ai,
            m_messageContainer
        );

    addMessageWidget(bubble);
    scrollMessagesToBottom();
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
            QString fromUserId = message.value("from_user_id").toString();
            QString fromUserName = message.value("from_user_name").toString();
            QString requestMessage = message.value("message").toString();

            FriendRequest request(requestId,fromUserId,fromUserName,requestMessage,FriendRequest::Pending,QDateTime::currentDateTime().toString(Qt::ISODate));

            addOrUpdateFriendRequest(request);

            if (!isFriendRequestsConversation(m_currentConversationId)) {
                incrementUnreadCount(kFriendRequestsConversationId);
            }

            if (m_leftListMode == LeftListMode::Conversations) {
                ensureFriendRequestsConversation();
                refreshUnreadBadges();
            }

            if (isFriendRequestsConversation(m_currentConversationId)) {
                showFriendRequestMessages();
            }

            return;
        }

        if (type == "friend_request_status_updated") {
    QString requestId = message.value("request_id").toString();
    int updatedStatus = message.value("status").toInt();
    QString action = message.value("action").toString();
    QString updatedFriendId = message.value("friend_id").toString();
    QString updatedFriendName = message.value("friend_name").toString();
    QString conversationId = message.value("conversation_id").toString();

    qDebug() << "Friend request status updated:"
             << requestId
             << updatedStatus
             << action
             << updatedFriendId
             << updatedFriendName;

    bool found = false;

    for (FriendRequest& request : m_friendRequests) {
        if (request.requestId == requestId) {
            request.status = updatedStatus;

            if (request.outgoing) {
                request.toUserId = updatedFriendId;
                request.toUserName = updatedFriendName;
            }

            found = true;
            break;
        }
    }

    if (!found && !requestId.isEmpty()) {
        FriendRequest request(
            requestId,
            m_userId,
            m_userName,
            "好友申请已处理",
            updatedStatus,
            QDateTime::currentDateTime().toString(Qt::ISODate),
            true,
            updatedFriendId,
            updatedFriendName
        );

        addOrUpdateFriendRequest(request);
    }

    if (updatedStatus == FriendRequest::Accepted &&
        !updatedFriendId.isEmpty()) {
        Contact newContact(updatedFriendId,
                           updatedFriendName,
                           QString());

        addOrUpdateContact(newContact);

        m_networkClient.requestContacts(m_userId);

        QTimer::singleShot(500, this, [this]() {m_networkClient.requestContacts(m_userId);});

    }

    if (!isFriendRequestsConversation(m_currentConversationId)) {
        incrementUnreadCount(kFriendRequestsConversationId);
    }

    if (m_leftListMode == LeftListMode::Conversations) {
        ensureFriendRequestsConversation();
    }

    if (isFriendRequestsConversation(m_currentConversationId)) {
        showFriendRequestMessages();
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

    connect(&m_networkClient, &NetworkClient::respondFriendRequestResult,this,[this](bool success,const QString& action,const QString& friendId,
            const QString& friendName,const QString& avatarPath,const QString& conversationId,const QString& errorText)
    {
        Q_UNUSED(conversationId);

        qDebug() << "Respond friend request result:"
                << "success =" << success
                << "action =" << action
                << "friendId =" << friendId
                << "friendName =" << friendName
                << "error =" << errorText;

        if (!success) {
            QMessageBox::warning(this,"好友申请",errorText.isEmpty() ? "处理好友申请失败" : errorText);
            return;
        }

        for (FriendRequest& request : m_friendRequests) {
            if (request.requestId == m_pendingFriendRequestId) {
                if (action == "accept") {
                    request.status = FriendRequest::Accepted;
                } 
                else if (action == "reject") {
                    request.status = FriendRequest::Rejected;
            }

            break;
        }
        }

        ensureFriendRequestsConversation();

        if (isFriendRequestsConversation(m_currentConversationId)) {
            showFriendRequestMessages();
        }

        if (action == "accept") {
            if (!friendId.isEmpty()) {
                Contact newContact(friendId, friendName, avatarPath);
                addOrUpdateContact(newContact);
            }

            QTimer::singleShot(500, this, [this]() {m_networkClient.requestContacts(m_userId);});

            m_networkClient.requestContacts(m_userId);

            if (m_leftListMode == LeftListMode::Contacts) {
                showContactListMode();
            }

            
        }

        if (action == "reject") {
            QMessageBox::information(this, "好友申请", "已拒绝好友申请");
        }

        m_pendingFriendRequestId.clear();
        m_pendingFriendRequestAction.clear();
});
    connect(&m_networkClient, &NetworkClient::friendRequestsResult,
        this,
        [this](const QJsonArray& requests)
{
    qDebug() << "Friend requests loaded from server:"
             << requests.size();

    int pendingIncomingCount = 0;

    for (const QJsonValue& value : requests) {
        QJsonObject obj = value.toObject();

        FriendRequest request(
            obj.value("request_id").toString(),
            obj.value("from_user_id").toString(),
            obj.value("from_user_name").toString(),
            obj.value("message").toString(),
            obj.value("status").toInt(FriendRequest::Pending),
            obj.value("created_at").toString()
        );

        addOrUpdateFriendRequest(request);

        if (request.status == FriendRequest::Pending) {
            ++pendingIncomingCount;
        }
    }

    if (pendingIncomingCount > 0 &&
        !isFriendRequestsConversation(m_currentConversationId)) {
        m_unreadCounts[kFriendRequestsConversationId] = pendingIncomingCount;
    }

    if (m_leftListMode == LeftListMode::Conversations) {
        ensureFriendRequestsConversation();
        refreshUnreadBadges();
    }

    if (isFriendRequestsConversation(m_currentConversationId)) {
        showFriendRequestMessages();
    }
});

    connect(&m_networkClient, &NetworkClient::sendFriendRequestResult,
        this,
        [this](bool success,
               const QString& requestId,
               const QString& toUserId,
               const QString& toUserName,
               const QString& message,
               const QString& errorText)
{
    if (!success) {
        QMessageBox::warning(this,
                             "好友申请",
                             errorText.isEmpty() ? "好友申请发送失败" : errorText);
        return;
    }

    FriendRequest request(
        requestId,
        m_userId,
        m_userName,
        message,
        FriendRequest::Pending,
        QDateTime::currentDateTime().toString(Qt::ISODate),
        true,
        toUserId,
        toUserName
    );

    addOrUpdateFriendRequest(request);

    if (m_leftListMode == LeftListMode::Conversations) {
        ensureFriendRequestsConversation();
    }

    if (isFriendRequestsConversation(m_currentConversationId)) {
        showFriendRequestMessages();
    }

    qDebug() << "Send friend request shown as message:"
             << requestId
             << toUserName;
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

    if (conversationId != m_currentConversationId) {
        incrementUnreadCount(conversationId);
    }

    loadConversations();
    refreshUnreadBadges();

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
    ensureFriendRequestsConversation();
    

    m_networkClient.registerClient(m_userId, m_userName);
    m_networkClient.requestContacts(m_userId);
    m_networkClient.requestFriendRequests(m_userId);

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

    ensureAiConversation();

    loadConversations();

    ensureFriendRequestsConversation();

    refreshUnreadBadges();
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

    m_networkClient.addFriend(m_userId, friendUsername);
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

bool MainWindow::isFriendRequestsConversation(const QString& conversationId) const
{
    return conversationId == kFriendRequestsConversationId;
}

void MainWindow::ensureFriendRequestsConversation()
{
    if (!m_conversationList) {
        return;
    }

    if (m_leftListMode != LeftListMode::Conversations) {
        return;
    }

    int pendingIncomingCount = 0;
    int pendingOutgoingCount = 0;

    for (const FriendRequest& request : m_friendRequests) {
        if (request.status != FriendRequest::Pending) {
            continue;
        }

        if (request.outgoing) {
            ++pendingOutgoingCount;
        } else {
            ++pendingIncomingCount;
        }
    }

    QString subtitle;

    if (pendingIncomingCount > 0) {
        subtitle = QString("待处理好友申请：%1 条").arg(pendingIncomingCount);
    } else if (pendingOutgoingCount > 0) {
        subtitle = QString("等待对方验证：%1 条").arg(pendingOutgoingCount);
    } else {
        subtitle = "暂无待处理申请";
    }

    for (int i = 0; i < m_conversationList->count(); ++i) {
        QListWidgetItem *item = m_conversationList->item(i);

        if (!item) {
            continue;
        }

        QString conversationId = item->data(Qt::UserRole).toString();

        if (conversationId == kFriendRequestsConversationId) {
            setConversationListItemWidget(item,
                                          kFriendRequestsConversationTitle,
                                          subtitle,
                                          unreadCount(kFriendRequestsConversationId));
            return;
        }
    }

    QListWidgetItem *item = new QListWidgetItem;
    item->setData(Qt::UserRole, kFriendRequestsConversationId);

    m_conversationList->insertItem(0, item);

    setConversationListItemWidget(item,
                                  kFriendRequestsConversationTitle,
                                  subtitle,
                                  unreadCount(kFriendRequestsConversationId));
}

void MainWindow::showFriendRequestMessages()
{
    m_currentConversationId = kFriendRequestsConversationId;

    m_chatTitleLabel->setText(kFriendRequestsConversationTitle);

    m_messageInput->clear();
    m_messageInput->setEnabled(false);
    m_sendButton->setEnabled(false);

    clearMessageArea();

    if (m_friendRequests.isEmpty()) {
        addSystemNoticeCard("暂无好友申请");
        scrollMessagesToBottom();
        return;
    }

    for (const FriendRequest& request : m_friendRequests) {
        addFriendRequestCard(request);
    }

    scrollMessagesToBottom();
}

void MainWindow::handleFriendRequestLinkClicked(const QUrl& url)
{
    if (!isFriendRequestsConversation(m_currentConversationId)) {
        return;
    }

    if (url.scheme() != "friend-request") {
        return;
    }

    QString action = url.host();
    QString requestId = url.path();

    if (requestId.startsWith("/")) {
        requestId.remove(0, 1);
    }

    if (requestId.isEmpty()) {
        return;
    }

    if (action != "accept" && action != "reject") {
        return;
    }

    bool found = false;

    for (const FriendRequest& request : m_friendRequests) {
        if (request.requestId == requestId &&
            request.status == FriendRequest::Pending) {
            found = true;
            break;
        }
    }

    if (!found) {
        QMessageBox::information(this,
                                 "好友申请",
                                 "该好友申请已处理或不存在");
        return;
    }

    m_pendingFriendRequestId = requestId;
    m_pendingFriendRequestAction = action;

    qDebug() << "Friend request link clicked:"
             << "requestId =" << requestId
             << "action =" << action;

    m_networkClient.respondFriendRequest(requestId, m_userId, action);
}

void MainWindow::addOrUpdateFriendRequest(const FriendRequest& request)
{
    if (request.requestId.isEmpty()) {
        return;
    }

    for (FriendRequest& existingRequest : m_friendRequests) {
        if (existingRequest.requestId == request.requestId) {
            existingRequest.fromUserId = request.fromUserId;
            existingRequest.fromUserName = request.fromUserName;
            existingRequest.toUserId = request.toUserId;
            existingRequest.toUserName = request.toUserName;
            existingRequest.message = request.message;
            existingRequest.status = request.status;
            existingRequest.createdAt = request.createdAt;
            existingRequest.outgoing = request.outgoing;
            return;
        }
    }

    m_friendRequests.append(request);
}

int MainWindow::unreadCount(const QString& conversationId) const
{
    return m_unreadCounts.value(conversationId, 0);
}

void MainWindow::incrementUnreadCount(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    int count = m_unreadCounts.value(conversationId, 0);
    m_unreadCounts[conversationId] = count + 1;

    refreshUnreadBadges();
}

void MainWindow::clearUnreadCount(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return;
    }

    if (!m_unreadCounts.contains(conversationId)) {
        return;
    }

    m_unreadCounts.remove(conversationId);

    refreshUnreadBadges();
}

void MainWindow::refreshUnreadBadges()
{
    if (!m_conversationList) {
        return;
    }

    if (m_leftListMode != LeftListMode::Conversations) {
        return;
    }

    for (int i = 0; i < m_conversationList->count(); ++i) {
        QListWidgetItem *item = m_conversationList->item(i);

        if (!item) {
            continue;
        }

        QString conversationId = item->data(Qt::UserRole).toString();

        if (conversationId.isEmpty()) {
            continue;
        }

        QString title = itemRawTitle(item);
        QString subtitle = itemRawSubtitle(item);

        if (title.isEmpty()) {
            title = item->text();
        }

        int count = unreadCount(conversationId);

        setConversationListItemWidget(item, title, subtitle, count);
    }
}

void MainWindow::clearMessageArea()
{
    if (!m_messageLayout) {
        return;
    }

    while (QLayoutItem *item = m_messageLayout->takeAt(0)) {
        QWidget *widget = item->widget();

        if (widget) {
            widget->deleteLater();
        }

        delete item;
    }

    m_messageLayout->addStretch();
}

void MainWindow::addMessageWidget(QWidget *widget)
{
    if (!m_messageLayout || !widget) {
        return;
    }

    int insertIndex = qMax(0, m_messageLayout->count() - 1);
    m_messageLayout->insertWidget(insertIndex, widget);
}

void MainWindow::scrollMessagesToBottom()
{
    if (!m_messageScrollArea || !m_messageContainer) {
        return;
    }

    m_messageContainer->adjustSize();
    m_messageContainer->updateGeometry();
    m_messageScrollArea->updateGeometry();

    auto scrollToBottom = [this]() {
        if (!m_messageScrollArea) {
            return;
        }

        QScrollBar *bar = m_messageScrollArea->verticalScrollBar();

        if (bar) {
            bar->setValue(bar->maximum());
        }
    };

    QTimer::singleShot(0, this, scrollToBottom);
    QTimer::singleShot(50, this, scrollToBottom);
    QTimer::singleShot(120, this, scrollToBottom);
}

void MainWindow::addSystemNoticeCard(const QString& text)
{
    QFrame *card = new QFrame(m_messageContainer);
    card->setStyleSheet(
        "QFrame {"
        "background-color: #303030;"
        "border: 1px solid #444444;"
        "border-radius: 10px;"
        "}"
    );

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);

    QLabel *label = new QLabel(text, card);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(
        "color: #aaaaaa;"
        "font-size: 14px;"
    );

    layout->addWidget(label);

    addMessageWidget(card);
}

void MainWindow::addFriendRequestCard(const FriendRequest& request)
{
    QFrame *card = new QFrame(m_messageContainer);
    card->setStyleSheet(
        "QFrame {"
        "background-color: #2b2b2b;"
        "border: 1px solid #4a4a4a;"
        "border-radius: 12px;"
        "}"
    );

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    QLabel *titleLabel = new QLabel(card);
    titleLabel->setWordWrap(true);
    titleLabel->setStyleSheet(
        "color: #ffffff;"
        "font-size: 15px;"
        "font-weight: bold;"
    );

    QLabel *messageLabel = new QLabel(card);
    messageLabel->setWordWrap(true);
    messageLabel->setStyleSheet(
        "color: #bbbbbb;"
        "font-size: 13px;"
    );

    QLabel *statusLabel = new QLabel(card);
    statusLabel->setStyleSheet(
        "color: #888888;"
        "font-size: 12px;"
    );

    QString statusText;

    if (request.status == FriendRequest::Pending) {
        statusText = request.outgoing ? "等待对方处理" : "待处理";
    } else if (request.status == FriendRequest::Accepted) {
        statusText = request.outgoing ? "对方已同意" : "已同意";
    } else if (request.status == FriendRequest::Rejected) {
        statusText = request.outgoing ? "对方已拒绝" : "已拒绝";
    } else {
        statusText = "未知状态";
    }

    if (request.outgoing) {
        QString targetName = request.toUserName.isEmpty()
                                 ? request.toUserId
                                 : request.toUserName;

        titleLabel->setText("你已向 " + targetName + " 发送好友申请");
        messageLabel->setText("等待对方处理你的好友申请");
    } else {
        titleLabel->setText(request.fromUserName + " 请求添加你为好友");
        messageLabel->setText(request.message);
    }

    statusLabel->setText("状态：" + statusText);

    layout->addWidget(titleLabel);
    layout->addWidget(messageLabel);
    layout->addWidget(statusLabel);

    if (!request.outgoing &&
        request.status == FriendRequest::Pending) {
        QHBoxLayout *buttonLayout = new QHBoxLayout;
        buttonLayout->setContentsMargins(0, 4, 0, 0);
        buttonLayout->addStretch();

        QPushButton *acceptButton = new QPushButton("同意", card);
        QPushButton *rejectButton = new QPushButton("拒绝", card);

        acceptButton->setFixedHeight(30);
        rejectButton->setFixedHeight(30);
        acceptButton->setMinimumWidth(72);
        rejectButton->setMinimumWidth(72);

        acceptButton->setStyleSheet(
            "QPushButton {"
            "background-color: #3d7eff;"
            "color: white;"
            "border: none;"
            "border-radius: 6px;"
            "padding: 4px 12px;"
            "}"
            "QPushButton:hover {"
            "background-color: #5a91ff;"
            "}"
        );

        rejectButton->setStyleSheet(
            "QPushButton {"
            "background-color: #3a3a3a;"
            "color: #dddddd;"
            "border: 1px solid #555555;"
            "border-radius: 6px;"
            "padding: 4px 12px;"
            "}"
            "QPushButton:hover {"
            "background-color: #4a4a4a;"
            "}"
        );

        
       connect(acceptButton, &QPushButton::clicked,
        this, [this, request]() {
    m_pendingFriendRequestId = request.requestId;
    m_pendingFriendRequestAction = "accept";

    m_networkClient.respondFriendRequest(
        request.requestId,
        m_userId,
        "accept"
    );

    // 关键修复：同意后，当前处理方主动刷新通讯录
    QTimer::singleShot(300, this, [this]() {
        qDebug() << "Refresh contacts after accepting friend request:"
                 << m_userId;
        m_networkClient.requestContacts(m_userId);
    });

    QTimer::singleShot(1000, this, [this]() {
        qDebug() << "Refresh contacts again after accepting friend request:"
                 << m_userId;
        m_networkClient.requestContacts(m_userId);
    });
});

        connect(rejectButton, &QPushButton::clicked,
                this, [this, request]() {
            m_pendingFriendRequestId = request.requestId;
            m_pendingFriendRequestAction = "reject";

            m_networkClient.respondFriendRequest(
                request.requestId,
                m_userId,
                "reject"
            );
        });

        buttonLayout->addWidget(acceptButton);
        buttonLayout->addWidget(rejectButton);

        layout->addLayout(buttonLayout);
    }

    QWidget *wrapper = new QWidget(m_messageContainer);
    QHBoxLayout *wrapperLayout = new QHBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 4, 0, 4);

    wrapperLayout->addWidget(card);
    wrapperLayout->addStretch();

    card->setMaximumWidth(460);

    addMessageWidget(wrapper);
}

void MainWindow::setConversationListItemWidget(QListWidgetItem *item,
                                               const QString& title,
                                               const QString& subtitle,
                                               int unreadCount)
{
    if (!item || !m_conversationList) {
        return;
    }

    item->setData(Qt::UserRole + 1, title);
    item->setData(Qt::UserRole + 2, subtitle);

    QWidget *rowWidget = new QWidget(m_conversationList);
    rowWidget->setStyleSheet("background: transparent;");

    QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(10, 8, 10, 8);
    rowLayout->setSpacing(8);

    QVBoxLayout *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(3);

    QLabel *titleLabel = new QLabel(title, rowWidget);
    titleLabel->setStyleSheet(
        "color: #ffffff;"
        "font-size: 14px;"
        "font-weight: bold;"
        "background: transparent;"
    );

    QLabel *subtitleLabel = new QLabel(subtitle, rowWidget);
    subtitleLabel->setStyleSheet(
        "color: #b0b0b0;"
        "font-size: 12px;"
        "background: transparent;"
    );
    subtitleLabel->setMaximumHeight(18);

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(subtitleLabel);

    rowLayout->addLayout(textLayout);
    rowLayout->addStretch();

    if (unreadCount > 0) {
        QLabel *badgeLabel = new QLabel(rowWidget);

        QString badgeText = unreadCount > 99 ? "99+" : QString::number(unreadCount);
        badgeLabel->setText(badgeText);
        badgeLabel->setAlignment(Qt::AlignCenter);

        badgeLabel->setStyleSheet(
            "QLabel {"
            "background-color: #ff4d4f;"
            "color: white;"
            "border-radius: 9px;"
            "font-size: 11px;"
            "font-weight: bold;"
            "padding: 1px 5px;"
            "}"
        );

        badgeLabel->setMinimumWidth(18);
        badgeLabel->setFixedHeight(18);

        rowLayout->addWidget(badgeLabel, 0, Qt::AlignTop);
    }

    item->setSizeHint(QSize(200, 64));
    item->setText(QString());

    m_conversationList->setItemWidget(item, rowWidget);
}

QString MainWindow::itemRawTitle(QListWidgetItem *item) const
{
    if (!item) {
        return QString();
    }

    return item->data(Qt::UserRole + 1).toString();
}

QString MainWindow::itemRawSubtitle(QListWidgetItem *item) const
{
    if (!item) {
        return QString();
    }

    return item->data(Qt::UserRole + 2).toString();
}