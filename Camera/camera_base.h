#ifndef CAMERA_BASE_H
#define CAMERA_BASE_H

#include <string>
#include <vector>
#include <functional>
#include "MvCameraControl.h" // 包含海康SDK头文件
#include <thread>
#include <atomic>

class camera_base
{
public:
    camera_base();
    virtual ~camera_base();

    // 1. 全局环境初始化与清理（一般在main或系统启动时调用）
    static int InitSDK();
    static int FinalizeSDK();

    // 2. 枚举所有在线设备
    static int EnumDevices(MV_CC_DEVICE_INFO_LIST& stDeviceList);

    // 3. 生命周期管理
    int OpenCamera(MV_CC_DEVICE_INFO* pstDevInfo);
    int CloseCamera();

    // 4. 参数配置
    int SetTriggerMode(bool bTrigger);
    int SetExposureTime(float fExposureTime);
    int SetGain(float fGain);
    int GetStringValue(const char* strKey, std::string& strValue);
    int SetBoolValue(const char* strKey, bool bValue);
    int SetIntValue(const char* strKey, int64_t nValue);
    int SetFloatValue(const char* strKey, float fValue);
    int SetEnumValue(const char* strKey, unsigned int nValue);
    int GetEnumValue(const char* strKey, unsigned int& nValue);
    // 5. 图像采集控制
    int StartGrabbing();
    int StopGrabbing();

    // 6. 注册图像回调（使用标准库function抛出给外层业务逻辑处理）
    //using ImageCallback = std::function<void(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo)>;
    // 优化回调定义，直接传递结构体
    using ImageCallback = std::function<void(MV_FRAME_OUT* pstFrame)>;
    void SetImageCallback(ImageCallback callback);

    // 7. 软触发
    int TriggerSoftware();
    int GetFloatValue(const char* strKey, float& fValue);
    int GetIntValue(const char* strKey, int64_t& nValue);

    void* GetHandle() const { return m_hCamera; }


private:
    // SDK 内部回调静态函数
    //static void __stdcall InternalImageCallback(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser);
    static void __stdcall InternalImageCallback(MV_FRAME_OUT* pstFrame, void* pUser, bool bAutoFree);
    void WorkThread();


private:
    void* m_hCamera;                   // 内部相机句柄
    std::atomic<bool> m_bIsConnected{false};            // 相机连接状态
    std::atomic<bool> m_bIsGrabbing{false};             // 是否正在取流
    ImageCallback m_imageCallback;         // 外层注册的回调函数

    // 新增：抓图专用后台拉取线程
    std::thread* m_pGrabThread = nullptr;
};
#endif // CAMERA_BASE_H
