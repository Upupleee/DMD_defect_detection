#include "camera_base.h"
#include <iostream>

camera_base::camera_base() 
    : m_hCamera(nullptr)
    , m_bIsConnected(false)
    , m_bIsGrabbing(false)
{
}

camera_base::~camera_base()
{
    CloseCamera();
}

int camera_base::InitSDK()
{
    return MV_CC_Initialize();
}

int camera_base::FinalizeSDK()
{
    return MV_CC_Finalize();
}

int camera_base::EnumDevices(MV_CC_DEVICE_INFO_LIST& stDeviceList)
{
    // 枚举千兆网口和USB相机
    unsigned int nTLayerType = MV_GIGE_DEVICE | MV_USB_DEVICE;
    return MV_CC_EnumDevices(nTLayerType, &stDeviceList);
}

int camera_base::OpenCamera(MV_CC_DEVICE_INFO* pstDevInfo)
{
    if (m_hCamera != nullptr) {
        return MV_OK;
    }

    int nRet = MV_OK;

    // 1. 创建句柄
    nRet = MV_CC_CreateHandle(&m_hCamera, pstDevInfo);
    if (MV_OK != nRet) return nRet;

    // 2. 打开设备
    nRet = MV_CC_OpenDevice(m_hCamera);
    if (MV_OK != nRet) {
        MV_CC_DestroyHandle(m_hCamera);
        m_hCamera = nullptr;
        return nRet;
    }
    // ==============================================================
    // 【解封并安全启用最佳包大小】
    // 只针对纯网口相机才做这个动作，而且即使失败也不阻断打开流程
    // ==============================================================
    if (pstDevInfo->nTLayerType == MV_GIGE_DEVICE) {
        int nPacketSize = MV_CC_GetOptimalPacketSize(m_hCamera);
        if (nPacketSize > 0) {
            // 将网卡探测出能支持的最大负载单元压给相机
            int tempRet = SetIntValue("GevSCPSPacketSize", nPacketSize);
            if (tempRet != MV_OK) {
                std::cerr << "Warning: Failed to set packet size, error code: " << std::hex << tempRet << std::endl;
            }
        }
    }


    m_bIsConnected = true;
    return MV_OK;
}

int camera_base::CloseCamera()
{
    if (m_hCamera != nullptr) {
        if (m_bIsGrabbing.load()) {
            StopGrabbing();
        }
        MV_CC_CloseDevice(m_hCamera);
        MV_CC_DestroyHandle(m_hCamera);
        m_hCamera = nullptr;
        m_bIsConnected = false;
    }
    return MV_OK;
}

int camera_base::SetExposureTime(float fExposureTime)
{
    if (m_hCamera == nullptr) return MV_E_HANDLE;
    // 必须先关闭自动曝光才能设置具体的曝光值
    MV_CC_SetEnumValue(m_hCamera, "ExposureAuto", MV_EXPOSURE_AUTO_MODE_OFF);
    return MV_CC_SetFloatValue(m_hCamera, "ExposureTime", fExposureTime);
}

int camera_base::SetGain(float fGain)
{
    if (m_hCamera == nullptr) return MV_E_HANDLE;
    MV_CC_SetEnumValue(m_hCamera, "GainAuto", MV_GAIN_MODE_OFF);
    return MV_CC_SetFloatValue(m_hCamera, "Gain", fGain);
}

int camera_base::SetIntValue(const char* strKey, int64_t nValue)
{
    if (m_hCamera == nullptr) return MV_E_HANDLE;
    return MV_CC_SetIntValueEx(m_hCamera, strKey, nValue);
}

void camera_base::SetImageCallback(ImageCallback callback)
{
    m_imageCallback = callback;
}

// 取代原来的 InternalImageCallback
void camera_base::WorkThread()
{
    int nRet = MV_OK;
    MV_FRAME_OUT stOutFrame = {0};

    while(m_bIsGrabbing.load())
    {
        // 高带宽下必须调用官方主动取流接口，附带 1000ms 超时
        // 如果没图（比如触发模式未触发），他会返回超时，随后继续下一轮循环
        nRet = MV_CC_GetImageBuffer(m_hCamera, &stOutFrame, 1000);
        
        if (nRet == MV_OK) {
            // 如果拉到图了，把图传给上层 Manager (此时依旧处于此分线程中)
            if (m_imageCallback) {
                m_imageCallback(&stOutFrame);
            }
            
            // 重要：用完之后原线程即可将 Buffer 的锁定权限还回 SDK 底层！
            nRet = MV_CC_FreeImageBuffer(m_hCamera, &stOutFrame);
            if(nRet != MV_OK) {
                std::cerr << "Free Image Buffer fail!" << std::endl;
            }
        }
    }
}

int camera_base::StartGrabbing()
{
    if (m_hCamera == nullptr || m_bIsGrabbing.load()) return MV_E_HANDLE;

    // 【1】按大带宽相机的官方建议，设置最优内部节点数为 2~3 个防止饥饿
    MV_CC_SetImageNodeNum(m_hCamera, 2);

    // 【2】开启相机的物理吐流引擎
    int nRet = MV_CC_StartGrabbing(m_hCamera);
    if (MV_OK == nRet) {
        m_bIsGrabbing = true;
        // 【3】启动我们在应用层自己管理的一个拉流线程，疯狂调用 GetImageBuffer
        m_pGrabThread = new std::thread(&camera_base::WorkThread, this);
    }
    return nRet;
}

int camera_base::StopGrabbing()
{
    if (m_hCamera == nullptr || !m_bIsGrabbing.load()) return MV_E_HANDLE;

    // 【1】首先将抓流标识位置假，让我们的拉流线程不再继续锁 SDK
    m_bIsGrabbing = false;
    if (m_pGrabThread && m_pGrabThread->joinable()) {
        m_pGrabThread->join(); // 等待拉流线程彻底安息出清
        delete m_pGrabThread;
        m_pGrabThread = nullptr;
    }

    // 【2】然后再安全请求海康关闭物理引擎
    int nRet = MV_CC_StopGrabbing(m_hCamera);
    return nRet;
}

// 完善 SetTriggerMode：当开启触发时，默认指定为软触发（也可指定其它Line输入）
int camera_base::SetTriggerMode(bool bTrigger)
{
    if (m_hCamera == nullptr) return MV_E_HANDLE;
    int nRet = MV_CC_SetEnumValue(m_hCamera, "TriggerMode", bTrigger ? MV_TRIGGER_MODE_ON : MV_TRIGGER_MODE_OFF);

    // 如果是开启触发，默认设置为软触发源
    if (bTrigger && nRet == MV_OK) {
        MV_CC_SetEnumValue(m_hCamera, "TriggerSource", MV_TRIGGER_SOURCE_SOFTWARE);
    }
    return nRet;
}

// 执行一次软触发
int camera_base::TriggerSoftware()
{
    if (m_hCamera == nullptr) return MV_E_HANDLE;
    return MV_CC_SetCommandValue(m_hCamera, "TriggerSoftware");
}

int camera_base::GetFloatValue(const char* strKey, float& fValue)
{
    if (m_hCamera == nullptr) return MV_E_HANDLE;
    MVCC_FLOATVALUE stParam;
    memset(&stParam, 0, sizeof(MVCC_FLOATVALUE));
    int nRet = MV_CC_GetFloatValue(m_hCamera, strKey, &stParam);
    if (nRet == MV_OK) fValue = stParam.fCurValue;
    return nRet;
}

int camera_base::GetIntValue(const char* strKey, int64_t& nValue)
{
    if (m_hCamera == nullptr) return MV_E_HANDLE;
    MVCC_INTVALUE_EX stParam;
    memset(&stParam, 0, sizeof(MVCC_INTVALUE_EX));
    int nRet = MV_CC_GetIntValueEx(m_hCamera, strKey, &stParam);
    if (nRet == MV_OK) nValue = stParam.nCurValue;
    return nRet;
}

int camera_base::SetFloatValue(const char* strKey, float fValue)
{
    if (m_hCamera == nullptr) return MV_E_HANDLE;
    return MV_CC_SetFloatValue(m_hCamera, strKey, fValue);
}

int camera_base::SetEnumValue(const char* strKey, unsigned int nValue)
{
    if (m_hCamera == nullptr) return MV_E_HANDLE;
    return MV_CC_SetEnumValue(m_hCamera, strKey, nValue);
}

int camera_base::GetEnumValue(const char* strKey, unsigned int& nValue)
{
    if (m_hCamera == nullptr) return MV_E_HANDLE;
    MVCC_ENUMVALUE stParam;
    memset(&stParam, 0, sizeof(MVCC_ENUMVALUE));
    int nRet = MV_CC_GetEnumValue(m_hCamera, strKey, &stParam);
    if (nRet == MV_OK) nValue = stParam.nCurValue;
    return nRet;
}
int camera_base::SetBoolValue(const char* strKey, bool bValue)
{
    if (m_hCamera == nullptr) return MV_E_HANDLE;
    return MV_CC_SetBoolValue(m_hCamera, strKey, bValue);
}
