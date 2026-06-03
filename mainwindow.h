#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

#include "model/Conversation.h"
#include "model/Message.h"
#include "database/DatabaseManager.h"
#include "service/AiService.h"
#include "network/NetworkClient.h"
#include "config/AppConfig.h"
#include <QJsonObject>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupChatUi();
    void loadMockData();
    void loadConversations();
    void showMessagesForConversation(const QString& conversationId);
    void sendCurrentMessage();
    void sendNetworkChatMessage(const QString& content);
    void handleAiAssistantReply(const QString& conversationId,const QString& userMessage);
    void showAiThinkingMessage();
    void createNewConversation();
    void setupNetwork();

    void loadClientConfig();
    void handleJsonNetworkMessage(const QJsonObject& json);
    QString conversationIdForPeer(const QString& peerId) const;
    void ensureAiConversation();

    QString m_userId;
    QString m_userName;
    QString m_peerId;
    QString m_peerName;

private:
    Ui::MainWindow *ui;

    QListWidget *m_conversationList;
    QLabel *m_chatTitleLabel;
    QTextEdit *m_messageDisplay;
    QLineEdit *m_messageInput;
    QPushButton *m_sendButton;
    QPushButton *m_newConversationButton;
    DatabaseManager m_dbManager;
    QString m_currentConversationId;
    AiService m_aiService;
    NetworkClient m_networkClient;

    QList<Conversation> m_conversations;
    QList<Message> m_messages;
    
};

#endif // MAINWINDOW_H