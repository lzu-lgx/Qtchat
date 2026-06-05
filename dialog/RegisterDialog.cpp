#include "RegisterDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent),
      m_usernameEdit(new QLineEdit(this)),
      m_passwordEdit(new QLineEdit(this)),
      m_confirmPasswordEdit(new QLineEdit(this)),
      m_captchaEdit(new QLineEdit(this)),
      m_titleLabel(new QLabel("注册新用户", this)),
      m_captchaHintLabel(new QLabel("验证码：1234", this)),
      m_registerButton(new QPushButton("注册", this)),
      m_cancelButton(new QPushButton("取消", this))
{
    setWindowTitle("注册 Qtchat");
    setModal(true);
    resize(340, 230);

    m_usernameEdit->setPlaceholderText("用户名");
    m_passwordEdit->setPlaceholderText("密码");
    m_confirmPasswordEdit->setPlaceholderText("确认密码");
    m_captchaEdit->setPlaceholderText("请输入验证码");

    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(m_titleLabel);
    mainLayout->addWidget(m_usernameEdit);
    mainLayout->addWidget(m_passwordEdit);
    mainLayout->addWidget(m_confirmPasswordEdit);
    mainLayout->addWidget(m_captchaHintLabel);
    mainLayout->addWidget(m_captchaEdit);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_registerButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    connect(m_registerButton, &QPushButton::clicked,
            this, &RegisterDialog::accept);

    connect(m_cancelButton, &QPushButton::clicked,
            this, &RegisterDialog::reject);
}

QString RegisterDialog::username() const
{
    return m_usernameEdit->text().trimmed();
}

QString RegisterDialog::password() const
{
    return m_passwordEdit->text();
}

QString RegisterDialog::confirmPassword() const
{
    return m_confirmPasswordEdit->text();
}

QString RegisterDialog::captcha() const
{
    return m_captchaEdit->text().trimmed();
}