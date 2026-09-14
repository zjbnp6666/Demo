#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    camera=std::make_unique<Camera>();
    connect(camera->sink.get(), &QVideoSink::videoFrameChanged, this, &Widget::upVideo);

    au = std::make_unique<AudioFormat>(44100, 2, this);
    au->start();
    if(!au->io){
        qDebug() << "麦克风打开失败 只推视频";
    }else{
        connect(au->io, &QIODevice::readyRead, this, &Widget::onAuReadAll);
    }

    worker=new AV;
    connect(worker,&QThread::finished,worker,&QObject::deleteLater);
    worker->start();
    t.start();
}
void Widget::onAuReadAll()
{
    QByteArray b=au->io->readAll();
    worker->appedAudio(b);
}


void Widget::upVideo(QVideoFrame vframe)
{
    if(!vframe.isValid()) return;
    vframe.map(QVideoFrame::ReadOnly);
    qDebug()<<"第"<<i<<"fps:"<<t.elapsed();

    i++;
    const uchar* srcData[2]={vframe.bits(0),vframe.bits(1)};
    int srcStride[2]={vframe.bytesPerLine(0),vframe.bytesPerLine(1)};
    // qDebug() << "fmt=" << int(vframe.surfaceFormat().pixelFormat())
    //          << "size=" << vframe.size();
    Nv12Frame f;
    f.width = vframe.size().width();
    f.height = vframe.size().height();
    f.strideY = vframe.bytesPerLine(0);
    f.strideUV = vframe.bytesPerLine(1);
    f.y.assign(vframe.bits(0), vframe.bits(0) + f.strideY * f.height);
    f.uv.assign(vframe.bits(1), vframe.bits(1) + f.strideUV * f.height /
                                                     2);
    worker->pushFrame(std::move(f));
    currentImage=vframe.toImage();
    update();

     vframe.unmap();
}
void Widget::paintEvent(QPaintEvent *event)
{
    if(currentImage.isNull()) return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawImage(rect(),currentImage);
    currentImage=QImage();
}
Widget::~Widget()
{
    camera.reset();
    worker->stopFrameQueue();
    worker->requestInterruption();
    worker->wait();
    delete ui;
}