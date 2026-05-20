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
    void handleAiAssistantReply(const QString& conversationId,const QString& userMessage);

private:
    Ui::MainWindow *ui;

    QListWidget *m_conversationList;
    QLabel *m_chatTitleLabel;
    QTextEdit *m_messageDisplay;
    QLineEdit *m_messageInput;
    QPushButton *m_sendButton;
    DatabaseManager m_dbManager;
    QString m_currentConversationId;
    AiService m_aiService;

    QList<Conversation> m_conversations;
    QList<Message> m_messages;
    
};

#endif // MAINWINDOW_H