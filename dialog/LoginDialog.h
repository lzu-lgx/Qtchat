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
    QString password() const;

signals:
    void registerRequested();

private:
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;

    QPushButton *m_loginButton;
    QPushButton *m_cancelButton;
    QLabel *m_titleLabel;
    QPushButton *m_registerButton;
};

#endif // LOGINDIALOG_H