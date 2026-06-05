#include "LoginDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent),
      m_usernameEdit(new QLineEdit(this)),
      m_passwordEdit(new QLineEdit(this)),
      m_loginButton(new QPushButton("登录", this)),
      m_cancelButton(new QPushButton("取消", this)),
      m_titleLabel(new QLabel("请输入用户名和密码", this))
{
    setWindowTitle("登录 Qtchat");
    setModal(true);
    resize(320, 180);

    m_usernameEdit->setPlaceholderText("用户名，例如：张三 / 李四");

    m_passwordEdit->setPlaceholderText("密码，例如：123456");
    m_passwordEdit->setEchoMode(QLineEdit::Password);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(m_titleLabel);
    mainLayout->addWidget(m_usernameEdit);
    mainLayout->addWidget(m_passwordEdit);

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

QString LoginDialog::password() const
{
    return m_passwordEdit->text();
}