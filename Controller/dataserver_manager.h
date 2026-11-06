#ifndef DATASERVER_MANAGER_H
#define DATASERVER_MANAGER_H

#include <QObject>
#include <QString>
#include <QLibrary>

// 保持与 DLL 提供的数据结构一致
struct ds_result_t {
    int ok;
    int ret_val;
    int err_code;
    const char* msg;
};

class DataServerManager : public QObject
{
    Q_OBJECT
public:
    explicit DataServerManager(QObject* parent = nullptr);
    ~DataServerManager();

    // 加载 DLL
    bool initLibrary(const QString& dllPath = "libdatasvr_comm.dll");
    
    // 设置/获取目标 IP
    void setIpAddress(const QString& ip);
    QString getIpAddress() const;

    // 发送指令
    bool sendCommand(const char cmd[2], uint32_t val, int timeoutMs = 3000, QString* outErrorMsg = nullptr);

signals:
    void sendLog(const QString& msg);

private:
    QLibrary m_lib;
    QString m_ipAddress;

    // 定义函数指针类型
    typedef ds_result_t (*fn_ds_send_cmd)(const char*, const char[2], uint32_t, int);
    typedef void (*fn_ds_free_msg)(const char*);

    fn_ds_send_cmd m_ds_send_cmd;
    fn_ds_free_msg m_ds_free_msg;
};

#endif // DATASERVER_MANAGER_H