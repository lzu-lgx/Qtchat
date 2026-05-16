#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "data/MockData.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QListWidgetItem>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_conversationList(nullptr)
    , m_chatTitleLabel(nullptr)
    , m_messageDisplay(nullptr)
    , m_messageInput(nullptr)
    , m_sendButton(nullptr)
{
    ui->setupUi(this);

    setupChatUi();
    loadMockData();
    loadConversations();

    if (!m_conversations.isEmpty())
    {
        m_conversationList->setCurrentRow(0);
        showMessagesForConversation(m_conversations.first().id());
        m_chatTitleLabel->setText(m_conversations.first().title());
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

    m_conversationList = new QListWidget(central);
    m_conversationList->setFixedWidth(220);

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

    mainLayout->addWidget(m_conversationList);
    mainLayout->addWidget(chatWidget);

    connect(m_conversationList, &QListWidget::itemClicked,
            this, [this](QListWidgetItem *item)
            {
                QString conversationId = item->data(Qt::UserRole).toString();
                m_chatTitleLabel->setText(item->text());
                showMessagesForConversation(conversationId);
            });
}

void MainWindow::loadMockData()
{
    m_conversations = MockData::createConversations();
    m_messages = MockData::createMessages();
}

void MainWindow::loadConversations()
{
    m_conversationList->clear();

    for (const Conversation& conversation : m_conversations)
    {
        QString itemText = conversation.title() + "\n" + conversation.lastMessage();

        QListWidgetItem *item = new QListWidgetItem(itemText);
        item->setData(Qt::UserRole, conversation.id());

        m_conversationList->addItem(item);
    }
}

void MainWindow::showMessagesForConversation(const QString& conversationId)
{
    m_messageDisplay->clear();

    for (const Message& message : m_messages)
    {
        if (message.conversationId() == conversationId)
        {
            QString sender = message.senderId();

            if (sender == "me")
            {
                sender = "我";
            }

            QString line = sender + "： " + message.content();

            m_messageDisplay->append(line);
        }
    }
}