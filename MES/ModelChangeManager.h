#ifndef MODELCHANGEMANAGER_H
#define MODELCHANGEMANAGER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QDateTime>
#include <QJsonArray>
#include <QList>
#include "ControlCenterHttpApi.h"
#include "MesHttpPost.h"

// 换型最终结果枚举
enum ModelChangeResult
{
    Result_Success,
    Result_Fail_NoTask,         // 获取任务失败
    Result_Fail_CannotChange,   // 设备不可换型
    Result_Fail_MissingFile,    // 配方切换NG缺少文件
    Result_FixtureCheck_Fail,   // 治具校验失败
    Result_SelfCheck_Fail       // 自检NG
};

// 流程状态枚举
enum ModelStatus
{
    Status_None,        // 空闲无流程
    Status_Prepare,     // 收到ModelChangePrepare，等待Complete指令
    Status_Complete     // 收到ModelChangeComplete，执行业务流程
};

// 常量定义
const int POLL_INTERVAL = 10000;        // 10s轮询指令

class ModelChangeManager : public QObject
{
    Q_OBJECT
public:
    explicit ModelChangeManager(QObject *parent = nullptr);
    ~ModelChangeManager();

    void StartModelChange(const QString& deviceCode, const QString& deviceIp);
    void StopModelChange();
    QString GetLastErrorMsg() const;

signals:
    void SignalModelChangeFinished(ModelChangeResult result, const QString& msg);
    void SignalAlarmTrigger(const QString& alarmInfo);

private slots:
    void SlotPollInstruction();

private:
    void ProcessInstruction();
    void NotifyLocalModelComplete();
    void ReportModelResult(ModelChangeResult res, const QString& extraMsg);

    bool HandleProdSwitch(const QString& changeRes, QString& outErr);
    bool HandleFixtureCheck(QList<BindFixtureItem>& fixtureList, QString& outErr);
    bool HandleSelfCheck(const QString& selfRes, QString& outErr);

    bool PullDeviceInstruction(DeviceExecCommand& outCmd, QString& errMsg);
    bool PullCurrentTask(DeviceTaskInfo& outTask, QString& errMsg);
    bool CheckFixtureNeed(QList<BindFixtureItem>& list, QString& errMsg);

    //上报接口
    void UploadCmdResult(const QString& cmd, bool isSuccess, const QString& msg = "");
    void UploadAlarm(const QString& alarmText);
    void UploadDeviceStatus(const QString& status);

private:
    ControlCenterHttpApi* m_ctrlApi;
    MesHttpPost* m_mesApi;
    QTimer m_pollTimer;
    QString m_deviceCode;
    QString m_deviceIp;
    QString m_lastError;
    bool m_waitCompleteFlag;
    ModelStatus m_modelStatus;
};

#endif // MODELCHANGEMANAGER_H
