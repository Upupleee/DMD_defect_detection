#ifndef VISION_MANAGER_H
#define VISION_MANAGER_H

#include <QObject>
#include <QImage>
#include <QString>
#include "detect.h"

class VisionManager : public QObject
{
    Q_OBJECT
public:
    explicit VisionManager(QObject *parent = nullptr);
    ~VisionManager();

    // 修改函数签名，增加一个 QStringList 参数来输出无坏点记录
    std::vector<Detect::DetectResult> runDetection(const QImage& image, QImage* outPaintedImg = nullptr, QStringList* outValidRowStrings = nullptr);
    // --- OCR 相关接口 ---
    void initOCR();
    QString runSerialDetection(const QImage& image);

    // DMD属性设置接口
    void setDMDResolution(int width, int height);
    void getDMDResolution(int &width, int &height) const;

    // 算法调试参数接口（前端可动态设置）
    // algorithmType: 0=旧算法, 1=新算法
    // invert: true=反色场景（暗场亮点检测），false=正常场景（亮场暗点检测）
    void setDetectionDebugConfig(int algorithmType, double minArea, double grayThresh, bool genCross, bool invert = false);
    void getDetectionDebugConfig(int& algorithmType, double& minArea, double& grayThresh, bool& genCross, bool& invert) const;


signals:
    // 将处理日志通过信号发给 UI
    void sendLog(const QString& logMsg);

private:
    Detect* m_detect;

    // DMD 参数缓存，默认值 1920x1080
    double m_dmdGridX = 1920;
    double m_dmdGridY = 1080;

    // 算法参数缓存
    int    m_algorithmType = 1;
    double m_minArea       = 4.0;
    double m_grayThresh    = 128;
    bool   m_genCross      = true;
    bool   m_invert        = false; // 新增：反色控制标志
};

#endif // VISION_MANAGER_H
