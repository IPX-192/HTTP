#pragma execution_character_set("utf-8")
#include "ProductionTestManager.h"
#include <QFile>
#include <QDebug>
#include <QMessageBox>

ProductionTestManager::ProductionTestManager(QObject *parent)
    : QObject(parent)
{
    // 全局上下文清空
    m_workOrderId.clear();
    m_processId.clear();
    m_processKey.clear();
    // 工位1上下文清空
    m_station1_Key.clear();
    m_station1_WorkOrderId.clear();
    m_station1_ProcessId.clear();
    // 工位2上下文清空
    m_station2_Key.clear();
    m_station2_WorkOrderId.clear();
    m_station2_ProcessId.clear();

    QString str1;
    QString str2;
    RunFullAutoProduction(str1,true,str2);
}

// 根据工位获取独立processKey
QString ProductionTestManager::GetStationProcessKey(StationId station)
{
    if (station == STATION_1)
        return m_station1_Key.isEmpty() ? m_processKey : m_station1_Key;
    else
        return m_station2_Key.isEmpty() ? m_processKey : m_station2_Key;
}

// 带工位标识日志打印
void ProductionTestManager::EmitLog(StationId station, const QString& apiName, const QString& reqJson, const QString& respJson, bool success)
{
    QString stationTag = station == STATION_1 ? "工位1" : "工位2";
    qDebug()<< stationTag << "apiName:" << apiName<< " reqJson:" << reqJson<< " respJson:" << respJson << " isOK:" <<success;
}

// 1. 治具寿命消耗
QString ProductionTestManager::ConsumeFixtureLife(StationId station, const QString& fixtureSn, int consumeNum, FixtureConsumeResult& outResult, QString& errMsg)
{
    errMsg.clear();
    QString stationKey = GetStationProcessKey(station);
    QString reqJson = QString("fixtureSn:%1, consumeNum:%2, processKey:%3").arg(fixtureSn, QString::number(consumeNum), stationKey);

    if (m_localTestMode)
    {
        MockFixtureConsumeData(outResult);
        EmitLog(station, "ConsumeFixtureLife", reqJson, "本地模拟：治具消耗成功", true);
        return "";
    }

    QList<FixtureConsumeItem> list;
    FixtureConsumeItem item;
    item.number = fixtureSn;
    item.consumption = consumeNum;
    list.append(item);
    //接口测试：治具消耗接口
    QString retErr = MesHttpPost::Instance()->ConsumeFixtureLife(list, outResult);
    if (!retErr.isEmpty())
    {
        EmitLog(station, "ConsumeFixtureLife", reqJson, "接口失败:" + retErr, false);
        errMsg = retErr;
        emit SignalErrorMsg(QString("工位%1治具寿命消耗失败：%2").arg(station).arg(retErr));
        return retErr;
    }
    QString respLog = QString("剩余寿命：%1").arg(outResult.fixtureList.empty() ? "无" : QString::number(outResult.fixtureList[0].residueLife));
    EmitLog(station, "ConsumeFixtureLife", reqJson, respLog, true);
    return "";
}

// 2. 标准件条码校验
QString ProductionTestManager::ValidateStandardElementNumber(StationId station, const QString& barcode, QString& errMsg)
{
    errMsg.clear();
    QString stationKey = GetStationProcessKey(station);
    QString reqJson = QString("barcode:%1, processKey:%2").arg(barcode, stationKey);

    if (m_localTestMode)
    {
        EmitLog(station, "ValidateStandardElementNumber", reqJson, "本地模拟：标准件校验通过", true);
        return "";
    }

    //接口测试：条码判断
    bool ok;
    QString retErr = MesHttpPost::Instance()->ValidateStandardElementNumber(barcode,ok);
    if (!retErr.isEmpty())
    {
        EmitLog(station, "ValidateStandardElementNumber", reqJson, "接口失败:" + retErr, false);
        errMsg = retErr;
        emit SignalErrorMsg(QString("工位%1标准件条码校验失败：%2").arg(station).arg(retErr));
        return retErr;
    }
    EmitLog(station, "ValidateStandardElementNumber", reqJson, "校验通过", true);
    return "";
}

// 3. 生产条码校验
QString ProductionTestManager::ValidateNumber(StationId station, const QString& barcode, QString& errMsg)
{
    errMsg.clear();
    QString stationKey = GetStationProcessKey(station);
    QString reqJson = QString("barcode:%1, processKey:%2").arg(barcode, stationKey);

    if (m_localTestMode)
    {
        EmitLog(station, "ValidateNumber", reqJson, "本地模拟：条码校验通过", true);
        return "";
    }

    //接口测试，条码校验
    bool ok;
    QString retErr = MesHttpPost::Instance()->ValidateNumber(barcode,ok);
    if (!retErr.isEmpty())
    {
        EmitLog(station, "ValidateNumber", reqJson, "接口失败:" + retErr, false);
        errMsg = retErr;
        emit SignalErrorMsg(QString("工位%1生产条码校验失败：%2").arg(station).arg(retErr));
        return retErr;
    }
    EmitLog(station, "ValidateNumber", reqJson, "校验通过", true);
    return "";
}

// 4. 保存工序操作结果
QString ProductionTestManager::SaveProcessOpResult(StationId station, const QString& barcode, int opResult, const QJsonArray& detailArr, QString& outTaskMainId, QString& errMsg)
{
    errMsg.clear();
    outTaskMainId.clear();
    QString reqJson = QString("barcode:%1, result:%2").arg(barcode, QString::number(opResult));

    if (m_localTestMode)
    {
        outTaskMainId = "TASK_" + QDateTime::currentDateTime().toString("yyyyMMddHHmmss");
        EmitLog(station, "SaveProcessOpResult", reqJson, "本地模拟：工序结果保存成功, mainId=" + outTaskMainId, true);
        return "";
    }

    //接口测试：保存工序操作
    QString retErr = MesHttpPost::Instance()->SaveProcessOpResult(barcode, opResult, detailArr, outTaskMainId);
    if (!retErr.isEmpty())
    {
        EmitLog(station, "SaveProcessOpResult", reqJson, "接口失败:" + retErr, false);
        errMsg = retErr;
        emit SignalErrorMsg(QString("工位%1工序结果保存失败：%2").arg(station).arg(retErr));
        return retErr;
    }
    EmitLog(station, "SaveProcessOpResult", reqJson, "保存成功，任务ID：" + outTaskMainId, true);
    return "";
}

// 5. 过站完成上报
QString ProductionTestManager::CompleteTask(StationId station, const QString& barcode, bool isSuccess, const QString& errCode, const QString& errInfo, bool bindMat, bool& outTaskResult, QString& errMsg)
{
    errMsg.clear();
    QString reqJson = QString("barcode:%1, success:%2").arg(barcode, isSuccess ? "1" : "0");

    if (m_localTestMode)
    {
        outTaskResult = true;
        EmitLog(station, "CompleteTask", reqJson, "本地模拟：过站上报成功", true);
        return "";
    }

    //接口测试：完成上报
    QString retErr = MesHttpPost::Instance()->CompleteTask(barcode, isSuccess, errCode, errInfo, bindMat, outTaskResult);
    if (!retErr.isEmpty())
    {
        EmitLog(station, "CompleteTask", reqJson, "接口失败:" + retErr, false);
        errMsg = retErr;
        emit SignalErrorMsg(QString("工位%1过站上报失败：%2").arg(station).arg(retErr));
        return retErr;
    }
    EmitLog(station, "CompleteTask", reqJson, "过站上报完成", true);
    return "";
}

// 6. 文件上传
QString ProductionTestManager::UploadSingle(StationId station, const QString& localFilePath, const QString& filePid, const QString& nsId, UploadFileResp& outUploadResp, QString& errMsg)
{
    errMsg.clear();
    outUploadResp = UploadFileResp();
    QString reqJson = QString("localFile:%1, filePid:%2").arg(localFilePath, filePid);

    if (m_localTestMode)
    {
        MockFileUpload(localFilePath, outUploadResp);
        EmitLog(station, "UploadSingle", reqJson, "本地模拟：文件上传成功 fileId=" + outUploadResp.fileId, true);
        return "";
    }

    QFile file(localFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        errMsg = "本地文件不存在或无法读取";
        EmitLog(station, "UploadSingle", reqJson, errMsg, false);
        return errMsg;
    }
    QByteArray fileBin = file.readAll();
    QString fileName = localFilePath.split("/").last();
    file.close();

    //组装请求头

    GatewayUploadHeader header;
    QString retErr = MesHttpPost::Instance()->UploadSingle(header,fileName, fileBin, filePid, nsId, outUploadResp);
    if (!retErr.isEmpty())
    {
        EmitLog(station, "UploadSingle", reqJson, "接口失败:" + retErr, false);
        errMsg = retErr;
        emit SignalErrorMsg(QString("工位%1文件上传失败：%2").arg(station).arg(retErr));
        return retErr;
    }
    EmitLog(station, "UploadSingle", reqJson, "上传成功 fileId=" + outUploadResp.fileId, true);
    return "";
}

// 7. 绑定条码与文件路径
QString ProductionTestManager::SaveProductFilePath(StationId station, const QString& barcode, const QString& fileId, const QString& type, const QString& nsId, const QString& fileName, QString& errMsg)
{
    errMsg.clear();
    QString reqJson = QString("barcode:%1, fileId:%2").arg(barcode, fileId);

    if (m_localTestMode)
    {
        EmitLog(station, "SaveProductFilePath", reqJson, "本地模拟：条码文件绑定成功", true);
        return "";
    }

    //接口测试：保存产品路径

    bool ok;
    QString retErr = MesHttpPost::Instance()->SaveProductFilePath(barcode, fileId, type, nsId, fileName,ok);
    if (!retErr.isEmpty())
    {
        EmitLog(station, "SaveProductFilePath", reqJson, "接口失败:" + retErr, false);
        errMsg = retErr;
        emit SignalErrorMsg(QString("工位%1条码文件绑定失败：%2").arg(station).arg(retErr));
        return retErr;
    }
    EmitLog(station, "SaveProductFilePath", reqJson, "绑定成功", true);
    return "";
}

bool ProductionTestManager::RunStationSingleFlow(StationId station, const QString& barcode, QString& errMsg)
{
    errMsg.clear();
    // 1. 工位条码校验 ValidateNumber
    QString valErr;
    QString retVal = ValidateNumber(station, barcode, valErr);
    if (!valErr.isEmpty())
    {
        errMsg = valErr;
        return false;
    }

    // 2. 保存工序操作结果 SaveProcessOpResult
    QJsonArray detailArr;
    QString taskMainId;
    QString saveErr;
    QString retSave = SaveProcessOpResult(station, barcode, 1, detailArr, taskMainId, saveErr);
    if (!saveErr.isEmpty())
    {
        errMsg = saveErr;
        return false;
    }

    // 3. 过站完成 CompleteTask
    bool taskResult;
    QString taskErr;
    QString retTask = CompleteTask(station, barcode, true, "", "", false, taskResult, taskErr);
    if (!taskErr.isEmpty())
    {
        errMsg = taskErr;
        return false;
    }

    // 4. 文件上传+绑定 SaveProductFilePath + UploadSingle
    UploadFileResp uploadResp;
    QString uploadErr;
    QString fileTag = station == STATION_1 ? "ST1" : "ST2";
    QString retUpload = UploadSingle(station, "./station_result.csv", "FILEPID_" + fileTag, "NS_001", uploadResp, uploadErr);
    if (!uploadErr.isEmpty())
    {
        errMsg = uploadErr;
        return false;
    }

    QString bindErr;
    QString retBind = SaveProductFilePath(station, barcode, uploadResp.fileId, fileTag, "NS_001", "station_result.csv", bindErr);
    if (!bindErr.isEmpty())
    {
        errMsg = bindErr;
        return false;
    }
    return true;
}

bool ProductionTestManager::RunFullAutoProduction(const QString& scanBarcode, bool enableFixtureControl, QString& errMsg)
{
    errMsg.clear();
    // 1. 治具管控分支
    if (enableFixtureControl)
    {
        QString fixtureErr;
        FixtureConsumeResult fixtureRes;
        QString ret = ConsumeFixtureLife(STATION_1, "FIX_001", 1, fixtureRes, fixtureErr);
        if (!fixtureErr.isEmpty())
        {
            errMsg = "治具寿命消耗失败：" + fixtureErr;
            return false;
        }
    }

    // 2. 标准件判断
    QString stdErr;
    QString retStd = ValidateStandardElementNumber(STATION_1, scanBarcode, stdErr);
    if (!stdErr.isEmpty())
    {
        errMsg = "标准件校验失败：" + stdErr;
        return false;
    }

    // 本地模拟分支分流：奇数条码=直通，偶数=正常生产
    if (m_localTestMode)
    {
        int num = scanBarcode.split("-").last().toInt();
        if (num % 2 == 0)
            return RunStandardPartDirectOut(scanBarcode, errMsg);
        else
            return RunNormalProduceProcess(scanBarcode, errMsg);
    }
    return RunNormalProduceProcess(scanBarcode, errMsg);
}

// 标准件直通流出流程
bool ProductionTestManager::RunStandardPartDirectOut(const QString& stdBarcode, QString& errMsg)
{
    errMsg.clear();
    QString valErr;
    QString ret = ValidateNumber(STATION_1, stdBarcode, valErr);
    if (!valErr.isEmpty())
    {
        errMsg = "标准件工位校验失败：" + valErr;
        return false;
    }

    QJsonArray detailArr;
    QString taskMainId;
    QString saveErr;
    QString saveRet = SaveProcessOpResult(STATION_1, stdBarcode, 1, detailArr, taskMainId, saveErr);
    if (!saveErr.isEmpty())
    {
        errMsg = "保存工序结果失败：" + saveErr;
        return false;
    }

    bool taskResult;
    QString taskErr;
    QString taskRet = CompleteTask(STATION_1, stdBarcode, true, "", "", false, taskResult, taskErr);
    if (!taskErr.isEmpty())
    {
        errMsg = "过站上报失败：" + taskErr;
        return false;
    }

    UploadFileResp uploadResp;
    QString uploadErr;
    QString uploadRet = UploadSingle(STATION_1, "./test_std.csv", "FILE_PID_001", "NS_001", uploadResp, uploadErr);
    if (!uploadErr.isEmpty())
    {
        errMsg = "文件上传失败：" + uploadErr;
        return false;
    }

    QString bindErr;
    QString bindRet = SaveProductFilePath(STATION_1, stdBarcode, uploadResp.fileId, "STD", "NS_001", "test_std.csv", bindErr);
    if (!bindErr.isEmpty())
    {
        errMsg = "文件绑定失败：" + bindErr;
        return false;
    }
    return true;
}

// 正常生产加工流程
bool ProductionTestManager::RunNormalProduceProcess(const QString& prodBarcode, QString& errMsg)
{
    errMsg.clear();
    QString valErr;
    QString ret = ValidateNumber(STATION_1, prodBarcode, valErr);
    if (!valErr.isEmpty())
    {
        errMsg = "生产条码校验失败：" + valErr;
        return false;
    }

    QJsonArray detailArr;
    QString taskMainId;
    QString saveErr;
    QString saveRet = SaveProcessOpResult(STATION_1, prodBarcode, 1, detailArr, taskMainId, saveErr);
    if (!saveErr.isEmpty())
    {
        errMsg = "保存工序结果失败：" + saveErr;
        return false;
    }

    bool taskResult;
    QString taskErr;
    QString taskRet = CompleteTask(STATION_1, prodBarcode, true, "", "", true, taskResult, taskErr);
    if (!taskErr.isEmpty())
    {
        errMsg = "过站上报失败：" + taskErr;
        return false;
    }

    UploadFileResp uploadResp;
    QString uploadErr;
    QString uploadRet = UploadSingle(STATION_1, "./prod_result.csv", "FILE_PID_002", "NS_001", uploadResp, uploadErr);
    if (!uploadErr.isEmpty())
    {
        errMsg = "文件上传失败：" + uploadErr;
        return false;
    }

    QString bindErr;
    QString bindRet = SaveProductFilePath(STATION_1, prodBarcode, uploadResp.fileId, "PROD", "NS_001", "prod_result.csv", bindErr);
    if (!bindErr.isEmpty())
    {
        errMsg = "文件绑定失败：" + bindErr;
        return false;
    }
    return true;
}

void ProductionTestManager::MockFixtureConsumeData(FixtureConsumeResult& out)
{
    out = FixtureConsumeResult();
    out.warnFlag = false;
    out.warnMessage = "";
    FixtureLifeItem item;
    item.number = "FIX_001";
    item.totalLife = 1000;
    item.residueLife = 899;
    item.consumption = 1;
    out.fixtureList.append(item);
}

bool ProductionTestManager::MockFileUpload(const QString& fileName, UploadFileResp& outUploadResp)
{
    QString timeStr = QDateTime::currentDateTime().toString("yyyyMMddHHmmss");
    outUploadResp.fileId = "FILE_" + timeStr;
    outUploadResp.filePId = "FILE_PID_001";
    outUploadResp.fullPath = "./upload_cache/" + fileName;
    return true;
}
