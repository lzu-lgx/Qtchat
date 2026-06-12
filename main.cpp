#include "mainwindow.h"
#include <QFile>
#include <QDebug>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    QFile styleFile(":/styles/app.qss");

    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QString::fromUtf8(styleFile.readAll());
        a.setStyleSheet(styleSheet);
        qDebug() << "Loaded app.qss";
    } 
    else {
        qDebug() << "Failed to load app.qss";
    }
    
    MainWindow w;
    w.show();
    return QApplication::exec();
}
