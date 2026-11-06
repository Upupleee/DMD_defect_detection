#include "camera_manager.h"


CameraManager::CameraManager(QObject *parent)
    : QObject(parent), m_brand(CameraBrand::HIKVISION)
{
    memset(&m_deviceListHik, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    memset(&m_deviceListOpt, 0, sizeof(SCI_DEVICE_INFO_LIST));
}

CameraManager::~CameraManager()
{
    closeCamera();
    finalizeSystem();
    
    // 释放终极内存池
    if(m_pCameraBuffer) {
        free(m_pCameraBuffer);
        m_pCameraBuffer = nullptr;
    }
}

void CameraManager::initSystem()
{
    camera_base::InitSDK();
    camera_opt::InitSDK();
    emit sendLog(tr("Camera SDKs Initialized."));
}

void CameraManager::finalizeSystem()
{
    camera_base::FinalizeSDK();
    camera_opt::FinalizeSDK();
}

void CameraManager::setCameraBrand(CameraBrand brand)
{
    // 注意：切换品牌时必须强制关闭现有旧环境设备进程
    closeCamera();
    m_brand = brand;
    emit sendLog(brand == CameraBrand::HIKVISION ? tr("Switched to Hikvision Camera Mode.") : tr("Switched to OPT Camera Mode."));
}

QStringList CameraManager::scanCameras()
{
    QStringList cameraNames;
    
    // 海康分支
    if (m_brand == CameraBrand::HIKVISION) {
        m_deviceListHik.nDeviceNum = 0;
        int nRet = camera_base::EnumDevices(m_deviceListHik);
        if (nRet == MV_OK) {
            for (unsigned int i = 0; i < m_deviceListHik.nDeviceNum; ++i) {
                MV_CC_DEVICE_INFO* pDeviceInfo = m_deviceListHik.pDeviceInfo[i];
                QString name;
                if (pDeviceInfo->nTLayerType == MV_GIGE_DEVICE) {
                    name = QString("Hik_GigE: %1 (%2)")
                           .arg(reinterpret_cast<char*>(pDeviceInfo->SpecialInfo.stGigEInfo.chManufacturerName))
                           .arg(reinterpret_cast<char*>(pDeviceInfo->SpecialInfo.stGigEInfo.chUserDefinedName));
                } else if (pDeviceInfo->nTLayerType == MV_USB_DEVICE) {
                    name = QString("Hik_USB: %1 (%2)")
                           .arg(reinterpret_cast<char*>(pDeviceInfo->SpecialInfo.stUsb3VInfo.chManufacturerName))
                           .arg(reinterpret_cast<char*>(pDeviceInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName));
                } else {
                    name = QString("Hik_Unknown %1").arg(i);
                }
                cameraNames << name;
            }
        }
        emit sendLog(tr("Scanned %1 Hikvision cameras.").arg(m_deviceListHik.nDeviceNum));
    } 
    // OPT 分支
    else {
        m_deviceListOpt.count = 0;
        int nRet = camera_opt::EnumDevices(&m_deviceListOpt);
        if (nRet == 0) { // 0 取代 OK
            for (unsigned int i = 0; i < m_deviceListOpt.count; ++i) {
                SCI_DEVICE_INFO* pInfo = &m_deviceListOpt.pDevInfo[i];
                QString name;
                if (pInfo->tlType == SciCam_TLType_Gige) {
                    name = QString("OPT_GigE: %1 (%2)")
                           .arg(reinterpret_cast<char*>(pInfo->info.gigeInfo.manufactureName))
                           .arg(reinterpret_cast<char*>(pInfo->info.gigeInfo.userDefineName));
                } else if (pInfo->tlType == SciCam_TLType_Usb3) {
                    name = QString("OPT_USB: %1 (%2)")
                           .arg(reinterpret_cast<char*>(pInfo->info.usb3Info.manufactureName))
                           .arg(reinterpret_cast<char*>(pInfo->info.usb3Info.userDefineName));
                } else {
                    name = QString("OPT_Device %1").arg(i);
                }
                cameraNames << name;
            }
        }
        emit sendLog(tr("Scanned %1 OPT cameras.").arg(m_deviceListOpt.count));
    }
    
    return cameraNames;
}

bool CameraManager::openCamera(int index)
{
    //if (m_brand == CameraBrand::HIKVISION) {
    //    if (index < 0 || index >= (int)m_deviceListHik.nDeviceNum) return false;

    //    if (m_cameraHik.OpenCamera(m_deviceListHik.pDeviceInfo[index]) == MV_OK) {
    //        emit sendLog("Hikvision Camera opened successfully.");
    //        emit cameraStateChanged(true);

    //        m_cameraHik.SetImageCallback([this](unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo) {
    //            if (pData == nullptr || pFrameInfo == nullptr) return; 
    //            if (pFrameInfo->enPixelType == PixelType_Gvsp_Mono8) {
    //                int width = pFrameInfo->nWidth;
    //                int height = pFrameInfo->nHeight;
    //                QImage safeImg(width, height, QImage::Format_Grayscale8);
    //                for (int y = 0; y < height; ++y) {
    //                    memcpy(safeImg.scanLine(y), pData + y * width, width);
    //                }
    //                emit imageAcquired(safeImg);
    //            }
    //        });
    //        return true;
    //    }
    //} 
    if (m_brand == CameraBrand::HIKVISION) {
        if (index < 0 || index >= (int)m_deviceListHik.nDeviceNum) return false;

        if (m_cameraHik.OpenCamera(m_deviceListHik.pDeviceInfo[index]) == MV_OK) {
            emit sendLog(tr("Hikvision Camera opened successfully."));
            emit cameraStateChanged(true);

            // =========================================================================
            // 采用常驻内存预分配 + UI 强制熔断机制 (最强 OOM 防御与卡顿根除)
            // =========================================================================
            m_cameraHik.SetImageCallback([this](MV_FRAME_OUT* pstFrame) {
                if (pstFrame == nullptr || pstFrame->pBufAddr == nullptr) return;

                // 【绝招 1：跨线程背压限流丢图】
                // 如果这是连续抓取模式，且主界面发来的“处理完毕”信号还没到（代表还在处理旧图），
                // 那么这长 6500w 像素新图直接无视并丢弃！绝不堆积。
                // 如果是“触发模式”，为了防止漏检重要部件，不惜一切代价必发！
                if (!m_isTriggerMode.load() && m_bIsUIProcessing.load()) {
                    return; // 直接返回，让拉流线程立刻自行 Free 并跑下一帧循环
                }

                if (pstFrame->stFrameInfo.enPixelType == PixelType_Gvsp_Mono8) {
                    
                    int width = pstFrame->stFrameInfo.nWidth;
                    int height = pstFrame->stFrameInfo.nHeight;
                    // 我们需要的字节容量
                    size_t frameLen = static_cast<size_t>(width) * height;

                    // 【绝招 2：永不 New 的常驻内存储备】
                    // 如果这辈子还没分配过，或者相机突然切分辨率变大了，再分配一次。否则永久复用！
                    if (m_pCameraBuffer == nullptr || m_nCameraBufferSize < frameLen) {
                        if (m_pCameraBuffer) free(m_pCameraBuffer);
                        m_pCameraBuffer = (unsigned char *)malloc(frameLen);
                        m_nCameraBufferSize = frameLen;
                        
                        if (m_pCameraBuffer == nullptr) {
                            qDebug() << tr("Fatal Error: Failed to allocate static buffer for camera payload!");
                            return; 
                        }
                    }

                    // 将底层的帧硬拷贝到我们永远常驻的、唯一的池子里。速度极快。
                    memcpy(m_pCameraBuffer, pstFrame->pBufAddr, frameLen);

                    // 基于我们自己的这块独占内存，通过 QImage 的无损包装指针发射（浅拷贝机制）
                    QImage safeImg(m_pCameraBuffer, width, height, width, QImage::Format_Grayscale8);

                    // 将门锁死。必须等 UI 层绘制或转算法用完了这张 safeImg，才能把门打开接待下一张。
                    m_bIsUIProcessing.store(true);

                    // 将图抛向 UI 事件队列，由于是指针包装而不是 copy()，发送过程的耗时从大几十毫秒直接降为 0！
                    emit imageAcquired(safeImg);
                }
            });
            return true;
        }
    }
    else {
        if (index < 0 || index >= (int)m_deviceListOpt.count) return false;

        if (m_cameraOpt.OpenCamera(&m_deviceListOpt.pDevInfo[index]) == 0) {
            emit sendLog(tr("OPT Camera opened successfully."));
            emit cameraStateChanged(true);

            m_cameraOpt.SetImageCallback([this](unsigned char* pData, int width, int height, int pixelType) {
                if (pData == nullptr) return;
                // SciCam API 中的 Mono8 枚举值为0x01080001
                if (pixelType == 0x01080001) {
                    QImage safeImg(width, height, QImage::Format_Grayscale8);
                    for (int y = 0; y < height; ++y) {
                        memcpy(safeImg.scanLine(y), pData + y * width, width);
                    }
                    emit imageAcquired(safeImg);
                }
            });
            return true;
        }
    }
    emit sendLog(tr("Failed to open camera."));
    return false;
}

void CameraManager::closeCamera()
{
    if (m_brand == CameraBrand::HIKVISION) {
        m_cameraHik.StopGrabbing();
        m_cameraHik.CloseCamera();
    } else {
        m_cameraOpt.StopGrabbing();
        m_cameraOpt.CloseCamera();
    }
    emit cameraStateChanged(false);
    emit sendLog(tr("Camera closed."));
}

void CameraManager::startGrabbing()
{
    if (m_brand == CameraBrand::HIKVISION) {
        if (m_cameraHik.StartGrabbing() == MV_OK) emit sendLog(tr("Hikvision: Start grabbing images."));
    } else {
        if (m_cameraOpt.StartGrabbing() == 0) emit sendLog(tr("OPT: Start grabbing images."));
    }
}

void CameraManager::stopGrabbing()
{
    if (m_brand == CameraBrand::HIKVISION) m_cameraHik.StopGrabbing();
    else m_cameraOpt.StopGrabbing();
    emit sendLog(tr("Stop grabbing images."));
}

void CameraManager::executeSoftwareTrigger()
{
    if (m_brand == CameraBrand::HIKVISION) {
        m_cameraHik.TriggerSoftware();
    } else {
        m_cameraOpt.TriggerSoftware();
    }
}

void CameraManager::setContinuousMode()
{
    m_isTriggerMode = false; // 同步记录
    if (m_brand == CameraBrand::HIKVISION) m_cameraHik.SetTriggerMode(false);
    else m_cameraOpt.SetTriggerMode(false);
}

void CameraManager::setTriggerMode()
{
    m_isTriggerMode = true; // 同步记录
    if (m_brand == CameraBrand::HIKVISION) m_cameraHik.SetTriggerMode(true);
    else m_cameraOpt.SetTriggerMode(true);
}

void CameraManager::setExposureTime(float exposureUs)
{
    if (m_brand == CameraBrand::HIKVISION) m_cameraHik.SetExposureTime(exposureUs);
    else m_cameraOpt.SetExposureTime(exposureUs);
}

void CameraManager::setGain(float gain)
{
    if (m_brand == CameraBrand::HIKVISION) m_cameraHik.SetGain(gain);
    else m_cameraOpt.SetGain(gain);
    emit sendLog(tr("Gain set to %1").arg(gain));
}

void CameraManager::setFrameRate(float frameRate)
{
    if (m_brand == CameraBrand::HIKVISION) {
        m_cameraHik.SetBoolValue("AcquisitionFrameRateEnable", true);
        m_cameraHik.SetFloatValue("AcquisitionFrameRate", frameRate);
    } else {
        m_cameraOpt.SetBoolValue("AcquisitionFrameRateEnable", true);
        m_cameraOpt.SetFloatValue("AcquisitionFrameRate", frameRate);
    }
}

void CameraManager::setPixelFormat(unsigned int pixelFormat)
{
    if (m_brand == CameraBrand::HIKVISION) m_cameraHik.SetEnumValue("PixelFormat", pixelFormat);
    else m_cameraOpt.SetEnumValue("PixelFormat", pixelFormat);
}

float CameraManager::getExposureTime()
{
    float val = 0.0f;
    if (m_brand == CameraBrand::HIKVISION) m_cameraHik.GetFloatValue("ExposureTime", val);
    else m_cameraOpt.GetFloatValue("ExposureTime", val); // OPT 获取如失败会默认 0
    return val;
}

float CameraManager::getGain()
{
    float val = 0.0f;
    if (m_brand == CameraBrand::HIKVISION) m_cameraHik.GetFloatValue("Gain", val);
    else m_cameraOpt.GetFloatValue("Gain", val);
    return val;
}

float CameraManager::getFrameRate()
{
    float val = 0.0f;
    if (m_brand == CameraBrand::HIKVISION) m_cameraHik.GetFloatValue("ResultingFrameRate", val);
    else m_cameraOpt.GetFloatValue("ResultingFrameRate", val);
    return val;
}

unsigned int CameraManager::getPixelFormat()
{
    unsigned int val = 0;
    if (m_brand == CameraBrand::HIKVISION) m_cameraHik.GetEnumValue("PixelFormat", val);
    else m_cameraOpt.GetEnumValue("PixelFormat", val);
    return val;
}