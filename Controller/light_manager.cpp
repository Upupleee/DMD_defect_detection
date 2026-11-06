#include "light_manager.h"

LightManager::LightManager(QObject* parent)
    : QObject(parent), m_handle(0), m_bConnected(false), m_rawSocket(new QTcpSocket(this))
{
    connect(m_rawSocket, &QTcpSocket::readyRead, this, &LightManager::onRawReadyRead);
}

LightManager::~LightManager()
{
    disconnectRawTCP();
    disconnectController();
}

bool LightManager::connectIP(const QString& ip)
{
    if (m_bConnected) disconnectController();

    long lRet = OPTController_CreateEthernetConnectionByIP(ip.toLocal8Bit().data(), &m_handle);
    if (lRet == OPT_SUCCEED) {
        m_bConnected = true;
        m_bIsEthernet = true; // <---- 记录为网口连接
        emit sendLog(tr("LightController: Ethernet connected to %1").arg(ip));
        emit connectionStateChanged(true);
        return true;
    }
    emit sendLog(tr("LightController: Failed to connect IP %1, error: %2").arg(ip).arg(lRet));
    return false;
}

bool LightManager::connectSerial(const QString& comPort)
{
    if (m_bConnected) disconnectController();

    long lRet = OPTController_InitSerialPort(comPort.toLocal8Bit().data(), &m_handle);
    if (lRet == OPT_SUCCEED) {
        m_bConnected = true;
        m_bIsEthernet = false; // <---- 记录为串口连接
        emit sendLog(tr("LightController: Serial connected to %1").arg(comPort));
        emit connectionStateChanged(true);
        return true;
    }
    emit sendLog(tr("LightController: Failed to connect Serial %1, error: %2").arg(comPort).arg(lRet));
    return false;
}

void LightManager::disconnectController()
{
    if (m_handle != 0 && m_bConnected) {
        if (m_bIsEthernet) {
            OPTController_DestroyEthernetConnection(m_handle);
        }
        else {
            OPTController_ReleaseSerialPort(m_handle);
        }
        m_handle = 0;
        m_bConnected = false;
        emit sendLog(tr("LightController: Disconnected."));
        emit connectionStateChanged(false);
    }
}

bool LightManager::isConnected() const { return m_bConnected; }

bool LightManager::turnOnChannel(int channelIndex)
{
    if (!m_bConnected) return false;
    long lRet = OPTController_TurnOnChannel(m_handle, channelIndex);
    if (lRet == OPT_SUCCEED) {
        emit sendLog(tr("LightController: Ch %1 Turn On").arg(channelIndex));
        return true;
    }
    return false;
}

bool LightManager::turnOffChannel(int channelIndex)
{
    if (!m_bConnected) return false;
    long lRet = OPTController_TurnOffChannel(m_handle, channelIndex);
    if (lRet == OPT_SUCCEED) {
        emit sendLog(tr("LightController: Ch %1 Turn Off").arg(channelIndex));
        return true;
    }
    return false;
}

bool LightManager::setIntensity(int channelIndex, int intensity)
{
    if (!m_bConnected) return false;
    long lRet = OPTController_SetIntensity(m_handle, channelIndex, intensity);
    if (lRet == OPT_SUCCEED) {
        emit sendLog(tr("LightController: Ch %1 Intensity set to %2").arg(channelIndex).arg(intensity));
        return true;
    }
    emit sendLog(tr("LightController: Failed to set Ch %1 Intensity %2, error: %3").arg(channelIndex).arg(intensity).arg(lRet));
    return false;
}

bool LightManager::getIntensity(int channelIndex, int& outIntensity)
{
    if (!m_bConnected) return false;
    long lRet = OPTController_ReadIntensity(m_handle, channelIndex, &outIntensity);
    return lRet == OPT_SUCCEED;
}

// ------ 新增：多通道并发功能 ------
bool LightManager::turnOnMultiChannel(const QList<int>& channels)
{
    if (!m_bConnected || channels.isEmpty()) return false;
    int* arr = new int[channels.size()];
    for (int i = 0; i < channels.size(); ++i) arr[i] = channels[i];
    
    long lRet = OPTController_TurnOnMultiChannel(m_handle, arr, channels.size());
    delete[] arr;
    
    if (lRet == OPT_SUCCEED) {
        emit sendLog(tr("LightController: Multiple Channels Turned On simultaneously"));
        return true;
    }
    return false;
}

bool LightManager::turnOffMultiChannel(const QList<int>& channels)
{
    if (!m_bConnected || channels.isEmpty()) return false;
    int* arr = new int[channels.size()];
    for (int i = 0; i < channels.size(); ++i) arr[i] = channels[i];
    
    long lRet = OPTController_TurnOffMultiChannel(m_handle, arr, channels.size());
    delete[] arr;
    return lRet == OPT_SUCCEED;
}

bool LightManager::setMultiIntensity(const QList<QPair<int, int>>& channelIntensities)
{
    if (!m_bConnected || channelIntensities.isEmpty()) return false;
    
    IntensityItem* items = new IntensityItem[channelIntensities.size()];
    for (int i = 0; i < channelIntensities.size(); ++i) {
        items[i].channelIndex = channelIntensities[i].first;
        items[i].intensity = channelIntensities[i].second;
    }
    
    long lRet = OPTController_SetMultiIntensity(m_handle, items, channelIntensities.size());
    delete[] items;
    
    if(lRet == OPT_SUCCEED){
        emit sendLog(tr("LightController: Multiple Channels Intensity applied simultaneously"));
        return true;
    }
    return false;
}

// ------ 新增：Raw 原生透传连接功能 ------
bool LightManager::connectRawTCP(const QString& ip, quint16 port)
{
    disconnectRawTCP();
    disconnectController();
    // 禁用该 Socket 的代理行为，防止被系统 VPN 或代理软件拦截
    m_rawSocket->setProxy(QNetworkProxy::NoProxy);
    m_rawSocket->connectToHost(ip, port);
    if (m_rawSocket->waitForConnected(2000)) {
        emit sendLog(tr("LightController: Raw TCP Socket connected! (%1:%2)").arg(ip).arg(port));
        return true;
    }
    // 如果失败，利用 errorString() 抓取具体的底层网络拒绝原因
    QString errorMsg = m_rawSocket->errorString();
    emit sendLog(tr("LightController: Raw TCP connect failed! Reason: %1").arg(errorMsg));
    return false;
}

void LightManager::disconnectRawTCP()
{
    if (m_rawSocket->state() == QAbstractSocket::ConnectedState) {
        m_rawSocket->disconnectFromHost();
        emit sendLog(tr("LightController: Raw TCP Socket disconnected."));
    }
}

void LightManager::sendRawHex(const QString& hexStr)
{
    if (m_rawSocket->state() != QAbstractSocket::ConnectedState) {
        emit sendLog(tr("Cannot send Hex. Raw connection is not established."));
        return;
    }
    // 去掉空格直接按 16 进制转译发送
    QByteArray data = QByteArray::fromHex(hexStr.toUtf8());
    m_rawSocket->write(data);
    m_rawSocket->flush();
    emit sendLog(tr("RawTx (Hex): %1").arg(QString(data.toHex(' ').toUpper())));
}

void LightManager::onRawReadyRead()
{
    QByteArray data = m_rawSocket->readAll();
    emit rawDataReceived(data);
}