#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>

class AppConfig
{
public:
    static QString configPath();

    static QString value(const QString& key,const QString& defaultValue = QString());

    static void setRuntimeValue(const QString& key, const QString& value);
};

#endif // APPCONFIG_H