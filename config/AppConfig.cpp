#include "AppConfig.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

QString AppConfig::configPath()
{
    QStringList args = QCoreApplication::arguments();

    int index = args.indexOf("--config");

    if (index >= 0 && index + 1 < args.size()) {
        QString path = args.at(index + 1);

        QFileInfo fileInfo(path);

        if (fileInfo.isAbsolute()) {
            return fileInfo.absoluteFilePath();
        }

        return QCoreApplication::applicationDirPath() + "/" + path;
    }

    return QCoreApplication::applicationDirPath() + "/config.ini";
}

QString AppConfig::value(const QString& key, const QString& defaultValue)
{
    QSettings settings(configPath(), QSettings::IniFormat);

    return settings.value(key, defaultValue)
        .toString()
        .trimmed();
}