#ifndef ADDFRIENDDIALOG_H
#define ADDFRIENDDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;

class AddFriendDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddFriendDialog(QWidget *parent = nullptr);

    QString friendUsername() const;

private:
    QLabel *m_titleLabel;
    QLineEdit *m_usernameEdit;
    QPushButton *m_addButton;
    QPushButton *m_cancelButton;
};

#endif // ADDFRIENDDIALOG_H