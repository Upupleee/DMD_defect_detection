#include "mainwindow.h"
#include "PixelType.h" // 引入海康像素格式的宏定义，用于后面的下拉框填充(确保工程里有该头文件)



MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
    m_bWaitingForDefectDetection(false),
    m_bWaitingForSerialDetection(false)
{
    resize(1200, 800);
    setWindowState(Qt::WindowMaximized);
    // 1. 初始化管理器
    m_visionManager = new VisionManager(this);
    m_cameraManager = new CameraManager(this);
    m_exportManager = new ExportManager(this); 
    
    m_dmdSettingDialog = nullptr; // 【新增】初始化

    // 加载 OCR 字典模型
    m_visionManager->initOCR(); 

    // 调用相机SDK的全局初始化
    m_cameraManager->initSystem();

    // 2. 构建界面
    createCentralWidget();
    createMenu();

    // 3. 绑定日志与图像信号
    connect(m_visionManager, &VisionManager::sendLog, logEdit, &QTextEdit::append);
    connect(m_cameraManager, &CameraManager::sendLog, logEdit, &QTextEdit::append);
    
    // Qt::AutoConnection 会自动识别跨线程并放入主线程队列，确保 UI 更新安全
    connect(m_cameraManager, &CameraManager::imageAcquired, this, &MainWindow::onImageAcquired);

    statusBar()->showMessage(tr("Ready"));
    QTimer::singleShot(0, this, &MainWindow::onFindDevices);

    connect(m_cameraManager, &CameraManager::cameraStateChanged, this, &MainWindow::onCameraStateChanged);

    // ======== 新增 DataServer 模块并绑定日志 ========
    m_dataServerManager = new DataServerManager(this);
    connect(m_dataServerManager, &DataServerManager::sendLog, logEdit, &QTextEdit::append);
    m_dataServerManager->initLibrary(); // 加载默认同级目录底下的 DLL

    // USB 发图模块
    m_dlpUsbManager = new DlpUsbManager(this);
    connect(m_dlpUsbManager, &DlpUsbManager::sendLog, logEdit, &QTextEdit::append);


    //光源控制模块
    m_lightManager = new LightManager(this);
    connect(m_lightManager, &LightManager::sendLog, logEdit, &QTextEdit::append);


}

MainWindow::~MainWindow()
{
    // 防止窗体关闭时相机还在取流导致崩溃
    m_cameraManager->closeCamera();
    m_cameraManager->finalizeSystem();
}

void MainWindow::createMenu()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("File"));
    QAction* importImageAction = fileMenu->addAction(tr("Import Image"));
    connect(importImageAction, &QAction::triggered, this, &MainWindow::onOpenImage);

    // 获取 Tools 菜单的指针并添加 Action
    QMenu* toolsMenu = menuBar()->addMenu(tr("Tools"));

    // 切换相机品牌
    QMenu* cameraMenu = toolsMenu->addMenu(tr("Switch Camera Brand"));
    QAction* hikAction = cameraMenu->addAction(tr("Hikvision (MVS)"));
    QAction* optAction = cameraMenu->addAction(tr("OPT (SciCam)"));

    connect(hikAction, &QAction::triggered, this, [this]() {
        m_cameraManager->setCameraBrand(CameraBrand::HIKVISION);
        onFindDevices();
        });
    connect(optAction, &QAction::triggered, this, [this]() {
        m_cameraManager->setCameraBrand(CameraBrand::OPT);
        onFindDevices();
        });

    QAction* dmdSettingsAction = toolsMenu->addAction(tr("DMD Parameter Settings"));
    connect(dmdSettingsAction, &QAction::triggered, this, &MainWindow::onOpenDMDSettings);

    // 新增：DataServer 设置弹窗
    QAction* dsSettingsAction = toolsMenu->addAction(tr("DataServer IP Settings"));
    connect(dsSettingsAction, &QAction::triggered, this, &MainWindow::onOpenDataServerSettings);

    // 新增：高级设置弹窗    
    QAction* advancedSettingsAction = toolsMenu->addAction(tr("Advanced Settings"));
    connect(advancedSettingsAction, &QAction::triggered, this, &MainWindow::onOpenAlgorithmAdvancedSettings);

    // 将"保存画了十字架标记坏点的原图"作为一个后台Action状态来维护，不再展示在Tool工具栏中
    m_actionSavePaintedImage = new QAction(tr("Save Painted Defect Images"), this);
    m_actionSavePaintedImage->setCheckable(true);
    m_actionSavePaintedImage->setChecked(true); // 默认开启

    toolsMenu->addSeparator();

    QMenu* helpMenu = menuBar()->addMenu(tr("Help"));
    QMenu* langMenu = helpMenu->addMenu(tr("Language (Restart Required)"));

    QAction* zhAction = langMenu->addAction("中文");
    QAction* enAction = langMenu->addAction("English");

    connect(zhAction, &QAction::triggered, this, &MainWindow::onSwitchLanguageToChinese);
    connect(enAction, &QAction::triggered, this, &MainWindow::onSwitchLanguageToEnglish);

	//光源控制设置
    QAction* lightSettingsAction = toolsMenu->addAction(tr("Light Controller Settings"));
    connect(lightSettingsAction, &QAction::triggered, this, &MainWindow::onOpenLightSettings);

    // USB 发图调试弹窗
    QAction* usbSenderAction = toolsMenu->addAction(tr("USB Image Sender Debug"));
    connect(usbSenderAction, &QAction::triggered, this, &MainWindow::onOpenUsbSenderDebug);

}

void MainWindow::createCentralWidget()
{
    QWidget* central = new QWidget(this);

    // =========================================================
    // 左栏上部 (原 Tool Flow 区)：控制面板布局
    // =========================================================
    QWidget* controlPanel = new QWidget();
    QVBoxLayout* controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(0, 0, 0, 0);

    // [区域 1：初始化相机]
    QGroupBox* grpInit = new QGroupBox(tr("Camera Initialization"), controlPanel);
    QVBoxLayout* layInit = new QVBoxLayout(grpInit);

    m_cbDeviceList = new QComboBox(grpInit);
    QPushButton* btnFindDevice = new QPushButton(tr("Find Devices"), grpInit);
    QPushButton* btnOpenCam = new QPushButton(tr("Open Camera"), grpInit);
    QPushButton* btnCloseCam = new QPushButton(tr("Close Camera"), grpInit);

    layInit->addWidget(m_cbDeviceList);
    layInit->addWidget(btnFindDevice);
    layInit->addWidget(btnOpenCam);
    layInit->addWidget(btnCloseCam);

    connect(btnFindDevice, &QPushButton::clicked, this, &MainWindow::onFindDevices);
    connect(btnOpenCam, &QPushButton::clicked, this, &MainWindow::onOpenCamera);
    connect(btnCloseCam, &QPushButton::clicked, this, &MainWindow::onCloseCamera);

    // [区域 2：图像采集功能]
    QGroupBox* grpAcquire = new QGroupBox(tr("Image Acquisition"), controlPanel);
    QVBoxLayout* layAcquire = new QVBoxLayout(grpAcquire);

    m_radioContinuous = new QRadioButton(tr("Continuous Mode"), grpAcquire);
    m_radioTrigger = new QRadioButton(tr("Trigger Mode"), grpAcquire);
    m_radioTrigger->setChecked(true);
    m_radioContinuous->setChecked(false);

    m_chkSoftwareTrigger = new QCheckBox(tr("Software Trigger Source"), grpAcquire);
    m_chkSoftwareTrigger->setChecked(true);

    QPushButton* btnStartGrab = new QPushButton(tr("Start Grabbing"), grpAcquire);
    QPushButton* btnStopGrab = new QPushButton(tr("Stop Grabbing"), grpAcquire);
    QPushButton* btnTriggerOnce = new QPushButton(tr("Software Trigger Once"), grpAcquire);

    layAcquire->addWidget(m_radioContinuous);
    layAcquire->addWidget(m_radioTrigger);
    layAcquire->addWidget(m_chkSoftwareTrigger);
    layAcquire->addWidget(btnStartGrab);
    layAcquire->addWidget(btnStopGrab);
    layAcquire->addWidget(btnTriggerOnce);

    connect(m_radioContinuous, &QRadioButton::toggled, this, &MainWindow::onTriggerModeChanged);
    connect(m_radioTrigger, &QRadioButton::toggled, this, &MainWindow::onTriggerModeChanged);
    connect(btnStartGrab, &QPushButton::clicked, this, &MainWindow::onStartGrabbing);
    connect(btnStopGrab, &QPushButton::clicked, this, &MainWindow::onStopGrabbing);
    connect(btnTriggerOnce, &QPushButton::clicked, this, &MainWindow::onSoftwareTriggerOnce);

    // [区域 3：DMD检测功能]
    QGroupBox* grpDetect = new QGroupBox(tr("DMD Detection"), controlPanel);
    QVBoxLayout* layDetect = new QVBoxLayout(grpDetect);

    // 修复点：使用成员指针，不再使用局部变量
    m_btnDefectDetect = new QPushButton(tr("Start Defect Detection"), grpDetect);
    m_btnSerialDetect = new QPushButton(tr("Start Serial Detection"), grpDetect);

    layDetect->addWidget(m_btnDefectDetect);
    layDetect->addWidget(m_btnSerialDetect);

    m_chkInvertMode = new QCheckBox(tr("Invert Mode (Dark Field / Bright Defect)"), grpDetect);
    layDetect->addWidget(m_chkInvertMode);

    {
        int algorithmType = 1;
        double minArea = 4.0;
        double grayThresh = 130.0;
        bool genCross = true;
        bool invert = false;
        m_visionManager->getDetectionDebugConfig(algorithmType, minArea, grayThresh, genCross, invert);
        m_chkInvertMode->setChecked(invert);
    }

    connect(m_chkInvertMode, &QCheckBox::toggled, this, [this](bool checked) {
        applyInvertMode(checked);
        if (logEdit) {
            logEdit->append(tr("Invert mode switched to: %1").arg(checked ? "true" : "false"));
        }
        });

    connect(m_btnDefectDetect, &QPushButton::clicked, this, &MainWindow::onStartDefectDetect);
    connect(m_btnSerialDetect, &QPushButton::clicked, this, &MainWindow::onStartSerialDetect);

    // [区域 4：图像保存区域]
    QGroupBox* grpSave = new QGroupBox(tr("Image Save"), controlPanel);
    QVBoxLayout* laySave = new QVBoxLayout(grpSave);

    QPushButton* btnSaveImage = new QPushButton(tr("Image Save"), grpSave);
    laySave->addWidget(btnSaveImage);
    connect(btnSaveImage, &QPushButton::clicked, this, &MainWindow::onSaveImage);

    controlLayout->addWidget(grpInit);
    controlLayout->addWidget(grpAcquire);
    controlLayout->addWidget(grpDetect);
    controlLayout->addWidget(grpSave);
    controlLayout->addStretch();

    QScrollArea* controlScrollArea = new QScrollArea(central);
    controlScrollArea->setWidget(controlPanel);
    controlScrollArea->setWidgetResizable(true);
    controlScrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* paramContainer = new QWidget(central);
    QVBoxLayout* paramLayout = new QVBoxLayout(paramContainer);
    paramLayout->setContentsMargins(0, 0, 0, 0);
    createParameterPanel(paramLayout);

    imageViewer = new ImageViewer(central);
    imageViewer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QWidget* imageContainer = new QWidget(central);
    QVBoxLayout* imageLayout = new QVBoxLayout(imageContainer);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    imageLayout->setSpacing(4);

    QLabel* imageLabel = new QLabel(tr("Image"), imageContainer);
    imageLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    imageLayout->addWidget(imageLabel, 0);
    imageLayout->addWidget(imageViewer, 1);

    logEdit = new QTextEdit(central);
    logEdit->setReadOnly(true);
    logEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(logEdit, &QTextEdit::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu* menu = logEdit->createStandardContextMenu();
        menu->addSeparator();
        QAction* clearAction = menu->addAction(tr("Clear Log"));
        connect(clearAction, &QAction::triggered, logEdit, &QTextEdit::clear);
        menu->exec(logEdit->mapToGlobal(pos));
        delete menu;
        });

    QWidget* logContainer = new QWidget(central);
    QVBoxLayout* logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->addWidget(new QLabel(tr("Log"), logContainer));
    logLayout->addWidget(logEdit);

    QGridLayout* gridLayout = new QGridLayout(central);
    gridLayout->setContentsMargins(10, 10, 10, 10);
    gridLayout->setSpacing(10);

    gridLayout->addWidget(controlScrollArea, 0, 0);
    gridLayout->addWidget(imageContainer, 0, 1);
    gridLayout->addWidget(paramContainer, 1, 0);
    gridLayout->addWidget(logContainer, 1, 1);

    gridLayout->setColumnStretch(0, 1);
    gridLayout->setColumnStretch(1, 3);
    gridLayout->setRowStretch(0, 3);
    gridLayout->setRowStretch(1, 1);

    setCentralWidget(central);
}

void MainWindow::createParameterPanel(QVBoxLayout* parentLayout)
{
    QGroupBox* defaultGroupBox = new QGroupBox(tr("Camera Parameters"));
    
    // 给 GroupBox 一个主垂直布局，里面将存放“参数表单”和“按钮栏"
    QVBoxLayout* mainParamGrpLayout = new QVBoxLayout(defaultGroupBox);

    // 1. 创建参数表单布局
    QFormLayout* formLayout = new QFormLayout();

    // 定义控件
    m_spinExposure = new QDoubleSpinBox();
    m_spinExposure->setRange(10.0, 10000000.0); // 定义安全范围 10us 到很大
    m_spinExposure->setSingleStep(100.0);

    m_spinGain = new QDoubleSpinBox();
    m_spinGain->setRange(0.0, 30.0); // 正常增益调整范围 0.0 ~ 20.0
    m_spinGain->setSingleStep(1.0);

    m_spinFrameRate = new QDoubleSpinBox();
    m_spinFrameRate->setRange(1.0, 500.0); // 帧率从1到500帧
    m_spinFrameRate->setSingleStep(5.0);

    m_cbPixelFormat = new QComboBox();
    // 填充海康常见的两个格式作为示例(具体参考宏定义类型 PixelType_Gvsp_xxx)
    m_cbPixelFormat->addItem("Mono 8", PixelType_Gvsp_Mono8);
    m_cbPixelFormat->addItem("Bayer GB 8", PixelType_Gvsp_BayerGB8);
    m_cbPixelFormat->addItem("Bayer RG 8", PixelType_Gvsp_BayerRG8);

    formLayout->addRow(tr("Exposure Time(us):"), m_spinExposure);
    formLayout->addRow(tr("Gain(dB):"), m_spinGain);
    formLayout->addRow(tr("Frame Rate(fps):"), m_spinFrameRate);
    formLayout->addRow(tr("Pixel Format:"), m_cbPixelFormat);

    // 2. 创建横向的按钮布局，把两颗按钮放在同一行
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* btnGetParams = new QPushButton(tr("Get Params"));
    QPushButton* btnSetParams = new QPushButton(tr("Set Params"));
    btnLayout->addWidget(btnGetParams);
    btnLayout->addWidget(btnSetParams);
    
    // 3. 将表单和按钮栏依次加入 GroupBox 的主垂直布局中
    mainParamGrpLayout->addLayout(formLayout);
    mainParamGrpLayout->addLayout(btnLayout);
    // 可选：为了美观可以加一个占位符，压紧控件
    mainParamGrpLayout->addStretch(); 
    
    // 4. 将包好的 GroupBox 放入父级布局
    parentLayout->addWidget(defaultGroupBox);

    // 槽函数连接
    connect(btnGetParams, &QPushButton::clicked, this, &MainWindow::onGetCameraParams);
    connect(btnSetParams, &QPushButton::clicked, this, &MainWindow::onSetCameraParams);
}

void MainWindow::onOpenImage()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Import Image"), "", "Images (*.png *.jpg *.jpeg *.bmp *.tif)");
    if (!fileName.isEmpty()) {
        if (imageViewer->loadImage(fileName)) {
            logEdit->append(tr("Successfully loaded image: %1").arg(fileName));
            statusBar()->showMessage(tr("Image loaded"), 3000);
        } else {
            showWarning(this, tr("Error"), tr("Failed to load image."));
        }
    }
}

void MainWindow::onSaveImage()
{
    QImage currentImg = imageViewer->getCurrentImage();
    if (currentImg.isNull()) {
        showWarning(this, tr("Warning"), tr("No image to save."));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Image"), "", "Images (*.bmp *.png *.jpg)");
    if (!fileName.isEmpty()) {
        currentImg.save(fileName);
        logEdit->append(tr("Image saved to: ") + fileName);
        statusBar()->showMessage(tr("Image saved"), 3000);
    }
}

void MainWindow::onFindDevices()
{
    m_cbDeviceList->clear();
    QStringList cams = m_cameraManager->scanCameras();
    if (cams.isEmpty()) {
        logEdit->append(tr("No camera devices found. Please check connection."));
        // 静默化处理：软件自动启动时不弹窗，只有用户强行点寻找才弹窗
        if (sender() != nullptr) {
            showWarning(this, tr("Camera"), tr("No camera devices found!"));
        }
    }
    else {
        m_cbDeviceList->addItems(cams);
        m_cbDeviceList->setCurrentIndex(0); // 自动选中找到的第1个相机
        logEdit->append(QString(tr("Found %1 cameras. Ready to connect.")).arg(cams.size()));
        QTimer::singleShot(100, this, &MainWindow::onOpenCamera);
    }
}

void MainWindow::onOpenCamera()
{
    int index = m_cbDeviceList->currentIndex();
    if (index < 0) {
        showWarning(this, tr("Warning"), tr("Please select a camera from the list."));
        return;
    }

    if (m_cameraManager->openCamera(index)) {
        // 1. 同步单选框设置触发/连续模式状态到相机
        onTriggerModeChanged();

        statusBar()->showMessage(QString(tr("Connected to: %1")).arg(m_cbDeviceList->currentText()), 3000);

        // 【优化项 2】： 相机打开后，紧接着直接下发拉流指令！
        // 如果当时界面处于“连续模式”，那么相机会立刻开始在右侧面板疯狂刷图显示实时画面
        // 如果处于“触发模式”，相机也会立刻进入等待被触发的正确工作状态
        m_cameraManager->startGrabbing();

        // 2. 自动拉取一次参数刷新到界面上
        onGetCameraParams();
    }
}

void MainWindow::onCloseCamera()
{
    m_cameraManager->closeCamera();
    statusBar()->showMessage(tr("Camera closed"), 3000);
}

void MainWindow::onStartGrabbing()
{
    m_cameraManager->startGrabbing();
}

void MainWindow::onStopGrabbing()
{
    m_cameraManager->stopGrabbing();
}

void MainWindow::onTriggerModeChanged()
{
    // 安全检查：如果是通过信号触发，过滤掉"取消选中"的事件
    QRadioButton* radioBtn = qobject_cast<QRadioButton*>(sender());
    if (radioBtn && !radioBtn->isChecked()) {
        return;
    }

    // 如果选中了触发模式，就能勾选软触发复选框；反之灰掉
    if (m_radioTrigger->isChecked()) {
        m_chkSoftwareTrigger->setEnabled(true);
        m_cameraManager->setTriggerMode(); // 设置为触发模式(默认会调通软触发)
        logEdit->append(tr("Switched to Trigger Mode."));
    }
    else {
        m_chkSoftwareTrigger->setEnabled(false);
        m_cameraManager->setContinuousMode();
        logEdit->append(tr("Switched to Continuous Mode."));
    }
}

void MainWindow::onSoftwareTriggerOnce()
{
    // 只有在开启相机、开启采集时发送才有效，底层会拦截
    m_cameraManager->executeSoftwareTrigger();
}

void MainWindow::onImageAcquired(const QImage& img)
{
    imageViewer->setImage(img);

    // [单步动作]：坏点检测（触发模式）
    if (m_bWaitingForDefectDetection) {
        logEdit->append(tr("Image acquired. Starting defect detection automatically..."));
        m_bWaitingForDefectDetection = false;
        // 传递绘图回调结构与无坏点行记录容器
        QImage paintedImg;
        QStringList validRowStrings;
        auto defects = m_visionManager->runDetection(
            img,
            &paintedImg, // 始终请求标记图，用于前端显示
            &validRowStrings);

        m_exportManager->recordDefectData(img, defects, validRowStrings);

        // 始终展示（只要算法返回了标记图）
        if (!paintedImg.isNull()) {
            imageViewer->setImage(paintedImg);
            imageViewer->repaint();
        }

        // 是否保存到导出记录，仍由开关控制
        if (m_actionSavePaintedImage->isChecked() && !paintedImg.isNull()) {
            m_exportManager->setPaintedDefectImage(paintedImg);
        }

        if (defects.empty()) {
            logEdit->append(tr("No defects found."));
        }
        else {
            logEdit->append(tr("Found %1 defects.").arg(defects.size()));
        }

        QString exportMsg;
        const bool ok = m_exportManager->exportCurrentRecord(exportMsg);
        logEdit->append(exportMsg);
        if (ok) {
            logEdit->append(tr("Result table path: %1").arg(m_exportManager->getCsvFilePath()));
        }
    }
    // [单步动作]：序列号 OCR（触发模式）
    else if (m_bWaitingForSerialDetection) {
        logEdit->append(tr("Image acquired. Starting serial OCR automatically..."));
        m_bWaitingForSerialDetection = false;

        const QString serialStr = m_visionManager->runSerialDetection(img);

        // 对齐导出格式：写入当前图像 + OCR 结果后导出
        m_exportManager->resetCurrentRecord();
        m_exportManager->recordDefectData(img, std::vector<Detect::DetectResult>{}); // 保证图像落盘
        m_exportManager->recordSerialData(img, serialStr);

        QString exportMsg;
        const bool ok = m_exportManager->exportCurrentRecord(exportMsg);
        logEdit->append(exportMsg);
        if (ok) {
            logEdit->append(tr("Result table path: %1").arg(m_exportManager->getCsvFilePath()));
        }
    }

    m_cameraManager->unblockImageProcessing();
}

void MainWindow::onStartDefectDetect()
{
    if (m_isDefectDetectRunning) {
        if (logEdit) {
            logEdit->append(tr("Defect detection is already running, duplicated request ignored."));
        }
        return;
    }

    const QString appPath = QCoreApplication::applicationDirPath();
    const QString whiteField = appPath + "/Assets/white_field.bmp";
    const QString blackField = appPath + "/Assets/black_field.bmp";

    bool ok = false;
    const QString manualSN = QInputDialog::getText(
        this,
        tr("Input Serial Number"),
        tr("Please enter the DMD serial number:"),
        QLineEdit::Normal,
        "",
        &ok);

    if (!ok || manualSN.trimmed().isEmpty()) {
        logEdit->append(tr("Defect detection cancelled: Serial number not provided."));
        return;
    }

    // 保存现场状态
    const bool originalInvert = (m_chkInvertMode != nullptr) ? m_chkInvertMode->isChecked() : false;

    // 进入运行态（禁用按钮 + 互斥）
    m_isDefectDetectRunning = true;
    if (m_btnDefectDetect) m_btnDefectDetect->setEnabled(false);
    if (m_btnSerialDetect) m_btnSerialDetect->setEnabled(false);

    auto restoreSceneState = [&]() {
        if (m_chkInvertMode) {
            m_chkInvertMode->setChecked(originalInvert);
        }

        if (m_lightManager && m_lightManager->isConnected()) {
            m_lightManager->turnOffMultiChannel({ 1, 2, 3, 4 });
        }

        if (m_btnDefectDetect) m_btnDefectDetect->setEnabled(true);
        if (m_btnSerialDetect) m_btnSerialDetect->setEnabled(true);
        m_isDefectDetectRunning = false;
        };

    m_exportManager->resetCurrentRecord();
    m_exportManager->recordSerialData(QImage(), manualSN.trimmed());

    if (m_bCameraOpened && m_radioContinuous->isChecked()) {
        m_radioTrigger->setChecked(true);
        m_chkSoftwareTrigger->setChecked(true);
        logEdit->append(tr("Defect detection: Continuous mode detected, auto switched to Trigger Mode + Software Trigger."));
    }

    const bool useCameraTriggerMode = (m_bCameraOpened && m_radioTrigger->isChecked() && m_chkSoftwareTrigger->isChecked());

    struct StepLightConfig
    {
        int chA = 1;
        int chB = 2;
        int valA = 255;
        int valB = 45;
    };

    struct StepPlan
    {
        QString name;
        QString tag;
        bool invert = false;
        bool needPattern = false;
        QString patternPath;
        StepLightConfig lightCfg;
    };

    auto isValidChannel = [](int ch) { return ch >= 1 && ch <= 4; };

    auto readStepConfig = [](QSettings& settings, int stepIndex, const StepLightConfig& defaults) -> StepLightConfig {
        StepLightConfig cfg;
        cfg.chA = settings.value(QString("LightSettings/Step%1_ChannelA").arg(stepIndex), defaults.chA).toInt();
        cfg.chB = settings.value(QString("LightSettings/Step%1_ChannelB").arg(stepIndex), defaults.chB).toInt();
        cfg.valA = settings.value(QString("LightSettings/Step%1_IntensityA").arg(stepIndex), defaults.valA).toInt();
        cfg.valB = settings.value(QString("LightSettings/Step%1_IntensityB").arg(stepIndex), defaults.valB).toInt();
        return cfg;
        };

    QSettings settings;
    StepLightConfig stepCfg[4];
    stepCfg[0] = readStepConfig(settings, 1, StepLightConfig{ 1, 2, 255, 45 });
    stepCfg[1] = readStepConfig(settings, 2, StepLightConfig{ 1, 2, 255, 45 });
    stepCfg[2] = readStepConfig(settings, 3, StepLightConfig{ 3, 4, 45, 255 });
    stepCfg[3] = readStepConfig(settings, 4, StepLightConfig{ 3, 4, 45, 255 });

    for (int i = 0; i < 4; ++i) {
        const auto& cfg = stepCfg[i];
        if (!isValidChannel(cfg.chA) || !isValidChannel(cfg.chB) || cfg.chA == cfg.chB) {
            showWarning(this, tr("Invalid Light Settings"),
                tr("Step %1 light channels are invalid. Please configure in Light Controller Panel.").arg(i + 1));
            logEdit->append(tr("Invalid light settings at step %1.").arg(i + 1));
            restoreSceneState();
            return;
        }
    }

    auto sendPatternOrAbort = [&](const QString& stepName, const QString& imagePath) -> bool {
        logEdit->append(tr("%1: Sending DLP image...").arg(stepName));
        const bool sendOk = m_dlpUsbManager->sendStaticImage(imagePath);
        if (!sendOk) {
            logEdit->append(tr("%1: DLP image send failed. Sequence aborted.").arg(stepName));
            showWarning(this, tr("DLP Send Failed"),
                tr("%1 failed to send DLP pattern, sequence aborted.").arg(stepName));
            return false;
        }
        logEdit->append(tr("%1: DLP image send success.").arg(stepName));
        return true;
        };

    auto applyLightProfileOrAbort = [&](const QString& stepName, const StepLightConfig& cfg) -> bool {
        if (!m_lightManager->isConnected()) {
            logEdit->append(tr("%1: Light controller not connected, skip light profile apply.").arg(stepName));
            return true;
        }

        QList<int> onChannels = { cfg.chA, cfg.chB };
        QList<int> offChannels = { 1, 2, 3, 4 };
        offChannels.removeAll(cfg.chA);
        offChannels.removeAll(cfg.chB);

        const bool offOk = m_lightManager->turnOffMultiChannel(offChannels);
        const bool onOk = m_lightManager->turnOnMultiChannel(onChannels);
        const bool setOk = m_lightManager->setMultiIntensity({
            qMakePair(cfg.chA, cfg.valA),
            qMakePair(cfg.chB, cfg.valB)
            });

        if (!(offOk && onOk && setOk)) {
            logEdit->append(tr("%1: Failed to apply light profile (off=%2, on=%3, set=%4).")
                .arg(stepName)
                .arg(offOk ? "true" : "false")
                .arg(onOk ? "true" : "false")
                .arg(setOk ? "true" : "false"));
            showWarning(this, tr("Light Apply Failed"),
                tr("%1 failed to apply light profile, sequence aborted.").arg(stepName));
            return false;
        }

        logEdit->append(tr("%1: Light profile applied. Ch%2=%3, Ch%4=%5")
            .arg(stepName).arg(cfg.chA).arg(cfg.valA).arg(cfg.chB).arg(cfg.valB));
        return true;
        };

    auto acquireImageForStep = [&](const QString& stepName) -> QImage {
        if (useCameraTriggerMode) {
            QThread::msleep(300);

            QImage receivedImg;
            bool gotImage = false;
            QEventLoop loop;
            QTimer timeoutTimer;
            timeoutTimer.setSingleShot(true);

            auto conn = connect(m_cameraManager, &CameraManager::imageAcquired, [&](const QImage& img) {
                receivedImg = img;
                gotImage = true;
                loop.quit();
                });

            connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
            timeoutTimer.start(3000);
            m_cameraManager->executeSoftwareTrigger();
            loop.exec();
            disconnect(conn);

            if (!gotImage) {
                logEdit->append(tr("%1: Acquire image timeout/failed.").arg(stepName));
                return QImage();
            }
            return receivedImg;
        }

        const QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("Import Image"),
            "",
            "Images (*.png *.jpg *.jpeg *.bmp *.tif)");

        if (fileName.isEmpty()) {
            logEdit->append(tr("%1: Operator cancelled image loading.").arg(stepName));
            return QImage();
        }

        if (!imageViewer->loadImage(fileName)) {
            showWarning(this, tr("Error"), tr("Failed to load image."));
            logEdit->append(tr("%1: Failed to load selected image.").arg(stepName));
            return QImage();
        }

        logEdit->append(tr("Successfully loaded image: %1").arg(fileName));
        statusBar()->showMessage(tr("Image loaded"), 3000);
        return imageViewer->getCurrentImage();
        };

    auto runStep = [&](const StepPlan& step) -> bool {
        if (!applyLightProfileOrAbort(step.name, step.lightCfg)) {
            return false;
        }

        if (m_chkInvertMode) {
            m_chkInvertMode->setChecked(step.invert);
        }

        if (step.needPattern && !sendPatternOrAbort(step.name, step.patternPath)) {
            return false;
        }

        QImage img = acquireImageForStep(step.name);
        if (img.isNull()) {
            if (useCameraTriggerMode) {
                showWarning(this, tr("Acquire Timeout"),
                    tr("%1 failed to acquire image in time, sequence aborted.").arg(step.name));
            }
            return false;
        }

        logEdit->append(tr("%1: Image acquired. Starting detection...").arg(step.name));

        QImage paintedImg;
        QStringList validRowStrings;
        auto defects = m_visionManager->runDetection(
            img,
            m_actionSavePaintedImage->isChecked() ? &paintedImg : nullptr,
            &validRowStrings);

        m_exportManager->recordDefectData(img, defects, validRowStrings);

        if (m_actionSavePaintedImage->isChecked() && !paintedImg.isNull()) {
            m_exportManager->setPaintedDefectImage(paintedImg);
            imageViewer->setImage(paintedImg);
            imageViewer->repaint();
        }

        logEdit->append(defects.empty()
            ? tr("%1: No defects found.").arg(step.name)
            : tr("%1: Found %2 defects.").arg(step.name).arg(defects.size()));

        QString exportMsg;
        const bool exportOk = m_exportManager->exportCurrentRecord(exportMsg, step.tag);
        logEdit->append(exportMsg);
        if (!exportOk) {
            showWarning(this, tr("Export Failed"),
                tr("%1 export failed, sequence aborted.").arg(step.name));
            return false;
        }

        return true;
        };

    QList<StepPlan> steps = {
        { "Step 1 (White Field / Dark Defect)",    "l1_wf", false, true,  whiteField, stepCfg[0] },
        { "Step 2 (Dark Field / Bright Defect)",   "l1_df", true,  true,  blackField, stepCfg[1] },
        { "Step 3 (Switched Light - Dark Defect)", "l2_df", false, false, "",         stepCfg[2] },
        { "Step 4 (White Field / Bright Defect)",  "l2_wf", true,  true,  whiteField, stepCfg[3] }
    };

    logEdit->append(useCameraTriggerMode
        ? tr("Starting 4-step sequence detect wizard...")
        : tr("Starting 4-step sequence wizard (Static Mode)..."));

    for (const auto& step : steps) {
        if (!runStep(step)) {
            restoreSceneState();
            return;
        }
    }

    int dmdW = 1920, dmdH = 1080;
    m_visionManager->getDMDResolution(dmdW, dmdH);

    QString summaryMsg;
    const bool summaryOk = m_exportManager->exportSummaryReport(summaryMsg, dmdH);
    logEdit->append(summaryMsg);
    if (!summaryOk) {
        showWarning(this, tr("Summary Export Failed"), tr("Summary export failed, sequence aborted."));
        restoreSceneState();
        return;
    }

    showInfo(this, tr("Completed"),
        useCameraTriggerMode
        ? tr("坏点检测向导四步流程及汇总报告已全部生成完毕！")
        : tr("静态图坏点检测向导四步流程已全部完成！"));

    restoreSceneState();
}

void MainWindow::onStartSerialDetect()
{
    // 仅在“相机已打开 + 触发模式 + 软件触发”时走软触发流程
    if (m_bCameraOpened && m_radioTrigger->isChecked() && m_chkSoftwareTrigger->isChecked()) {
        m_bWaitingForSerialDetection = true;
        m_cameraManager->executeSoftwareTrigger();
        logEdit->append(tr("Software trigger sent, waiting for image to run serial OCR..."));
        return;
    }

    QImage currentImg = imageViewer->getCurrentImage();
    if (currentImg.isNull()) {
        showWarning(this, tr("Warning"), tr("No camera detected trigger mode and no image loaded.\nPlease acquire or load an image first."));
        m_bWaitingForSerialDetection = false;
        return;
    }

    logEdit->append(tr("Running serial detection on current static image..."));
    const QString serialStr = m_visionManager->runSerialDetection(currentImg);

    // 与单次坏点/全流程对齐：记录并导出
    m_exportManager->resetCurrentRecord();
    m_exportManager->recordDefectData(currentImg, std::vector<Detect::DetectResult>{}); // 保证图像落盘
    m_exportManager->recordSerialData(currentImg, serialStr);

    QString exportMsg;
    const bool ok = m_exportManager->exportCurrentRecord(exportMsg);
    logEdit->append(exportMsg);
    if (ok) {
        logEdit->append(tr("Result table path: %1").arg(m_exportManager->getCsvFilePath()));
    }
}


void MainWindow::onCameraStateChanged(bool isOpen)
{
    m_bCameraOpened = isOpen;
}

// ---------------------------------------------------------
// 参数读写交互
// ---------------------------------------------------------
void MainWindow::onGetCameraParams()
{
    // 注：若并未连接相机，底层应拦截并返回错误或默认值（我们在 UI 端简单拿即可）
    float exp = m_cameraManager->getExposureTime();
    float gain = m_cameraManager->getGain();
    float fps = m_cameraManager->getFrameRate();
    unsigned int pxFormat = m_cameraManager->getPixelFormat();

    m_spinExposure->setValue(exp);
    m_spinGain->setValue(gain);
    m_spinFrameRate->setValue(fps);

    // 同步下来选择项
    int idx = m_cbPixelFormat->findData(pxFormat);
    if (idx != -1) {
        m_cbPixelFormat->setCurrentIndex(idx);
    }
    
    logEdit->append(tr("Camera parameters retrieved."));
}

void MainWindow::onSetCameraParams()
{
    float exp = m_spinExposure->value();
    float gain = m_spinGain->value();
    float fps = m_spinFrameRate->value();
    
    unsigned int pxFormat = m_cbPixelFormat->currentData().toUInt();

    m_cameraManager->setExposureTime(exp);
    m_cameraManager->setGain(gain);
    m_cameraManager->setFrameRate(fps);
    m_cameraManager->setPixelFormat(pxFormat);

    logEdit->append(tr("Camera parameters configured and pushed."));
}

void MainWindow::onSwitchLanguageToChinese()
{
    // 判断询问用户是否要立即重启
    QMessageBox::StandardButton reply;
    reply = showQuestion(this, tr("Restart Required"),
        tr("Language switch to Chinese requires a software restart.\nDo you want to restart now?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // 1. 将意图存入本地注册表或配置文件
        QSettings settings;
        settings.setValue("Language", "zh_CN");

        // 2. 准备一条自动重启本程序的命令（在新进程中）"
        QString program = QCoreApplication::applicationFilePath();
        QStringList arguments = QCoreApplication::arguments();

        // 3. 关闭当前程序，启动刚配置的新进程
        qApp->quit();
        QProcess::startDetached(program, arguments);
    }
}

void MainWindow::onSwitchLanguageToEnglish()
{
    // 同理处理英文版本
    QMessageBox::StandardButton reply;
    reply = showQuestion(this, tr("Restart Required"),
        tr("Language switch to English requires a software restart.\nDo you want to restart now?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QSettings settings;
        settings.setValue("Language", "en_US");

        QString program = QCoreApplication::applicationFilePath();
        QStringList arguments = QCoreApplication::arguments();

        qApp->quit();
        QProcess::startDetached(program, arguments);
    }
}

// 实现开启悬浮设置窗口的逻辑
void MainWindow::onOpenDMDSettings()
{
    if (!m_dmdSettingDialog) {
        // 创建浮动窗口
        m_dmdSettingDialog = new QDialog(this);
        m_dmdSettingDialog->setWindowTitle(tr("DMD Parameter Settings"));
        
        // 设置窗口属性为非模态，始终置顶在主窗口前面但是不阻塞后台操作
        m_dmdSettingDialog->setWindowModality(Qt::NonModal);
        // 如果想让它像工具面板一样在主界面上浮动，可以添加 Tool 属性：
        // m_dmdSettingDialog->setWindowFlags(m_dmdSettingDialog->windowFlags() | Qt::Tool);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(m_dmdSettingDialog);
        QFormLayout* formLayout = new QFormLayout();

        m_spinDmdWidth = new QSpinBox(m_dmdSettingDialog);
        m_spinDmdWidth->setRange(50, 6000);
        
        m_spinDmdHeight = new QSpinBox(m_dmdSettingDialog);
        m_spinDmdHeight->setRange(50, 6000);

        m_spinDmdMirrorSize = new QDoubleSpinBox(m_dmdSettingDialog);
        m_spinDmdMirrorSize->setRange(0.1, 100.0);
        m_spinDmdMirrorSize->setSingleStep(0.1);
        m_spinDmdMirrorSize->setSuffix(" um");
        m_spinDmdMirrorSize->setValue(5.4); // 默认尺寸备用

        formLayout->addRow(tr("Horizontal Grid Count (Width):"), m_spinDmdWidth);
        formLayout->addRow(tr("Vertical Grid Count (Height):"), m_spinDmdHeight);
        formLayout->addRow(tr("Mirror Size (Reserved):"), m_spinDmdMirrorSize);

        mainLayout->addLayout(formLayout);

        // 应用 与 关闭 按钮
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close);
        mainLayout->addWidget(buttonBox);

        // 更新按钮逻辑: 应用时写回到 VisionManager
        connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [=]() {
            m_visionManager->setDMDResolution(m_spinDmdWidth->value(), m_spinDmdHeight->value());
        });
        connect(buttonBox, &QDialogButtonBox::rejected, m_dmdSettingDialog, &QDialog::hide);
    }

    // 每次打开前，从 VisionManager 同步一下最新的数据到面板上
    int currentW, currentH;
    m_visionManager->getDMDResolution(currentW, currentH);
    m_spinDmdWidth->setValue(currentW);
    m_spinDmdHeight->setValue(currentH);

    m_dmdSettingDialog->show();
    m_dmdSettingDialog->raise();
    m_dmdSettingDialog->activateWindow();
}

void MainWindow::onOpenAlgorithmAdvancedSettings()
{
    if (!m_detectDebugDialog) {
        m_detectDebugDialog = new QDialog(this);
        m_detectDebugDialog->setWindowTitle(tr("Algorithm Advanced Settings"));
        m_detectDebugDialog->setWindowModality(Qt::NonModal);

        QVBoxLayout* mainLayout = new QVBoxLayout(m_detectDebugDialog);
        QFormLayout* formLayout = new QFormLayout();

        m_cbDetectAlgorithm = new QComboBox(m_detectDebugDialog);
        m_cbDetectAlgorithm->addItem(tr("Legacy (GetDPointLocation)"), 0);
        m_cbDetectAlgorithm->addItem(tr("New (dmd_defect_detection)"), 1);

        m_spinDetectMinArea = new QDoubleSpinBox(m_detectDebugDialog);
        m_spinDetectMinArea->setRange(1.0, 1000000.0);
        m_spinDetectMinArea->setDecimals(2);
        m_spinDetectMinArea->setSingleStep(1.0);

        m_spinDetectGrayThresh = new QDoubleSpinBox(m_detectDebugDialog);
        m_spinDetectGrayThresh->setRange(0.0, 255.0);
        m_spinDetectGrayThresh->setDecimals(2);
        m_spinDetectGrayThresh->setSingleStep(1.0);

        m_chkDetectGenCross = new QCheckBox(tr("Generate Cross Overlay"), m_detectDebugDialog);

        QCheckBox* chkSavePaintedImage = new QCheckBox(tr("Save Painted Defect Images"), m_detectDebugDialog);
        chkSavePaintedImage->setObjectName("chkSavePaintedImage");

        formLayout->addRow(tr("Algorithm:"), m_cbDetectAlgorithm);
        formLayout->addRow(tr("Min Area:"), m_spinDetectMinArea);
        formLayout->addRow(tr("Gray Threshold:"), m_spinDetectGrayThresh);

        mainLayout->addLayout(formLayout);
        mainLayout->addWidget(m_chkDetectGenCross);
        mainLayout->addWidget(chkSavePaintedImage);

        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close);
        mainLayout->addWidget(buttonBox);

        connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
            const int algorithmType = m_cbDetectAlgorithm->currentData().toInt();
            const double minArea = m_spinDetectMinArea->value();
            const double grayThresh = m_spinDetectGrayThresh->value();
            const bool genCross = m_chkDetectGenCross->isChecked();
            const bool invert = (m_chkInvertMode != nullptr) ? m_chkInvertMode->isChecked() : false;

            m_visionManager->setDetectionDebugConfig(algorithmType, minArea, grayThresh, genCross, invert);

            QCheckBox* chkSave = m_detectDebugDialog->findChild<QCheckBox*>("chkSavePaintedImage");
            if (chkSave) {
                m_actionSavePaintedImage->setChecked(chkSave->isChecked());
            }

            logEdit->append(tr("Debug parameters updated: Alg=%1, MinArea=%2, GrayThresh=%3, GenCross=%4, Invert=%5, SavePainted=%6")
                .arg(algorithmType)
                .arg(minArea, 0, 'f', 2)
                .arg(grayThresh, 0, 'f', 2)
                .arg(genCross ? "true" : "false")
                .arg(invert ? "true" : "false")
                .arg(m_actionSavePaintedImage->isChecked() ? "true" : "false"));
            });

        connect(buttonBox, &QDialogButtonBox::rejected, m_detectDebugDialog, &QDialog::hide);
    }

    int algorithmType = 1;
    double minArea = 4.0;
    double grayThresh = 130.0;
    bool genCross = true;
    bool invert = false;
    m_visionManager->getDetectionDebugConfig(algorithmType, minArea, grayThresh, genCross, invert);

    const int idx = m_cbDetectAlgorithm->findData(algorithmType);
    if (idx >= 0) {
        m_cbDetectAlgorithm->setCurrentIndex(idx);
    }
    m_spinDetectMinArea->setValue(minArea);
    m_spinDetectGrayThresh->setValue(grayThresh);
    m_chkDetectGenCross->setChecked(genCross);

    if (m_chkInvertMode) {
        m_chkInvertMode->setChecked(invert);
    }

    QCheckBox* chkSave = m_detectDebugDialog->findChild<QCheckBox*>("chkSavePaintedImage");
    if (chkSave) {
        chkSave->setChecked(m_actionSavePaintedImage->isChecked());
    }

    m_detectDebugDialog->show();
    m_detectDebugDialog->raise();
    m_detectDebugDialog->activateWindow();
}

// ---------------------------------------------------------
// 弹窗辅助函数实现
// ---------------------------------------------------------
void MainWindow::showInfo(QWidget* parent, const QString& title, const QString& text, int minWidth)
{
    QMessageBox msgBox(QMessageBox::Information, title, text, QMessageBox::Ok, parent);
    QSpacerItem* spacer = new QSpacerItem(minWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QGridLayout* layout = qobject_cast<QGridLayout*>(msgBox.layout());
    if (layout) {
        layout->addItem(spacer, layout->rowCount(), 0, 1, layout->columnCount());
    }
    msgBox.exec();
}

void MainWindow::showWarning(QWidget* parent, const QString& title, const QString& text, int minWidth)
{
    QMessageBox msgBox(QMessageBox::Warning, title, text, QMessageBox::Ok, parent);
    QSpacerItem* spacer = new QSpacerItem(minWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QGridLayout* layout = qobject_cast<QGridLayout*>(msgBox.layout());
    if (layout) {
        layout->addItem(spacer, layout->rowCount(), 0, 1, layout->columnCount());
    }
    msgBox.exec();
}

QMessageBox::StandardButton MainWindow::showQuestion(QWidget* parent, const QString& title, const QString& text, int minWidth)
{
    QMessageBox msgBox(QMessageBox::Question, title, text,
        QMessageBox::Yes | QMessageBox::No, parent);
    QSpacerItem* spacer = new QSpacerItem(minWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QGridLayout* layout = qobject_cast<QGridLayout*>(msgBox.layout());
    if (layout) {
        layout->addItem(spacer, layout->rowCount(), 0, 1, layout->columnCount());
    }
    return static_cast<QMessageBox::StandardButton>(msgBox.exec());
}

void MainWindow::applyInvertMode(bool invertEnabled)
{
    int algorithmType = 1;
    double minArea = 4.0;
    double grayThresh = 130.0;
    bool genCross = true;
    bool invert = false;

    m_visionManager->getDetectionDebugConfig(algorithmType, minArea, grayThresh, genCross, invert);
    m_visionManager->setDetectionDebugConfig(algorithmType, minArea, grayThresh, genCross, invertEnabled);
}

// ---------------------------------------------------------
// DataServer 配置相关
// ---------------------------------------------------------
void MainWindow::onOpenDataServerSettings()
{
    bool ok;
    QString currentIp = m_dataServerManager->getIpAddress();
    QString newIp = QInputDialog::getText(this, tr("DataServer Settings"),
        tr("Please enter the target DataServer IP Address:"), QLineEdit::Normal,
        currentIp, &ok);

    if (ok && !newIp.isEmpty()) {
        m_dataServerManager->setIpAddress(newIp.trimmed());
        logEdit->append(tr("DataServer IP updated to: %1").arg(newIp));
    }
}

// 光源悬浮面板函数：
void MainWindow::onOpenLightSettings()
{
    if (!m_lightSettingDialog) {
        m_lightSettingDialog = new QDialog(this);
        m_lightSettingDialog->setWindowTitle(tr("Light Controller Panel"));
        m_lightSettingDialog->setWindowModality(Qt::NonModal);
        m_lightSettingDialog->resize(920, 800);

        QVBoxLayout* mainLayout = new QVBoxLayout(m_lightSettingDialog);

        // ------------------ 1. 网络连接块 -------------------
        QGroupBox* grpConn = new QGroupBox(tr("Connection Settings (Server)"));
        QGridLayout* layConn = new QGridLayout(grpConn);

        QLineEdit* leIp = new QLineEdit("0.0.0.0");
        leIp->setMinimumWidth(180);
        leIp->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        QLineEdit* lePort = new QLineEdit("8000");
        lePort->setFixedWidth(90);

        QPushButton* btnConnectSDK = new QPushButton(tr("Connect (SDK)"));
        QPushButton* btnConnectRaw = new QPushButton(tr("Connect (RAW TCP)"));
        QPushButton* btnDisconnect = new QPushButton(tr("Disconnect All"));

        QLabel* lblStatus = new QLabel(tr("Status: Disconnected"));
        lblStatus->setStyleSheet("color: red; font-weight: bold;");
        lblStatus->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        layConn->addWidget(new QLabel("IP / COM:"), 0, 0);
        layConn->addWidget(leIp, 0, 1);
        layConn->addWidget(new QLabel("Port:"), 0, 2);
        layConn->addWidget(lePort, 0, 3);
        layConn->addWidget(lblStatus, 0, 4, 1, 2);

        layConn->addWidget(btnConnectSDK, 1, 0, 1, 2);
        layConn->addWidget(btnConnectRaw, 1, 2, 1, 2);
        layConn->addWidget(btnDisconnect, 1, 4, 1, 2);

        // ------------------ 2. 检测流程光源配置（每一步独立） -------------------
        QGroupBox* grpSeq = new QGroupBox(tr("Detection Sequence Light Config (Step 1~4 Independent)"));
        QGridLayout* laySeq = new QGridLayout(grpSeq);

        std::vector<QSpinBox*> stepChA(4);
        std::vector<QSpinBox*> stepChB(4);
        std::vector<QSpinBox*> stepValA(4);
        std::vector<QSpinBox*> stepValB(4);

        for (int i = 0; i < 4; ++i) {
            stepChA[i] = new QSpinBox(grpSeq); stepChA[i]->setRange(1, 4);
            stepChB[i] = new QSpinBox(grpSeq); stepChB[i]->setRange(1, 4);
            stepValA[i] = new QSpinBox(grpSeq); stepValA[i]->setRange(0, 255);
            stepValB[i] = new QSpinBox(grpSeq); stepValB[i]->setRange(0, 255);

            laySeq->addWidget(new QLabel(tr("Step %1 Channel A").arg(i + 1)), i, 0);
            laySeq->addWidget(stepChA[i], i, 1);
            laySeq->addWidget(new QLabel(tr("Intensity A")), i, 2);
            laySeq->addWidget(stepValA[i], i, 3);

            laySeq->addWidget(new QLabel(tr("Step %1 Channel B").arg(i + 1)), i, 4);
            laySeq->addWidget(stepChB[i], i, 5);
            laySeq->addWidget(new QLabel(tr("Intensity B")), i, 6);
            laySeq->addWidget(stepValB[i], i, 7);
        }

        QPushButton* btnLoadSeq = new QPushButton(tr("Load Preset"), grpSeq);
        QPushButton* btnSaveSeq = new QPushButton(tr("Save Preset"), grpSeq);
        QPushButton* btnApplyStep1 = new QPushButton(tr("Apply Step1"));
        QPushButton* btnApplyStep2 = new QPushButton(tr("Apply Step2"));
        QPushButton* btnApplyStep3 = new QPushButton(tr("Apply Step3"));
        QPushButton* btnApplyStep4 = new QPushButton(tr("Apply Step4"));

        laySeq->addWidget(btnLoadSeq, 4, 0, 1, 2);
        laySeq->addWidget(btnSaveSeq, 4, 2, 1, 2);
        laySeq->addWidget(btnApplyStep1, 4, 4);
        laySeq->addWidget(btnApplyStep2, 4, 5);
        laySeq->addWidget(btnApplyStep3, 4, 6);
        laySeq->addWidget(btnApplyStep4, 4, 7);

        // ------------------ 3. 光源 1-4 独立/并发控制块 -------------------
        QGroupBox* grpCtrl = new QGroupBox(tr("Channels (1-4) Individual & Concurrent Control"));
        QGridLayout* layCtrl = new QGridLayout(grpCtrl);

        std::vector<QSpinBox*> spinIntensities(4);
        std::vector<QPushButton*> btnOnInd(4);
        std::vector<QPushButton*> btnOffInd(4);
        std::vector<QPushButton*> btnSetInd(4);

        for (int i = 0; i < 4; ++i) {
            int ch = i + 1;
            layCtrl->addWidget(new QLabel(QString("Channel %1 :").arg(ch)), i, 0);

            spinIntensities[i] = new QSpinBox();
            spinIntensities[i]->setRange(0, 255);
            spinIntensities[i]->setValue(125);
            layCtrl->addWidget(spinIntensities[i], i, 1);

            btnSetInd[i] = new QPushButton(tr("Set Param"));
            btnOnInd[i] = new QPushButton(tr("Turn ON"));
            btnOffInd[i] = new QPushButton(tr("Turn OFF"));

            layCtrl->addWidget(btnSetInd[i], i, 2);
            layCtrl->addWidget(btnOnInd[i], i, 3);
            layCtrl->addWidget(btnOffInd[i], i, 4);

            connect(btnSetInd[i], &QPushButton::clicked, this, [=]() { m_lightManager->setIntensity(ch, spinIntensities[i]->value()); });
            connect(btnOnInd[i], &QPushButton::clicked, this, [=]() { m_lightManager->turnOnChannel(ch); });
            connect(btnOffInd[i], &QPushButton::clicked, this, [=]() { m_lightManager->turnOffChannel(ch); });
        }

        QPushButton* btnAllOn = new QPushButton(tr("Simultaneous Turn ON (1-4)"));
        QPushButton* btnAllOff = new QPushButton(tr("Simultaneous Turn OFF (1-4)"));
        QPushButton* btnAllSet = new QPushButton(tr("Simultaneous Set Params (1-4)"));

        btnAllOn->setStyleSheet("background-color: #dcedc8; font-weight:bold;");
        btnAllOff->setStyleSheet("background-color: #ffcdd2; font-weight:bold;");
        btnAllSet->setStyleSheet("background-color: #bbdefb; font-weight:bold;");

        layCtrl->addWidget(btnAllSet, 4, 1, 1, 2);
        layCtrl->addWidget(btnAllOn, 4, 3);
        layCtrl->addWidget(btnAllOff, 4, 4);

        connect(btnAllSet, &QPushButton::clicked, this, [=]() {
            QList<QPair<int, int>> items;
            for (int i = 0; i < 4; i++) {
                items.append(qMakePair(i + 1, spinIntensities[i]->value()));
            }
            m_lightManager->setMultiIntensity(items);
            });
        connect(btnAllOn, &QPushButton::clicked, this, [=]() { m_lightManager->turnOnMultiChannel({ 1, 2, 3, 4 }); });
        connect(btnAllOff, &QPushButton::clicked, this, [=]() { m_lightManager->turnOffMultiChannel({ 1, 2, 3, 4 }); });

        // ------------------ 4. RAW 调试块 -------------------
        QGroupBox* grpRaw = new QGroupBox(tr("Raw Protocol Debug (Requires 'Connect RAW TCP')"));
        QVBoxLayout* layRaw = new QVBoxLayout(grpRaw);

        QHBoxLayout* laySend = new QHBoxLayout();
        QLineEdit* leHexCmd = new QLineEdit();
        leHexCmd->setPlaceholderText("Enter HEX separated by space (ex: EE 01 03)");
        QPushButton* btnSendHex = new QPushButton(tr("Send Hex"));
        laySend->addWidget(leHexCmd);
        laySend->addWidget(btnSendHex);

        QPushButton* btnToggleMode = new QPushButton(tr("Current Display: HEX"));
        btnToggleMode->setCheckable(true);
        btnToggleMode->setStyleSheet("QPushButton:checked { background-color: #cfd8dc; }");
        QTextEdit* teRawLog = new QTextEdit();
        teRawLog->setReadOnly(true);

        layRaw->addLayout(laySend);
        layRaw->addWidget(btnToggleMode);
        layRaw->addWidget(teRawLog);

        mainLayout->addWidget(grpConn);
        mainLayout->addWidget(grpSeq);
        mainLayout->addWidget(grpCtrl);
        mainLayout->addWidget(grpRaw, 1);

        auto loadSequencePreset = [=]() {
            QSettings settings;

            const int dChA[4] = { 2, 2, 1, 1 };
            const int dChB[4] = { 1, 1, 2, 2 };
            const int dValA[4] = { 255, 255, 35, 35 };
            const int dValB[4] = { 35, 35, 255, 255 };

            for (int i = 0; i < 4; ++i) {
                stepChA[i]->setValue(settings.value(QString("LightSettings/Step%1_ChannelA").arg(i + 1), dChA[i]).toInt());
                stepChB[i]->setValue(settings.value(QString("LightSettings/Step%1_ChannelB").arg(i + 1), dChB[i]).toInt());
                stepValA[i]->setValue(settings.value(QString("LightSettings/Step%1_IntensityA").arg(i + 1), dValA[i]).toInt());
                stepValB[i]->setValue(settings.value(QString("LightSettings/Step%1_IntensityB").arg(i + 1), dValB[i]).toInt());
            }
            };

        auto saveSequencePreset = [=]() -> bool {
            for (int i = 0; i < 4; ++i) {
                if (stepChA[i]->value() == stepChB[i]->value()) {
                    showWarning(m_lightSettingDialog, tr("Invalid Preset"),
                        tr("Step %1: Channel A and B cannot be the same.").arg(i + 1));
                    return false;
                }
            }

            QSettings settings;
            for (int i = 0; i < 4; ++i) {
                settings.setValue(QString("LightSettings/Step%1_ChannelA").arg(i + 1), stepChA[i]->value());
                settings.setValue(QString("LightSettings/Step%1_ChannelB").arg(i + 1), stepChB[i]->value());
                settings.setValue(QString("LightSettings/Step%1_IntensityA").arg(i + 1), stepValA[i]->value());
                settings.setValue(QString("LightSettings/Step%1_IntensityB").arg(i + 1), stepValB[i]->value());
            }

            if (logEdit) {
                logEdit->append(tr("Sequence light preset saved (Step1~Step4 independent)."));
            }
            return true;
            };

        auto applyStepProfileNow = [=](int stepIndex) {
            if (!m_lightManager->isConnected()) {
                showWarning(m_lightSettingDialog, tr("Warning"), tr("Light controller is not connected."));
                return;
            }

            const int chA = stepChA[stepIndex]->value();
            const int chB = stepChB[stepIndex]->value();
            const int valA = stepValA[stepIndex]->value();
            const int valB = stepValB[stepIndex]->value();

            if (chA == chB) {
                showWarning(m_lightSettingDialog, tr("Invalid Preset"),
                    tr("Step %1: Channel A and B cannot be the same.").arg(stepIndex + 1));
                return;
            }

            QList<int> onChannels = { chA, chB };
            QList<int> offChannels = { 1, 2, 3, 4 };
            offChannels.removeAll(chA);
            offChannels.removeAll(chB);

            bool okOff = m_lightManager->turnOffMultiChannel(offChannels);
            bool okOn = m_lightManager->turnOnMultiChannel(onChannels);
            bool okSet = m_lightManager->setMultiIntensity({ qMakePair(chA, valA), qMakePair(chB, valB) });

            if (!(okOff && okOn && okSet)) {
                showWarning(m_lightSettingDialog, tr("Apply Failed"),
                    tr("Failed to apply Step %1 profile.").arg(stepIndex + 1));
            }
            };

        connect(btnLoadSeq, &QPushButton::clicked, this, [=]() { loadSequencePreset(); });
        connect(btnSaveSeq, &QPushButton::clicked, this, [=]() { saveSequencePreset(); });
        connect(btnApplyStep1, &QPushButton::clicked, this, [=]() { if (saveSequencePreset()) applyStepProfileNow(0); });
        connect(btnApplyStep2, &QPushButton::clicked, this, [=]() { if (saveSequencePreset()) applyStepProfileNow(1); });
        connect(btnApplyStep3, &QPushButton::clicked, this, [=]() { if (saveSequencePreset()) applyStepProfileNow(2); });
        connect(btnApplyStep4, &QPushButton::clicked, this, [=]() { if (saveSequencePreset()) applyStepProfileNow(3); });

        loadSequencePreset();

        connect(btnConnectSDK, &QPushButton::clicked, this, [=]() {
            lblStatus->setText(tr("Status: Connecting SDK..."));
            lblStatus->setStyleSheet("color: orange; font-weight: bold;");
            QApplication::processEvents();

            QString text = leIp->text().trimmed();
            bool ok = text.startsWith("COM", Qt::CaseInsensitive)
                ? m_lightManager->connectSerial(text)
                : m_lightManager->connectIP(text);

            if (!ok) {
                showWarning(m_lightSettingDialog, tr("Error"), tr("SDK Connection Failed"));
            }
            });

        connect(btnConnectRaw, &QPushButton::clicked, this, [=]() {
            lblStatus->setText(tr("Status: Connecting RAW..."));
            lblStatus->setStyleSheet("color: orange; font-weight: bold;");
            QApplication::processEvents();

            bool ok = m_lightManager->connectRawTCP(leIp->text().trimmed(), lePort->text().toUShort());
            if (ok) {
                lblStatus->setText(tr("Status: RAW Connected"));
                lblStatus->setStyleSheet("color: blue; font-weight: bold;");
            }
            else {
                showWarning(m_lightSettingDialog, tr("Error"), tr("RAW Socket Connection Failed"));
                lblStatus->setText(tr("Status: Disconnected"));
                lblStatus->setStyleSheet("color: red; font-weight: bold;");
            }
            });

        connect(btnDisconnect, &QPushButton::clicked, this, [=]() {
            m_lightManager->disconnectController();
            m_lightManager->disconnectRawTCP();
            lblStatus->setText(tr("Status: Disconnected"));
            lblStatus->setStyleSheet("color: red; font-weight: bold;");
            });

        connect(m_lightManager, &LightManager::connectionStateChanged, this, [=](bool isConnected) {
            if (isConnected) {
                lblStatus->setText(tr("Status: SDK Connected"));
                lblStatus->setStyleSheet("color: green; font-weight: bold;");
            }
            else {
                lblStatus->setText(tr("Status: Disconnected"));
                lblStatus->setStyleSheet("color: red; font-weight: bold;");
            }
            });

        connect(btnToggleMode, &QPushButton::toggled, this, [=](bool checked) {
            btnToggleMode->setText(checked ? tr("Current Display: DEC") : tr("Current Display: HEX"));
            });

        connect(btnSendHex, &QPushButton::clicked, this, [=]() {
            QString hexStr = leHexCmd->text().remove(" ").remove("0x", Qt::CaseInsensitive);
            if (hexStr.isEmpty()) return;
            m_lightManager->sendRawHex(hexStr);
            teRawLog->append(tr("<b>[Tx]</b> Hex: %1").arg(hexStr.toUpper()));
            });

        connect(m_lightManager, &LightManager::rawDataReceived, this, [=](const QByteArray& data) {
            QString resStr;
            if (btnToggleMode->isChecked()) {
                QStringList decTokens;
                for (unsigned char byte : data) {
                    decTokens << QString::number(byte, 10);
                }
                resStr = decTokens.join(" ");
            }
            else {
                resStr = QString(data.toHex(' ')).toUpper();
            }
            teRawLog->append(tr("<font color='blue'><b>[Rx]</b> %1</font>").arg(resStr));
            });
    }

    m_lightSettingDialog->show();
    m_lightSettingDialog->raise();
    m_lightSettingDialog->activateWindow();
}

//usb发图模块面板函数
void MainWindow::onOpenUsbSenderDebug()
{
    if (!m_usbSenderDialog) {
        m_usbSenderDialog = new QDialog(this);
        m_usbSenderDialog->setWindowTitle(tr("USB Image Sender Debug"));
        m_usbSenderDialog->setWindowModality(Qt::NonModal);
        m_usbSenderDialog->resize(500, 150);

        QVBoxLayout* mainLayout = new QVBoxLayout(m_usbSenderDialog);

        QGroupBox* grpBox = new QGroupBox(tr("Send Image Request"), m_usbSenderDialog);
        QHBoxLayout* fileLayout = new QHBoxLayout(grpBox);

        QLineEdit* leFilePath = new QLineEdit(grpBox);
        leFilePath->setObjectName("leFilePath");
        leFilePath->setReadOnly(true);

        QPushButton* btnBrowse = new QPushButton(tr("Browse..."), grpBox);
        fileLayout->addWidget(new QLabel(tr("Image File:"), grpBox));
        fileLayout->addWidget(leFilePath);
        fileLayout->addWidget(btnBrowse);

        QPushButton* btnSend = new QPushButton(tr("Send via USB"), m_usbSenderDialog);
        btnSend->setMinimumHeight(40);

        mainLayout->addWidget(grpBox);
        mainLayout->addWidget(btnSend);
        mainLayout->addStretch();

        connect(btnBrowse, &QPushButton::clicked, this, [=]() {
            QString fileName = QFileDialog::getOpenFileName(m_usbSenderDialog, tr("Select Image"), "", "Images (*.bmp *.png *.jpg *.jpeg)");
            if (!fileName.isEmpty()) {
                leFilePath->setText(fileName);
            }
            });

        connect(btnSend, &QPushButton::clicked, this, [=]() {
            QString path = leFilePath->text();
            if (path.isEmpty()) {
                showWarning(m_usbSenderDialog, tr("Warning"), tr("Please select an image file first."));
                return;
            }
            m_dlpUsbManager->sendStaticImage(path);
            });
    }

    m_usbSenderDialog->show();
    m_usbSenderDialog->raise();
    m_usbSenderDialog->activateWindow();
}