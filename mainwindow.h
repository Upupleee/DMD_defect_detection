#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QComboBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QWidget>
#include <QPushButton>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QProcess>
#include <QCoreApplication>
#include <QApplication>
#include <QSettings>
#include <QScrollArea>
#include <QInputDialog>
#include <QThread>
#include "vision_manager.h"
#include "camera_manager.h"
#include "image_viewer.h"
#include "export_manager.h"
#include "dataserver_manager.h" 
#include "light_manager.h"
#include "dlp_usb_manager.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 文件菜单功能
    void onOpenImage();
    void onSaveImage();

    // 区域 1： 初始化相机槽函数
    void onFindDevices();
    void onOpenCamera();
    void onCloseCamera();

    // 区域 2： 图像采集槽函数
    void onStartGrabbing();
    void onStopGrabbing();
    void onTriggerModeChanged();
    void onSoftwareTriggerOnce();

    // 区域 3： DMD视觉检测槽函数
    void onStartDefectDetect();
    void onStartSerialDetect();

    // 相机参数获取与设置
    void onGetCameraParams();
    void onSetCameraParams();

    // 相机图像接收回调槽函数
    void onImageAcquired(const QImage& img);
    void onCameraStateChanged(bool isOpen);

    // 语言切换功能
    void onSwitchLanguageToChinese();
    void onSwitchLanguageToEnglish();

    // 打开DMD设置悬浮窗的槽函数
    void onOpenDMDSettings();

    // 打开高级设置弹窗
    void onOpenAlgorithmAdvancedSettings();

    // 弹出设置 DS IP 槽函数的声明
    void onOpenDataServerSettings();

    // 打开USB发图调试
    void onOpenUsbSenderDebug();

    // 光源模块设置
    void onOpenLightSettings();

private:
    // 构建窗口顶部菜单栏
    void createMenu();

    /**
     * @brief 构建主界面的核心布局容器与UI功能块
     */
    void createCentralWidget();

    /**
     * @brief 创建左下角的相机参数调整面板
     * @param parentLayout 放置此面板的父级垂直布局容器
     */
    void createParameterPanel(QVBoxLayout* parentLayout);

    // 新增：仅同步/应用反转模式到 VisionManager（保留其它参数不变）
    void applyInvertMode(bool invertEnabled);

    // 弹窗辅助函数：强制设定最小宽度，防止标题被截断
    static void showInfo(QWidget* parent, const QString& title, const QString& text, int minWidth = 320);
    static void showWarning(QWidget* parent, const QString& title, const QString& text, int minWidth = 320);
    static QMessageBox::StandardButton showQuestion(QWidget* parent, const QString& title, const QString& text, int minWidth = 320);



private:
    VisionManager* m_visionManager;
    CameraManager* m_cameraManager;
    ExportManager* m_exportManager;

    // 控制面板 UI 控件
    QComboBox*    m_cbDeviceList;
    QRadioButton* m_radioContinuous;
    QRadioButton* m_radioTrigger;
    QCheckBox*    m_chkSoftwareTrigger;
    QCheckBox* m_chkInvertMode = nullptr; // 新增：主界面反转模式开关

    // 相机参数 UI 控件
    QDoubleSpinBox* m_spinExposure;
    QDoubleSpinBox* m_spinGain;
    QDoubleSpinBox* m_spinFrameRate;
    QComboBox*      m_cbPixelFormat;

    // 日志与视图控件
    QTextEdit*   logEdit;
    ImageViewer* imageViewer;

    bool m_bCameraOpened = false;

    // 视觉检测状态标志位
    bool m_bWaitingForDefectDetection;
    bool m_bWaitingForSerialDetection;

    // 缓存的DMD设置浮窗指针，避免重复创建
    QDialog*        m_dmdSettingDialog  = nullptr;
    QSpinBox*       m_spinDmdWidth      = nullptr;
    QSpinBox*       m_spinDmdHeight     = nullptr;
    QDoubleSpinBox* m_spinDmdMirrorSize = nullptr;

    // 算法调试参数浮窗相关控件
    QDialog*        m_detectDebugDialog    = nullptr;
    QComboBox*      m_cbDetectAlgorithm    = nullptr;
    QDoubleSpinBox* m_spinDetectMinArea    = nullptr;
    QDoubleSpinBox* m_spinDetectGrayThresh = nullptr;
    QCheckBox*      m_chkDetectGenCross    = nullptr;
    QCheckBox*      m_chkDetectInvert      = nullptr; // 新增：反色模式开关

    // 新增：检测区按钮指针（用于流程中禁用/恢复）
    QPushButton* m_btnFullInspect = nullptr;
    QPushButton* m_btnDefectDetect = nullptr;
    QPushButton* m_btnSerialDetect = nullptr;

    // 新增：坏点四步流程互斥标志，防止重入
    bool m_isDefectDetectRunning = false;

    QAction* m_actionSavePaintedImage;

    //DS管理实例指针
    DataServerManager* m_dataServerManager;

    LightManager* m_lightManager;
    QDialog* m_lightSettingDialog = nullptr;

    // USB 发图模块
    DlpUsbManager* m_dlpUsbManager;
    QDialog* m_usbSenderDialog = nullptr;
};

#endif // MAINWINDOW_H
