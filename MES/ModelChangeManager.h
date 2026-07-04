#ifndef MODELCHANGEMANAGER_H
#define MODELCHANGEMANAGER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QDateTime>
#include <QJsonArray>
#include "ControlCenterHttpApi.h"
#include "MesHttpPost.h"

// 换型执行步骤状态机
enum ModelChangeStep
{
    Step_Idle,                                // 空闲，未启动换型
    Step_GetInstruction,             // 10s轮询获取下发指令
    Step_CheckDeviceStatus,     // 校验设备是否允许换型(等待外部返回状态)
    Step_WaitModelComplete,     // 等待本地硬件换型完成（异步）
    Step_GetTaskInfo,                    // 获取当前生产任务
    Step_ExecuteProductSwitch,  // 执行产品配方/型号切换（对应流程图调用程序）
    Step_CheckFixtureUse,          // 治具校验
    Step_DeviceSelfCheck,           // 设备自检
    Step_FinishModelChange      // 换型全流程结束
};

// 换型最终结果枚举
enum ModelChangeResult
{
    Result_Success,
    Result_Fail_NoTask,
    Result_Fail_CannotChange,
    Result_Fail_MissingFile,    // 配方/程序缺失
    Result_FixtureCheck_Fail,
    Result_SelfCheck_Fail,
    Result_Fail_WaitTimeout     // 等待本地换型超时失败
};

const int POLL_INTERVAL = 10000;
const int WAIT_LOCAL_TIMEOUT = 300000; // 等待本地换型5分钟超时

class ModelChangeManager : public QObject
{
    Q_OBJECT
public:
    explicit ModelChangeManager(QObject *parent = nullptr);
    ~ModelChangeManager();

    // 启动一键换型
    void StartModelChange(const QString& deviceCode, const QString& deviceIp);
    // 外部硬件/界面通知：本地机械换型动作完成
    void NotifyLocalModelComplete();
    // 强制终止整个换型流程
    void StopModelChange();

    // 获取当前流程步骤
    ModelChangeStep GetCurrentStep() const;
    // 获取最新错误文本
    QString GetLastErrorMsg() const;

signals:
    // 向外请求实时设备状态
    void SignalRequestDeviceStatus();
    // 通知UI执行确认换型逻辑
    void SignalExecuteProductModelSwitch(const QString& productModel);
    // 流程步骤切换通知
    void SignalStepChanged(ModelChangeStep step);
    // 整个换型流程最终结束回调
    void SignalModelChangeFinished(ModelChangeResult result, const QString& msg);
    // 产生设备告警，对外推送
    void SignalAlarmTrigger(const QString& alarmInfo);
    // 请求执行设备自检，回调带回整体结果、明细数组
    void SignalRequestDeviceSelfCheck(QString& outResult, QJsonArray& outDetailItems);

private slots:
    // 10s轮询定时器：拉取MES下发指令
    void SlotPollInstruction();
    // 等待本地换型超时定时器触发
    void SlotWaitLocalModelTimeout();
    // 外部收到状态请求后，回调传入实时设备状态
    void SlotRecvDeviceRealStatus(const QString& status);
    // UI执行完产品配方切换后，回调返回执行结果
    void SlotRecvProductSwitchResult(bool isOk, const QString& errMsg);

public slots:
    void SlotCheckFixtureNeedAndSelfCheck();

private:
    // 指令业务处理入口
    void ProcessInstruction(const DeviceExecCommand& cmd);
    // 统一结束流程，上报结果
    void ReportModelResult(ModelChangeResult res, const QString& extraMsg = "");

    // 接口封装工具函数
    bool PullDeviceInstruction(DeviceExecCommand& outCmd, QString& errMsg);
    bool PullCurrentTask(DeviceTaskInfo& outTask, QString& errMsg);
    //检查是否需要治具
    bool CheckFixtureNeed(QString& errMsg);
    bool RunDeviceSelfCheck(QString& errMsg);
    void UploadCmdResult(const QString& cmd, bool isSuccess, const QString& msg = "");
    void UploadAlarm(const QString& alarmText);
    void UploadDeviceStatus(const QString& status);

private:
    ControlCenterHttpApi* m_ctrlApi;
    MesHttpPost* m_mesApi;

    QTimer m_pollTimer;         // 10s拉取指令定时器
    QTimer m_waitLocalTimer;    // 等待本地换型超时定时器

    QString m_deviceCode;
    QString m_deviceIp;
    ModelChangeStep m_curStep;
    QString m_lastError;
    bool m_localModelComplete;
    bool m_waitingDevStatus;    // 标记当前是否等待外部回传设备状态
    bool m_waitProductSwitch;   // 标记当前是否等待产品配方切换完成
};

#endif // MODELCHANGEMANAGER_H
