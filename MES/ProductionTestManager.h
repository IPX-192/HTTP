#ifndef PRODUCTIONTESTMANAGER_H
#define PRODUCTIONTESTMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonArray>
#include "MesHttpPost.h"

// 工位枚举
enum StationId
{
    STATION_1 = 1,
    STATION_2 = 2
};

class ProductionTestManager : public QObject
{
    Q_OBJECT
public:
    explicit ProductionTestManager(QObject *parent = nullptr);

    // 本地模拟开关：true=不走真实网络，返回假数据，无超时弹窗
    bool m_localTestMode = true;


    QString m_workOrderId;
    QString m_processId;
    QString m_processKey;


    QString m_station1_Key;
    QString m_station2_Key;
    QString m_station1_WorkOrderId;
    QString m_station2_WorkOrderId;
    QString m_station1_ProcessId;
    QString m_station2_ProcessId;

    // ===================== 独立单接口（新增stationId入参，区分工位） =====================
    QString ConsumeFixtureLife(StationId station, const QString& fixtureSn, int consumeNum, FixtureConsumeResult& outResult, QString& errMsg);
    QString ValidateStandardElementNumber(StationId station, const QString& barcode, QString& errMsg);
    QString ValidateNumber(StationId station, const QString& barcode, QString& errMsg);
    QString SaveProcessOpResult(StationId station, const QString& barcode, int opResult, const QJsonArray& detailArr, QString& outTaskMainId, QString& errMsg);
    QString CompleteTask(StationId station, const QString& barcode, bool isSuccess, const QString& errCode, const QString& errInfo, bool bindMat, bool& outTaskResult, QString& errMsg);
    QString UploadSingle(StationId station, const QString& localFilePath, const QString& filePid, const QString& nsId, UploadFileResp& outUploadResp, QString& errMsg);
    QString SaveProductFilePath(StationId station, const QString& barcode, const QString& fileId, const QString& type, const QString& nsId, const QString& fileName, QString& errMsg);

    // 完整自动生产总流程（原有全局流程，保留）
    bool RunFullAutoProduction(const QString& scanBarcode, bool enableFixtureControl, QString& errMsg);
    // 标准件直通流出简化流程（原有全局流程，保留）
    bool RunStandardPartDirectOut(const QString& stdBarcode, QString& errMsg);
    // 正常生产加工完整流程（原有全局流程，保留）
    bool RunNormalProduceProcess(const QString& prodBarcode, QString& errMsg);

    bool RunStationSingleFlow(StationId station, const QString& barcode, QString& errMsg);

signals:
    void SignalApiLog(const QString& apiName, const QString& reqJson, const QString& respJson, bool isSuccess);
    void SignalErrorMsg(const QString& errText);

private:
    // 根据工位获取当前工位独立processKey
    QString GetStationProcessKey(StationId station);
    // 统一日志打印，追加工位标识
    void EmitLog(StationId station, const QString& apiName, const QString& reqJson, const QString& respJson, bool success);
    // 本地模拟假数据
    void MockFixtureConsumeData(FixtureConsumeResult& out);
    bool MockFileUpload(const QString& fileName, UploadFileResp& outUploadResp);
};

#endif // PRODUCTIONTESTMANAGER_H
