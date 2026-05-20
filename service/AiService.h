#ifndef AISERVICE_H
#define AISERVICE_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class AiService : public QObject
{
    Q_OBJECT

public:
    explicit AiService(QObject *parent = nullptr);

    void requestReply(const QString& conversationId,
                      const QString& userMessage);

signals:
    void replyReady(const QString& conversationId,
                    const QString& replyText);

    void replyFailed(const QString& conversationId,
                     const QString& errorText);

private:
    QString apiKey() const;
    QString model() const;
    QString baseUrl() const;

    QNetworkAccessManager *m_networkManager;
};

#endif // AISERVICE_H