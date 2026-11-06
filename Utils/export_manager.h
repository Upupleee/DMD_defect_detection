#ifndef EXPORT_MANAGER_H
#define EXPORT_MANAGER_H

#include <QObject>
#include <QImage>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include "detect.h"

// 定义一次全流程检测的所有相关数据
struct FullInspectionData {
    QDateTime timeStamp;
    QImage defectImage;          // 镜片区域图（坏点检测图）
    QImage paintedDefectImage;
    QImage serialImage;          // 序列号区域图
    QString serialNumber;
    int defectCount = 0;
    std::vector<Detect::DetectResult> defectResults; // 仅使用 row/column
    QStringList validRowStrings; // 新增：无坏点区域的描述集合
};

class ExportManager : public QObject
{
    Q_OBJECT
public:
    explicit ExportManager(QObject *parent = nullptr);
    ~ExportManager() = default;

    void resetCurrentRecord();
    void recordDefectData(const QImage& img, const std::vector<Detect::DetectResult>& defects, const QStringList& validRowStrings = QStringList());
    void recordSerialData(const QImage& img, const QString& serialStr);
    
    // 加入 tag 标记以支持保存独立的 4 种检测表格和小图片
    bool exportCurrentRecord(QString& outMessage, const QString& tag = "");

    // 新增：用于在 4 步结束后输出汇总的第 5 个表格
    bool exportSummaryReport(QString& outMessage, int dmdGridHeight);

    QString getCsvFilePath(const QString& tag = "") const;
    void setPaintedDefectImage(const QImage& img);

private:
    FullInspectionData m_currentRecord;
    QString m_exportDir; // 导出的默认保存目录

    // 缓存四步流程产生的数据
    std::map<QString, FullInspectionData> m_sequenceRecords;
};

#endif // EXPORT_MANAGER_H