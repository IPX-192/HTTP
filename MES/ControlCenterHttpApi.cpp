#pragma execution_character_set("utf-8")
#include "ControlCenterHttpApi.h"
#include <QHostInfo>
#include <QFile>
#include <QApplication>
#include <QUrlQuery>
#include <QDir>

ControlCenterHttpApi* ControlCenterHttpApi::m_instance = nullptr;
QMap<int, QString> ControlCenterHttpApi::m_parseErrMap;

ControlCenterHttpApi* ControlCenterHttpApi::Instance(QObject *parent)
{
    if (!m_instance)
        m_instance = new ControlCenterHttpApi(parent);
    return m_instance;
}

void ControlCenterHttpApi::Uninstance()
{
    if (m_instance)
    {
        delete m_instance;
        m_instance = nullptr;
    }
}

ControlCenterHttpApi::ControlCenterHttpApi(QObject *parent) : QObject(parent)
{
    connect(&m_netMgr, &QNetworkAccessManager::finished, this, &ControlCenterHttpApi::slotCtrlRequestFinished);

    QString hostName = QHostInfo::localHostName();
    QHostInfo hostInfo = QHostInfo::fromName(hostName);
    for (const auto& addr : hostInfo.addresses())
    {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback())
        {
            m_localIp = addr.toString();
            break;
        }
    }
}

ControlCenterHttpApi::~ControlCenterHttpApi()
{
    for (auto ctx : m_reqMap.values())
    {
        if (ctx.reply)
        {
            ctx.reply->abort();
            ctx.reply->deleteLater();
        }
    }
    m_reqMap.clear();
    m_reqRunning.clear();
    m_parseErrMap.clear();
}

void ControlCenterHttpApi::SetCtrlIpInfo(const QString &ip, const QString &port)
{
    m_ctrlIp = ip;
    m_ctrlPort = port;
}

QString ControlCenterHttpApi::GetCtrlBaseUrl() const
{
    return QString("http://%1:%2/mes/service/hirain/ipeo/").arg(m_ctrlIp).arg(m_ctrlPort);
}

bool ControlCenterHttpApi::WaitRequestFinish(QNetworkReply *reply, int timeoutMs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    return timer.isActive();
}

void ControlCenterHttpApi::ClearRequestContext(QNetworkReply *reply)
{
    if (m_reqMap.contains(reply))
        m_reqMap.remove(reply);
}

void ControlCenterHttpApi::HandleRequestError(QNetworkReply *reply, int reqType)
{
    if (reply)
    {
        reply->abort();
        ClearRequestContext(reply);
        reply->deleteLater();
    }
    m_reqRunning[reqType] = false;
}

void ControlCenterHttpApi::SaveCtrlLog(const QByteArray &data)
{
    QDateTime now = QDateTime::currentDateTime();
    QString dateStr = now.toString("yyyy-MM-dd");
    QString logDir = QCoreApplication::applicationDirPath() + QDir::separator() + "LOG" + QDir::separator();
    QDir dir;
    if (!dir.exists(logDir))
        dir.mkpath(logDir);
    QString logPath = logDir + "CtrlCenter-" + dateStr + ".txt";
    QFile file(logPath);
    QString timeLabel = now.toString("yyyy-MM-dd HH:mm:ss.zzz");
    if (file.open(QIODevice::Append | QIODevice::Text))
    {
        file.write(timeLabel.toLocal8Bit());
        file.write(" | ");
        file.write(data);
        file.write("\r\n");
    }
    file.close();
}

void ControlCenterHttpApi::slotCtrlRequestFinished(QNetworkReply *reply)
{
    if (!m_reqMap.contains(reply))
    {
        reply->deleteLater();
        return;
    }
    CtrlRequestContext& ctx = m_reqMap[reply];
    ctx.respData = reply->readAll();
}

QString ControlCenterHttpApi::SendCtrlRequestImpl(int reqType, const QString &apiPath, bool isGet, const QJsonObject &body, void* outData)
{
    QString outMsg;
    outMsg.clear();
    if (m_reqRunning.value(reqType, false))
    {
        outMsg = "控制中心接口重复请求拦截";
        return outMsg;
    }
    m_reqRunning[reqType] = true;
    m_parseErrMap.remove(reqType);

    QString fullUrl = GetCtrlBaseUrl() + apiPath;
    QUrl url(fullUrl);
    QNetworkRequest req;
    req.setRawHeader("Content-Type", "application/json;charset=utf-8");

    QNetworkReply* reply = nullptr;
    QJsonDocument doc(body);
    QByteArray postData;
    QString queryString;

    if (isGet)
    {
        QUrlQuery query;
        for (auto iter = body.begin(); iter != body.end(); ++iter)
        {
            query.addQueryItem(iter.key(), iter.value().toString());
        }
        url.setQuery(query);
        queryString = query.toString();
        req.setUrl(url);
        reply = m_netMgr.get(req);
    }
    else
    {
        postData = doc.toJson(QJsonDocument::Compact);
        req.setUrl(url);
        reply = m_netMgr.post(req, postData);
    }

    if (!reply)
    {
        outMsg = "创建网络请求失败";
        HandleRequestError(reply, reqType);
        return outMsg;
    }

    CtrlRequestContext ctx;
    ctx.reqType = reqType;
    ctx.reply = reply;
    ctx.reqBody = body;
    ctx.respData.clear();
    m_reqMap.insert(reply, ctx);

    QString logText = QString("[%1] %2 \n%3")
            .arg(isGet ? "GET" : "POST")
            .arg(apiPath)
            .arg(isGet ? "Query: " + queryString : "Body: " + QString::fromUtf8(postData));
    SaveCtrlLog(logText.toLocal8Bit());

    bool waitOk = WaitRequestFinish(reply, CTRL_NET_REQUEST_TIMEOUT_MS);
    if (!waitOk)
    {
        outMsg = "控制中心接口30s超时";
        HandleRequestError(reply, reqType);
        return outMsg;
    }

    if (!m_reqMap.contains(reply))
    {
        outMsg = "请求上下文丢失";
        HandleRequestError(reply, reqType);
        return outMsg;
    }
    CtrlRequestContext finishCtx = m_reqMap.take(reply);
    QByteArray respBytes = finishCtx.respData;

    int httpCode = finishCtx.reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpCode < 200 || httpCode >= 300)
    {
        outMsg = QString("HTTP异常，状态码：%1").arg(httpCode);
        if (!respBytes.isEmpty())
        {
            QString errorText = QString::fromUtf8(respBytes);
            if (errorText.length() > 200)
                errorText = errorText.left(200) + "...";
            outMsg += QString("，响应：%1").arg(errorText);
        }
        HandleRequestError(reply, reqType);
        return outMsg;
    }
    if (respBytes.isEmpty())
    {
        outMsg = "控制中心返回空数据";
        HandleRequestError(reply, reqType);
        return outMsg;
    }

    QJsonParseError jsonErr;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(respBytes, &jsonErr);
    if (jsonErr.error != QJsonParseError::NoError)
    {
        QString respText = QString::fromUtf8(respBytes);
        if (respText.length() > 200)
            respText = respText.left(200) + "...";
        outMsg = QString("JSON解析失败：%1，原始响应：%2")
                .arg(jsonErr.errorString())
                .arg(respText);
        HandleRequestError(reply, reqType);
        return outMsg;
    }
    QJsonObject jsonObj = jsonDoc.object();

    //先统一进行全局解析
    bool parseOk = ParseCommonResponse(jsonObj, reqType);
    if (!parseOk)
    {
        outMsg = m_parseErrMap.value(reqType);
        HandleRequestError(reply, reqType);
        return outMsg;
    }

    // 分类型解析返回结构体
    switch (reqType)
    {
    case ctrlGetDeviceExecCommands:
    {
        if (!outData)
        {
            m_parseErrMap[reqType] = "输出DeviceExecCommand指针为空";
            parseOk = false;
            break;
        }
        auto& ref = *static_cast<DeviceExecCommand*>(outData);
        parseOk = ParseExecCommandResp(jsonObj, ref);
        break;
    }
    case ctrlGetDeviceTaskInfo:
    {
        if (!outData)
        {
            m_parseErrMap[reqType] = "输出DeviceTaskInfo指针为空";
            parseOk = false;
            break;
        }
        auto& ref = *static_cast<DeviceTaskInfo*>(outData);
        parseOk = ParseTaskInfoResp(jsonObj, ref);
        break;
    }
        // 其余接口无输出结构体，无需解析
    case ctrlDeviceSelfCheckStatus:
    case ctrlDeviceSelfCheckResult:
    case ctrlDeviceSelfCheckResultExt:
    case ctrlDeviceAlarmEvent:
    case ctrlDeviceStatus:
    case ctrlSaveDeviceCmdExecResult:
    case ctrlSaveDeviceInstructionFeedback:
        parseOk = true;
        break;
    default:
        parseOk = true;
        break;
    }

    if (!parseOk)
    {
        outMsg = m_parseErrMap.value(reqType);
    }

    // 正常成功统一释放
    finishCtx.reply->deleteLater();
    m_reqRunning[reqType] = false;
    return outMsg;
}

// 公共校验：失败错误存入静态map
bool ControlCenterHttpApi::ParseCommonResponse(const QJsonObject &jsonObj, int reqType)
{
    QString status = jsonObj["status"].toString();
    if (status.compare("PASS", Qt::CaseInsensitive) != 0 && status.compare("success", Qt::CaseInsensitive) != 0)
    {
        QString msg = jsonObj["message"].toString("未知错误");
        m_parseErrMap[reqType] = QString("接口返回失败：%1").arg(msg);
        return false;
    }
    return true;
}

bool ControlCenterHttpApi::ParseExecCommandResp(const QJsonObject &jsonObj, DeviceExecCommand &outCmd)
{
    outCmd = DeviceExecCommand();
    QJsonObject content = jsonObj["content"].toObject();
    if (content.isEmpty())
    {
        m_parseErrMap[ctrlGetDeviceExecCommands] = "指令查询返回空content";
        return false;
    }
    if (!content.contains("command"))
    {
        m_parseErrMap[ctrlGetDeviceExecCommands] = "指令返回content缺少command字段";
        return false;
    }
    QString cmdVal = content["command"].toString().trimmed();
    QStringList validCmd = {"NONE", "ModelChangePrepare", "ModelChangeComplete"};
    if (!validCmd.contains(cmdVal))
    {
        m_parseErrMap[ctrlGetDeviceExecCommands] = QString("非法指令：%1，仅支持NONE/ModelChangePrepare/ModelChangeComplete").arg(cmdVal);
        return false;
    }
    outCmd.command = cmdVal;
    return true;
}

bool ControlCenterHttpApi::ParseTaskInfoResp(const QJsonObject &jsonObj, DeviceTaskInfo &outTask)
{
    outTask = DeviceTaskInfo();
    QJsonObject content = jsonObj["content"].toObject();
    outTask.workOrderNum = content["workOrderNum"].toString();
    outTask.processName = content["processName"].toString();
    outTask.workOrderTotal = content["workOrderTotal"].toInt();
    outTask.token = content["token"].toString();
    outTask.processKey = content["processKey"].toString();
    outTask.productionNum = content["productionNum"].toString();
    outTask.programName = content["programName"].toString();
    return true;
}

QString ControlCenterHttpApi::UploadDeviceSelfCheckStatus(const QString& deviceCode, const QString& deviceIp, const QString& channel,
                                                          const QString& result, const QJsonArray& productResultItem)
{
    QJsonObject body;
    body["deviceCode"] = deviceCode;
    body["deviceIp"] = deviceIp;
    body["channel"] = channel;
    body["result"] = result;
    body["productResultItem"] = productResultItem;
    return SendCtrlRequest(ctrlDeviceSelfCheckStatus, "upload/DeviceSelfStatus", false, body);
}

QString ControlCenterHttpApi::UploadDeviceSelfCheckResult(const QString& deviceCode, const QString& deviceIp, const QString& result)
{
    QJsonObject body;
    body["deviceCode"] = deviceCode;
    body["deviceIp"] = deviceIp;
    body["result"] = result;
    return SendCtrlRequest(ctrlDeviceSelfCheckResult, "upload/DeviceSelfCheckResult", false, body);
}

QString ControlCenterHttpApi::UploadDeviceSelfCheckResultExt(const QString& deviceCode, const QString& deviceIp, const QString& channel,
                                                             const QString& result, const QJsonArray& productResultItem)
{
    QJsonObject body;
    body["deviceCode"] = deviceCode;
    body["deviceIp"] = deviceIp;
    body["channel"] = channel;
    body["result"] = result;
    body["productResultItem"] = productResultItem;
    return SendCtrlRequest(ctrlDeviceSelfCheckResultExt, "upload/DeviceSelfCheckResultExt", false, body);
}

QString ControlCenterHttpApi::UploadDeviceAlarmEvent(const QString& deviceCode, const QString& deviceIp,
                                                     const QString& sourceAlarmEvent, const QDateTime& alarmTime)
{
    QJsonObject body;
    body["deviceCode"] = deviceCode;
    body["deviceIp"] = deviceIp;
    body["sourceAlarmEvent"] = sourceAlarmEvent;
    body["alarmTime"] = alarmTime.toString("yyyy-MM-ddTHH:mm:ss.zzz");
    return SendCtrlRequest(ctrlDeviceAlarmEvent, "upload/deviceAlarmEvent", false, body);
}

QString ControlCenterHttpApi::UploadDeviceStatus(const QString& deviceCode, const QString& deviceIp, const QString& status)
{
    QJsonObject body;
    body["deviceCode"] = deviceCode;
    body["deviceIp"] = deviceIp;
    body["status"] = status;
    return SendCtrlRequest(ctrlDeviceStatus, "upload/deviceStatus", false, body);
}

// GET查询带结构体输出
QString ControlCenterHttpApi::GetDeviceExecCommands(const QString& deviceCode, const QString& ip, DeviceExecCommand& outCmd)
{
    QJsonObject body;
    body["deviceCode"] = deviceCode;
    body["ip"] = ip;
    return SendCtrlRequest(ctrlGetDeviceExecCommands, "deviceInstruction/getDeviceExecutionCommands", true, body, outCmd);
}

QString ControlCenterHttpApi::GetDeviceTaskInfo(const QString& deviceCode, const QString& ip, DeviceTaskInfo& outTask)
{
    QJsonObject body;
    body["deviceCode"] = deviceCode;
    body["ip"] = ip;
    return SendCtrlRequest(ctrlGetDeviceTaskInfo, "deviceInstruction/getDeviceTaskInfo", true, body, outTask);
}

// GET反馈上报无结构体
QString ControlCenterHttpApi::SaveDeviceCmdExecResult(const QString& deviceCode, const QString& ip, const QString& command, const QString& result)
{
    QJsonObject body;
    body["deviceCode"] = deviceCode;
    body["ip"] = ip;
    body["command"] = command;
    body["result"] = result;
    return SendCtrlRequest(ctrlSaveDeviceCmdExecResult, "deviceInstruction/saveDeviceCommandsExecutionResult", true, body);
}

QString ControlCenterHttpApi::SaveDeviceInstructionFeedback(const QString& deviceCode, const QString& ip, const QString& command,
                                                            const QString& result, const QString& message)
{
    QJsonObject body;
    body["deviceCode"] = deviceCode;
    body["ip"] = ip;
    body["command"] = command;
    body["result"] = result;
    if (!message.isEmpty())
        body["message"] = message;
    return SendCtrlRequest(ctrlSaveDeviceInstructionFeedback, "deviceInstruction/saveDeviceInstructionExecutionResult", true, body);
}
