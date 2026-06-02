#include <QCoreApplication>
#include <QDebug>

#include "ChatServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    ChatServer server;

    if (!server.startServer(8888)) {
        return -1;
    }

    return app.exec();
}