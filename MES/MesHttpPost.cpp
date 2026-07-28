#pragma execution_character_set("utf-8")
#include "MesHttpPost.h"
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDateTime>
#include <QApplication>
#include <QUrl>
#include <QDir>

MesHttpPost* MesHttpPost::m_instance = nullptr;
QMap<QString, QString> MesHttpPost::m_parseErrMap;

MesHttpPost::MesHttpPost(QObject *parent) : QObject(parent)
{
    connect(&m_httpPost, &QNetworkAccessManager::finished, this, &MesHttpPost::slotPostFinished);
}

MesHttpPost::~MesHttpPost()
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

void MesHttpPost::Uninstance()
{
    if (m_instance)
    {
        delete m_instance;
        m_instance = nullptr;
    }
}

void MesHttpPost::SetMesIpInfo(QString ip, QString port)
{
    MesServerIp = ip;
    MesServerPort = port;
    QString localHostName = QHostInfo::localHostName();
    QHostInfo info = QHostInfo::fromName(localHostName);
    foreach(QHostAddress address, info.addresses())
    {
        if(address.protocol() == QAbstractSocket::IPv4Protocol)
        {
            m_ipInfo = address.toString();
            break;
        }
    }
}

QString MesHttpPost::GetMesBaseUrl() const
{
    return QString("http://%1:%2/mes/service/auto/authToken/").arg(MesServerIp).arg(MesServerPort);
}

QString MesHttpPost::GetStorageGatewayUrl() const
{
    return QString("http://%1:%2/storageGateway/rest/exposeApi/uploadSingle").arg(MesServerIp).arg(MesServerPort);
}

bool MesHttpPost::WaitRequestFinish(QNetworkReply *reply, int timeoutMs)
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

// 私有底层实现，仅内部使用void*
QString MesHttpPost::SendMesPostRequestImpl(ReplyStatus reqType, const QString& apiPath, const QJsonObject& body, void* outData)
{
    QString outMsg;
    outMsg.clear();
    if (m_reqRunning.value(reqType, false))
    {
        outMsg = "接口请求正在处理中，禁止重复调用";
        return outMsg;
    }
    m_reqRunning[reqType] = true;

    QString fullUrl = GetMesBaseUrl() + apiPath;
    QUrl targetUrl(fullUrl);
    QNetworkRequest netReq(targetUrl);
    netReq.setRawHeader("Content-Type", "application/json;charset=utf-8");

    if (reqType != replyUserInfoAuth && !m_iovtoken.isEmpty())
    {
        netReq.setRawHeader("iovtoken", m_iovtoken.toUtf8());
    }

    QList<ReplyStatus> needProcessKeyHeader = {
        replyQueryProductionInfo,replyValidateNumber,
        replySaveProcessOpResult, replyCompleteTask,
        replyStationHeartbeat,replyValidateDeviceUseFixture, replyBindFixtureChannel,
        replyConsumeFixtureLife, replyValidateStandardElementNumber,
        replyUploadSingle, replySaveProductFilePath
    };
    if (needProcessKeyHeader.contains(reqType) && !m_processKey.isEmpty())
    {
        netReq.setRawHeader("processKey", m_processKey.toUtf8());
    }

    QJsonDocument doc(body);
    QByteArray postData = doc.toJson(QJsonDocument::Compact);
    QNetworkReply* reply = m_httpPost.post(netReq, postData);

    //组装本次请求上下文，存入全局 map 缓存
    MesRequestContext ctx;
    ctx.reqType = reqType;
    ctx.reply = reply;
    ctx.reqBody = body;
    ctx.respData.clear();
    m_reqMap.insert(reply, ctx);

    QString logtxt = QString("%1:\n%2").arg(apiPath).arg(QString::fromUtf8(postData));
    QByteArray logByte = logtxt.toLocal8Bit();
    SaveTestLog(logByte);

    bool waitOk = WaitRequestFinish(reply, MES_NET_REQUEST_TIMEOUT_MS);
    if (!waitOk)
    {
        outMsg = u8"HTTP请求超时30秒";
        HandleRequestError(reply, reqType);
        return outMsg;
    }

    // 请求完成后，从 map 取出上下文并删除缓存
    MesRequestContext finishCtx = m_reqMap.take(reply);
    QByteArray respBytes = finishCtx.respData;
    int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpCode < 200 || httpCode >= 300)
    {
        outMsg = QString("服务响应异常，HTTP状态码：%1").arg(httpCode);
        HandleRequestError(reply, reqType);
        return outMsg;
    }
    if (respBytes.isEmpty())
    {
        outMsg = "MES返回空数据";
        HandleRequestError(reply, reqType);
        return outMsg;
    }

    QJsonParseError jsonErr;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(respBytes, &jsonErr);
    if (jsonErr.error != QJsonParseError::NoError)
    {
        outMsg = QString("JSON解析失败:%1").arg(jsonErr.errorString());
        HandleRequestError(reply, reqType);
        return outMsg;
    }
    QJsonObject jsonObj = jsonDoc.object();

    bool parseSuccess = false;
    QString reqKey = QString::number((int)reqType);
    m_parseErrMap.remove(reqKey);

    switch (reqType)
    {
    case replyQueryWorkOrderInfo:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出结构体指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<QVector<PdLineList>*>(outData);
        parseSuccess = ReplyJsonFromQueryWorkOrderInfo(jsonObj, ref);
        break;
    }
    case replyUserInfoAuth:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出结构体指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<AuthUserInfo*>(outData);
        parseSuccess = ReplyJsonFromUserInfoAuth(jsonObj, ref);
        break;
    }
    case replyConfigProcess:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出字符串指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<QString*>(outData);
        parseSuccess = ReplyJsonFromConfigProcess(jsonObj,ref);
        break;
    }
    case replyQueryProductionInfo:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出结构体指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<ProductionInfo*>(outData);
        parseSuccess = ReplyJsonFromQueryProductionInfo(jsonObj, ref);
        break;
    }
    case replyValidateNumber:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出布尔指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<bool*>(outData);
        parseSuccess = ReplyJsonFromValidateNumber(jsonObj,ref);
        break;
    }
    case replySaveProcessOpResult:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出字符串指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<QString*>(outData);
        parseSuccess = ReplyJsonFromSaveProcessOpResult(jsonObj, ref);
        break;
    }
    case replyCompleteTask:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出布尔指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<bool*>(outData);
        parseSuccess = ReplyJsonFromCompleteTask(jsonObj, ref);
        break;
    }
    case replyQueryDeviceProcessInfo:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出列表指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<QList<DeviceProcessItem>*>(outData);
        parseSuccess = ReplyJsonFromQueryDeviceProcessInfo(jsonObj, ref);
        break;
    }
    case replyStationHeartbeat:
        parseSuccess = ReplyJsonFromStationHeartbeat(jsonObj);
        break;
    case replyValidateDeviceUseFixture:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出布尔指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<bool*>(outData);
        parseSuccess = ReplyJsonFromValidateDeviceUseFixture(jsonObj, ref);
        break;
    }
    case replyBindFixtureChannel:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出治具结果指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<FixtureConsumeResult*>(outData);
        parseSuccess = ReplyJsonFromBindFixtureChannel(jsonObj, ref);
        break;
    }
    case replyConsumeFixtureLife:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出治具消耗结果指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<FixtureConsumeResult*>(outData);
        parseSuccess = ReplyJsonFromConsumeFixtureLife(jsonObj, ref);
        break;
    }
    case replyValidateStandardElementNumber:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出布尔指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<bool*>(outData);
        parseSuccess = ReplyJsonFromValidateStandardElementNumber(jsonObj,ref);
        break;
    }
    case replyUploadSingle:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "上传返回结构体指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<UploadFileResp*>(outData);
        parseSuccess = ReplyJsonFromUploadSingle(jsonObj, ref);
        break;
    }
    case replySaveProductFilePath:
    {
        if (!outData)
        {
            m_parseErrMap[reqKey] = "输出布尔指针为空";
            parseSuccess = false;
            break;
        }
        auto& ref = *static_cast<bool*>(outData);
        parseSuccess = ReplyJsonFromSaveProductFilePath(jsonObj,ref);
        break;
    }
    default:
        parseSuccess = true;
        break;
    }

    if (!parseSuccess)
    {
        outMsg = m_parseErrMap.value(reqKey);
    }

    //流程结束，统一释放
    finishCtx.reply->deleteLater();
    m_reqRunning[reqType] = false;
    return outMsg;
}

QString MesHttpPost::EncodeWholePath(const QString& path)
{
    // 兼容Qt5/Qt6 标准枚举，消除Qt::KeepEmptyParts编译报错
    QStringList parts = path.split("/", QString::SplitBehavior::KeepEmptyParts);
    // 所有分段统一URL编码，空字符串编码后仍为空，不破坏层级分隔
    for (int i = 0; i < parts.size(); ++i)
    {
        parts[i] = QString::fromUtf8(QUrl::toPercentEncoding(parts[i]));
    }
    return parts.join("/");
}

QString MesHttpPost::SendMultipartUploadRequest(const GatewayUploadHeader& headerInfo,
                                                const QString& fileName,
                                                const QByteArray& fileBin,
                                                const QString& filePid,
                                                const QString& nsId,
                                                UploadFileResp& outUploadResp)
{
    QString outMsg;

    // 1. 防重复调用互斥锁
    if (m_reqRunning.value(replyUploadSingle, false))
    {
        outMsg = "文件上传正在处理中，禁止重复调用";
        return outMsg;
    }
    m_reqRunning[replyUploadSingle] = true;

    // 2. 初始化返回结构体
    outUploadResp = UploadFileResp{};

    // 前置参数校验
    if (fileName.isEmpty() || fileBin.isEmpty())
    {
        outMsg = "文件名或文件二进制内容不能为空";
        HandleRequestError(nullptr, replyUploadSingle);
        return outMsg;
    }

    // 3. 构建请求基础信息
    QString urlStr = GetStorageGatewayUrl();
    QUrl targetUrl(urlStr);
    QNetworkRequest netReq(targetUrl);

    // 拷贝局部processKey，避免多线程并发覆盖成员变量
    QString localProcessKey = m_processKey;

    // 写入网关4个必填鉴权Header
    netReq.setRawHeader("user", headerInfo.user.toUtf8());
    QByteArray encodeUserName = QUrl::toPercentEncoding(headerInfo.userName);
    netReq.setRawHeader("userName", encodeUserName);
    netReq.setRawHeader("appCode", headerInfo.appCode.toUtf8());
    netReq.setRawHeader("appSecretKey", headerInfo.appSecretKey.toUtf8());

    // 追加工位流程标识
    if (!localProcessKey.isEmpty())
    {
        netReq.setRawHeader("processKey", localProcessKey.toUtf8());
    }

    // 4. 初始化 multipart 分隔标识
    netReq.setRawHeader("Content-Type",
                        QString("multipart/form-data; boundary=%1").arg(MES_UPLOAD_BOUNDARY).toUtf8());
    QByteArray postData;
    const QByteArray bound = QString("--%1\r\n").arg(MES_UPLOAD_BOUNDARY).toUtf8();
    const QByteArray endBound = QString("--%1--\r\n").arg(MES_UPLOAD_BOUNDARY).toUtf8();

    // 封装通用表单字段写入逻辑
    auto addFormField = [&postData, &bound](const QString& name, const QString& value)
    {
        postData += bound;
        postData += QString("Content-Disposition: form-data; name=\"%1\"\r\n\r\n").arg(name).toUtf8();
        postData += value.toUtf8() + "\r\n";
    };

    // 业务完整路径字段（按接口规则分段编码）
    QString encodedFileName = EncodeWholePath(fileName);
    addFormField("filename", encodedFileName);
    addFormField("filePId", filePid);
    addFormField("nameSpaceId", nsId);

    // 5. 处理文件二进制段，文件名转义防报文截断
    QString baseFileName = fileName.section("/", -1);
    QString safeFileName = baseFileName;
    safeFileName.replace("\"", "\\\"");
    safeFileName.replace("\r", "");
    safeFileName.replace("\n", "");
    QString encodedFileNameForHeader = QUrl::toPercentEncoding(baseFileName);

    postData += bound;
    postData += QString("Content-Disposition: form-data; name=\"file\"; "
                        "filename=\"%1\"; filename*=UTF-8''%2\r\n"
                        "Content-Type: application/octet-stream\r\n\r\n")
            .arg(safeFileName, encodedFileNameForHeader).toUtf8();
    postData += fileBin;
    // 部分老网关需要文件流后换行，按需开启
    // postData += "\r\n";
    postData += "\r\n" + endBound;

    // 6. 发起网络请求
    QNetworkReply* reply = m_httpPost.post(netReq, postData);
    if (!reply)
    {
        outMsg = "创建网络POST请求失败";
        HandleRequestError(reply, replyUploadSingle);
        return outMsg;
    }

    // 7. 缓存请求上下文用于等待、接收响应
    MesRequestContext ctx;
    ctx.reqType = replyUploadSingle;
    ctx.reply = reply;
    ctx.reqBody = QJsonObject();
    ctx.respData.clear();
    m_reqMap.insert(reply, ctx);

    // 8. 30秒超时等待
    bool waitOk = WaitRequestFinish(reply, MES_NET_REQUEST_TIMEOUT_MS);
    if (!waitOk)
    {
        outMsg = "文件上传请求超时30秒";
        HandleRequestError(reply, replyUploadSingle);
        return outMsg;
    }

    // 校验上下文是否存在
    if (!m_reqMap.contains(reply))
    {
        outMsg = "请求上下文丢失，响应处理异常";
        HandleRequestError(reply, replyUploadSingle);
        return outMsg;
    }
    MesRequestContext finishCtx = m_reqMap.take(reply);

    // 9. HTTP 状态码校验
    int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpCode < 200 || httpCode >= 300)
    {
        outMsg = QString("上传网关响应异常，HTTP状态码：%1").arg(httpCode);
        QByteArray errorBody = finishCtx.respData;
        if (!errorBody.isEmpty())
        {
            QString errorText = QString::fromUtf8(errorBody);
            if (errorText.size() > UPLOAD_ERR_TRUNCATE_LEN)
                errorText = errorText.left(UPLOAD_ERR_TRUNCATE_LEN) + "...";
            outMsg += QString("，响应内容：%1").arg(errorText);
        }
        HandleRequestError(reply, replyUploadSingle);
        return outMsg;
    }

    // 10. 响应空值拦截
    QByteArray respBytes = finishCtx.respData;
    if (respBytes.isEmpty())
    {
        outMsg = "上传网关返回空数据";
        HandleRequestError(reply, replyUploadSingle);
        return outMsg;
    }

    // 11. JSON 解析错误捕获并打印完整原始报文
    QJsonParseError jsonErr;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(respBytes, &jsonErr);
    if (jsonErr.error != QJsonParseError::NoError)
    {
        QString respText = QString::fromUtf8(respBytes);
        if (respText.size() > UPLOAD_JSON_ERR_TRUNCATE_LEN)
            respText = respText.left(UPLOAD_JSON_ERR_TRUNCATE_LEN) + "...";
        outMsg = QString("上传返回JSON解析失败：%1，原始响应：%2")
                .arg(jsonErr.errorString())
                .arg(respText);
        HandleRequestError(reply, replyUploadSingle);
        return outMsg;
    }

    // 12. 解析业务返回结构体
    QJsonObject jsonObj = jsonDoc.object();
    bool parseSuccess = ReplyJsonFromUploadSingle(jsonObj, outUploadResp);
    if (!parseSuccess)
    {
        QString errKey = QString::number(static_cast<int>(replyUploadSingle));
        outMsg = m_parseErrMap.value(errKey, "文件上传返回数据解析失败");
    }

    // 资源释放、解锁
    finishCtx.reply->deleteLater();
    m_reqRunning[replyUploadSingle] = false;
    return outMsg;
}

void MesHttpPost::HandleRequestError(QNetworkReply* reply, ReplyStatus reqType)
{
    if (reply)
    {
        reply->abort();
        ClearRequestContext(reply);
        reply->deleteLater();
    }
    // 根据当前接口类型释放专属防并发锁
    m_reqRunning[reqType] = false;
}

void MesHttpPost::ClearRequestContext(QNetworkReply* reply)
{
    if (reply && m_reqMap.contains(reply))
    {
        m_reqMap.remove(reply);
    }
}

void MesHttpPost::slotPostFinished(QNetworkReply *reply)
{
    if (!m_reqMap.contains(reply))
    {
        reply->deleteLater();
        return;
    }
    MesRequestContext& ctx = m_reqMap[reply];
    ctx.respData = reply->readAll();
}

void  MesHttpPost::SaveTestLog(const QByteArray& bytedata)
{
    QDateTime current_time = QDateTime::currentDateTime();
    QString current_date = current_time.toString("yyyy-MM-dd");
    QString logPath = QCoreApplication::applicationDirPath() + QDir::separator() + "LOG" + QDir::separator();
    QDir dir;
    if (!dir.exists(logPath))
        dir.mkpath(logPath);
    QString filename = logPath + "MES-" + current_date + ".txt";
    QFile file(filename);
    current_date = current_time.toString("yyyy-MM-dd hh:mm:ss.zzz");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text| QIODevice::Append))
    {
        file.write(current_date.toLocal8Bit());
        file.write(" ");
        file.write(bytedata);
        file.write("\r\n");
    }
    file.close();
}

QString MesHttpPost::QueryWorkOrderInfo(QVector<PdLineList>& outPdLineList)
{
    QJsonObject body;
    return SendMesPostRequest(replyQueryWorkOrderInfo, "queryWorkOrderInfo", body, outPdLineList);
}

QString MesHttpPost::UserInfoAuth(const QString& user, const QString& passwd, const QString& workOrderId, const QString& processId, AuthUserInfo& outUserInfo)
{
    m_processId = processId;
    QJsonObject body;
    body["username"] = user;
    body["password"] = passwd;
    body["workOrderId"] = workOrderId;
    body["processId"] = processId;
    body["deviceIp"] = m_ipInfo;
    body["type"] = "device";
    body["mode"] = "";
    return SendMesPostRequest(replyUserInfoAuth, "userInfoAuth", body, outUserInfo);
}

QString MesHttpPost::ConfigProcess(const QString& processId, QString& outProcessKey)
{
    QJsonObject body;
    body["processId"] = processId;
    body["mode"] = "";
    return SendMesPostRequest(replyConfigProcess, "configProcess", body, outProcessKey);
}

QString MesHttpPost::QueryProductionInfo(ProductionInfo& outProdInfo)
{
    QJsonObject body;
    body["processKey"] = m_processKey;
    return SendMesPostRequest(replyQueryProductionInfo, "queryProductionInfo", body, outProdInfo);
}

QString MesHttpPost::QueryDeviceProcessInfo(const QString& deviceIp, const QString& workOrderId, QList<DeviceProcessItem>& outDeviceList)
{
    QJsonObject body;
    body["deviceIp"] = deviceIp;
    body["workOrderId"] = workOrderId;
    return SendMesPostRequest(replyQueryDeviceProcessInfo, "queryDeviceProcessInfo", body, outDeviceList);
}

QString MesHttpPost::StationHeartbeat()
{
    if(m_processKey.isEmpty())
    {
        return  QString();
    }
    QJsonObject body;
    body["processKey"] = m_processKey;
    return SendMesPostRequest(replyStationHeartbeat, "stationHeartbeat", body);
}

QString MesHttpPost::ValidateDeviceUseFixture(bool& outNeedFixture)
{
    QJsonObject body;
    body["processKey"] = m_processKey;
    return SendMesPostRequest(replyValidateDeviceUseFixture, "validateDeviceUseFixture", body, outNeedFixture);
}

QString MesHttpPost::BindFixtureChannel( const QList<BindFixtureItem>& bindList,FixtureConsumeResult& outResult)
{
    QJsonObject body;
    body["processKey"] = m_processKey;
    QJsonArray listArr;
    for (auto &item : bindList)
    {
        QJsonObject obj;
        obj["number"] = item.number;
        obj["channel"] = item.channel;
        listArr.append(obj);
    }
    body["list"] = listArr;
    return SendMesPostRequest(replyBindFixtureChannel, "bindFixtureChannel", body, outResult);
}

QString MesHttpPost::ConsumeFixtureLife(const QList<FixtureConsumeItem>& consumeList, FixtureConsumeResult& outResult)
{
    QJsonObject body;
    body["processKey"] = m_processKey;
    QJsonArray listArr;
    for (auto &item : consumeList)
    {
        QJsonObject obj;
        obj["number"] = item.number;
        obj["consumption"] = item.consumption;
        listArr.append(obj);
    }
    body["list"] = listArr;
    return SendMesPostRequest(replyConsumeFixtureLife, "consumeFixtureLife", body, outResult);
}

QString MesHttpPost::ValidateStandardElementNumber(const QString& sn,bool& outStandard)
{
    QJsonObject body;
    body["number"] = sn;
    body["processKey"] = m_processKey;
    return SendMesPostRequest(replyValidateStandardElementNumber, "validateStandardElementNumber", body,outStandard);
}

QString MesHttpPost::ValidateNumber(const QString& sn,bool& outValidate)
{
    QJsonObject body;
    body["number"] = sn;
    body["mode"] = "";
    body["processKey"] = m_processKey;
    return SendMesPostRequest(replyValidateNumber, "validateNumber", body,outValidate);
}

QString MesHttpPost::SaveProcessOpResult(const QString& sn, int opResult, const QJsonArray& detailArr, QString& outMainId)
{
    QJsonObject body;
    body["processKey"] = m_processKey;
    body["number"] = sn;
    body["result"] = QString::number(opResult);
    body["detail"] = detailArr;
    body["mode"] = "";
    return SendMesPostRequest(replySaveProcessOpResult, "saveProcessOpResult", body, outMainId);
}

QString MesHttpPost::CompleteTask(const QString& sn, bool isSuccess, const QString& errCode, const QString& errInfo, bool bindMat, bool& outTaskResult)
{
    QJsonObject body;
    body["processKey"] = m_processKey;
    body["number"] = sn;
    body["isSuccess"] = isSuccess ? "1" : "0";
    body["errorCode"] = errCode;
    body["errorInfo"] = errInfo;
    body["bindMaterial"] = bindMat;
    body["mode"] = "";
    return SendMesPostRequest(replyCompleteTask, "completeTask", body, outTaskResult);
}

QString MesHttpPost::UploadSingle(const GatewayUploadHeader& headerInfo,const QString& fileName, const QByteArray& fileBin, const QString& filePid, const QString& nsId, UploadFileResp& outUploadResp)
{
    return SendMultipartUploadRequest(headerInfo,fileName, fileBin, filePid, nsId, outUploadResp);
}

QString MesHttpPost::SaveProductFilePath(const QString& sn, const QString& fileId, const QString& type, const QString& nsId, const QString& fileName,bool& outSave)
{
    QJsonObject body;
    body["processKey"] = m_processKey;
    body["number"] = sn;
    body["fileId"] = fileId;
    body["type"] = type;
    body["nameSpaceId"] = nsId;
    body["fileName"] = fileName;
    return SendMesPostRequest(replySaveProductFilePath, "saveProductFilePath", body,outSave);
}

bool MesHttpPost::ReplyJsonFromQueryWorkOrderInfo(QJsonObject & jsonObject, QVector<PdLineList>& outPdLineList)
{
    outPdLineList.clear();
    QString status = jsonObject["status"].toString();
    QString logtxt = "ReplyJsonFromQueryWorkOrderInfo:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());
    if (status != "PASS")
    {
        m_parseErrMap[QString::number((int)replyQueryWorkOrderInfo)] = jsonObject["message"].toString();
        return false;
    }
    if (!jsonObject.contains("content") || !jsonObject["content"].isObject())
    {
        m_parseErrMap[QString::number((int)replyQueryWorkOrderInfo)] = "工单接口无content返回";
        return false;
    }
    QJsonObject contentObj = jsonObject["content"].toObject();
    QJsonArray lineArr = contentObj["pdLineList"].toArray();
    for (auto lineVal : lineArr)
    {
        QJsonObject lineObj = lineVal.toObject();
        PdLineList lineItem;
        lineItem.id = lineObj["id"].toString();
        lineItem.name = lineObj["name"].toString();
        QJsonArray pdArr = lineObj["pdList"].toArray();
        for (auto pdVal : pdArr)
        {
            QJsonObject pdObj = pdVal.toObject();
            PdList pdItem;
            pdItem.id = pdObj["id"].toString();
            pdItem.name = pdObj["name"].toString();
            WorkOrderList woList;
            QJsonArray woArr = pdObj["workOrderList"].toArray();
            for (auto woVal : woArr)
            {
                QJsonObject woObj = woVal.toObject();
                WorkOrder woItem;
                woItem.workOrderId = woObj["id"].toString();
                woItem.name = woObj["name"].toString();
                woList.vecWorkOrder.push_back(woItem);
            }
            pdItem.workOrderList = woList;
            lineItem.vecPdList.push_back(pdItem);
        }
        outPdLineList.push_back(lineItem);
    }
    return true;
}

bool MesHttpPost::ReplyJsonFromUserInfoAuth(QJsonObject & jsonObject, AuthUserInfo& outUserInfo)
{
    outUserInfo = AuthUserInfo();
    QString status = jsonObject["status"].toString();
    QString logtxt = "ReplyJsonFromUserInfoAuth:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());

    if (status.compare("PASS", Qt::CaseInsensitive) != 0)
    {
        m_parseErrMap[QString::number((int)replyUserInfoAuth)] = jsonObject["message"].toString("用户认证失败，无错误描述");
        m_iovtoken.clear();
        return false;
    }

    QJsonValue contentVal = jsonObject["content"];
    if (!contentVal.isObject())
    {
        m_parseErrMap[QString::number((int)replyUserInfoAuth)] = "认证返回content非JSON对象，解析失败";
        m_iovtoken.clear();
        return false;
    }
    QJsonObject contentObj = contentVal.toObject();

    if (!contentObj.contains("IOVTOKEN"))
    {
        m_parseErrMap[QString::number((int)replyUserInfoAuth)] = "认证返回缺失IOVTOKEN字段";
        m_iovtoken.clear();
        return false;
    }
    m_iovtoken = contentObj["IOVTOKEN"].toString();

    if (contentObj.contains("userInfo") && contentObj["userInfo"].isObject())
    {
        QJsonObject userObj = contentObj["userInfo"].toObject();
        outUserInfo.deptName = userObj["deptName"].toString();
        outUserInfo.updateDate = userObj["updateDate"].toString();
        outUserInfo.mobilePhone = userObj["mobilePhone"].toString();
        outUserInfo.phone = userObj["phone"].toString();
        outUserInfo.name = userObj["name"].toString();
        outUserInfo.roleName = userObj["roleName"].toString();
        outUserInfo.id = userObj["id"].toString();
        outUserInfo.jobNumber = userObj["jobNumber"].toString();
        outUserInfo.email = userObj["email"].toString();
        outUserInfo.createDate = userObj["createDate"].toString();
        outUserInfo.username = userObj["username"].toString();
    }
    return true;
}

bool MesHttpPost::ReplyJsonFromConfigProcess(QJsonObject& jsonObject,QString& outProcessKey)
{
    QString status = jsonObject["status"].toString();
    QString logtxt = "ReplyJsonFromConfigProcess:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());
    if (status != "PASS")
    {
        m_parseErrMap[QString::number((int)replyConfigProcess)] = jsonObject["message"].toString();
        return false;
    }
    QJsonObject contentObj = jsonObject["content"].toObject();
    outProcessKey = contentObj["processKey"].toString();
    m_processKey = outProcessKey;
    return true;
}

bool MesHttpPost::ReplyJsonFromQueryProductionInfo(QJsonObject & jsonObject, ProductionInfo& outProdInfo)
{
    outProdInfo = ProductionInfo();
    QString logtxt = "ReplyJsonFromQueryProductionInfo:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());

    QString status = jsonObject["status"].toString().trimmed();
    if (status.compare("PASS", Qt::CaseInsensitive) != 0)
    {
        m_parseErrMap[QString::number((int)replyQueryProductionInfo)] = jsonObject["message"].toString("查询生产信息失败，无错误描述");
        return false;
    }

    QJsonValue contentVal = jsonObject["content"];
    if (!contentVal.isObject())
    {
        m_parseErrMap[QString::number((int)replyQueryProductionInfo)] = "返回content非JSON对象，解析失败";
        return false;
    }
    QJsonObject contentObj = contentVal.toObject();

    QJsonValue prodArrVal = contentObj["productionInfo"];
    if (!prodArrVal.isArray())
    {
        m_parseErrMap[QString::number((int)replyQueryProductionInfo)] = "content中缺失productionInfo数组";
        return false;
    }
    QJsonArray prodArr = prodArrVal.toArray();

    QList<ProductionInfoItem> itemList;
    for (const auto& itemVal : prodArr)
    {
        if (!itemVal.isObject()) continue;
        QJsonObject itemObj = itemVal.toObject();
        ProductionInfoItem item;
        item.name = itemObj["name"].toString();
        item.value = itemObj["value"].toString();
        item.key = itemObj["key"].toString();
        itemList.append(item);

        if (item.key == "pdPartNumber")
            outProdInfo.pdPartNumber = item.value;
        else if (item.key == "pdName")
            outProdInfo.pdName = item.value;
        else if (item.key == "pdCode")
            outProdInfo.pdCode = item.value;
        else if (item.key == "bomVersion")
            outProdInfo.bomVersion = item.value;
        else if (item.key == "workOrderNum")
            outProdInfo.workOrderNum = item.value;
        else if (item.key == "workOrderTotal")
            outProdInfo.workOrderTotal = item.value;
        else if (item.key == "batchNum")
            outProdInfo.batchNum = item.value;
        else if (item.key == "partNo")
            outProdInfo.partNo = item.value;
        else if (item.key == "processName")
            outProdInfo.processName = item.value;
        else if (item.key == "processSuccess")
            outProdInfo.processSuccess = item.value;
        else if (item.key == "processFail")
            outProdInfo.processFail = item.value;
    }
    outProdInfo.rawList = itemList;
    return true;
}

bool MesHttpPost::ReplyJsonFromValidateNumber(QJsonObject& jsonObject,bool& outValidate)
{
    QString status = jsonObject["status"].toString();
    QString logtxt = "ReplyJsonFromValidateNumber:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());
    if (status != "PASS")
    {
        m_parseErrMap[QString::number((int)replyValidateNumber)] = jsonObject["message"].toString();
        return false;
    }
    if (!jsonObject.contains("content") || !jsonObject["content"].isObject())
    {
        m_parseErrMap[QString::number((int)replyValidateNumber)] = "返回缺失content对象";
        return false;
    }
    QJsonObject contentObj = jsonObject["content"].toObject();
    if (contentObj["result"].toString().trimmed() == "1")
        outValidate = true;
    else
        outValidate = false;
    return true;
}

bool MesHttpPost::ReplyJsonFromSaveProcessOpResult(QJsonObject & jsonObject, QString& outMainId)
{
    outMainId.clear();
    QString status = jsonObject["status"].toString();
    QString logtxt = "ReplyJsonFromSaveProcessOpResult:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());
    if (status != "PASS")
    {
        m_parseErrMap[QString::number((int)replySaveProcessOpResult)] = jsonObject["message"].toString();
        return false;
    }
    if (jsonObject.contains("content"))
    {
        QJsonValue contentVal = jsonObject["content"];
        if (contentVal.isObject())
        {
            QJsonObject contentObj = contentVal.toObject();
            outMainId = contentObj["resultMainId"].toString();
        }
    }
    return true;
}

bool MesHttpPost::ReplyJsonFromCompleteTask(QJsonObject & jsonObject, bool& outTaskResult)
{
    outTaskResult = false;
    QString status = jsonObject["status"].toString();
    QString logtxt = "ReplyJsonFromCompleteTask:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());
    if (status != "PASS")
    {
        m_parseErrMap[QString::number((int)replyCompleteTask)] = jsonObject["message"].toString();
        return false;
    }
    if (jsonObject.contains("content"))
    {
        QJsonValue contentVal = jsonObject["content"];
        if (contentVal.isObject())
        {
            QJsonObject contentObj = contentVal.toObject();
            outTaskResult = contentObj["result"].toBool();
        }
    }
    return true;
}

bool MesHttpPost::ReplyJsonFromQueryDeviceProcessInfo(QJsonObject & jsonObject, QList<DeviceProcessItem>& outDeviceList)
{
    outDeviceList.clear();
    QString logtxt = "ReplyJsonFromQueryDeviceProcessInfo:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());

    QString status = jsonObject["status"].toString().trimmed();
    if (status.compare("PASS", Qt::CaseInsensitive) != 0)
    {
        m_parseErrMap[QString::number((int)replyQueryDeviceProcessInfo)] = jsonObject["message"].toString("获取工单工序信息失败，无错误描述");
        return false;
    }

    QJsonValue contentVal = jsonObject["content"];
    if (!contentVal.isObject())
    {
        m_parseErrMap[QString::number((int)replyQueryDeviceProcessInfo)] = "返回content节点不是JSON对象，解析失败";
        return false;
    }
    QJsonObject contentObj = contentVal.toObject();

    QJsonValue listVal = contentObj["processList"];
    if (!listVal.isArray())
    {
        m_parseErrMap[QString::number((int)replyQueryDeviceProcessInfo)] = "content内缺失processList工序数组";
        return false;
    }
    QJsonArray processArr = listVal.toArray();

    for (const QJsonValue& itemVal : processArr)
    {
        if (!itemVal.isObject())
            continue;
        QJsonObject itemObj = itemVal.toObject();
        DeviceProcessItem item;
        item.workstationCode = itemObj["workstationCode"].toString();
        item.workCenterCode = itemObj["workCenterCode"].toString();
        item.processId = itemObj["processId"].toString();
        item.processName = itemObj["processName"].toString();
        item.workstationName = itemObj["workstationName"].toString();
        item.deviceCode = itemObj["deviceCode"].toString();
        item.workCenterName = itemObj["workCenterName"].toString();
        item.deviceName = itemObj["deviceName"].toString();
        outDeviceList.append(item);
    }
    return true;
}

bool MesHttpPost::ReplyJsonFromStationHeartbeat(QJsonObject & jsonObject)
{
    QString logtxt = "ReplyJsonFromStationHeartbeat:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());

    QString status = jsonObject["status"].toString().trimmed();
    if (status.compare("PASS", Qt::CaseInsensitive) != 0)
    {
        m_parseErrMap[QString::number((int)replyStationHeartbeat)] = jsonObject["message"].toString("心跳上报失败，无错误描述");
        return false;
    }

    QJsonValue contentVal = jsonObject["content"];
    if (!contentVal.isBool())
    {
        m_parseErrMap[QString::number((int)replyStationHeartbeat)] = "心跳返回content非布尔值，解析异常";
        return false;
    }
    bool bHeartOk = contentVal.toBool();
    if (!bHeartOk)
    {
        m_parseErrMap[QString::number((int)replyStationHeartbeat)] = "MES返回心跳中断，通信异常";
        return false;
    }
    return true;
}

bool MesHttpPost::ReplyJsonFromValidateDeviceUseFixture(QJsonObject & jsonObject, bool& outNeedFixture)
{
    outNeedFixture = false;
    QString status = jsonObject["status"].toString();
    QString logtxt = "ReplyJsonFromValidateDeviceUseFixture:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());
    if (status != "PASS")
    {
        m_parseErrMap[QString::number((int)replyValidateDeviceUseFixture)] = jsonObject["message"].toString();
        return false;
    }
    QJsonValue contentVal = jsonObject["content"];
    if (!contentVal.isBool())
    {
        m_parseErrMap[QString::number((int)replyValidateDeviceUseFixture)] = "返回content不是布尔值";
        return false;
    }
    outNeedFixture = jsonObject["content"].toBool();
    return true;
}

bool MesHttpPost::ReplyJsonFromBindFixtureChannel(QJsonObject & jsonObject, FixtureConsumeResult& outResult)
{
    // 重置结构体默认值
    outResult = FixtureConsumeResult();
    QString logtxt = "ReplyJsonFromBindFixtureChannel:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());

    QString status = jsonObject["status"].toString();
    if (status.compare("success", Qt::CaseInsensitive) != 0 && status.compare("PASS", Qt::CaseInsensitive) != 0)
    {
        m_parseErrMap[QString::number((int)replyBindFixtureChannel)] = jsonObject["message"].toString();
        return false;
    }

    QJsonValue contentVal = jsonObject["content"];
    if (!contentVal.isArray())
    {
        m_parseErrMap[QString::number((int)replyBindFixtureChannel)] = "bindFixtureChannel返回content非数组";
        return false;
    }
    QJsonArray contentArr = contentVal.toArray();
    for (auto itemVal : contentArr)
    {
        if (!itemVal.isObject())
            continue;
        QJsonObject itemObj = itemVal.toObject();
        FixtureLifeItem lifeItem;
        lifeItem.number = itemObj["number"].toString();
        lifeItem.totalLife = itemObj["totalLife"].toDouble();
        lifeItem.residueLife = itemObj["residueLife"].toDouble();
        // 文档未标注consumption，若无则默认0
        lifeItem.consumption = itemObj.contains("consumption") ? itemObj["consumption"].toInt() : 0;
        outResult.fixtureList.append(lifeItem);
    }
    outResult.warnFlag = false;
    outResult.warnMessage = "";
    return true;
}

bool MesHttpPost::ReplyJsonFromConsumeFixtureLife(QJsonObject & jsonObject, FixtureConsumeResult& outResult)
{
    outResult = FixtureConsumeResult();
    QString status = jsonObject["status"].toString();
    QString logtxt = "ReplyJsonFromConsumeFixtureLife:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());

    if (status != "PASS")
    {
        m_parseErrMap[QString::number((int)replyConsumeFixtureLife)] = jsonObject["message"].toString("治具扣减失败，无错误描述");
        return false;
    }

    QJsonValue contentVal = jsonObject["content"];
    if (!contentVal.isObject())
    {
        m_parseErrMap[QString::number((int)replyConsumeFixtureLife)] = "consumeFixtureLife返回content非对象";
        return false;
    }
    QJsonObject contentObj = contentVal.toObject();

    // 预警标识与预警消息
    outResult.warnFlag = contentObj["warnFlag"].toBool();
    outResult.warnMessage = contentObj["warnMessage"].toString();

    // 治具列表数组
    QJsonValue listVal = contentObj["fixtureList"];
    if (!listVal.isArray())
    {
        m_parseErrMap[QString::number((int)replyConsumeFixtureLife)] = "content内缺失fixtureList数组";
        return false;
    }
    QJsonArray listArr = listVal.toArray();
    for (auto itemVal : listArr)
    {
        if (!itemVal.isObject()) continue;
        QJsonObject itemObj = itemVal.toObject();
        FixtureLifeItem item;
        item.number = itemObj["number"].toString();
        item.totalLife = itemObj["totalLife"].toDouble();
        item.residueLife = itemObj["residueLife"].toDouble();
        item.consumption = itemObj["consumption"].toInt();
        outResult.fixtureList.append(item);
    }
    return true;
}

bool MesHttpPost::ReplyJsonFromValidateStandardElementNumber(QJsonObject& jsonObject, bool& outStandard)
{
    outStandard = false;
    QString status = jsonObject["status"].toString();
    QString logtxt = "ReplyJsonFromValidateStandardElementNumber:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());

    if (status != "PASS")
    {
        m_parseErrMap[QString::number((int)replyValidateStandardElementNumber)] = jsonObject["message"].toString("标准件条码校验失败");
        return false;
    }
    if (!jsonObject.contains("content") || !jsonObject["content"].isObject())
    {
        m_parseErrMap[QString::number((int)replyValidateStandardElementNumber)] = "返回缺失content对象";
        return false;
    }
    QJsonObject contentObj = jsonObject["content"].toObject();
    if (contentObj["type"].toString().trimmed() == "1")
        outStandard = true;
    else
        outStandard = false;
    return true;
}

bool MesHttpPost::ReplyJsonFromUploadSingle(QJsonObject & jsonObject, UploadFileResp& outUploadResp)
{
    outUploadResp = UploadFileResp{};
    QString status = jsonObject["status"].toString();
    QString logtxt = "ReplyJsonFromUploadSingle:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());

    if (status != "success")
    {
        m_parseErrMap[QString::number((int)replyUploadSingle)] = jsonObject["message"].toString("文件上传网关接口调用失败");
        return false;
    }

    QJsonValue contentVal = jsonObject["content"];
    if (!contentVal.isObject())
    {
        m_parseErrMap[QString::number((int)replyUploadSingle)] = "上传返回content节点不是JSON对象";
        return false;
    }
    QJsonObject contentObj = contentVal.toObject();
    outUploadResp.fileId = contentObj["fileId"].toString();
    outUploadResp.fullPath = contentObj["fullPath"].toString();
    outUploadResp.filePId = contentObj["filePId"].toString();
    outUploadResp.contentMsg = contentObj["message"].toString();
    outUploadResp.contentStatus = contentObj["status"].toString();
    outUploadResp.version = contentObj["version"].toString();
    // createDirs 原始数组字符串暂存不解析
    QJsonValue dirsVal = contentObj["createDirs"];
    if (dirsVal.isArray())
    {
        QJsonDocument dirDoc(dirsVal.toArray());
        outUploadResp.createDirsRaw = dirDoc.toJson(QJsonDocument::Compact);
    }
    else
    {
        outUploadResp.createDirsRaw = "";
    }
    return true;
}

bool MesHttpPost::ReplyJsonFromSaveProductFilePath(QJsonObject& jsonObject, bool& outSave)
{

    outSave = false;
    QString logtxt = "ReplyJsonFromSaveProductFilePath:\n" + QJsonDocument(jsonObject).toJson();
    SaveTestLog(logtxt.toLocal8Bit());

    QString status = jsonObject["status"].toString();
    if (status != "PASS")
    {
        m_parseErrMap[QString::number((int)(replySaveProductFilePath))] = jsonObject["message"].toString();
        return false;
    }
    QJsonValue contentVal = jsonObject["content"];
    if (!contentVal.isBool())
    {
        m_parseErrMap[QString::number((int)(replySaveProductFilePath))] = "返回content不是布尔值";
        return false;
    }
    outSave = contentVal.toBool();
    return true;
}
