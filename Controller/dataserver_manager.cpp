#include "dataserver_manager.h"

DataServerManager::DataServerManager(QObject* parent)
    : QObject(parent),
      m_ds_send_cmd(nullptr),
      m_ds_free_msg(nullptr),
      m_ipAddress("0.0.0.0") // Ĭ�� IP
{
}

DataServerManager::~DataServerManager()
{
    if (m_lib.isLoaded()) {
        m_lib.unload();
    }
}

bool DataServerManager::initLibrary(const QString& dllPath)
{
    m_lib.setFileName(dllPath);
    if (!m_lib.load()) {
        emit sendLog(tr("DataServerManager: Failed to load library %1.").arg(dllPath));
        return false;
    }

    m_ds_send_cmd = reinterpret_cast<fn_ds_send_cmd>(m_lib.resolve("ds_send_cmd"));
    m_ds_free_msg = reinterpret_cast<fn_ds_free_msg>(m_lib.resolve("ds_free_msg"));

    if (!m_ds_send_cmd || !m_ds_free_msg) {
        emit sendLog(tr("DataServerManager: Failed to resolve functions from library."));
        m_lib.unload();
        return false;
    }

    emit sendLog(tr("DataServerManager: Library loaded successfully."));
    return true;
}

void DataServerManager::setIpAddress(const QString& ip)
{
    m_ipAddress = ip;
}

QString DataServerManager::getIpAddress() const
{
    return m_ipAddress;
}

bool DataServerManager::sendCommand(const char cmd[2], uint32_t val, int timeoutMs, QString* outErrorMsg)
{
    if (!m_ds_send_cmd || !m_ds_free_msg) {
        if (outErrorMsg) *outErrorMsg = tr("Library not loaded or functions missing.");
        emit sendLog(tr("DataServerManager: Cannot send command, DLL not fully loaded."));
        return false;
    }

    QByteArray ipBytes = m_ipAddress.toUtf8();
    
    emit sendLog(tr("DataServerManager: Sending command %1%2 to %3 with value %4...")
                 .arg(cmd[0]).arg(cmd[1]).arg(m_ipAddress).arg(val));

    ds_result_t result = m_ds_send_cmd(ipBytes.constData(), cmd, val, timeoutMs);

    QString resultInfo = QString(tr("DataServerManager: Result: ok=%1, ret=%2, err=%3, msg=%4"))
                             .arg(result.ok)
                             .arg(result.ret_val)
                             .arg(result.err_code)
                             .arg(result.msg ? result.msg : "");
    emit sendLog(resultInfo);

    bool isSuccess = (result.ok != 0);
    if (!isSuccess && outErrorMsg) {
        *outErrorMsg = result.msg ? QString::fromUtf8(result.msg) : tr("Unknown error");
    }

    if (result.msg) {
        m_ds_free_msg(result.msg);
    }

    return isSuccess;
}