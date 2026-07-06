#pragma execution_character_set("utf-8")
#include "ModelChangeManager.h"
#include <QCoreApplication>
#include "VisAppBus.h"

// 业务总线全局常量

const QByteArray EVENT_REQ_PROD_SWITCH = "ReqProductModelSwitch";
const QByteArray EVENT_MODEL_CHANGE_VALID = "ModelChangeValid";

ModelChangeManager::ModelChangeManager(QObject *parent) : QObject(parent)
{
    m_ctrlApi = ControlCenterHttpApi::Instance();
    m_mesApi = MesHttpPost::Instance();
    m_curStep = Step_Idle;

    // 10s轮询指令定时器
    m_pollTimer.setInterval(POLL_INTERVAL);
    connect(&m_pollTimer, &QTimer::timeout, this, &ModelChangeManager::SlotPollInstruction);
}

ModelChangeManager::~ModelChangeManager()
{
    m_pollTimer.stop();
}

void ModelChangeManager::StartModelChange(const QString &deviceCode, const QString &deviceIp)
{
    if (m_curStep != Step_Idle)
    {
        m_lastError = "换型流程正在运行，禁止重复启动";
        emit SignalModelChangeFinished(Result_Fail_CannotChange, m_lastError);
        return;
    }
    m_deviceCode = deviceCode;
    m_deviceIp = deviceIp;
    m_curStep = Step_GetInstruction;
    m_lastError.clear();
    m_pollTimer.start();
}

void ModelChangeManager::StopModelChange()
{
    m_pollTimer.stop();
    m_curStep = Step_Idle;
}

ModelChangeStep ModelChangeManager::GetCurrentStep() const
{
    return m_curStep;
}

QString ModelChangeManager::GetLastErrorMsg() const
{
    return m_lastError;
}

void ModelChangeManager::SlotPollInstruction()
{
    if (m_curStep != Step_GetInstruction)
        return;

    DeviceExecCommand cmd;
    QString errMsg;
    bool ok = PullDeviceInstruction(cmd, errMsg);
    if (!ok)
    {
        m_lastError = errMsg;
        return;
    }
    ProcessInstruction(cmd);
}

// 收到ModelChangeComplete指令后进入任务获取流程
void ModelChangeManager::NotifyLocalModelComplete()
{
    if (m_curStep != Step_WaitModelComplete)
        return;

    // 1：获取生产任务
    m_curStep = Step_GetTaskInfo;
    DeviceTaskInfo taskInfo;
    QString taskErr;
    bool taskOk = PullCurrentTask(taskInfo, taskErr);
    if (!taskOk)
    {
        UploadAlarm("获取生产任务失败：" + taskErr);
        //UploadCmdResult("ModelChangeComplete", false, taskErr);
        ReportModelResult(Result_Fail_NoTask, taskErr);
        return;
    }

    QString changeRes;
    QList<QPair<QString,QString>> fixtureList;
    QString selfRes;

    // 2：下发总线请求配方切换
    m_curStep = Step_ExecuteProductSwitch;

    int busRet = VisAppBus::sendEvent(EVENT_REQ_PROD_SWITCH, taskInfo.productionNum,changeRes,fixtureList,selfRes);
    if (busRet != 0)
    {
        QString err = QString("下发配方切换总线失败，错误码：%1").arg(busRet);
        UploadAlarm(err);
        //UploadCmdResult("ModelChangeComplete", false, err);
        ReportModelResult(Result_Fail_MissingFile, err);
        return;
    }

    QList<BindFixtureItem> bindList;
    if(fixtureList.size())
    {
        bindList.reserve(fixtureList.size());
        for (const auto& pair : fixtureList)
        {
            BindFixtureItem item;
            item.number = pair.first;
            item.channel = pair.second;
            bindList.append(item);
        }
    }

    ProdSwitchBusMsg(changeRes,bindList,selfRes);
}

// 指令处理主逻辑
void ModelChangeManager::ProcessInstruction(const DeviceExecCommand &cmd)
{
    const QString& cmdText = cmd.command.trimmed();
    // NONE：无指令，持续轮询
    if (cmdText == "NONE" || cmdText.isEmpty())
        return;

    // 收到ModelChangePrepare：总线校验设备是否允许换型
    if (cmdText == "ModelChangePrepare")
    {
        m_pollTimer.stop();
        m_curStep = Step_CheckDeviceStatus;
        bool canChange = false;
        int busRet = VisAppBus::sendEvent(EVENT_MODEL_CHANGE_VALID, canChange);

        // 总线发送失败
        if (busRet != 0)
        {
            QString errMsg = QString("下发设备可换型校验总线失败，错误码：%1").arg(busRet);
            UploadCmdResult("ModelChangePrepare", false, errMsg);
            ReportModelResult(Result_Fail_CannotChange, errMsg);
            return;
        }

        // 上报Prepare执行结果
        UploadCmdResult("ModelChangePrepare", canChange);
        if (!canChange)
        {
            QString errMsg = "当前设备状态不可执行换型";
            ReportModelResult(Result_Fail_CannotChange, errMsg);
            return;
        }

        // 校验通过，进入无限等待ModelChangeComplete，重启10s轮询
        m_curStep = Step_WaitModelComplete;
        m_pollTimer.start();
        return;
    }

    // 收到ModelChangeComplete：停止轮询，进入任务&配方切换流程
    else if (cmdText == "ModelChangeComplete")
    {
        m_pollTimer.stop();
        NotifyLocalModelComplete();
        return;
    }
}

// 统一流程收尾，重置状态并抛出结束信号
void ModelChangeManager::ReportModelResult(ModelChangeResult res, const QString &extraMsg)
{
    m_pollTimer.stop();
    m_curStep = Step_Idle;
    m_lastError = extraMsg;
    emit SignalModelChangeFinished(res, extraMsg);
}

void ModelChangeManager::ProdSwitchBusMsg(QString changeRes, QList<BindFixtureItem>& fixtureList, QString selfRes)
{
    if (m_curStep != Step_ExecuteProductSwitch)
        return;

    // 配方切换NG（缺少文件）
    if (changeRes.compare("NG", Qt::CaseInsensitive) == 0)
    {
        QString errMsg = "配方切换失败，缺失生产程序文件";
        UploadAlarm(errMsg);
        UploadCmdResult("ModelChangeComplete", false, errMsg);
        ReportModelResult(Result_Fail_MissingFile, errMsg);
        return;
    }

    // 配方OK，进入治具校验
    m_curStep = Step_CheckFixtureUse;
    QString fixtureErr;
    bool fixtureOk = true;
    if (fixtureList.size())
    {
        fixtureOk = CheckFixtureNeed(fixtureList,fixtureErr);
        if (!fixtureOk)
        {
            UploadAlarm("治具校验失败：" + fixtureErr);
            UploadCmdResult("ModelChangeComplete", false, fixtureErr);
            ReportModelResult(Result_FixtureCheck_Fail, fixtureErr);
            return;
        }
    }

    // 治具校验通过，执行设备自检
    m_curStep = Step_DeviceSelfCheck;
    QString selfCheckErr;
    bool selfOk = RunDeviceSelfCheck(selfCheckErr);

    // 自检NG
    if (!selfOk || selfRes.compare("NG", Qt::CaseInsensitive) == 0)
    {
        QString errMsg = "设备自检未通过：" + selfCheckErr;
        UploadAlarm(errMsg);
        UploadDeviceStatus("FAULT");
        UploadCmdResult("ModelChangeComplete", false, errMsg);
        ReportModelResult(Result_SelfCheck_Fail, errMsg);
        return;
    }

    // 全流程成功收尾
    m_curStep = Step_FinishModelChange;
    UploadCmdResult("ModelChangeComplete", true);
    UploadDeviceStatus("RUNNING");
    ReportModelResult(Result_Success, "一键换型全部流程执行完成");
}

// 拉取控制中心下发指令
bool ModelChangeManager::PullDeviceInstruction(DeviceExecCommand &outCmd, QString &errMsg)
{
    errMsg = m_ctrlApi->GetDeviceExecCommands(m_deviceCode, m_deviceIp, outCmd);
    return errMsg.isEmpty();
}

// 获取当前生产任务
bool ModelChangeManager::PullCurrentTask(DeviceTaskInfo &outTask, QString &errMsg)
{
    errMsg = m_ctrlApi->GetDeviceTaskInfo(m_deviceCode, m_deviceIp, outTask);
    return errMsg.isEmpty();
}

// MES治具校验逻辑
bool ModelChangeManager::CheckFixtureNeed(QList<BindFixtureItem>&list, QString& errMsg)
{
    QString msg;
    bool needFixture = false;
    msg = m_mesApi->ValidateDeviceUseFixture(needFixture);
    if (!msg.isEmpty())
    {
        errMsg = msg;
        return false;
    }

    if (needFixture)
    {
        FixtureConsumeResult dummyResult;
        //传入条码和通道
        msg = m_mesApi->BindFixtureChannel(list, dummyResult);
        if (!msg.isEmpty())
        {
            errMsg = "治具通道绑定校验失败：" + msg;
            return false;
        }
    }
    return true;
}

// 执行设备自检并上报自检结果
bool ModelChangeManager::RunDeviceSelfCheck(QString &errMsg)
{
    QString selfResult;
    QJsonArray detailItems;
    emit SignalRequestDeviceSelfCheck(selfResult, detailItems);

    if (selfResult.compare("NG", Qt::CaseInsensitive) == 0)
    {
        errMsg = "硬件自检未通过";
        return false;
    }

    QString uploadErr;
    uploadErr = m_ctrlApi->UploadDeviceSelfCheckResultExt(m_deviceCode, m_deviceIp, "", selfResult, detailItems);
    if (!uploadErr.isEmpty())
    {
        errMsg = "自检结果上报接口异常：" + uploadErr;
        return false;
    }
    return true;
}

// 上报指令执行结果
void ModelChangeManager::UploadCmdResult(const QString &cmd, bool isSuccess, const QString &msg)
{
    QString err;
    err = m_ctrlApi->SaveDeviceCmdExecResult(m_deviceCode, m_deviceIp, cmd, isSuccess ? "success" : "fail");
    if (!msg.isEmpty())
    {
        err = m_ctrlApi->SaveDeviceInstructionFeedback(m_deviceCode, m_deviceIp, cmd, isSuccess ? "success" : "fail", msg);
    }
}

// 上报告警事件
void ModelChangeManager::UploadAlarm(const QString &alarmText)
{
    QString err;
    err = m_ctrlApi->UploadDeviceAlarmEvent(m_deviceCode, m_deviceIp, alarmText, QDateTime::currentDateTime());
    emit SignalAlarmTrigger(alarmText);
}

// 上报设备运行状态
void ModelChangeManager::UploadDeviceStatus(const QString &status)
{
    QString err;
    err = m_ctrlApi->UploadDeviceStatus(m_deviceCode, m_deviceIp, status);
}
