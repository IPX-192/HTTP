#pragma execution_character_set("utf-8")
#include "ModelChangeManager.h"
#include <QCoreApplication>
#include "VisAppBus.h"

// ========== 业务总线全局常量 ==========
const QByteArray EVENT_MODEL_CHANGE_VALID     = "ModelChangeValid";
const QByteArray EVENT_REQ_PROD_SWITCH            = "ReqProductModelSwitch";
const QByteArray EVENT_GET_FIXTURE_CHANNEL     = "GetFixtureChannelList";
const QByteArray EVENT_GET_SELF_CHECK                = "GetSelfCheckResult";

ModelChangeManager::ModelChangeManager(QObject *parent) : QObject(parent)
{
    m_ctrlApi = ControlCenterHttpApi::Instance();
    m_mesApi = MesHttpPost::Instance();
    m_lastError.clear();
    m_waitCompleteFlag = false;
    m_modelStatus = Status_None;
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
    if (m_pollTimer.isActive())
    {
        m_lastError = "换型流程正在运行，禁止重复启动";
        emit SignalModelChangeFinished(Result_Fail_CannotChange, m_lastError);
        return;
    }
    m_modelStatus = Status_None;
    m_deviceCode = deviceCode;
    m_deviceIp = deviceIp;
    m_lastError.clear();
    m_waitCompleteFlag = false;
    m_pollTimer.start();
}

void ModelChangeManager::StopModelChange()
{
    m_modelStatus = Status_None;
    m_pollTimer.stop();
    m_waitCompleteFlag = false;
}

QString ModelChangeManager::GetLastErrorMsg() const
{
    return m_lastError;
}

void ModelChangeManager::SlotPollInstruction()
{
    DeviceExecCommand cmd;
    QString errMsg;
    bool ok = PullDeviceInstruction(cmd, errMsg);
    if (!ok)
    {
        m_lastError = errMsg;
        return;
    }

    const QString& cmdText = cmd.command.trimmed();

    if (cmdText == "NONE")
    {
        m_modelStatus = Status_None;
    }
    else if (cmdText == "ModelChangePrepare")
    {
        m_modelStatus = Status_Prepare;
    }
    else if (cmdText == "ModelChangeComplete")
    {
        m_modelStatus = Status_Complete;
    }
    ProcessInstruction();
}

// 收到ModelChangeComplete指令后进入任务获取流程
void ModelChangeManager::NotifyLocalModelComplete()
{
    m_waitCompleteFlag = false;

    // 1：获取生产任务
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
    QString err;
    // 1. 请求配方切换
    QString changeRes;
    int busRet1 = VisAppBus::sendEvent(EVENT_REQ_PROD_SWITCH, taskInfo.productionNum, changeRes);
    if (busRet1 != 0)
    {
        QString err = QString("下发【配方切换】总线失败，错误码：%1").arg(busRet1);
        UploadAlarm(err);
        //UploadCmdResult("ModelChangeComplete", false, err);
        ReportModelResult(Result_Fail_MissingFile, err);
        return;
    }
    // 配方校验
    if (!HandleProdSwitch(changeRes, err))
    {
        UploadAlarm(err);
        UploadCmdResult("ModelChangeComplete", false, err);
        ReportModelResult(Result_Fail_MissingFile, err);
        return;
    }

    // 2：请求治具通道数组
    QList<QPair<QString,QString>> fixturePairList;
    int busRet2 = VisAppBus::sendEvent(EVENT_GET_FIXTURE_CHANNEL, taskInfo.productionNum, fixturePairList);
    if (busRet2 != 0)
    {
        QString err = QString("下发【治具通道查询】总线失败，错误码：%1").arg(busRet2);
        UploadAlarm(err);
        //UploadCmdResult("ModelChangeComplete", false, err);
        ReportModelResult(Result_FixtureCheck_Fail, err);
        return;
    }
    QList<BindFixtureItem> bindList;
    if(!fixturePairList.isEmpty())
    {
        bindList.reserve(fixturePairList.size());
        for (const auto& pair : fixturePairList)
        {
            BindFixtureItem item;
            item.number = pair.first;
            item.channel = pair.second;
            bindList.append(item);
        }
    }
    // 治具校验
    if (!HandleFixtureCheck(bindList, err))
    {
        UploadAlarm(err);
        UploadCmdResult("ModelChangeComplete", false, err);
        ReportModelResult(Result_FixtureCheck_Fail, err);
    }

    //设备自检前上报设备切换成功
    UploadCmdResult("ModelChangeComplete", true);

    // 3：请求自检状态
    QString selfRes;
    int busRet3 = VisAppBus::sendEvent(EVENT_GET_SELF_CHECK, taskInfo.productionNum, selfRes);
    if (busRet3 != 0)
    {
        QString err = QString("下发【自检结果查询】总线失败，错误码：%1").arg(busRet3);
        UploadAlarm(err);
        //UploadCmdResult("ModelChangeComplete", false, err);
        ReportModelResult(Result_SelfCheck_Fail, err);
        return;
    }
    //  自检校验
    if (!HandleSelfCheck(selfRes, err))
    {
        UploadAlarm(err);
        UploadDeviceStatus("FAULT");
        UploadCmdResult("ModelChangeComplete", false, err);
        ReportModelResult(Result_SelfCheck_Fail, err);
        return;
    }

    // 全部校验通过
    UploadDeviceStatus("RUNNING");
    ReportModelResult(Result_Success, "一键换型全部流程执行完成");
}

void ModelChangeManager::ProcessInstruction()
{
    if(m_modelStatus == Status_None)
    {
        QString errMsg = "无任务消息，退出等待换型完成指令";
        UploadAlarm(errMsg);
        UploadCmdResult("ModelChangePrepare", false, errMsg);
        m_waitCompleteFlag = false;
        return;
    }

    else if (m_modelStatus == Status_Prepare )
    {
        bool canChange = false;
        int busRet = VisAppBus::sendEvent(EVENT_MODEL_CHANGE_VALID, canChange);
        if (busRet != 0)
        {
            QString errMsg = QString("下发设备可换型校验总线失败，错误码：%1").arg(busRet);
            //UploadCmdResult("ModelChangePrepare", false, errMsg);
            ReportModelResult(Result_Fail_CannotChange, errMsg);
            return;
        }
        //上报是否可以执行换型
        UploadCmdResult("ModelChangePrepare", canChange);
        if (!canChange)
        {
            QString errMsg = "当前设备状态不可执行换型";
            ReportModelResult(Result_Fail_CannotChange, errMsg);
            return;
        }
        // 校验通过，标记等待Complete
        m_waitCompleteFlag = true;
        return;
    }

    // 等待阶段收到Complete，执行业务
    if(m_waitCompleteFlag && m_modelStatus == Status_Complete )
    {
        NotifyLocalModelComplete();
    }
}

// 统一流程收尾，重置状态并抛出结束信号
void ModelChangeManager::ReportModelResult(ModelChangeResult res, const QString &extraMsg)
{
    m_modelStatus = Status_None;
    m_waitCompleteFlag = false;
    m_lastError = extraMsg;
    emit SignalModelChangeFinished(res, extraMsg);
}

// 1. 处理配方切换
bool ModelChangeManager::HandleProdSwitch(const QString& changeRes, QString& outErr)
{
    if (changeRes.compare("NG", Qt::CaseInsensitive) == 0)
    {
        outErr = "配方切换失败，缺失生产程序文件";
        return false;
    }
    return true;
}

// 2. 处理治具校验
bool ModelChangeManager::HandleFixtureCheck(QList<BindFixtureItem>& fixtureList, QString& outErr)
{
    if (fixtureList.isEmpty())
        return true;

    QString errMsg;
    bool ok = CheckFixtureNeed(fixtureList, errMsg);
    if (!ok)
    {
        outErr = "治具校验失败：" + errMsg;
        return false;
    }
    return true;
}

// 3. 处理自检结果
bool ModelChangeManager::HandleSelfCheck(const QString& selfRes, QString& outErr)
{
    if (selfRes.compare("NG", Qt::CaseInsensitive) == 0)
    {
        outErr = "设备自检未通过";
        return false;
    }
    return true;
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
bool ModelChangeManager::CheckFixtureNeed(QList<BindFixtureItem>&list, QString &errMsg)
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
        msg = m_mesApi->BindFixtureChannel(list, dummyResult);
        if (!msg.isEmpty())
        {
            errMsg = "治具通道绑定校验失败：" + msg;
            return false;
        }
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
