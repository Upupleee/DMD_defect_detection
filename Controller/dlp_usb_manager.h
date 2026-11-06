#ifndef DLP_USB_MANAGER_H
#define DLP_USB_MANAGER_H

#include <QObject>
#include <QString>
#include "dlp_usb_c.h"

class DlpUsbManager : public QObject
{
    Q_OBJECT
public:
    explicit DlpUsbManager(QObject* parent = nullptr);
    ~DlpUsbManager();

    // 提供对外的发图接口
    bool sendStaticImage(const QString& imagePath);

signals:
    // 用于向主界面输出日志
    void sendLog(const QString& msg);
};

#endif // DLP_USB_MANAGER_H