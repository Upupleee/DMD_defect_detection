#include "camera_opt.h"

camera_opt::camera_opt()
    : m_hCamera(nullptr)
    , m_bIsConnected(false)
    , m_bIsGrabbing(false)
{
}

camera_opt::~camera_opt()
{
    CloseCamera();
}

int camera_opt::InitSDK()
{
    return 0; // SciCam API 不需要像海康一样进行全局初始化环境
}

int camera_opt::FinalizeSDK()
{
    return 0;
}

int camera_opt::EnumDevices(PSCI_DEVICE_INFO_LIST devInfos, unsigned int tlType)
{
    return SciCam_DiscoveryDevices(devInfos, tlType);
}

int camera_opt::OpenCamera(const PSCI_DEVICE_INFO pstDevInfo)
{
    if (m_hCamera != nullptr) {
        return 0;
    }

    int nRet = SciCam_CreateDevice(&m_hCamera, pstDevInfo);
    if (nRet != 0) return nRet;

    nRet = SciCam_OpenDevice(m_hCamera);
    if (nRet != 0) {
        SciCam_DeleteDevice(m_hCamera);
        m_hCamera = nullptr;
        return nRet;
    }

    m_bIsConnected = true;
    return 0;
}

int camera_opt::CloseCamera()
{
    if (m_hCamera != nullptr) {
        if (m_bIsGrabbing) {
            StopGrabbing();
        }
        SciCam_CloseDevice(m_hCamera);
        SciCam_DeleteDevice(m_hCamera);
        m_hCamera = nullptr;
        m_bIsConnected = false;
    }
    return 0;
}

int camera_opt::SetTriggerMode(bool bTrigger)
{
    if (m_hCamera == nullptr) return -1;
    int nRet = SciCam_SetEnumValueByString(m_hCamera, "TriggerMode", bTrigger ? "On" : "Off");
    if (bTrigger && nRet == 0) {
        // 如果切到触发，默认选中软触发源
        SciCam_SetEnumValueByString(m_hCamera, "TriggerSource", "Software");
    }
    return nRet;
}

int camera_opt::SetExposureTime(float fExposureTime)
{
    if (m_hCamera == nullptr) return -1;
    SciCam_SetEnumValueByString(m_hCamera, "ExposureAuto", "Off");
    return SciCam_SetFloatValue(m_hCamera, "ExposureTime", static_cast<double>(fExposureTime));
}

int camera_opt::SetGain(float fGain)
{
    if (m_hCamera == nullptr) return -1;
    SciCam_SetEnumValueByString(m_hCamera, "GainAuto", "Off");
    return SciCam_SetFloatValue(m_hCamera, "Gain", static_cast<double>(fGain));
}

int camera_opt::SetIntValue(const char* strKey, int64_t nValue)
{
    if (m_hCamera == nullptr) return -1;
    return SciCam_SetIntValue(m_hCamera, strKey, nValue);
}

int camera_opt::SetFloatValue(const char* strKey, float fValue)
{
    if (m_hCamera == nullptr) return -1;
    return SciCam_SetFloatValue(m_hCamera, strKey, static_cast<double>(fValue));
}

int camera_opt::SetEnumValue(const char* strKey, unsigned int nValue)
{
    if (m_hCamera == nullptr) return -1;
    return SciCam_SetEnumValue(m_hCamera, strKey, static_cast<int64_t>(nValue));
}

int camera_opt::SetBoolValue(const char* strKey, bool bValue)
{
    if (m_hCamera == nullptr) return -1;
    return SciCam_SetBoolValue(m_hCamera, strKey, bValue);
}

int camera_opt::GetFloatValue(const char* strKey, float& fValue)
{
    if (m_hCamera == nullptr) return -1;
    
    SCI_NODE_VAL_FLOAT val;
    int nRet = SciCam_GetFloatValue(m_hCamera, strKey, &val);
    if (nRet == 0) {
        fValue = static_cast<float>(val.dVal);
    }
    return nRet;
}

int camera_opt::GetIntValue(const char* strKey, int64_t& nValue)
{
    if (m_hCamera == nullptr) return -1;
    
    SCI_NODE_VAL_INT val;
    int nRet = SciCam_GetIntValue(m_hCamera, strKey, &val);
    if (nRet == 0) {
        nValue = val.nVal;
    }
    return nRet;
}

int camera_opt::GetEnumValue(const char* strKey, unsigned int& nValue)
{
    if (m_hCamera == nullptr) return -1;
    
    SCI_NODE_VAL_ENUM val;
    int nRet = SciCam_GetEnumValue(m_hCamera, strKey, &val);
    if (nRet == 0) {
        nValue = static_cast<unsigned int>(val.nVal);
    }
    return nRet;
}

void camera_opt::SetImageCallback(ImageCallback callback)
{
    m_imageCallback = callback;
}

void SCICALL camera_opt::InternalImageCallback(void* payload, void* tag)
{
    camera_opt* pCamera = static_cast<camera_opt*>(tag);
    if (pCamera && pCamera->m_imageCallback) {
        SCI_CAM_PAYLOAD_ATTRIBUTE attr;
        // 先解析出 Payload 格式元数据
        if (SciCam_Payload_GetAttribute(payload, &attr) == 0) {
            void* pImg = nullptr;
            // 拿到真实的缓存首地址
            if (SciCam_Payload_GetImage(payload, &pImg) == 0 && pImg != nullptr) {
                pCamera->m_imageCallback((unsigned char*)pImg, 
                                         attr.imgAttr.width, 
                                         attr.imgAttr.height, 
                                         (int)attr.imgAttr.pixelType);
            }
        }
    }
}

int camera_opt::StartGrabbing()
{
    if (m_hCamera == nullptr || m_bIsGrabbing) return -1;

    // 最后一个参数设为 true 代表执行完毕后自动回收 payload 内存
    int nRet = SciCam_RegisterPayloadCallBack(m_hCamera, InternalImageCallback, this, true);
    if (nRet != 0) return nRet;

    nRet = SciCam_StartGrabbing(m_hCamera);
    if (nRet == 0) {
        m_bIsGrabbing = true;
    }
    return nRet;
}

int camera_opt::StopGrabbing()
{
    if (m_hCamera == nullptr || !m_bIsGrabbing) return -1;

    int nRet = SciCam_StopGrabbing(m_hCamera);
    if (nRet == 0) {
        m_bIsGrabbing = false;
    }
    return nRet;
}

int camera_opt::TriggerSoftware()
{
    if (m_hCamera == nullptr) return -1;
    return SciCam_SetCommandValue(m_hCamera, "TriggerSoftware");
}

int camera_opt::SetFrameRate(float fFrameRate)
{
    if (m_hCamera == nullptr) return -1;

    // 第一步：需要先启用帧率控制 (AcquisitionFrameRateEnable 或 AcquisitionFrameRateControlEnable, 依相机固件可能是Bool也可能是Enum)
    // 通常标准 GenICam 设定为：
    int nRet = SciCam_SetBoolValue(m_hCamera, "AcquisitionFrameRateEnable", true);
    if(nRet != 0){
        // 如果是枚举型
        SciCam_SetEnumValueByString(m_hCamera, "AcquisitionFrameRateControlEnable", "On");
    }

    // 第二步：写入具体的帧率浮点值
    return SciCam_SetFloatValue(m_hCamera, "AcquisitionFrameRate", static_cast<double>(fFrameRate));
}

int camera_opt::GetFrameRate(float& fFrameRate)
{
    if (m_hCamera == nullptr) return -1;
    
    PSCI_NODE_VAL_FLOAT pVal = new SCI_NODE_VAL_FLOAT;
    // 获取实际输出帧率
    int nRet = SciCam_GetFloatValue(m_hCamera, "ResultingFrameRate", pVal);
    if (nRet == 0) {
        fFrameRate = static_cast<float>(pVal->dVal);
    } else {
        // 如果相机不支持 ResultingFrameRate 节点，就读取设置的理论帧率
        nRet = SciCam_GetFloatValue(m_hCamera, "AcquisitionFrameRate", pVal);
        if (nRet == 0) {
            fFrameRate = static_cast<float>(pVal->dVal);
        }
    }
    
    delete pVal;
    return nRet;
}