#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include<QPainter>
#include<QVideoFrame>
#include"camera.h"
#include<QImage>
#include<QElapsedTimer>
extern "C"{
#include<libavcodec/avcodec.h>
#include<libswscale/swscale.h>
#include<libswresample/swresample.h>
#include<libavformat/avformat.h>
#include<libavutil/opt.h>
}

#include<qdebug.h>
#include"audioformat.h"
#include<QMessageBox>
#include<QElapsedTimer>
#include"streamlane.h"
#include"av.h"
QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;
    void paintEvent(QPaintEvent *event) override;

private:
    Ui::Widget *ui;
    void extracted(int &retsend);
    void upVideo(QVideoFrame frame);
    void setupAudio();
    std::unique_ptr<Camera> camera;
    QImage currentImage;

    //创建麦克风
    std::unique_ptr<AudioFormat> au;
    //麦克风缓冲区
    void onAuReadAll();

    AV *worker=nullptr;

    QElapsedTimer t;
    int i=0;
};
#endif // WIDGET_H
