#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

    QString username() const;

private:
    QLineEdit *m_usernameEdit;
    QPushButton *m_loginButton;
    QPushButton *m_cancelButton;
    QLabel *m_titleLabel;
};

#endif // LOGINDIALOG_H