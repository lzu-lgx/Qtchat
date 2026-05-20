#ifndef AISERVICE_H
#define AISERVICE_H

#include <QString>

class AiService
{
public:
    AiService();

    QString generateReply(const QString& userMessage) const;

private:
    QString apiKey() const;
    QString model() const;
    QString baseUrl() const;
};

#endif // AISERVICE_H