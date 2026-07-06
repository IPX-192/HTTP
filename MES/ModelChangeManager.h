#ifndef MODELCHANGEMANAGER_H
#define MODELCHANGEMANAGER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QDateTime>
#include <QJsonArray>
#include "ControlCenterHttpApi.h"
#include "MesHttpPost.h"

// 换型步骤状态机（严格匹配流程图）
enum ModelChangeStep
{
    Step_Idle,                  // 空闲
    Step_GetInstruction,        // 10s轮询拉取指令
    Step_CheckDeviceStatus,     // 总线校验设备是否允许换型
    Step_WaitModelComplete,     // 等待下发ModelChangeComplete指令（无超时）
    Step_GetTaskInfo,           // 获取生产任务
    Step_ExecuteProductSwitch,  // 下发总线执行配方切换
    Step_CheckFixtureUse,       // MES治具校验
    Step_DeviceSelfCheck,       // 设备自检
    Step_FinishModelChange      // 流程结束
};

// 换型结果枚举
enum ModelChangeResult
{
    Result_Success,
    Result_Fail_NoTask,         // 获取任务失败
    Result_Fail_CannotChange,   // 设备不可换型
    Result_Fail_MissingFile,    // 配方切换NG缺少文件
    Result_FixtureCheck_Fail,   // 治具校验失败
    Result_SelfCheck_Fail       // 自检NG
};

// 常量定义
const int POLL_INTERVAL = 10000;        // 10s轮询指令
const int BUS_WAIT_TIMEOUT = 5000;      // 总线应答超时5s

class ModelChangeManager : public QObject
{
    Q_OBJECT
public:
    explicit ModelChangeManager(QObject *parent = nullptr);
    ~ModelChangeManager();

    // 启动换型流程
    void StartModelChange(const QString& deviceCode, const QString& deviceIp);
    // 强制终止流程
    void StopModelChange();

    // 对外查询
    ModelChangeStep GetCurrentStep() const;
    QString GetLastErrorMsg() const;

signals:
    // 流程最终结束回调
    void SignalModelChangeFinished(ModelChangeResult result, const QString& msg);
    // 推送告警
    void SignalAlarmTrigger(const QString& alarmInfo);
    // 请求硬件自检，外部回填结果
    void SignalRequestDeviceSelfCheck(QString& outResult, QJsonArray& outDetailItems);

private slots:
    void SlotPollInstruction();                                   // 10s轮询指令

private:
    // 主流程处理
    void ProcessInstruction(const DeviceExecCommand& cmd);
    void NotifyLocalModelComplete();                    // 收到ModelChangeComplete进入任务流程
    void ReportModelResult(ModelChangeResult res, const QString& extraMsg = ""); // 统一收尾上报

    void ProdSwitchBusMsg(QString changeRes,  QList<BindFixtureItem>& fixtureList, QString selfRes); // 总线接收配方切换结果


    // MES/控制中心接口封装
    bool PullDeviceInstruction(DeviceExecCommand& outCmd, QString& errMsg);
    bool PullCurrentTask(DeviceTaskInfo& outTask, QString& errMsg);
    bool CheckFixtureNeed( QList<BindFixtureItem>&list, QString& errMsg);
    bool RunDeviceSelfCheck(QString& errMsg);

    // 上报工具函数
    void UploadCmdResult(const QString& cmd, bool isSuccess, const QString& msg = "");
    void UploadAlarm(const QString& alarmText);
    void UploadDeviceStatus(const QString& status);

private:
    ControlCenterHttpApi* m_ctrlApi;
    MesHttpPost* m_mesApi;

    QTimer m_pollTimer;                 // 10s指令轮询定时器

    QString m_deviceCode;
    QString m_deviceIp;
    ModelChangeStep m_curStep;
    QString m_lastError;

};

#endif // MODELCHANGEMANAGER_H
