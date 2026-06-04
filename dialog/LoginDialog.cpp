#include "LoginDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent),
      m_usernameEdit(new QLineEdit(this)),
      m_loginButton(new QPushButton("登录", this)),
      m_cancelButton(new QPushButton("取消", this)),
      m_titleLabel(new QLabel("请输入用户名", this))
{
    setWindowTitle("登录 Qtchat");
    setModal(true);
    resize(320, 140);

    m_usernameEdit->setPlaceholderText("例如：张三 / 李四");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(m_titleLabel);
    mainLayout->addWidget(m_usernameEdit);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_loginButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    connect(m_loginButton, &QPushButton::clicked,
            this, &LoginDialog::accept);

    connect(m_cancelButton, &QPushButton::clicked,
            this, &LoginDialog::reject);
}

QString LoginDialog::username() const
{
    return m_usernameEdit->text().trimmed();
}