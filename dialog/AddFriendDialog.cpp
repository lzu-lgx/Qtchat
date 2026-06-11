#include "AddFriendDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

AddFriendDialog::AddFriendDialog(QWidget *parent)
    : QDialog(parent),
      m_titleLabel(new QLabel("添加好友", this)),
      m_usernameEdit(new QLineEdit(this)),
      m_addButton(new QPushButton("添加", this)),
      m_cancelButton(new QPushButton("取消", this))
{
    setWindowTitle("添加好友");
    setModal(true);
    resize(320, 140);

    m_usernameEdit->setPlaceholderText("请输入对方用户名");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_titleLabel);
    mainLayout->addWidget(m_usernameEdit);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    connect(m_addButton, &QPushButton::clicked,
            this, &AddFriendDialog::accept);

    connect(m_cancelButton, &QPushButton::clicked,
            this, &AddFriendDialog::reject);
}

QString AddFriendDialog::friendUsername() const
{
    return m_usernameEdit->text().trimmed();
}