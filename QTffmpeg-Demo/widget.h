#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QThread>
#include <QMessageBox>
#include <QTimer>
#include <QImage>
#include <QKeyEvent>
#include <QPainter>
#include <QSlider>
#include <QLabel>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QListWidget>
#include <QPushButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>

#include "framequeue.h"
#include "decoderworker.h"
#include "qaudio.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    // ---- 播放控制 ----
    void onPlaybackTick();               // 定时器回调：取帧→同步→显示
    void seek();                         // 进度条松开 → seekTo
    void seekTo(int64_t targetUs);       // 统一的 seek 入口
    void addToPlaylist(const QStringList &files);

    Ui::Widget *ui = nullptr;

    // ---- 解码线程 ----
    FrameQueue   *m_videoQueue    = nullptr;
    DecoderWorker *m_worker       = nullptr;
    QThread      *m_decoderThread = nullptr;

    // ---- 播放状态 ----
    QTimer *m_playbackTimer = nullptr;
    bool    m_paused        = false;

    // ---- 音视频同步 ----
    QImage    m_currentImage;             // 当前显示帧
    QImage    m_pendingFrame;             // 太早到达暂存的帧
    long long m_pendingPts = -1;          // 暂存帧的 PTS（微秒），-1 表示空

    // ---- 进度条 / Seek ----
    QSlider *m_seekSlider     = nullptr;
    QLabel  *m_currentTimeLbl = nullptr;  // 当前时间 "0:00"
    QLabel  *m_durationLbl    = nullptr;  // 总时长   "0:00"
    bool     m_isSeeking      = false;
    bool     m_justSeeked     = false;    // seek 后跳过第一帧的丢帧检查

    // ---- 音量 ----
    QSlider *m_volumeSlider = nullptr;
    QLabel  *m_volumeLabel  = nullptr;

    // ---- 倍速 ----
    QLabel *m_speedLabel    = nullptr;
    double  m_playbackSpeed = 1.0;        // 当前倍率

    // ---- 音频 ----
    AudioPlayer *m_audioPlayer = nullptr;
    bool         m_audioNeedsInit = false; // 是否需要重新创建 AudioPlayer

    // ---- 播放列表 ----
    QListWidget *m_playlistWidget = nullptr;
    QPushButton *m_playlistBtn    = nullptr;
    QPushButton *m_addFilesBtn    = nullptr;
    bool         m_playlistVisible = false;

private slots:
    void onAudioReady(QByteArray pcm, int channels, int sampleRate);
    void onDurationReady(double seconds);
    void onOpenFailed(const QString &msg);
    void onPlaybackFinished();           // 视频播完

signals:
    void requestSeek(int64_t targetUs);
    void speedChanged(double speed);
    void fileOpened(const QString &path);
};

#endif // WIDGET_H
