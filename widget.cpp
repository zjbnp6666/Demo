#include "widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    // ---- 非原生文件对话框（可套 QSS）----
    QFileDialog dlg(this, "选择视频", "",
                    "视频文件(*.mp4)");
    dlg.setOption(QFileDialog::DontUseNativeDialog);
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setStyleSheet(R"(
        QFileDialog { background-color: #0a0a0f; }
        QLabel { color: #6c7086; font-size: 11px; }
        QLineEdit { background: #1e1e2e; color: #cccccc; border: 1px solid #333; border-radius: 3px; padding: 3px; }
        QPushButton { background: #1e1e2e; color: #cccccc; border: 1px solid #333; border-radius: 3px; padding: 4px 12px; }
        QPushButton:hover { background: #00d4ff; color: #0a0a0f; }
        QListView { background: #0a0a0f; color: #cccccc; border: none; outline: none; }
        QListView::item { padding: 5px 8px; }
        QListView::item:hover { background: #1e1e2e; }
        QListView::item:selected { background: #00d4ff; color: #0a0a0f; }
    )");
    if (dlg.exec() != QDialog::Accepted) return;
    QString filepath = dlg.selectedFiles().first();
    setFocusPolicy(Qt::StrongFocus);   // 接收键盘事件（空格暂停）

    // ---- 多线程解码初始化 ----
    videoQueue = new FrameQueue;
    worker = new DecoderWorker(videoQueue);
    decoderThread = new QThread(this);
    worker->moveToThread(decoderThread);

    connect(decoderThread, &QThread::started, worker, [=]() { worker->open(filepath); });
    connect(worker, &DecoderWorker::audioReady, this, &Widget::onAudioReady);
    connect(worker, &DecoderWorker::durationReady, this, &Widget::onDurationReady);
    connect(worker, &DecoderWorker::openFailed, this, &Widget::onOpenFailed);
    connect(decoderThread, &QThread::finished, worker, &QObject::deleteLater);

    // ---- UI 控件 ----
    times = new QTimer(this);
    timesize = new QSlider(Qt::Horizontal, this);
    zero = new QLabel(this);
    stop = new QLabel(this);
    timesize->setValue(0);
    timesize->setRange(0, 100); // 占位，真实范围在 onDurationReady 设
    zero->setText("0:00");
    stop->setText("0:00");
    // 垂直音量滑条：右侧
    volSlider = new QSlider(Qt::Vertical, this);
    volSlider->setRange(0, 100);
    volSlider->setValue(80);
    volLabel = new QLabel("Vol", this);

    // 倍速标签
    speedLabel = new QLabel("1.0x", this);
    speedLabel->setStyleSheet(R"(
        QLabel {
            color: #00d4ff;
            font-size: 13px;
            font-weight: bold;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(0,212,255,0.15), stop:1 rgba(123,47,247,0.15));
            border: 1px solid #00d4ff;
            border-radius: 4px;
            padding: 3px 10px;
        }
    )");
    speedLabel->adjustSize();

    // 播放列表按钮
    playlistBtn = new QPushButton("≡ 列表", this);
    playlistBtn->setStyleSheet(R"(
        QPushButton {
            color: #00d4ff;
            font-size: 12px;
            font-weight: bold;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(0,212,255,0.15), stop:1 rgba(123,47,247,0.15));
            border: 1px solid #00d4ff;
            border-radius: 4px;
            padding: 3px 10px;
        }
        QPushButton:hover {
            background: rgba(0,212,255,0.3);
            border-color: #ffffff;
            color: #ffffff;
        }
    )");
    playlistBtn->adjustSize();

    // 添加文件按钮
    addFilesBtn = new QPushButton("+", this);
    addFilesBtn->setStyleSheet(R"(
        QPushButton {
            color: #00d4ff;
            font-size: 14px;
            font-weight: bold;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(0,212,255,0.15), stop:1 rgba(123,47,247,0.15));
            border: 1px solid #00d4ff;
            border-radius: 4px;
            padding: 3px 8px;
        }
        QPushButton:hover {
            background: rgba(0,212,255,0.3);
            border-color: #ffffff;
            color: #ffffff;
        }
    )");
    addFilesBtn->adjustSize();

    // 播放列表面板
    playlist = new QListWidget(this);
    playlist->setDragDropMode(QAbstractItemView::InternalMove);
    playlist->setDefaultDropAction(Qt::MoveAction);
    playlist->setSelectionMode(QAbstractItemView::SingleSelection);
    playlist->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    playlist->hide();

    // 允许从外部拖入文件
    setAcceptDrops(true);

    // ---- QSS 美化 ----
    setStyleSheet(R"(
        Widget {
            background-color: #0a0a0f;
        }

        /* ── 水平进度条 ── */
        QSlider::groove:horizontal {
            height: 3px;
            background: #1e1e2e;
            border-radius: 1px;
        }
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #00d4ff, stop:1 #7b2ff7);
            border-radius: 1px;
        }
        QSlider::handle:horizontal {
            width: 14px;
            height: 14px;
            margin: -6px 0;
            background: #ffffff;
            border: 2px solid #00d4ff;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: #00d4ff;
            border-color: #ffffff;
        }

        /* ── 垂直音量条 ── */
        QSlider::groove:vertical {
            width: 3px;
            background: #1e1e2e;
            border-radius: 1px;
        }
        QSlider::sub-page:vertical {
            background: qlineargradient(x1:0, y1:1, x2:0, y2:0,
                stop:0 #00d4ff, stop:1 #7b2ff7);
            border-radius: 1px;
        }
        QSlider::handle:vertical {
            width: 14px;
            height: 14px;
            margin: 0 -6px;
            background: #ffffff;
            border: 2px solid #00d4ff;
            border-radius: 7px;
        }
        QSlider::handle:vertical:hover {
            background: #00d4ff;
            border-color: #ffffff;
        }

        /* ── 时间标签 ── */
        QLabel {
            color: #6c7086;
            font-size: 11px;
            font-family: "Consolas", "Courier New", monospace;
            background: transparent;
        }

        /* ── 播放列表 ── */
        QListWidget {
            background: rgba(10,10,30,0.95);
            border: 1px solid rgba(255,255,255,0.1);
            border-radius: 8px;
            outline: none;
        }
        QListWidget::item {
            color: #999;
            padding: 8px 12px;
            border-bottom: 1px solid rgba(255,255,255,0.05);
        }
        QListWidget::item:hover {
            background: rgba(255,255,255,0.06);
            color: #ddd;
        }
        QListWidget::item:selected {
            background: rgba(0,212,255,0.15);
            color: #00d4ff;
            border-left: 2px solid #00d4ff;
        }
    )");

    // ---- UI 布局（resize 之后 width/height 才正确）----
    this->resize(800, 600);
    // ---- 信号连接 ----
    connect(times, &QTimer::timeout, this, &Widget::strat);

    // 进度条值变化 → 更新时间标签（拖动时跟着变）
    connect(timesize, &QSlider::valueChanged, this, [=](int val) {
        int m = val / 60;
        int s = val % 60;
        zero->setText(QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0')));
    });

    // 用户按下/松开滑块 → seek
    connect(timesize, &QAbstractSlider::sliderPressed,  this, [=]() { m_isSeeking = true; });
    connect(timesize, &QAbstractSlider::sliderReleased, this, static_cast<void(Widget::*)()>(&Widget::seek));
    connect(volSlider,&QSlider::valueChanged,this,[=]()
            {
        int val=volSlider->value();
        if(audio){
        audio->setVolume(val/100.0);
        }
    });
    connect(worker,&DecoderWorker::closeFrame,this,&Widget::closeOver);
    connect(this, &Widget::requestSeek, worker, &DecoderWorker::seek);
    connect(this,&Widget::speedvalue,worker,&DecoderWorker::setSpeed);
    connect(playlistBtn, &QPushButton::clicked, this, [=]() {
        playlistVisible = !playlistVisible;
        playlist->setVisible(playlistVisible);
        addFilesBtn->setVisible(playlistVisible);
    });
    connect(addFilesBtn, &QPushButton::clicked, [=]() {
        QStringList files = QFileDialog::getOpenFileNames(this, "添加视频",
            "", "视频文件(*.mp4 *.mkv *.avi *.mov *.flv)");
        if (!files.isEmpty()) addToPlaylist(files);
    });
    connect(playlist, &QListWidget::itemDoubleClicked, this, [=](QListWidgetItem *item) {
        if(!item)
            {
            return;
        }
        QString filenames=item->data(Qt::UserRole).toString();
        times->stop();
        if(audio){
            delete audio;
            audio=nullptr;
        }
        newaudio=false;
        pendingFrame=QImage();
        pendingPts = -1;
        videoQueue->clear();
        emit openstart(filenames);
        image=QImage();
        justSeeket=false;
        paused=false;
    });
    connect(this,&Widget::openstart,worker,&DecoderWorker::open);
    QWidget::resizeEvent(nullptr);
    decoderThread->start();
}

Widget::~Widget() {
    times->stop();
    if (decoderThread) {
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
        decoderThread->quit();
        decoderThread->wait();
    }
    if (audio) {
        audio->stop();
        delete audio;
    }
    delete videoQueue;
    pendingFrame = QImage();
    image = QImage();
}

// ========== 每帧回调：从队列取帧 → 音视频同步 → 显示 ==========
void Widget::strat()
{
    if (m_isSeeking) return;
    if (paused) return;

    // 更新进度条
    timesize->setValue(worker->audioClock / 1000000);

    //跳转进度条更新
    // 检查缓存帧是否已到显示时间
    if (!pendingFrame.isNull() && pendingPts <= worker->audioClock + 30000) {
        image = pendingFrame;
        pendingFrame = QImage();
        update();
    }
    // 从队列取帧
    FrameData data = videoQueue->pop(); // 阻塞等帧
    // 太晚 → 丢弃 还有特殊情况 使用进度条跳转
    if(justSeeket)
    {
        justSeeket=false;
    }
    else if (data.pts_us < worker->audioClock - 100000) return;
    // 太早 → 缓存
    if (data.pts_us > worker->audioClock + 30000) {
        pendingFrame = data.image;
        pendingPts = data.pts_us;
        return;
    }
    // 正好 → 显示
    image = data.image;
    pendingFrame = QImage();
    update();
}

// ========== 绘制视频画面 ==========
void Widget::paintEvent(QPaintEvent *)
{
    if (image.isNull()) return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    int m = 35;                                      // 底部留给进度条
    QRect target = rect().adjusted(0, 0, 0, -m);
    painter.drawImage(target, image);
}

// ========== 键盘事件 ==========
void Widget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space){
        paused = !paused;
    }
    if(event->key()==Qt::Key_Escape&&this->isFullScreen())
    {
        this->showNormal();
        timesize->show(); zero->show(); stop->show();
        volSlider->show(); volLabel->show();speedLabel->show();
        playlistBtn->show();
    }
    // S 截图
    if (event->key() == Qt::Key_S && !image.isNull())
    {
        QString imagePath=QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)+"/";
        imagePath+=QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        imagePath+="_截图.png";
        image.save(imagePath);
    }

    // —— 倍速快捷键 ——
    if (event->key() == Qt::Key_1)       // 1.0x
    {
        spedd=1.0;
        speedLabel->setText("1.0x");
        emit speedvalue(spedd);
        newaudio=false;
        times->setInterval(16/spedd);
    }
    else if (event->key() == Qt::Key_2)  // 1.5x
    {
        spedd=1.5;
        speedLabel->setText("1.5x");
        emit speedvalue(spedd);
        newaudio=false;
        times->setInterval(16/spedd);
    }
    else if (event->key() == Qt::Key_3)  // 2.0x
    {
        spedd=2.0;
        speedLabel->setText("2.0x");
        emit speedvalue(spedd);
        newaudio=false;
        times->setInterval(16/spedd);
    }
}

void Widget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // 进度条：水平，底部
    int barY = height() - 35;
    timesize->setGeometry(50, barY, width() - 100, 20);

    // 时间标签
    zero->move(10, barY + 2);
    stop->move(width() - 45, barY + 2);

    // 音量滑条：右下角垂直
    int volH = 100;
    volSlider->setGeometry(width() - 30, barY - volH, 20, volH);
    volLabel->move(width() - 28, barY - volH - 18);

    // 倍速标签：左下角
    speedLabel->move(10, barY - 26);

    // 播放列表按钮：倍速标签右侧
    playlistBtn->move(65, barY - 26);
    addFilesBtn->move(130, barY - 26);

    // 播放列表：右侧面板
    int plW = 280;
    playlist->setGeometry(width() - plW, 0, plW, barY - 4);

}

void Widget::mouseDoubleClickEvent(QMouseEvent *)
{
    if(this->isFullScreen())
    {
        this->showNormal();
        timesize->show(); zero->show(); stop->show();
        volSlider->show(); volLabel->show();
        speedLabel->show();playlistBtn->show();
    }
    else{
        this->showFullScreen();
        timesize->hide(); zero->hide(); stop->hide();
        volSlider->hide(); volLabel->hide();
        speedLabel->hide();playlistBtn->hide();
    }
}

void Widget::closeEvent(QCloseEvent *event)
{
    paused=true;
    videoQueue->clear();
    int ret=QMessageBox::question(this,"确认","确定要退出吗");
    if(ret==QMessageBox::Yes)
    {  event->accept();
    }else{
        paused=!paused;
        event->ignore();
    }
}

void Widget::onAudioReady(QByteArray pcm, int channels, int sampleRate)
{
    if(!newaudio)
    {
        if(audio)
        {
            delete audio;
            audio=nullptr;
        }
        audio=new AudioPlayer(sampleRate,channels);
        audio->start();
        newaudio=true;
    }
    const char* data=pcm.data();
    int len=pcm.size();
    audio->addPCM(data,len);
}

void Widget::onDurationReady(double seconds)
{
    timesize->setRange(0,(int)seconds);
    timesize->setValue(0);
    int fen=seconds/60;
    int miao=(int)seconds%60;
    stop->setText(QString("%1:%2").arg(fen).arg(miao, 2, 10, QChar('0')));
    times->setInterval(16);
    times->start();
}

void Widget::onOpenFailed(const QString &msg)
{
    QMessageBox::critical(this,"错误",msg,QMessageBox::Cancel);
    qApp->exit(1);
}

void Widget::closeOver()
{
    times->stop();
    int ret=QMessageBox::information(this,"警告","视频已经读完 是否重播",QMessageBox::Yes|QMessageBox::No);
    if(ret==QMessageBox::Yes)
    {
        times->start();
        seek(1);
    }
    else{
        this->hide();
        this->deleteLater();
    }
}

// ========== Seek：拖进度条松开后跳转 ==========
void Widget::seek(int ret)
{
    if(audio)
    {
        audio->stop();
        audio->start();
    }
    int64_t seekTarget=-1;
    pendingFrame = QImage();
    pendingPts = -1;
    videoQueue->clear();
    m_isSeeking = false;
    emit requestSeek(seekTarget);
    justSeeket=true;
}

void Widget::seek()
{
    if(audio)
    {
        audio->stop();
        audio->start();
    }
    int64_t seekTarget;
    seekTarget = (int64_t)(timesize->value()) * AV_TIME_BASE;
    pendingFrame = QImage();
    pendingPts = -1;
    m_isSeeking = false;
    videoQueue->clear();
    emit requestSeek(seekTarget);
    justSeeket=true;
}

// ========== 拖拽文件到播放列表 ==========
void Widget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void Widget::dropEvent(QDropEvent *event)
{
    QStringList files;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            files.append(url.toLocalFile());
    }
    if (!files.isEmpty())
        addToPlaylist(files);
}

void Widget::addToPlaylist(const QStringList &files)
{
    for (const QString &f : files) {
        QFileInfo fi(f);
        // 去重
        bool dup = false;
        for (int i = 0; i < playlist->count(); i++) {
            if (playlist->item(i)->data(Qt::UserRole).toString() == f) {
                dup = true;
                break;
            }
        }
        if (dup) continue;

        QListWidgetItem *item = new QListWidgetItem(fi.fileName());
        item->setData(Qt::UserRole, f);  // 存完整路径
        item->setToolTip(f);
        playlist->addItem(item);
    }
}
