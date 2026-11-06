#include "export_manager.h"

namespace
{
    QString MakeSafeSerial(const QString& serialNumber)
    {
        QString safeSN = serialNumber.isEmpty() ? QObject::tr("Unknown_SN") : serialNumber;
        safeSN.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
        safeSN.replace(QRegularExpression("[\r\n]+"), "_");
        return safeSN;
    }

    QString MakeImageFileName(const QString& timeStr, const QString& safeSN, const QString& typeTag)
    {
        return QString("%1_%2_%3.png").arg(timeStr, safeSN, typeTag);
    }

    enum class FaceStatus
    {
        FullPass,      // 全场可用
        OneThirdPass,  // 1/3可用（336-672）
        Gap336Pass,    // 存在336连续可用行（黑白场交集）
        Fail
    };

    std::vector<double> CollectRows(const std::vector<Detect::DetectResult>& defects)
    {
        std::vector<double> rows;
        rows.reserve(defects.size());
        for (const auto& d : defects) {
            rows.push_back(d.row);
        }
        std::sort(rows.begin(), rows.end());
        return rows;
    }

    bool HasNoDefectInRange(const std::vector<double>& rows, double startRow, double endRow)
    {
        for (double r : rows) {
            if (r >= startRow && r <= endRow) {
                return false;
            }
        }
        return true;
    }

    bool HasContinuousClearRowsInIntersection(const std::vector<double>& wfRows,
        const std::vector<double>& dfRows,
        int dmdHeight,
        double requiredLen)
    {
        // 黑白场可用行交集 == (黑场坏点 ∪ 白场坏点) 的补集
        std::vector<double> blockedRows;
        blockedRows.reserve(wfRows.size() + dfRows.size());
        blockedRows.insert(blockedRows.end(), wfRows.begin(), wfRows.end());
        blockedRows.insert(blockedRows.end(), dfRows.begin(), dfRows.end());

        std::sort(blockedRows.begin(), blockedRows.end());
        blockedRows.push_back(static_cast<double>(dmdHeight)); // 尾边界

        double lastRow = 0.0;
        for (double r : blockedRows) {
            if ((r - lastRow) >= requiredLen) {
                return true;
            }
            lastRow = r;
        }
        return false;
    }

    FaceStatus EvaluateSingleFaceStatus(const std::vector<Detect::DetectResult>& wf,
        const std::vector<Detect::DetectResult>& df,
        int dmdHeight)
    {
        const std::vector<double> wfRows = CollectRows(wf);
        const std::vector<double> dfRows = CollectRows(df);

        // 1) 同一面黑白场都无坏点 -> 全场可用，合格
        if (wfRows.empty() && dfRows.empty()) {
            return FaceStatus::FullPass;
        }

        // 2) 同一面黑白场在336-672都无坏点 -> 1/3可用，合格
        const bool wfMiddleClear = HasNoDefectInRange(wfRows, 336.0, 672.0);
        const bool dfMiddleClear = HasNoDefectInRange(dfRows, 336.0, 672.0);
        if (wfMiddleClear && dfMiddleClear) {
            return FaceStatus::OneThirdPass;
        }

        // 3) 同一面黑白场存在336连续可用行，且该连续区间取交集 -> 合格
        if (HasContinuousClearRowsInIntersection(wfRows, dfRows, dmdHeight, 336.0)) {
            return FaceStatus::Gap336Pass;
        }

        return FaceStatus::Fail;
    }

    QString FaceStatusText(const QString& faceName, FaceStatus status)
    {
        switch (status) {
        case FaceStatus::FullPass:
            return QObject::tr("%1全场可用，合格").arg(faceName);
        case FaceStatus::OneThirdPass:
            return QObject::tr("%11/3可用，合格").arg(faceName);
        case FaceStatus::Gap336Pass:
            return QObject::tr("%1有336连续可用行，合格").arg(faceName);
        case FaceStatus::Fail:
        default:
            return QObject::tr("%1不可用，不合格").arg(faceName);
        }
    }

    // 按“正面/背面”分别判定：每个面都要求黑场+白场共同满足规则
    QString EvaluateGlobalDmdStatus(const std::vector<Detect::DetectResult>& l1_wf,
        const std::vector<Detect::DetectResult>& l1_df,
        const std::vector<Detect::DetectResult>& l2_wf,
        const std::vector<Detect::DetectResult>& l2_df,
        int dmdHeight)
    {
        const FaceStatus frontStatus = EvaluateSingleFaceStatus(l1_wf, l1_df, dmdHeight);
        const FaceStatus backStatus = EvaluateSingleFaceStatus(l2_wf, l2_df, dmdHeight);

        const QString frontText = FaceStatusText(QObject::tr("正面"), frontStatus);
        const QString backText = FaceStatusText(QObject::tr("背面"), backStatus);

        // 两面都不满足 -> 不合格
        if (frontStatus == FaceStatus::Fail && backStatus == FaceStatus::Fail) {
            return QObject::tr("正背面均不可用，不合格");
        }

        // 返回正背面各自结论，便于追溯
        return frontText + QObject::tr("；") + backText;
    }

    QString CsvEscape(const QString& text)
    {
        QString escaped = text;
        escaped.replace("\"", "\"\"");
        return "\"" + escaped + "\"";
    }

    QStringList BuildValidRowStringsFromDefects(const std::vector<Detect::DetectResult>& defects, int dmdHeight)
    {
        QStringList ranges;
        if (dmdHeight <= 0) {
            return ranges;
        }

        std::vector<bool> blockedRows(static_cast<size_t>(dmdHeight + 1), false);

        for (const auto& d : defects) {
            const int row = static_cast<int>(std::llround(d.row));
            if (row >= 1 && row <= dmdHeight) {
                blockedRows[static_cast<size_t>(row)] = true;
            }
        }

        int start = -1;
        for (int row = 1; row <= dmdHeight; ++row) {
            if (!blockedRows[static_cast<size_t>(row)]) {
                if (start < 0) {
                    start = row;
                }
            }
            else if (start >= 0) {
                const int end = row - 1;
                ranges << (start == end
                    ? QString::number(start)
                    : QString("%1-%2").arg(start).arg(end));
                start = -1;
            }
        }

        if (start >= 0) {
            const int end = dmdHeight;
            ranges << (start == end
                ? QString::number(start)
                : QString("%1-%2").arg(start).arg(end));
        }

        return ranges;
    }
}

ExportManager::ExportManager(QObject* parent)
    : QObject(parent), m_exportDir("Results")
{
    resetCurrentRecord();
}

void ExportManager::resetCurrentRecord()
{
    m_currentRecord = FullInspectionData();
    m_currentRecord.timeStamp = QDateTime::currentDateTime();
}

void ExportManager::recordDefectData(const QImage& img, const std::vector<Detect::DetectResult>& defects, const QStringList& validRowStrings)
{
    m_currentRecord.timeStamp = QDateTime::currentDateTime();
    m_currentRecord.defectImage = img;
    m_currentRecord.defectCount = (int)defects.size();
    m_currentRecord.defectResults = defects;
    m_currentRecord.validRowStrings = validRowStrings; // 保存记录
}

void ExportManager::recordSerialData(const QImage& img, const QString& serialStr)
{
    m_currentRecord.serialImage = img;
    m_currentRecord.serialNumber = serialStr.isEmpty() ? tr("Unknown_SN") : serialStr;
}

void ExportManager::setPaintedDefectImage(const QImage& img)
{
    m_currentRecord.paintedDefectImage = img;
}

bool ExportManager::exportCurrentRecord(QString& outMessage, const QString& tag)
{
    // 如果存在 tag ，我们将本次记录缓存起来以供总结生成使用
    if (!tag.isEmpty()) {
        m_sequenceRecords[tag] = m_currentRecord;
    }
    else {
        // 单图或者不受保护的流走清空操作
        m_sequenceRecords.clear();
    }

    // 1. 创建基于日期的子文件夹
    const QString dateStr = m_currentRecord.timeStamp.toString("yyyyMMdd");
    const QString safeSN = MakeSafeSerial(m_currentRecord.serialNumber);

    QDir baseDir(m_exportDir);
    // 路径拼接：Results / 年月日 / 序列号
    QString targetDirPath = QString("%1/%2").arg(dateStr, safeSN);
    if (!baseDir.exists(targetDirPath)) {
        baseDir.mkpath(targetDirPath);
    }

    // 切换到具体序列号目录
    QDir snDir(baseDir.absoluteFilePath(targetDirPath));

    const QString timeStr = m_currentRecord.timeStamp.toString("yyyyMMdd_HHmmss");

    // 若有 tag 则拼接，如 _l1_wf
    QString suffixTag = tag.isEmpty() ? "" : "_" + tag;
    const QString defectFileName = MakeImageFileName(timeStr, safeSN, "DefectView" + suffixTag);
    const QString paintedFileName = MakeImageFileName(timeStr, safeSN, "DefectView_Painted" + suffixTag);
    const QString serialFileName = MakeImageFileName(timeStr, safeSN, "SerialNumber"); // 序列号截图不加后缀

    const QString defectFilePath = snDir.filePath(defectFileName);
    const QString paintedFilePath = snDir.filePath(paintedFileName);
    const QString serialFilePath = snDir.filePath(serialFileName);

    // 分类型保存图像
    if (!m_currentRecord.defectImage.isNull()) {
        m_currentRecord.defectImage.save(defectFilePath);
    }
    if (!m_currentRecord.paintedDefectImage.isNull()) {
        m_currentRecord.paintedDefectImage.save(paintedFilePath); // 保存已绘制十字的图片
    }
    if (!m_currentRecord.serialImage.isNull()) {
        m_currentRecord.serialImage.save(serialFilePath);
    }

    // 优化：不再输出四个子表格来记录单次所有单个坏点详情，只保存图片操作
    outMessage = tr("Images successfully saved.");
    return true;
}

bool ExportManager::exportSummaryReport(QString& outMessage, int dmdGridHeight)
{
    if (m_sequenceRecords.empty()) {
        outMessage = tr("No sequence data to summarize.");
        return false;
    }

    // 取最后一条用于获取基本信息如SN等
    FullInspectionData baseRecord = m_currentRecord;
    const QString dateStr = baseRecord.timeStamp.toString("yyyyMMdd");
    const QString safeSN = MakeSafeSerial(baseRecord.serialNumber);

    QDir baseDir(m_exportDir);
    QString targetDirPath = QString("%1/%2").arg(dateStr, safeSN);
    QDir snDir(baseDir.absoluteFilePath(targetDirPath));

    // 生成总结报告文件
    const QString summaryFileName = QString("DMD_Summary_%1.csv").arg(safeSN);
    const QString summaryFilePath = snDir.filePath(summaryFileName);

    QFile file(summaryFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        outMessage = tr("Failed to open or save Summary CSV file.");
        return false;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::System);
#else
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
#endif

    std::vector<Detect::DetectResult> l1_wf_defects = m_sequenceRecords.count("l1_wf") ? m_sequenceRecords["l1_wf"].defectResults : std::vector<Detect::DetectResult>();
    std::vector<Detect::DetectResult> l1_df_defects = m_sequenceRecords.count("l1_df") ? m_sequenceRecords["l1_df"].defectResults : std::vector<Detect::DetectResult>();
    std::vector<Detect::DetectResult> l2_wf_defects = m_sequenceRecords.count("l2_wf") ? m_sequenceRecords["l2_wf"].defectResults : std::vector<Detect::DetectResult>();
    std::vector<Detect::DetectResult> l2_df_defects = m_sequenceRecords.count("l2_df") ? m_sequenceRecords["l2_df"].defectResults : std::vector<Detect::DetectResult>();

    QString globalConclusion = EvaluateGlobalDmdStatus(l1_wf_defects, l1_df_defects, l2_wf_defects, l2_df_defects, dmdGridHeight);

    stream << tr("Inspection Summary Report") << "\n";
    stream << tr("Date Time") << "," << baseRecord.timeStamp.toString("yyyy-MM-dd HH:mm:ss") << "\n";
    stream << tr("Serial Number") << "," << baseRecord.serialNumber << "\n";
    stream << tr("DMD Overall Status") << "," << globalConclusion << "\n\n";

    stream << tr("Condition") << "," << tr("Valid Ranges") << "\n";

    auto printCondition = [&](const QString& tag, const QString& friendlyName) {
        if (!m_sequenceRecords.count(tag)) {
            stream << friendlyName << "," << tr("NA") << "\n";
            return;
        }

        const FullInspectionData& record = m_sequenceRecords[tag];
        QStringList validRanges = record.validRowStrings;

        // 关键修复：算法未返回 validRowStrings 时，基于坏点坐标回算可用行范围
        if (validRanges.isEmpty()) {
            validRanges = BuildValidRowStringsFromDefects(record.defectResults, dmdGridHeight);
        }

        if (validRanges.isEmpty()) {
            stream << friendlyName << "," << tr("No valid row range") << "\n";
            return;
        }

        const QString validRangesCombined = validRanges.join("\n");
        stream << friendlyName << "," << CsvEscape(validRangesCombined) << "\n";
        };

    // 记录单面检测情况对应的无坏点行记录范围
    printCondition("l1_wf", tr("front white field"));
    printCondition("l1_df", tr("front black field"));
    printCondition("l2_df", tr("back black field"));
    printCondition("l2_wf", tr("back white field"));

    file.close();
    m_sequenceRecords.clear(); // 导出完成后清空当前的一组

    outMessage = tr("Summary successfully saved to %1").arg(summaryFilePath);
    return true;
}

QString ExportManager::getCsvFilePath(const QString& tag) const
{
    const QString dateStr = m_currentRecord.timeStamp.toString("yyyyMMdd");
    const QString safeSN = MakeSafeSerial(m_currentRecord.serialNumber);
    QString suffixTag = tag.isEmpty() ? "" : "_" + tag;

    QDir snDir(QDir(m_exportDir).absoluteFilePath(QString("%1/%2").arg(dateStr, safeSN)));
    return snDir.absoluteFilePath(QString("DMD_Report_%1%2.csv").arg(safeSN, suffixTag));
}