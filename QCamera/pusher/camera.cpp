#include "camera.h"

Camera::Camera(QObject *parent)
    : QObject{parent}
{
    QCamera *cameras = new QCamera(this);
    QMediaCaptureSession *sessions = new QMediaCaptureSession(this);
    QVideoSink *sinks = new QVideoSink(this);
    for (const QCameraFormat &f : cameras->cameraDevice().videoFormats()) {
        // for (const QCameraFormat &f : cameras->cameraDevice().videoFormats())
        //     qDebug() << f.resolution() << f.minFrameRate() << "-" << f.maxFrameRate();
        if (f.resolution() == QSize(1280,720) && f.maxFrameRate() >= 30.0) {
            cameras->setCameraFormat(f);
            break;
        }
    }
    // qDebug() << "设后格式:" << cameras->cameraFormat().resolution()
    //          << cameras->cameraFormat().maxFrameRate();
    this->camera.reset(cameras);
    this->sink.reset(sinks);
    this->session.reset(sessions);

    session.get()->setCamera(camera.get());
    session.get()->setVideoSink(sink.get());
    camera.get()->start();
}
