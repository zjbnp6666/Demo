#ifndef CAMERA_H
#define CAMERA_H

#include <QObject>
#include<QCamera>
#include<QVideoSink>
#include<QMediaCaptureSession>
#include<memory.h>
class Camera : public QObject
{
    Q_OBJECT
public:
    explicit Camera(QObject *parent = nullptr);
    std::unique_ptr<QVideoSink> sink;
private:
    std::unique_ptr<QCamera> camera;
    std::unique_ptr<QMediaCaptureSession> session;
signals:
};

#endif // CAMERA_H
