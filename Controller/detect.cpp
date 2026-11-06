#include "detect.h"
#include <QLabel>

using namespace HalconCpp;

Detect::Detect()
{
}

Detect::~Detect()
{
}

// 旧接口保留：默认走旧算法
bool Detect::ProcessImage(unsigned char* pData, int nWidth, int nHeight, int nPixelFormat, double dmdNumX, double dmdNumY)
{
    return ProcessImage(pData, nWidth, nHeight, nPixelFormat, dmdNumX, dmdNumY, 0);
}

// 新重载：按 algorithmType 手动切换，新增 invert 参数
bool Detect::ProcessImage(unsigned char* pData, int nWidth, int nHeight, int nPixelFormat,
    double dmdNumX, double dmdNumY, int algorithmType,
    double minArea, double grayThresh, bool genCross, bool invert, QImage* pPaintedImg)
{
    (void)nPixelFormat;

    if (pData == nullptr || nWidth <= 0 || nHeight <= 0 || dmdNumX <= 0 || dmdNumY <= 0) {
        return false;
    }

    vecResults.clear();
    m_validRowStrings.clear(); // 每次调用前清空行结果

    try
    {
        HObject ho_Image;
        GenImage1(&ho_Image, "byte", nWidth, nHeight, (Hlong)pData);

        if (algorithmType == 0)
        {
            // ===== 旧算法：GetDPointLocation =====
            HTuple hv_Location;
            GetDPointLocation(ho_Image, dmdNumX, dmdNumY, &hv_Location);

            const Hlong numDefects = hv_Location.TupleLength() / 2;
            for (Hlong i = 0; i < numDefects; ++i) {
                DetectResult res;
                res.row = hv_Location[i].D();
                res.column = hv_Location[i + numDefects].D();
                res.area = 0.0;
                vecResults.push_back(res);
            }
        }
        else
        {
            // ===== 新算法：detect_dmd_bad_pixels =====
            HObject ho_ImagePart, ho_DefectCrosses;
            HTuple hv_BadMirrorRows, hv_BadMirrorCols, hv_ValidRowStrings;

            // 注意：新算法参数是 (行数, 列数) => (dmdNumY, dmdNumX)
            detect_dmd_bad_pixels(
                ho_Image,
                &ho_ImagePart,
                &ho_DefectCrosses,
                dmdNumY,
                dmdNumX,
                minArea,
                grayThresh,
                genCross ? "true" : "false",
                invert ? "true" : "false",   // 新增：反色控制参数
                &hv_BadMirrorRows,
                &hv_BadMirrorCols,
                &hv_ValidRowStrings          // 新增：提取无坏点行分布记录
            );

            // 存储无坏点行信息记录到 QStringList
            const Hlong stringsCount = hv_ValidRowStrings.TupleLength();
            for (Hlong i = 0; i < stringsCount; ++i) {
                m_validRowStrings.push_back(QString::fromUtf8((const char*)hv_ValidRowStrings[i].S()));
            }

            // 当允许生成十字线图，且外部请求获取它时，触发绘画转换
            if (genCross && pPaintedImg != nullptr) {
                HObject ho_ImageOut;
                paint_crosses_to_image(ho_ImagePart, ho_DefectCrosses, &ho_ImageOut, "true");

                HTuple hv_Width, hv_Height;
                GetImageSize(ho_ImageOut, &hv_Width, &hv_Height);
                HTuple hv_PtrRed, hv_PtrGreen, hv_PtrBlue, hv_Type;
                GetImagePointer3(ho_ImageOut, &hv_PtrRed, &hv_PtrGreen, &hv_PtrBlue, &hv_Type, &hv_Width, &hv_Height);

                int w = hv_Width.I();
                int h = hv_Height.I();
                QImage imgTmp(w, h, QImage::Format_RGB888);

                const uchar* pR = (const uchar*)hv_PtrRed.L();
                const uchar* pG = (const uchar*)hv_PtrGreen.L();
                const uchar* pB = (const uchar*)hv_PtrBlue.L();

                // 将 Halcon 分离的通道数据组装成 Qt 的交错 RGB 格式
                for (int r = 0; r < h; r++) {
                    uchar* line = imgTmp.scanLine(r);
                    for (int c = 0; c < w; c++) {
                        line[c * 3] = pR[r * w + c];
                        line[c * 3 + 1] = pG[r * w + c];
                        line[c * 3 + 2] = pB[r * w + c];
                    }
                }
                *pPaintedImg = imgTmp.copy();
            }

            const Hlong rowCount = hv_BadMirrorRows.TupleLength();
            const Hlong colCount = hv_BadMirrorCols.TupleLength();
            const Hlong defectCount = (rowCount < colCount) ? rowCount : colCount;

            for (Hlong i = 0; i < defectCount; ++i) {
                DetectResult res;
                // 新算法输出为 0-based；这里 +1 保持和旧算法一致（1-based）
                res.row = hv_BadMirrorRows[i].D() + 1.0;
                res.column = hv_BadMirrorCols[i].D() + 1.0;
                res.area = 0.0;
                vecResults.push_back(res);
            }
        }

        return true;
    }
    catch (HException& e)
    {
        QString errMsg = QString::fromLocal8Bit(e.ErrorMessage().Text());
        qDebug() << QObject::tr("Halcon Exception in ProcessImage:") << errMsg;
        return false;
    }
}

bool Detect::CreateOCRModel()
{
    try
    {
        // 如果外部没有设置路径，则提供一个 Halcon 自带的标准内置字典路径备选
        if (ocrModelPath.Length() == 0) {
            ocrModelPath = "Industrial_0-9A-Z_NoRej.omc";
        }

        ReadOcrClassMlp(ocrModelPath, &OCRHandle);
        CreateTextModelReader("auto", OCRHandle, &TextModel);

        return true;
    }
    catch (HException& e)
    {
        QString errMsg = QString::fromLocal8Bit(e.ErrorMessage().Text());
        qDebug() << QObject::tr("Halcon OCR Initialization Exception:") << errMsg;
        return false;
    }
}

bool Detect::OCR_Read(HObject image, HTuple& text)
{
    try
    {
        // 调用 OCR_opt.cpp 中实现的完整 OCR 流程：
        // 包含区域定位、仿射旋转校正、动态阈值分割、字符筛选与分行识别
        HObject ho_ImagePart;
        HObject ho_SortedChars;
        HTuple hv_LineStrings;

        extract_and_read_ocr(image, &ho_ImagePart, &ho_SortedChars, OCRHandle, &hv_LineStrings);

        // 将多行结果拼接为单个字符串（行间用换行符分隔）
        const Hlong lineCount = hv_LineStrings.TupleLength();
        if (lineCount == 0) {
            text = HTuple();
            return true; // 流程正常，但图中无文字
        }

        HTuple joinedText = hv_LineStrings[0].S();
        for (Hlong i = 1; i < lineCount; ++i) {
            joinedText = joinedText.TupleConcat(HTuple("\n")).TupleConcat(hv_LineStrings[i].S());
        }
        TupleSum(joinedText, &text);

        return true;
    }
    catch (HException& e)
    {
        QString errMsg = QString::fromLocal8Bit(e.ErrorMessage().Text());
        qDebug() << QObject::tr("Halcon OCR Read Exception:") << errMsg;
        return false;
    }
}

