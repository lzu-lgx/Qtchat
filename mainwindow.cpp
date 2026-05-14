#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "data/mockdata.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QList<Conversation> conversations = MockData::createConversations();

    for (const Conversation& conversation : conversations)
    {
        qDebug() << conversation.title()
                 << conversation.lastMessage()
                 << conversation.lastMessageTime();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
