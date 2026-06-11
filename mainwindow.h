#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QTextBrowser>
#include<QTextEdit>
#include<QUrl>
#include <QLineEdit>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

#include "model/Conversation.h"
#include "model/Message.h"
#include "model/Contact.h"

#include "database/DatabaseManager.h"
#include "service/AiService.h"
#include "network/NetworkClient.h"

#include "dialog/LoginDialog.h"
#include "dialog/RegisterDialog.h"
#include "dialog/AddFriendDialog.h"
#include "model/FriendRequest.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    enum class LeftListMode
    {
        Conversations,
        Contacts
    };

private:
    // UI
    Ui::MainWindow *ui;

    QListWidget *m_conversationList;
    QPushButton *m_conversationModeButton;
    QPushButton *m_contactsModeButton;

    QLabel *m_chatTitleLabel;
    QTextBrowser *m_messageDisplay;
    QLineEdit *m_messageInput;
    QPushButton *m_sendButton;

    // 状态
    QString m_currentConversationId;

    QString m_userId;
    QString m_userName;

    // 旧配置字段，可以暂时保留，避免 cpp 里还有残留引用时报错
    QString m_peerId;
    QString m_peerName;

    LeftListMode m_leftListMode;

    // 数据
    QList<Conversation> m_conversations;
    QList<Contact> m_contacts;
    QList<FriendRequest> m_friendRequests;

    // 模块
    DatabaseManager m_dbManager;
    AiService m_aiService;
    NetworkClient m_networkClient;
    QPushButton *m_addFriendButton;
    
    QString m_pendingFriendRequestId;
    QString m_pendingFriendRequestAction;

private:
    // 初始化
    void setupChatUi();
    void setupNetwork();
    void loadClientConfig();
    void initializeAfterLogin(const QString& databaseName);

    // 登录 / 注册
    bool showLoginDialog();
    bool handleRegister();

    // 会话
    void loadConversations();
    void showMessagesForConversation(const QString& conversationId);

    QString conversationIdForUsers(const QString& userId1,
                                   const QString& userId2) const;

    QString conversationIdForPeer(const QString& peerId) const;

    // 联系人
    void handleContactsResult(const QJsonObject& json);
    Contact contactByUserId(const QString& userId) const;
    Contact contactByConversationId(const QString& conversationId) const;
    void openConversationWithContact(const Contact& contact);

    // 左侧列表模式
    void showConversationListMode();
    void showContactListMode();

    // 消息发送 / 接收
    void sendCurrentMessage();
    void sendNetworkChatMessage(const QString& content);
    void handleJsonNetworkMessage(const QJsonObject& json);

    // AI 助手
    void ensureAiConversation();
    void showAiThinkingMessage();
    void handleAiAssistantReply(const QString& conversationId,
                                const QString& userMessage);

    // 显示辅助
    QString displayNameForSender(const QString& senderId) const;
    void handleAddFriend();

    void addOrUpdateContact(const Contact& contact);
    void ensureFriendRequestsConversation();
    bool isFriendRequestsConversation(const QString& conversationId) const;
    void showFriendRequestMessages();
    void handleFriendRequestLinkClicked(const QUrl& url);
    void addOrUpdateFriendRequest(const FriendRequest& request);

};

#endif // MAINWINDOW_H