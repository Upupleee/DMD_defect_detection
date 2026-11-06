#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <QObject>
#include <QDebug>
#include <QImage>
#include <QStringList>
#include "camera_base.h"
#include "camera_opt.h"
#include <atomic> // 【新增】加入原子操作支持

enum class CameraBrand {
    HIKVISION,
    OPT
};

class CameraManager : public QObject
{
    Q_OBJECT
public:
    explicit CameraManager(QObject *parent = nullptr);
    ~CameraManager();

    void initSystem();
    void finalizeSystem();
    QStringList scanCameras();
    void setCameraBrand(CameraBrand brand);
    CameraBrand currentBrand() const { return m_brand; }
    bool openCamera(int index);
    void closeCamera();
    void startGrabbing();
    void stopGrabbing();
    void executeSoftwareTrigger(); 
    void setContinuousMode();      
    void setTriggerMode();         
    void setExposureTime(float exposureUs);
    void setGain(float gain);
    void setFrameRate(float frameRate);
    void setPixelFormat(unsigned int pixelFormat);
    float getExposureTime();
    float getGain();
    float getFrameRate();
    unsigned int getPixelFormat();

    // 【新增】解锁释放事件。主窗口每次画完后必须调用它！
    void unblockImageProcessing() { 
        m_bIsUIProcessing.store(false); 
    }

signals:
    void imageAcquired(const QImage& image);
    void sendLog(const QString& msg);
    void cameraStateChanged(bool isOpen);

private:
    CameraBrand m_brand;
    
    camera_base m_cameraHik;
    camera_opt  m_cameraOpt;

    MV_CC_DEVICE_INFO_LIST m_deviceListHik;
    SCI_DEVICE_INFO_LIST m_deviceListOpt;

    // 【核心改造】：防爆内存的预分配资源与挤压控制锁
    std::atomic<bool> m_isTriggerMode{false};    // 记录是否处于触发模式
    std::atomic<bool> m_bIsUIProcessing{false};  // 当 UI 正在处理且未完成前，这把锁会将其它所有帧全部扬了

    unsigned char* m_pCameraBuffer = nullptr;    // 永远开辟着的一块庞大后备内存（比如 90MB）
    size_t m_nCameraBufferSize = 0;              // 记录该块内存当前大小
};

#endif // CAMERA_MANAGER_H