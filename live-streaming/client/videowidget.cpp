#include "videowidget.h"
#include <QPainter>

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);      // 自绘，省掉背景擦除
    setMinimumSize(320, 180);
    setStyleSheet("background-color: black;");
}

void VideoWidget::setFrame(const QImage &img)
{
    m_img = img;
    update();
}

void VideoWidget::setStrategy(std::unique_ptr<Renderstrategy> s)
{
    m_strategy=std::move(s);
    update();
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if(m_img.isNull()) return;

    QRect dst = m_strategy->targetRect(m_img.size(), size());   // 原来手写的等比逻辑删掉，换这句

    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(dst, m_img);
}
