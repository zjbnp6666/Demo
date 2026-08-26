#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QImage>
#include"renderstrategy.h"
class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    void setFrame(const QImage &img);
    void setStrategy(std::unique_ptr<Renderstrategy> s);
    std::unique_ptr<Renderstrategy> m_strategy=std::make_unique<KeepAspectStrategy>();
protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_img;
};

#endif // VIDEOWIDGET_H
