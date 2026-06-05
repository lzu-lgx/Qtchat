#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);

    QString username() const;
    QString password() const;
    QString confirmPassword() const;
    QString captcha() const;

private:
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QLineEdit *m_confirmPasswordEdit;
    QLineEdit *m_captchaEdit;

    QLabel *m_titleLabel;
    QLabel *m_captchaHintLabel;

    QPushButton *m_registerButton;
    QPushButton *m_cancelButton;
};

#endif // REGISTERDIALOG_H