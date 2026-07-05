#pragma execution_character_set("utf-8")
#include "ModelChangeManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include "VisAppBus.h"

ModelChangeManager::ModelChangeManager(QObject *parent) : QObject(parent)
{
    m_ctrlApi = ControlCenterHttpApi::Instance();
    m_mesApi = MesHttpPost::Instance();
    m_curStep = Step_Idle;
    m_localModelComplete = false;
    m_waitingDevStatus = false;
    m_waitProductSwitch = false;

    // 轮询指令定时器
    m_pollTimer.setInterval(POLL_INTERVAL);
    connect(&m_pollTimer, &QTimer::timeout, this, &ModelChangeManager::SlotPollInstruction);

    // 等待本地换型超时定时器
    m_waitLocalTimer.setInterval(WAIT_LOCAL_TIMEOUT);
    m_waitLocalTimer.setSingleShot(true);
    connect(&m_waitLocalTimer, &QTimer::timeout, this, &ModelChangeManager::SlotWaitLocalModelTimeout);
}

ModelChangeManager::~ModelChangeManager()
{
    m_pollTimer.stop();
    m_waitLocalTimer.stop();
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
    m_localModelComplete = false;
    m_waitingDevStatus = false;
    m_waitProductSwitch = false;
    m_lastError.clear();

    m_pollTimer.start();

}

void ModelChangeManager::NotifyLocalModelComplete()
{
    if (m_curStep != Step_WaitModelComplete)
        return;
    m_localModelComplete = true;
    m_waitLocalTimer.stop();

    // 本地换型完成，流转下一步：获取生产任务
    m_curStep = Step_GetTaskInfo;

    DeviceTaskInfo taskInfo;
    QString taskErr;
    bool taskOk = PullCurrentTask(taskInfo, taskErr);
    if (!taskOk)
    {
        UploadAlarm("获取生产任务失败：" + taskErr);
        UploadCmdResult("ModelChangeComplete", false, taskErr);
        ReportModelResult(Result_Fail_NoTask, taskErr);
        return;
    }

    m_curStep = Step_ExecuteProductSwitch;
    m_waitProductSwitch = true;


    // 参数：设备编码、产品编号，给到UI模块
    int busRet = VisAppBus::sendEvent_Topic(BUS_TOPIC_MODEL_CHANGE, EVENT_REQ_PROD_SWITCH,
                                            m_deviceCode, taskInfo.productionNum);
    if (busRet != 0)
    {
        QString err = QString("下发配方切换总线失败，错误码：%1").arg(busRet);
        UploadAlarm(err);
        UploadCmdResult("ModelChangeComplete", false, err);
        ReportModelResult(Result_Fail_MissingFile, err);
        return;
    }
    // 启动超时计时，防止UI无回复卡死
    m_waitProdSwitchBusTimer.start();
    return;
}

void ModelChangeManager::SlotRecvProductSwitchResult(bool isOk, const QString &errMsg)
{
    // 仅处于等待产品切换阶段才处理回调
    if (!m_waitProductSwitch || m_curStep != Step_ExecuteProductSwitch)
        return;
    m_waitProductSwitch = false;

    if (!isOk)
    {
        // NG分支：配方缺失/切换失败，上报告警+上报换型失败，终止流程
        UploadAlarm(errMsg);
        UploadCmdResult("ModelChangeComplete", false, errMsg);
        ReportModelResult(Result_Fail_MissingFile, errMsg);
        return;
    }

    // OK：配方切换完成，进入治具校验步骤
    m_curStep = Step_CheckFixtureUse;

    QString fixtureErr;
    bool fixtureOk = CheckFixtureNeed(fixtureErr);
    if (!fixtureOk)
    {
        UploadAlarm("治具校验失败：" + fixtureErr);
        UploadCmdResult("ModelChangeComplete", false, fixtureErr);
        ReportModelResult(Result_FixtureCheck_Fail, fixtureErr);
        return;
    }

    // 设备切换成功，进入自检流程
    m_curStep = Step_DeviceSelfCheck;

    QString selfCheckErr;
    bool selfOk = RunDeviceSelfCheck(selfCheckErr);
    if (!selfOk)
    {
        UploadAlarm("设备自检异常：" + selfCheckErr);
        UploadDeviceStatus("FAULT");
        UploadCmdResult("ModelChangeComplete", false, selfCheckErr);
        ReportModelResult(Result_SelfCheck_Fail, selfCheckErr);
        return;
    }

    // 全流程成功收尾
    m_curStep = Step_FinishModelChange;

    UploadCmdResult("ModelChangeComplete", true);
    UploadDeviceStatus("RUNNING");
    ReportModelResult(Result_Success, "一键换型全部流程执行完成");
}

void ModelChangeManager::SlotCheckFixtureNeedAndSelfCheck()
{

}

void ModelChangeManager::StopModelChange()
{
    m_pollTimer.stop();
    m_waitLocalTimer.stop();
    m_curStep = Step_Idle;
    m_localModelComplete = false;
    m_waitingDevStatus = false;
    m_waitProductSwitch = false;
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

void ModelChangeManager::SlotWaitLocalModelTimeout()
{
    if (m_curStep != Step_WaitModelComplete)
        return;
    QString err = "等待本地设备换型完成超时(5分钟)，流程终止";
    UploadCmdResult("ModelChangePrepare", false, err);
    ReportModelResult(Result_Fail_WaitTimeout, err);
}

void ModelChangeManager::SlotRecvDeviceRealStatus(const QString &status)
{
    // 仅处于等待状态校验阶段才处理外部回传的状态
    if (!m_waitingDevStatus || m_curStep != Step_CheckDeviceStatus)
        return;
    m_waitingDevStatus = false;
    QString curStatus = status.trimmed();

    // 校验规则：仅IDLE/READY允许换型
    bool canChange = (curStatus.compare("IDLE", Qt::CaseInsensitive) == 0
                      || curStatus.compare("READY", Qt::CaseInsensitive) == 0);

    if (!canChange)
    {
        QString errMsg = QString("当前设备实时状态[%1]不可执行换型，请切换至空闲状态后重试").arg(curStatus);
        UploadCmdResult("ModelChangePrepare", false, errMsg);
        ReportModelResult(Result_Fail_CannotChange, errMsg);
        return;
    }

    // 状态校验通过，进入等待本地换型步骤
    m_curStep = Step_WaitModelComplete;

    m_waitLocalTimer.start();
}

void ModelChangeManager::ProcessInstruction(const DeviceExecCommand &cmd)
{
    const QString& cmdText = cmd.command.trimmed();
    // 无下发指令，继续轮询
    if (cmdText == "NONE" || cmdText.isEmpty())
        return;

    // 收到ModelChangePrepare换型指令，进入校验步骤
    if (cmdText == "ModelChangePrepare")
    {
        m_pollTimer.stop();
        m_curStep = Step_CheckDeviceStatus;

        //等待是否允许换型

        VisAppBus::subscibeEvent_Topic("ModelChangeBus", "RspDevStatus", this);

        // 1. 上报指令接收成功
        UploadCmdResult("ModelChangePrepare", true);

        // 向外发送信号，请求外部传入实时设备状态
        m_waitingDevStatus = true;

        return;
    }

    else if(cmdText == "ModelChangeComplete")
    {
        NotifyLocalModelComplete();
    }

}

void ModelChangeManager::ReportModelResult(ModelChangeResult res, const QString &extraMsg)
{
    m_pollTimer.stop();
    m_waitLocalTimer.stop();
    m_curStep = Step_Idle;
    m_lastError = extraMsg;
    emit SignalModelChangeFinished(res, extraMsg);
}

bool ModelChangeManager::PullDeviceInstruction(DeviceExecCommand &outCmd, QString &errMsg)
{
    errMsg = m_ctrlApi->GetDeviceExecCommands(m_deviceCode, m_deviceIp, outCmd);
    return errMsg.isEmpty();
}

bool ModelChangeManager::PullCurrentTask(DeviceTaskInfo &outTask, QString &errMsg)
{
    errMsg = m_ctrlApi->GetDeviceTaskInfo(m_deviceCode, m_deviceIp, outTask);
    return errMsg.isEmpty();
}

bool ModelChangeManager::CheckFixtureNeed(QString &errMsg)
{
    QString msg;
    bool needFixture = false;
    msg = m_mesApi->ValidateDeviceUseFixture(needFixture);
    if (!msg.isEmpty())
    {
        errMsg = msg;
        return false;
    }

    // 需要治具则校验通道绑定
    if (needFixture)
    {
        FixtureConsumeResult dummyResult;
        //传入条码和通道，暂未赋值
        msg = m_mesApi->BindFixtureChannel("", "", dummyResult);
        if (!msg.isEmpty())
        {
            errMsg = "治具通道校验失败：" + msg;
            return false;
        }
    }
    return true;
}

bool ModelChangeManager::RunDeviceSelfCheck(QString &errMsg)
{
    QString selfResult;
    QJsonArray detailItems;
    // 发送信号，外部硬件/工位类接收并执行真实自检，回填结果
    emit SignalRequestDeviceSelfCheck(selfResult, detailItems);

    // 外部执行完自检后回填结果，判断是否NG
    if (selfResult.compare("NG", Qt::CaseInsensitive) == 0)
    {
        errMsg = "设备硬件自检未通过";
        return false;
    }

    // 自检OK，再调用接口上报真实结果
    QString msg;
    msg = m_ctrlApi->UploadDeviceSelfCheckResultExt(m_deviceCode, m_deviceIp, "", selfResult, detailItems);
    if (!msg.isEmpty())
    {
        errMsg = "自检上报接口异常：" + msg;
        return false;
    }
    return true;
}

void ModelChangeManager::UploadCmdResult(const QString &cmd, bool isSuccess, const QString &msg)
{
    QString err;
    // 上报指令执行结果
    err = m_ctrlApi->SaveDeviceCmdExecResult(m_deviceCode, m_deviceIp, cmd, isSuccess ? "success" : "fail");
    if (!msg.isEmpty())
    {
        err = m_ctrlApi->SaveDeviceInstructionFeedback(m_deviceCode, m_deviceIp, cmd, isSuccess ? "success" : "fail", msg);
    }
}

void ModelChangeManager::UploadAlarm(const QString &alarmText)
{
    QString err;
    err = m_ctrlApi->UploadDeviceAlarmEvent(m_deviceCode, m_deviceIp, alarmText, QDateTime::currentDateTime());
    emit SignalAlarmTrigger(alarmText);
}

void ModelChangeManager::UploadDeviceStatus(const QString &status)
{
    QString err;
    err = m_ctrlApi->UploadDeviceStatus(m_deviceCode, m_deviceIp, status);
}
