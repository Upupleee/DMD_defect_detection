#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QPair>
#include <QTcpSocket>
#include <QNetworkProxy>
#include "OPTController.h"
#include "OPTErrorCode.h"

class LightManager : public QObject
{
    Q_OBJECT

public:
    explicit LightManager(QObject* parent = nullptr);
    ~LightManager();

    // SDK 基础连接控制
    bool connectIP(const QString& ip);
    bool connectSerial(const QString& comPort);
    void disconnectController();
    bool isConnected() const;

    // 单通道控制
    bool turnOnChannel(int channelIndex); // 0 为全部
    bool turnOffChannel(int channelIndex);
    bool setIntensity(int channelIndex, int intensity); // 0-255
    bool getIntensity(int channelIndex, int& outIntensity);

    // 多通道并发控制
    bool turnOnMultiChannel(const QList<int>& channels);
    bool turnOffMultiChannel(const QList<int>& channels);
    bool setMultiIntensity(const QList<QPair<int, int>>& channelIntensities);
        
    // Raw HEX 原生透传模式 (绕过SDK，用于发送特殊16进制指令)
    bool connectRawTCP(const QString& ip, quint16 port = 8000);
    void disconnectRawTCP();
    void sendRawHex(const QString& hexStr);

signals:
    void sendLog(const QString& msg);
    void connectionStateChanged(bool isConnected);
    void rawDataReceived(const QByteArray& data);

private slots:
    void onRawReadyRead();

private:
    OPTController_Handle m_handle;
    bool m_bConnected;
    bool m_bIsEthernet;
    QTcpSocket* m_rawSocket;
};