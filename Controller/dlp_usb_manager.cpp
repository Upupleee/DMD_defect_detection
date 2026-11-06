#include "dlp_usb_manager.h"
#include <vector>

DlpUsbManager::DlpUsbManager(QObject* parent) : QObject(parent)
{
}

DlpUsbManager::~DlpUsbManager()
{
}

bool DlpUsbManager::sendStaticImage(const QString& imagePath)
{
    // 将 QString 转换为 const char* 传入底层纯 C SDK
    QByteArray pathBytes = imagePath.toLocal8Bit();
    const char* bmp_path = pathBytes.constData();

    // 分配一个缓冲区用来接收错误信息
    const int ERR_MSG_SIZE = 1024;
    std::vector<char> err_msg_buf(ERR_MSG_SIZE, '\0');

    // 调用 C 接口，传入缓冲区的指针和大小
    int rc = dlp_send_static_bmp(bmp_path, err_msg_buf.data(), ERR_MSG_SIZE);
    
    if (rc != 0) {
        // 读取错误信息并发送到主界面日志
        QString errorMsg = QString::fromLocal8Bit(err_msg_buf.data());
        emit sendLog(tr("USB Send Failed: rc=%1, err=%2").arg(rc).arg(errorMsg));
        return false;
    }
    
    emit sendLog(tr("USB Send Success: %1").arg(imagePath));
    return true;
}