#ifndef MESSAGEBUBBLEWIDGET_H
#define MESSAGEBUBBLEWIDGET_H

#include <QWidget>
#include <QString>

class MessageBubbleWidget : public QWidget
{
public:
    enum class Role
    {
        Mine,
        Other,
        Ai
    };

    explicit MessageBubbleWidget(const QString& senderName,
                                 const QString& content,
                                 const QString& timeText,
                                 Role role,
                                 QWidget *parent = nullptr);
};

#endif // MESSAGEBUBBLEWIDGET_H