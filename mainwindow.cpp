#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "data/MockData.h"

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_conversationList(nullptr)
    , m_newConversationButton(nullptr)
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
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QHBoxLayout *mainLayout = new QHBoxLayout(central);

    QWidget *leftPanel = new QWidget(central);
    leftPanel->setFixedWidth(220);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);

    m_newConversationButton = new QPushButton("新建会话", leftPanel);
    m_conversationList = new QListWidget(leftPanel);
    

    QWidget *chatWidget = new QWidget(central);
    QVBoxLayout *chatLayout = new QVBoxLayout(chatWidget);

    m_chatTitleLabel = new QLabel("请选择一个会话", chatWidget);
    m_chatTitleLabel->setMinimumHeight(40);

    m_messageDisplay = new QTextEdit(chatWidget);
    m_messageDisplay->setReadOnly(true);

    QWidget *inputWidget = new QWidget(chatWidget);
    QHBoxLayout *inputLayout = new QHBoxLayout(inputWidget);

    m_messageInput = new QLineEdit(inputWidget);
    m_messageInput->setPlaceholderText("请输入消息...");

    m_sendButton = new QPushButton("发送", inputWidget);

    inputLayout->addWidget(m_messageInput);
    inputLayout->addWidget(m_sendButton);

    chatLayout->addWidget(m_chatTitleLabel);
    chatLayout->addWidget(m_messageDisplay);
    chatLayout->addWidget(inputWidget);
    leftLayout->addWidget(m_newConversationButton);
    leftLayout->addWidget(m_conversationList);

    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(chatWidget);

    connect(m_conversationList, &QListWidget::itemClicked,
        this, [this](QListWidgetItem *item)
    {
        m_currentConversationId = item->data(Qt::UserRole).toString();

        QString title = item->text().split('\n').first();
        m_chatTitleLabel->setText(title);

        showMessagesForConversation(m_currentConversationId);
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
    connect(m_newConversationButton, &QPushButton::clicked,
        this, [this]()
            {
                createNewConversation();
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

}

void MainWindow::loadMockData()
{
    m_conversations = MockData::createConversations();
    m_messages = MockData::createMessages();
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
    m_messages = m_dbManager.loadMessages(conversationId);

    m_messageDisplay->clear();

    QString text;

    for (const Message& message : m_messages) {
        
        QString senderName = displayNameForSender(message.senderId());
        
        QString timeText = message.timestamp().toString("yyyy-MM-dd hh:mm:ss");

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
void MainWindow::createNewConversation()
{
    Contact contact = selectContact();

    if (contact.userId().isEmpty()) {
        qDebug() << "Create conversation canceled or no contact selected";
        return;
    }

    QString conversationId = conversationIdForPeer(contact.userId());
    QString title = contact.userName();

    Conversation conversation(
        conversationId,
        title,
        contact.avatarPath(),
        Conversation::Type::PrivateChat,
        "",
        QDateTime::currentDateTime()
    );

    if (!m_dbManager.saveConversation(conversation)) {
        return;
    }

    m_currentConversationId = conversationId;

    loadConversations();

    m_chatTitleLabel->setText(title);
    showMessagesForConversation(conversationId);
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

        if (type == "chat_message") {
            handleJsonNetworkMessage(message);
            return;
        }

        qDebug() << "Unsupported JSON message type in MainWindow:" << type;
    });

    connect(&m_networkClient, &NetworkClient::connectionError,
            this, [this](const QString& errorText)
    {
        qDebug() << "Network error:" << errorText;
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
}

Contact MainWindow::selectContact()
{
    if (m_contacts.isEmpty()) {
        qDebug() << "No contacts available";
        return Contact();
    }

    QStringList contactNames;

    for (const Contact& contact : m_contacts) {
        contactNames.append(contact.userName() + " (" + contact.userId() + ")");
    }

    bool ok = false;

    QString selected = QInputDialog::getItem(
        this,
        "选择联系人",
        "请选择要聊天的联系人：",
        contactNames,
        0,
        false,
        &ok
    );

    if (!ok || selected.isEmpty()) {
        return Contact();
    }

    int index = contactNames.indexOf(selected);

    if (index < 0 || index >= m_contacts.size()) {
        return Contact();
    }

    return m_contacts.at(index);
}

bool MainWindow::showLoginDialog()
{
    while (true) {
        LoginDialog dialog(this);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        QString username = dialog.username();

        if (username.isEmpty()) {
            QMessageBox::warning(this, "登录失败", "用户名不能为空");
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

        m_networkClient.login(username);

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