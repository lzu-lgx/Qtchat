#include "AiService.h"

#include <QByteArray>
#include <QDebug>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QUrl>
#include <QCoreApplication>
#include <QSettings>

AiService::AiService()
{
}

QString AiService::apiKey() const
{
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";

    QSettings settings(configPath, QSettings::IniFormat);

    return settings.value("DeepSeek/ApiKey").toString().trimmed();
}
QString AiService::model() const
{
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";

    QSettings settings(configPath, QSettings::IniFormat);

    return settings.value("DeepSeek/Model", "deepseek-v4-flash")
        .toString()
        .trimmed();
}

QString AiService::baseUrl() const
{
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";

    QSettings settings(configPath, QSettings::IniFormat);

    return settings.value("DeepSeek/BaseUrl",
                          "https://api.deepseek.com/chat/completions")
        .toString()
        .trimmed();
}

QString AiService::generateReply(const QString& userMessage) const
{
    QString key = apiKey();

    if (key.isEmpty())
    {
        return "DeepSeek API Key 未配置。请在程序运行目录下创建 config.ini，并填写 [DeepSeek] ApiKey。";
    }

    QNetworkAccessManager manager;

    QNetworkRequest request{QUrl(baseUrl())};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + key.toUtf8());

    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = "你是一个嵌入在 Qt 聊天软件里的 AI 学习助手，回答要简洁、清楚。";

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;

    QJsonArray messages;
    messages.append(systemMessage);
    messages.append(userMsg);

    QJsonObject body;
    body["model"] = model();
    body["messages"] = messages;
    body["stream"] = false;

    QJsonDocument doc(body);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    QNetworkReply *reply = manager.post(request, data);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        QString errorText = reply->errorString();
        QByteArray errorBody = reply->readAll();

        qDebug() << "DeepSeek API request failed:" << errorText;
        qDebug() << "HTTP status:"
             << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "Response body:" << errorBody;

        reply->deleteLater();

        return "调用 DeepSeek API 失败：" + errorText;
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "Failed to parse DeepSeek response:" << parseError.errorString();
        return "DeepSeek 返回内容解析失败。";
    }

    QJsonObject responseObj = responseDoc.object();
    QJsonArray choices = responseObj.value("choices").toArray();

    if (choices.isEmpty()) {
        qDebug() << "DeepSeek response has no choices:" << responseData;
        return "DeepSeek 没有返回有效回复。";
    }

    QJsonObject firstChoice = choices.first().toObject();
    QJsonObject messageObj = firstChoice.value("message").toObject();

    QString content = messageObj.value("content").toString().trimmed();

    if (content.isEmpty()) {
        return "DeepSeek 返回了空回复。";
    }

    return content;
}