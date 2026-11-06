#include "vision_manager.h"
#include <cstring>

namespace
{
bool BuildContiguousGrayBuffer(const QImage& image,
                               QImage& grayImage,
                               QByteArray& contiguousData,
                               unsigned char*& pData,
                               int& width,
                               int& height)
{
    if (image.isNull()) {
        pData = nullptr;
        width = 0;
        height = 0;
        return false;
    }

    grayImage = image.convertToFormat(QImage::Format_Grayscale8);
    width = grayImage.width();
    height = grayImage.height();

    if (width <= 0 || height <= 0) {
        pData = nullptr;
        return false;
    }

    if (grayImage.bytesPerLine() == width) {
        pData = grayImage.bits();
    } else {
        contiguousData.resize(width * height);
        for (int y = 0; y < height; ++y) {
            std::memcpy(contiguousData.data() + y * width, grayImage.constScanLine(y), width);
        }
        pData = reinterpret_cast<unsigned char*>(contiguousData.data());
    }

    return (pData != nullptr);
}
}

VisionManager::VisionManager(QObject *parent)
    : QObject(parent),
      m_detect(new Detect())
{
}

VisionManager::~VisionManager()
{
    delete m_detect;
}

void VisionManager::initOCR()
{
    if (m_detect->CreateOCRModel()) {
        emit sendLog(tr("OCR Model initialized successfully."));
    } else {
        emit sendLog(tr("Warning: Failed to initialize OCR Model."));
    }
}

void VisionManager::setDMDResolution(int width, int height)
{
    m_dmdGridX = width;
    m_dmdGridY = height;
    emit sendLog(tr("DMD parameters updated: Grid %1 x %2").arg(width).arg(height));    
}

void VisionManager::getDMDResolution(int &width, int &height) const
{
    width = m_dmdGridX;
    height = m_dmdGridY;
}

void VisionManager::setDetectionDebugConfig(int algorithmType, double minArea, double grayThresh, bool genCross, bool invert)
{
    m_algorithmType = (algorithmType == 0) ? 0 : 1;
    m_minArea = (minArea > 0.0) ? minArea : 1.0;
    if (grayThresh < 0.0) grayThresh = 0.0;
    if (grayThresh > 255.0) grayThresh = 255.0;
    m_grayThresh = grayThresh;
    m_genCross = genCross;
    m_invert = invert;

    emit sendLog(tr("Detection debug config updated: Alg=%1, MinArea=%2, GrayThresh=%3, GenCross=%4, Invert=%5")
                 .arg(m_algorithmType)
                 .arg(m_minArea, 0, 'f', 2)
                 .arg(m_grayThresh, 0, 'f', 2)
                 .arg(m_genCross ? "true" : "false")
                 .arg(m_invert  ? "true" : "false"));
}

void VisionManager::getDetectionDebugConfig(int& algorithmType, double& minArea, double& grayThresh, bool& genCross, bool& invert) const
{
    algorithmType = m_algorithmType;
    minArea       = m_minArea;
    grayThresh    = m_grayThresh;
    genCross      = m_genCross;
    invert        = m_invert;
}

std::vector<Detect::DetectResult> VisionManager::runDetection(const QImage& image, QImage* outPaintedImg, QStringList* outValidRowStrings)
{
    std::vector<Detect::DetectResult> emptyRes;

    if (image.isNull()) {
        emit sendLog(tr("Error: No image to process."));
        return emptyRes;
    }

    if (m_detect == nullptr) {
        emit sendLog(tr("Error: Detect module is null."));
        return emptyRes;
    }

    if (m_dmdGridX <= 0 || m_dmdGridY <= 0) {
        emit sendLog(tr("Error: Invalid DMD resolution."));
        return emptyRes;
    }

    QImage grayImage;
    QByteArray contiguousData;
    unsigned char* pData = nullptr;
    int nWidth = 0;
    int nHeight = 0;
    const int nPixelFormat = 1;

    if (!BuildContiguousGrayBuffer(image, grayImage, contiguousData, pData, nWidth, nHeight)) {
        emit sendLog(tr("Error: Failed to prepare grayscale image buffer."));
        return emptyRes;
    }

    // 传递 outPaintedImg 给 ProcessImage
    bool success = m_detect->ProcessImage(
        pData, nWidth, nHeight, nPixelFormat,
        m_dmdGridX, m_dmdGridY, m_algorithmType,
        m_minArea, m_grayThresh, m_genCross, m_invert, outPaintedImg);  

    if (!success) {
        emit sendLog(tr("Error: Detection algorithm failed. Check application output."));
        return emptyRes;
    }

    const auto& results = m_detect->GetResults();
    
    // 【新增】如果外部传入了指针，就将无坏点行结果扔给它
    if (outValidRowStrings != nullptr) {
        *outValidRowStrings = m_detect->GetValidRowStrings();
    }

    emit sendLog(tr("Detection finished."));

    return results;
}

QString VisionManager::runSerialDetection(const QImage& image)
{
    if (image.isNull()) {
        emit sendLog(tr("Error: No image to process."));
        return "";
    }

    emit sendLog(tr("OCR Serial detection processing started..."));

    QImage grayImage;
    QByteArray contiguousData;
    unsigned char* pData = nullptr;
    int nWidth = 0;
    int nHeight = 0;

    if (!BuildContiguousGrayBuffer(image, grayImage, contiguousData, pData, nWidth, nHeight)) {
        emit sendLog(tr("Error: Failed to prepare OCR grayscale image buffer."));
        return "";
    }

    try {
        HalconCpp::HObject ho_Image;
        HalconCpp::GenImage1(&ho_Image, "byte", nWidth, nHeight, (Hlong)pData);

        HalconCpp::HTuple textTuple;
        if (m_detect->OCR_Read(ho_Image, textTuple)) {
            if (textTuple.Length() > 0) {
                // extract_and_read_ocr 已将多行结果以 "\n" 连接后 TupleSum 成单字符串
                QString ocrResult = QString::fromLocal8Bit(textTuple.S().Text());
                ocrResult = ocrResult.trimmed(); // 去除首尾空白/换行
                if (!ocrResult.isEmpty()) {
                    emit sendLog(tr("OCR Serial Result: %1").arg(ocrResult));
                    return ocrResult;
                }
            }
            emit sendLog(tr("Warning: No serial number detected in the image."));
        } else {
            emit sendLog(tr("Error: OCR Read failed."));
        }
    } catch (HalconCpp::HException& e) {
        QString errMsg = QString::fromLocal8Bit(e.ErrorMessage().Text());
        emit sendLog(tr("Halcon Exception during OCR: %1").arg(errMsg));
    }
    return "";
}
