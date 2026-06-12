#include "MessageBubbleWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QFontMetrics>

namespace
{
int calculateBubbleWidth(const QString& content)
{
    QFont font;
    font.setFamily("Microsoft YaHei");
    font.setPointSize(10);

    QFontMetrics metrics(font);

    int maxLineWidth = 0;

    const QStringList lines = content.split('\n');

    for (const QString& line : lines) {
        maxLineWidth = qMax(maxLineWidth, metrics.horizontalAdvance(line));
    }

    int width = maxLineWidth + 36;

    width = qMax(width, 72);
    width = qMin(width, 460);

    return width;
}
}

MessageBubbleWidget::MessageBubbleWidget(const QString& senderName,
                                         const QString& content,
                                         const QString& timeText,
                                         Role role,
                                         QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *rowLayout = new QHBoxLayout(this);
    rowLayout->setContentsMargins(8, 6, 8, 6);
    rowLayout->setSpacing(8);

    QWidget *bubbleColumn = new QWidget(this);
    QVBoxLayout *columnLayout = new QVBoxLayout(bubbleColumn);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(4);

    QLabel *senderLabel = new QLabel(senderName, bubbleColumn);
    senderLabel->setStyleSheet(
        "color: #9a9a9a;"
        "font-size: 12px;"
    );

    QLabel *bubbleLabel = new QLabel(bubbleColumn);
    bubbleLabel->setWordWrap(true);
    bubbleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bubbleLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    QString safeContent = content.toHtmlEscaped();
    safeContent.replace("\n", "<br>");

    int bubbleWidth = calculateBubbleWidth(content);
    bubbleLabel->setFixedWidth(bubbleWidth);

    bubbleLabel->setTextFormat(Qt::RichText);
    bubbleLabel->setText(safeContent);

    QLabel *timeLabel = new QLabel(timeText, bubbleColumn);
    timeLabel->setStyleSheet("color: #777777;""font-size: 11px;");

    // 时间文本通常比短消息气泡更宽，所以单独计算一下面板宽度
    QFontMetrics timeMetrics(timeLabel->font());
    int timeWidth = timeMetrics.horizontalAdvance(timeText) + 8;

    int columnWidth = qMax(bubbleWidth, timeWidth);

    bubbleColumn->setFixedWidth(columnWidth);
    senderLabel->setFixedWidth(columnWidth);
    timeLabel->setFixedWidth(columnWidth);

    if (role == Role::Mine) {
        senderLabel->setText("我");
        senderLabel->setAlignment(Qt::AlignRight);
        timeLabel->setAlignment(Qt::AlignRight);

        bubbleLabel->setStyleSheet(
            "QLabel {"
            "background-color: #3d7eff;"
            "color: white;"
            "border-radius: 12px;"
            "padding: 8px 12px;"
            "font-size: 14px;"
            "line-height: 150%;"
            "}"
        );

    columnLayout->addWidget(senderLabel);
    columnLayout->addWidget(bubbleLabel, 0, Qt::AlignRight);
    columnLayout->addWidget(timeLabel); 

        rowLayout->addStretch();
        rowLayout->addWidget(bubbleColumn);
    } else if (role == Role::Ai) {
        senderLabel->setText("AI 助手");
        senderLabel->setAlignment(Qt::AlignLeft);
        timeLabel->setAlignment(Qt::AlignLeft);

        senderLabel->setStyleSheet(
            "color: #8ab4ff;"
            "font-size: 12px;"
            "font-weight: bold;"
        );

        bubbleLabel->setStyleSheet(
            "QLabel {"
            "background-color: #2f3b4a;"
            "color: #eeeeee;"
            "border-radius: 12px;"
            "padding: 8px 12px;"
            "font-size: 14px;"
            "line-height: 150%;"
            "}"
        );

        columnLayout->addWidget(senderLabel);
        columnLayout->addWidget(bubbleLabel);
        columnLayout->addWidget(timeLabel);

        rowLayout->addWidget(bubbleColumn);
        rowLayout->addStretch();
    } else {
        senderLabel->setAlignment(Qt::AlignLeft);
        timeLabel->setAlignment(Qt::AlignLeft);

        bubbleLabel->setStyleSheet(
            "QLabel {"
            "background-color: #343434;"
            "color: #eeeeee;"
            "border-radius: 12px;"
            "padding: 8px 12px;"
            "font-size: 14px;"
            "line-height: 150%;"
            "}"
        );

        columnLayout->addWidget(senderLabel);
        columnLayout->addWidget(bubbleLabel);
        columnLayout->addWidget(timeLabel);

        rowLayout->addWidget(bubbleColumn);
        rowLayout->addStretch();
    }
}