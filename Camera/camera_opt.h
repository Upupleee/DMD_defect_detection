#ifndef CAMERA_OPT_H
#define CAMERA_OPT_H

#include <string>
#include <functional>
#include "SciCam.h"
#include "SciCamPayload.h"
#include "SciCamInfo.h"

class camera_opt
{
public:
    camera_opt();
    ~camera_opt();

    static int InitSDK();
    static int FinalizeSDK();
    // 搜索设备，tlType 为 0 表示搜索所有类型的 OPT 设备
    static int EnumDevices(PSCI_DEVICE_INFO_LIST devInfos, unsigned int tlType = 0);

    int OpenCamera(const PSCI_DEVICE_INFO pstDevInfo);
    int CloseCamera();

    int SetTriggerMode(bool bTrigger);
    int SetExposureTime(float fExposureTime);
    int SetGain(float fGain);
    
    // 【新增】帧率相关的接口
    int SetFrameRate(float fFrameRate);
    int GetFrameRate(float& fFrameRate);
    
    int SetIntValue(const char* strKey, int64_t nValue);
    int SetFloatValue(const char* strKey, float fValue);
    int SetEnumValue(const char* strKey, unsigned int nValue);
    int SetBoolValue(const char* strKey, bool bValue);

    int GetFloatValue(const char* strKey, float& fValue);
    int GetIntValue(const char* strKey, int64_t& nValue);
    int GetEnumValue(const char* strKey, unsigned int& nValue);

    int StartGrabbing();
    int StopGrabbing();
    int TriggerSoftware();

    // 回调抛出宽、高以及 PixelType
    using ImageCallback = std::function<void(unsigned char* pData, int width, int height, int pixelType)>;
    void SetImageCallback(ImageCallback callback);

private:
    static void SCICALL InternalImageCallback(void* payload, void* tag);

private:
    void* m_hCamera;               // 内部相机句柄
    bool  m_bIsConnected;          // 相机连接状态
    bool  m_bIsGrabbing;           // 是否正在取流
    ImageCallback m_imageCallback; // 外层注册的回调函数
};

#endif // CAMERA_OPT_H