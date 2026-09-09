#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    setupUI();

    time=std::make_unique<QTimer>(this);
    time->setInterval(30);
    utime=std::make_unique<QTimer>(this);
    utime->setInterval(10);

    time->start();
    utime->start();

    deio=std::make_unique<Avdeio>(url,this);
    au = std::make_unique<AudioSinke>(this);
    au->strat();   // 不调这行，sink 也是 nullptr

    connectDeio();
    //视频播放
    connect(time.get(),&QTimer::timeout,this,&Widget::streamstart);
    //音频播放
    connect(utime.get(),&QTimer::timeout,this,[this](){
        if(pause){
            while(!deio->m_Queue->isEmpty1())
            {
                deio->m_Queue->pop1();
            }
            return;
        }
        while(!deio->m_Queue->isEmpty1()&&au->byteFree()>4096){
            AudioData data=deio->m_Queue->pop1();
            au->addPcm(data.data.constData(),data.data.size());

            if(clockBasePts==0)
            {
                clockBasePts=data.pts_us;
                procBace=au->processedUSecs();
            }
            if(!headpst)
            {
                headpst=true;
                updateLoading();
            }
        }
        if(clockBasePts!=0){
            audioClock=clockBasePts+(au->processedUSecs()-procBace);
        }
    });
    deio->start();
    updateLoading();
    t.start();
}

Widget::~Widget()
{
    deio->requestInterruption();   // 设中断标志
    deio->wait();                  // 等线程真正退出，再销毁
    delete ui;
}


void Widget::start(QImage image)
{
    if(image.isNull()) return;
    videoWidget->setFrame(image);
}

void Widget::setupUI()
{
    resize(1280, 720);
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 视频区：grid 同一格放画布+齿轮，按钮浮在右上角
    auto *videoWrap = new QWidget(this);
    auto *grid = new QGridLayout(videoWrap);
    grid->setContentsMargins(0, 10, 0, 10);   // 上下各留 10px
    grid->setSpacing(0);

    videoWidget = std::make_unique<VideoWidget>(videoWrap);

    btnSyst = std::make_unique<QToolButton>(videoWrap);
    btnSyst->setText("⚙");
    btnSyst->setFixedSize(32, 32);
    btnSyst->setCursor(Qt::PointingHandCursor);
    QFont f = btnSyst->font();
    f.setPixelSize(18);
    btnSyst->setFont(f);
    btnSyst->setStyleSheet(
        "QToolButton { color: white; background: rgba(0,0,0,120); border: none; border-radius: 16px; }"
        "QToolButton:hover { background: rgba(70,70,70,180); }");
    connect(btnSyst.get(),&QToolButton::clicked,this,&Widget::setStystDialog);

    btnPause = std::make_unique<QToolButton>(videoWrap);
    btnPause->setText("⏸");
    btnPause->setFixedSize(40, 40);
    btnPause->setCursor(Qt::PointingHandCursor);
    QFont pf = btnPause->font();
    pf.setPixelSize(20);
    btnPause->setFont(pf);
    btnPause->setStyleSheet(
        "QToolButton { color: white; background: rgba(0,0,0,120); border: none; border-radius: 20px; }"
        "QToolButton:hover { background: rgba(70,70,70,180); }");

    grid->addWidget(videoWidget.get(), 0, 0);
    grid->addWidget(btnSyst.get(), 0, 0, Qt::AlignTop | Qt::AlignRight);
    grid->addWidget(btnPause.get(), 0, 0, Qt::AlignBottom | Qt::AlignLeft);

    //清晰度切换按钮：浮在视频左上角
    auto styleQuality = [](QToolButton *b){
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setCheckable(true);
        b->setStyleSheet(
            "QToolButton { color: white; background: rgba(0,0,0,120); border: none; border-radius: 12px; padding: 4px 14px; }"
            "QToolButton:hover { background: rgba(70,70,70,180); }"
            "QToolButton:checked { background: #4fc3f7; color: #101010; }");
    };
    btnHD = std::make_unique<QToolButton>(videoWrap);
    btnHD->setText("高清");
    styleQuality(btnHD.get());
    btnSD = std::make_unique<QToolButton>(videoWrap);
    btnSD->setText("标清");
    styleQuality(btnSD.get());
    btnLD = std::make_unique<QToolButton>(videoWrap);
    btnLD->setText("流畅");
    styleQuality(btnLD.get());

    rdoQuality = std::make_unique<QButtonGroup>(this);
    rdoQuality->addButton(btnHD.get(), 0);
    rdoQuality->addButton(btnSD.get(), 1);
    rdoQuality->addButton(btnLD.get(), 2);
    btnHD->setChecked(true);

    connect(rdoQuality.get(), &QButtonGroup::idClicked, this, [this](int id){
        const char* streams[3] = {
            "rtmp://127.0.0.1:1935/live/stream_720p",
            "rtmp://127.0.0.1:1935/live/stream_360p",
            "rtmp://127.0.0.1:1935/live/stream_180p",
        };
        switchStream(QString::fromLatin1(streams[id]));
    });

    auto *qualityBar = new QHBoxLayout;
    qualityBar->setSpacing(6);
    qualityBar->addWidget(btnHD.get());
    qualityBar->addWidget(btnSD.get());
    qualityBar->addWidget(btnLD.get());
    grid->addLayout(qualityBar, 0, 0, Qt::AlignTop | Qt::AlignLeft);

    mainLayout->addWidget(videoWrap, 1);

    //Dialog 渲染
    Urlname = std::make_unique<QLineEdit>();
    Urlname->setPlaceholderText("rtmp://127.0.0.1:1935/live/stream");
    btnconnect = std::make_unique<QPushButton>("连接");

    auto *topBar = new QHBoxLayout;
    topBar->addWidget(Urlname.get(), 1);
    topBar->addWidget(btnconnect.get());

    // 画面格式单选：注意用 parent 管理，不用 unique_ptr（原因见下面提醒）
    auto *rdoAspect  = new QRadioButton("等比", &dialogSyst);
    auto *rdoStretch = new QRadioButton("拉伸", &dialogSyst);
    auto *rdoCrop    = new QRadioButton("裁剪", &dialogSyst);
    rdoAspect->setChecked(true);                 // 默认等比（现在的样子）

    rdoGroup = std::make_unique<QButtonGroup>(this);
    rdoGroup->addButton(rdoAspect,  0);
    rdoGroup->addButton(rdoStretch, 1);
    rdoGroup->addButton(rdoCrop,    2);
    connect(rdoGroup.get(), &QButtonGroup::idClicked, this, &Widget::setRenderMode);

    auto *fmtBar = new QHBoxLayout;
    fmtBar->addWidget(new QLabel("画面格式:", &dialogSyst));
    fmtBar->addWidget(rdoAspect);
    fmtBar->addWidget(rdoStretch);
    fmtBar->addWidget(rdoCrop);

    auto *mainCol = new QVBoxLayout;
    mainCol->addLayout(topBar);
    mainCol->addLayout(fmtBar);
    dialogSyst.setLayout(mainCol);
    dialogSyst.setLayout(topBar);

    connect(btnconnect.get(), &QPushButton::clicked, this, [this](){
        QString t = Urlname->text().trimmed();
        if(t.isEmpty()){
            QMessageBox::critical(this, "错误", "提交地址有误");
            return;
        }
        switchStream(t);
    });
    connect(btnPause.get(),&QToolButton::clicked,this,&Widget::setStop);

    btnSyst->setFocusPolicy(Qt::NoFocus);//让控件不抢焦点
    btnPause->setFocusPolicy(Qt::NoFocus);

    loadingBar = std::make_unique<QProgressBar>(videoWrap);
    loadingBar->setRange(0, 0);                 // 0~0 = 无限忙碌态
    loadingBar->setFixedWidth(240);
    loadingBar->setTextVisible(false);          // 不显示百分比数字
    loadingBar->setAttribute(Qt::WA_TransparentForMouseEvents); // 不挡齿轮/暂停按钮
    loadingBar->setStyleSheet(
        "QProgressBar { background: transparent; border: none; }"
        "QProgressBar::chunk { background: #4fc3f7; border-radius: 3px; }");
    grid->addWidget(loadingBar.get(), 0, 0, Qt::AlignCenter);

    // 音量 + 静音
    volSlider = new QSlider(Qt::Horizontal, videoWrap);
    volSlider->setRange(0, 100);
    volSlider->setValue(80);
    volSlider->setFixedWidth(120);
    connect(volSlider, &QSlider::valueChanged, this, [this](int v) {
        muted = false;
        btnMute->setText("🔊");
        au->setVolume(v / 100.0);
    });
    btnMute = new QToolButton(videoWrap);
    btnMute->setText("🔊");
    btnMute->setCursor(Qt::PointingHandCursor);
    connect(btnMute, &QToolButton::clicked, this, [this] {
        muted = !muted;
        btnMute->setText(muted ? "🔇" : "🔊");
        au->setVolume(muted ? 0.0 : volSlider->value() / 100.0);
    });
    auto *volBar = new QHBoxLayout;
    volBar->addWidget(btnMute);
    volBar->addWidget(volSlider);
    grid->addLayout(volBar, 0, 0, Qt::AlignBottom | Qt::AlignRight);
}

void Widget::setStystDialog()
{
    dialogSyst.setMaximumSize(300, 200);
    dialogSyst.exec();
}

void Widget::setStop()
{
    pause=!pause;
    btnPause->setText(pause ? "▶" : "⏸");
    if(!pause){          // 恢复播放：让下一帧音频重新锁基准
        clockBasePts = 0;
        procBace    = 0;
    }
    updateLoading();
}

void Widget::streamstart()
{
    if(pause) {
        if(!deio->m_Queue->isEmpty()) deio->popFrame();
        return;
    }
    // 音频还没就绪：丢帧防积压，不显示（以后这里放加载动画）
    if(!headpst) {
        if(!deio->m_Queue->isEmpty()) {
            deio->popFrame();   // 消费掉，防止视频队列满阻塞解码线程
        }
        return;
    }
    if(images)//是否有缓存帧判断
    {
        if(q.pts_us<audioClock-100000)//缓存帧是否过慢了
        {
            q=FrameData();
            images=false;
            return;
        }
        start(q.image);
        q=FrameData();
        images=false;
        return;
    }
    if(deio->m_Queue->isEmpty()) return;
    FrameData data=deio->popFrame();
    //qDebug() << "video pts:" << data.pts_us << "clock:" << audioClock;
    if(data.pts_us<audioClock-100000) return;
    if(data.pts_us>audioClock+3000)
    {
        q=data;
        images=true;
        return;
    }
    start(data.image);
}



void Widget::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape)
        toggleFullScreen();
    if(event->key()==Qt::Key_Space)
        setStop();
}



void Widget::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    toggleFullScreen();
}

void Widget::toggleFullScreen()
{
    if(isFullScreen()){
        showNormal();
        btnSyst->show();
    }
    else{
        showFullScreen();
        btnSyst->hide();
    }
}

void Widget::setRenderMode(int id)//切换画面格式
{
    switch(id){
    case 0: videoWidget->setStrategy(std::make_unique<KeepAspectStrategy>()); break;
    case 1: videoWidget->setStrategy(std::make_unique<StretchStrategy>());    break;
    case 2: videoWidget->setStrategy(std::make_unique<CropStrategy>());       break;
    }
}

void Widget::connectDeio()//画面卡住 混在连接失败
{
    connect(deio.get(),&Avdeio::connectFail,this,[this](){
        QMessageBox::warning(this,"提示","连接失败 正在重连");
    });
    connect(deio.get(), &Avdeio::reconnecting, this, [this]{
        headpst = false;
        updateLoading();
    });
}

void Widget::switchStream(const QString &newUrl)//切换流
{
    t.restart();
    tstart=true;
    if(newUrl == url) return;
    url = newUrl;
    Urlname->setText(url);
    deio.reset();
    deio = std::make_unique<Avdeio>(url, this);
    connectDeio();
    headpst = false;
    audioClock = 0;
    clockBasePts = 0;
    procBace = 0;
    images = false;
    q = FrameData();
    deio->start();
    updateLoading();
}

void Widget::updateLoading()
{
    loadingBar->setVisible((!headpst) || pause);
}

