#pragma once
#ifndef DETECT_H
#define DETECT_H

#include "HalconCpp.h"
#include <vector>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <QObject>
#include <QImage>

using HalconCpp::HObject;
using HalconCpp::HTuple;
using namespace std;

// 由外部程序导出的函数的声明，供 Detect 类调用
void GetDPointLocation(HObject ho_Image, HTuple hv_DMD_Num1, HTuple hv_DMD_Num2, HTuple *hv_Location);

void scale_image_range(HObject ho_Image, HObject* ho_ImageScaled, HTuple hv_Min, HTuple hv_Max);

void detect_dmd_bad_pixels(HObject ho_Image, HObject* ho_ImagePart, HObject* ho_DefectCrosses,
    HTuple hv_DmdResRow, HTuple hv_DmdResCol, HTuple hv_MinArea, HTuple hv_GrayThresh,
    HTuple hv_GenCross, HTuple hv_invert, HTuple* hv_BadMirrorRows, HTuple* hv_BadMirrorCols,
    HTuple* hv_ValidRowStrings); // 新增接收无坏点行的参数

// OCR_opt.cpp 中实现的完整 OCR 流程函数
void extract_and_read_ocr(HObject ho_Image, HObject* ho_ImagePart, HObject* ho_SortedChars,
    HTuple hv_OCRHandle, HTuple* hv_LineStrings);

void paint_crosses_to_image(HalconCpp::HObject ho_ImageIn, HalconCpp::HObject ho_DefectCrosses, HalconCpp::HObject* ho_ImageOut, HalconCpp::HTuple hv_GenCrossOption);


class Detect
{
public:
    Detect();
    ~Detect();

    struct DetectResult {
        double row;     // 网格对应的行
        double column;  // 网格对应的列
        double area;    // (新算法不需要面积，先保留兼容结构)
    };

    // 旧接口保留：默认走旧算法（algorithmType=0）
    bool ProcessImage(unsigned char* pData, int nWidth, int nHeight, int nPixelFormat,
        double dmdNumX = 1920, double dmdNumY = 1080);

    // 新增重载：调用时手动传算法类型
    // algorithmType: 0=GetDPointLocation(旧), 1=detect_dmd_bad_pixels(新)
    // invert: true=反色场景（暗场亮点），false=正常场景（亮场暗点）
    // 在旧有声明后，更新 ProcessImage 签名，新增 QImage* pPaintedImg 参数
    bool ProcessImage(unsigned char* pData, int nWidth, int nHeight, int nPixelFormat,
        double dmdNumX, double dmdNumY, int algorithmType,
        double minArea = 3.0, double grayThresh = 230.0, bool genCross = false, bool invert = false, QImage* pPaintedImg = nullptr);
    
    bool CreateOCRModel();
    bool OCR_Read(const HObject image, HTuple& text);
    
    const vector<DetectResult>& GetResults() const {
        return vecResults;
    }

    // 获取格式化的按行坏点结果描述
    const QStringList& GetValidRowStrings() const {
        return m_validRowStrings;
    }

private:
    vector<DetectResult> vecResults;
    QStringList m_validRowStrings; // 存储连续无坏点行的描述文字
    HTuple OCRHandle; // OCR模型句柄
    HTuple ocrModelPath; // OCR模型路径
    HTuple TextModel; // OCR文本模型
};

#endif // DETECT_H
