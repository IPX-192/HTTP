#pragma execution_character_set("utf-8")
#include "MesHttpPost_1.h"
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include "CalibTcpTrans.h"
#include <fstream>
#include <QFile>
#include <QTextCodec>
#include <QApplication>

QString  g_sn = "";

MesHttpPost*MesHttpPost::m_instance = nullptr;
MesHttpPost::MesHttpPost(QObject *parent) : QObject(parent)
{
    // m_httpPost = new QNetworkAccessManager();
     connect(&m_httpPost, SIGNAL(finished(QNetworkReply*)), this, SLOT(slotPostFinished(QNetworkReply*)));
     // 下边这行也很重要，要发送json格式的数据必须要在header里设置，不然不会成功的
     m_netRequest.setHeader(QNetworkRequest::ContentTypeHeader,QVariant("application/json"));

	 //QByteArray bytes;
	 //QFile file("test.dat");
	 //if (file.open(QIODevice::ReadOnly)) {
	 //	bytes = file.readAll();
	 //}
	 //file.close();

	 ////数据含中文，先转换为QString
	 ////QString strData = QString::fromLocal8Bit(bytes.data());
	 //QString strData = QString::fromUtf8(bytes);
	 //qDebug() << "data QString:" << strData;
	 //// 将收到的body部分解析为json格式
	 //QJsonParseError jsonError;
	 //QJsonDocument jsonDocument = QJsonDocument::fromJson(strData.toUtf8(), &jsonError);
	 //if (jsonError.error != QJsonParseError::NoError);

	 //if (!ReplyJsonFromQueryWorkOrder(jsonDocument.object())) {

	 //}
}

MesHttpPost::~MesHttpPost()
{
	
}

void MesHttpPost::SetMesIpInfo(QString ip, QString port)
{
	MesServerIp = ip;
	MesServerPort = port;
}

void MesHttpPost::QueryWorkOrderInfo()
{
	//QByteArray bytes;
	//QFile file("test.dat");
	//if (file.open(QIODevice::ReadOnly)) {
	//	bytes = file.readAll();
	//}
	//file.close();

	////数据含中文，先转换为QString
	////QString strData = QString::fromLocal8Bit(bytes.data());
	//QString strData = QString::fromUtf8(bytes);
	//qDebug() << "data QString:" << strData;
	//// 将收到的body部分解析为json格式
	//QJsonParseError jsonError;
	//QJsonDocument jsonDocument = QJsonDocument::fromJson(strData.toUtf8(), &jsonError);
	//if (jsonError.error != QJsonParseError::NoError);

	//if (!ReplyJsonFromQueryWorkOrder(jsonDocument.object())) {

	//}
	//emit emitQueryWorkOrderResult(false, "");
	//return;
     QString url="http://";
     url+=MesServerIp;
     url+=":";
     url+=MesServerPort;
     url+="/mes/service/hirain/authToken/queryWorkOrderInfo";
     m_netRequest.setUrl(QUrl(url));

     QString localHostName = QHostInfo::localHostName();
     QHostInfo info = QHostInfo::fromName(localHostName);
     foreach(QHostAddress address,info.addresses()){
         if(address.protocol() == QAbstractSocket::IPv4Protocol) {
             m_ipInfo=address.toString();
             qDebug() << address.toString();
         }
     }
     QJsonObject object;
     object.insert("deviceIp", m_ipInfo);
     QJsonDocument document=QJsonDocument(object);

     // 这里要将json格式的数据转换为QByteArray才行
     QByteArray post_data = document.toJson();
	 m_replyQueryWorkOrder = m_httpPost.post(m_netRequest, post_data);
	 m_replyStatus = replyQueryWorkOrder;
	 QString logtxt = "QueryWorkOrder:\n" + post_data;
	 SaveTestLog(logtxt.toLocal8Bit());
}

void MesHttpPost::QueryDeviceProcessInfo(QString workOrderId)
{
    QString url="http://";
    url+=MesServerIp;
    url+=":";
    url+=MesServerPort;
    url+="/mes/service/hirain/authToken/queryDeviceProcessInfo";
    m_netRequest.setUrl(QUrl(url));

    QJsonObject object;
    object.insert("deviceIp", m_ipInfo);
	object.insert("workOrderId", workOrderId);
    QJsonDocument document=QJsonDocument(object);

    // 这里要将json格式的数据转换为QByteArray才行
    QByteArray post_data = document.toJson();
	qDebug() << "查询工序：" << QString::fromLocal8Bit(post_data);
	m_replyQueryDeviceProcess = m_httpPost.post(m_netRequest, post_data);
	m_replyStatus = replyQueryDeviceProcess;
	QString logtxt = "QueryDeviceProcess:\n" + post_data;
	SaveTestLog(logtxt.toLocal8Bit());
}

void MesHttpPost::CheckUserInfoAuth(QString user, QString passwd,QString workOrderId,QString processId)
{
	m_processId = processId;
	QString url = "http://";
	url += MesServerIp;
	url += ":";
	url += MesServerPort;
	url += "/mes/service/hirain/authToken/userInfoAuth";
	m_netRequest.setUrl(QUrl(url));

	QJsonObject object;
	object.insert("username", user);
	object.insert("password", passwd);
	object.insert("workOrderId", workOrderId);
	object.insert("processId", processId);
	object.insert("deviceIp", m_ipInfo);
	//object.insert("scanInfo", "");
	object.insert("type", "device");
	
	QJsonDocument document = QJsonDocument(object);
	// 这里要将json格式的数据转换为QByteArray才行
	QByteArray post_data = document.toJson();

	qDebug()<< "用户校验：" << QString::fromLocal8Bit(post_data);
	m_replyCheckUserInfoAuth = m_httpPost.post(m_netRequest, post_data);
	m_replyStatus = replyCheckUserInfoAuth;
	QString logtxt = "CheckUserInfoAuth:\n" + post_data;
	SaveTestLog(logtxt.toLocal8Bit());
}

void MesHttpPost::ConfigProcess()
{
	QString url = "http://";
	url += MesServerIp;
	url += ":";
	url += MesServerPort;
	url += "/mes/service/hirain/configProcess";
	m_netRequest.setUrl(QUrl(url));
	m_netRequest.setRawHeader("iovtoken", m_iovtoken.toStdString().data());

	QJsonObject object;
	object.insert("processId", m_processId);

	QJsonDocument document = QJsonDocument(object);
	// 这里要将json格式的数据转换为QByteArray才行
	QByteArray post_data = document.toJson();
	qDebug() << "设置工序：" << QString::fromLocal8Bit(post_data);
	m_replyConfigProcess = m_httpPost.post(m_netRequest, post_data);
	m_replyStatus = replyConfigProcess;
	QString logtxt = "ConfigProcess:\n" + post_data;
	SaveTestLog(logtxt.toLocal8Bit());
}

void MesHttpPost::CheckValidateNumber(QString sn)
{
	QString url = "http://";
	url += MesServerIp;
	url += ":";
	url += MesServerPort;
	url += "/mes/service/hirain/validateNumber";
	m_netRequest.setUrl(QUrl(url));
	m_netRequest.setRawHeader("iovtoken", m_iovtoken.toStdString().data());

	QJsonObject object;
	object.insert("number", sn);
	object.insert("processKey", m_processKey);
	object.insert("mode", "");
	g_sn = sn;
	QJsonDocument document = QJsonDocument(object);
	// 这里要将json格式的数据转换为QByteArray才行
	QByteArray post_data = document.toJson();
	//qDebug() << "校验SN：" << QString::fromLocal8Bit(post_data);
	m_replyCheckValidateNumber = m_httpPost.post(m_netRequest, post_data);
	m_replyStatus = replyCheckValidateNumber;
	QString logtxt = "CheckValidateNumber:\n" + post_data;
	SaveTestLog(logtxt.toLocal8Bit());
}

void MesHttpPost::SaveCalibrationResultFileContent(QByteArray&dataArray)
{
	QString url = "http://";
	url += MesServerIp;
	url += ":";
	url += MesServerPort;
	url += "/mes/service/hirain/saveCalibrationResultFileContent";
	m_netRequest.setUrl(QUrl(url));
	m_netRequest.setRawHeader("iovtoken", m_iovtoken.toStdString().data());
	//qDebug() << "上传标定数据：" << QString::fromLocal8Bit(dataArray);
	m_replySaveCalibrationResult = m_httpPost.post(m_netRequest, dataArray);
	m_replyStatus = replySaveCalibrationResult;
	QString logtxt = "SaveCalibrationResult:\n" + dataArray;
	SaveTestLog(logtxt.toLocal8Bit());
}

void MesHttpPost::SaveProductTestResult(QByteArray&dataArray)
{
	QString url = "http://";
	url += MesServerIp;
	url += ":";
	url += MesServerPort;
	url += "/mes/service/hirain/saveProductTestResult";
	m_netRequest.setUrl(QUrl(url));
	m_netRequest.setRawHeader("iovtoken", m_iovtoken.toStdString().data());

	QJsonObject object = QJsonDocument::fromJson(dataArray).object();
	object.insert("processKey", m_processKey);
	object.insert("channel", "1");
	object.insert("mode", "");

	QJsonDocument document = QJsonDocument(object);
	// 这里要将json格式的数据转换为QByteArray才行
	QByteArray post_data = document.toJson();
	//qDebug() << "上传标定测试结果：" << QString::fromLocal8Bit(post_data);
	m_replySaveProductTestResult = m_httpPost.post(m_netRequest, post_data);
	m_replyStatus = replySaveProductTestResult;
	QString logtxt = "SaveProductTestResult:\n" + post_data;
	SaveTestLog(logtxt.toLocal8Bit());
}

void MesHttpPost::SaveCompleteTask(QByteArray & dataArray)
{
	QString url = "http://";
	url += MesServerIp;
	url += ":";
	url += MesServerPort;
	url += "/mes/service/hirain/completeTask";
	m_netRequest.setUrl(QUrl(url));
	m_netRequest.setRawHeader("iovtoken", m_iovtoken.toStdString().data());

	QJsonObject object = QJsonDocument::fromJson(dataArray).object();
	object.insert("processKey", m_processKey);
	object.insert("testMainId", m_testMainId);
	object.insert("mode", "");
	/*object.insert("errorCode", "");
	object.insert("errorInfo", "");*/
	object.insert("bindMaterial", false);

	QJsonDocument document = QJsonDocument(object);
	// 这里要将json格式的数据转换为QByteArray才行
	QByteArray post_data = document.toJson();
	//qDebug() << "上传过站信息：" << QString::fromLocal8Bit(post_data);
	m_replySaveCompleteTask = m_httpPost.post(m_netRequest, post_data);
	m_replyStatus = replySaveCompleteTask;
	QString logtxt = "SaveCompleteTask:\n" + post_data;
	SaveTestLog(logtxt.toLocal8Bit());
}

void MesHttpPost::slotPostFinished(QNetworkReply *reply)
{
	QJsonObject jsonObject;
	bool bError = false;
	QString errmsg;
	if (reply->error() != QNetworkReply::NoError) {
		QVariant statusCodeV = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
		/* statusCodeV是HTTP服务器的相应码，reply->error()是Qt定义的错误码，可以参考QT的文档 */
		bError = true;
		errmsg = "http err code:" + QString::number(statusCodeV.toInt());
		errmsg += ", qt status code:" + QString::number(reply->error());
	}
	else {
		QByteArray bytes = reply->readAll();

        //QFile file("test.dat");
        //if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        //{
        //    file.write(bytes.data(), bytes.length());
        //}
        //file.close();

		//数据含中文，先转换为QString
		//QString strData = QString::fromLocal8Bit(bytes.data());
		QString strData = QString::fromUtf8(bytes);
        qDebug() <<"data QString:"<< strData;
		// 将收到的body部分解析为json格式
		QJsonParseError jsonError;
        QJsonDocument jsonDocument = QJsonDocument::fromJson(strData.toUtf8(), &jsonError);

		if (jsonError.error != QJsonParseError::NoError) {
			bError = true;
			errmsg = "json err:" + QString::number(jsonError.error);
		}	
		else
			jsonObject = jsonDocument.object();
	}

	/*if (reply == m_replyQueryWorkOrder) {
		if (!bError) {
			if (!ReplyJsonFromQueryWorkOrder(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}	
		}	
		emit emitQueryWorkOrderResult(bError, errmsg);
	}
	else if (reply == m_replyQueryDeviceProcess) {
        if (!bError) {
			if (!ReplyJsonFromQueryDeviceProcess(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
			m_queryWorkProcess++;
			if (QueryListDeviceProcessInfo())	
				emit emitQueryWorkProcessResult(bError, errmsg);
        }
		else {
			m_queryWorkProcess = 0;
			emit emitQueryWorkProcessResult(bError, errmsg);
		}	
	}
	else if (reply == m_replyCheckUserInfoAuth) {
		if (!bError) {
			if (!ReplyJsonFromCheckUserInfoAuth(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		emit emitCheckUserInfoAuthResult(bError, errmsg);
	}
	else if (reply == m_replyConfigProcess) {
		if (!bError) {
			if (!ReplyJsonFromConfigProcess(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		emit emitConfigProcessResult(bError, errmsg);
	}
	else if (reply == m_replyCheckValidateNumber) {
		if (!bError) {
			if (!ReplyJsonFromCheckValidateNumber(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		CalibTcpTrans::Instance()->SendMesResult(SN, !bError, errmsg);
	}
	else if (reply == m_replySaveCalibrationResult) {
		if (!bError) {
			if (!ReplyJsonFromSaveCalibrationResult(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		CalibTcpTrans::Instance()->SendMesResult(CalibData, !bError, errmsg);
	}
	else if (reply == m_replySaveProductTestResult) {
		if (!bError) {
			if (!ReplyJsonFromSaveProductTestResult(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		CalibTcpTrans::Instance()->SendMesResult(CalibResult, !bError, errmsg);
	}
	else if (reply == m_replySaveCompleteTask) {
		if (!bError) {
			if (!ReplyJsonFromSaveCompleteTask(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		CalibTcpTrans::Instance()->SendMesResult(TaskResult, !bError, errmsg);
	}*/
	if (m_replyStatus == replyQueryWorkOrder) {
		if (!bError) {
			if (!ReplyJsonFromQueryWorkOrder(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		emit emitQueryWorkOrderResult(bError, errmsg);
	}
	else if (m_replyStatus == replyQueryDeviceProcess) {
		if (!bError) {
			if (!ReplyJsonFromQueryDeviceProcess(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
			m_queryWorkProcess++;
			if (QueryListDeviceProcessInfo())
				emit emitQueryWorkProcessResult(bError, errmsg);
		}
		else {
			m_queryWorkProcess = 0;
			emit emitQueryWorkProcessResult(bError, errmsg);
		}
	}
	else if (m_replyStatus == replyCheckUserInfoAuth) {
		if (!bError) {
			if (!ReplyJsonFromCheckUserInfoAuth(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		emit emitCheckUserInfoAuthResult(bError, errmsg);
	}
	else if (m_replyStatus == replyConfigProcess) {
		if (!bError) {
			if (!ReplyJsonFromConfigProcess(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		emit emitConfigProcessResult(bError, errmsg);
	}
	else if (m_replyStatus == replyCheckValidateNumber) {
		if (!bError) {
			if (!ReplyJsonFromCheckValidateNumber(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		CalibTcpTrans::Instance()->SendMesResult(SN, !bError, errmsg);
	}
	else if (m_replyStatus == replySaveCalibrationResult) {
		if (!bError) {
			if (!ReplyJsonFromSaveCalibrationResult(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		CalibTcpTrans::Instance()->SendMesResult(CalibData, !bError, errmsg);
	}
	else if (m_replyStatus == replySaveProductTestResult) {
		if (!bError) {
			if (!ReplyJsonFromSaveProductTestResult(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		CalibTcpTrans::Instance()->SendMesResult(CalibResult, !bError, errmsg);
	}
	else if (m_replyStatus == replySaveCompleteTask) {
		if (!bError) {
			if (!ReplyJsonFromSaveCompleteTask(jsonObject)) {
				errmsg = m_lastmsg;
				bError = true;
			}
		}
		CalibTcpTrans::Instance()->SendMesResult(TaskResult, !bError, errmsg);
	}
	reply->deleteLater();
}

bool MesHttpPost::ReplyJsonFromQueryWorkOrder(QJsonObject & jsonObject)
{
	QString value = jsonObject.value("status").toString();
	qDebug() << "message:" << jsonObject.value("message").toString();
	
	QString logtxt = "ReplyJsonFromQueryWorkOrder:\n" + QJsonDocument(jsonObject).toJson();
	SaveTestLog(logtxt.toLocal8Bit());
	if (value != "PASS") {
		m_lastmsg = jsonObject.value("message").toString();
		return false;
	}

	if (jsonObject.contains("content")&& jsonObject["content"].isObject()) {
		QJsonObject subObj = jsonObject["content"].toObject();
		//产品线列表
		if (subObj.contains("pdLineList") && subObj["pdLineList"].isArray()) {
			QJsonArray lineListArray = subObj["pdLineList"].toArray();
			for (int i = 0; i < lineListArray.size(); i++) {
				if (lineListArray[i].isObject()) {
					QJsonObject childObj = lineListArray[i].toObject();
					PdLineList pdLineList;
					pdLineList.id = childObj.value("id").toString();
					pdLineList.name = childObj.value("name").toString();

					//产品列表
					if (childObj.contains("pdList") && childObj["pdList"].isArray()){
						QJsonArray pdListArray = childObj["pdList"].toArray();
						PdList pdList;
						for (int j = 0; j < pdListArray.size(); j++) {
							if (pdListArray[j].isObject()) {
								childObj = pdListArray[j].toObject();
								pdList.id = childObj.value("id").toString();
								pdList.name = childObj.value("name").toString();

								//workOrderList
								WorkOrderList workOrderList;			
								if (childObj.contains("workOrderList") && childObj["workOrderList"].isArray()) {
									QJsonArray workOrderListArray = childObj["workOrderList"].toArray();
									for (int k = 0; k < workOrderListArray.size(); k++) {
										if (workOrderListArray[i].isObject()) {
											childObj = workOrderListArray[k].toObject();
											WorkOrder workOrder;
											workOrder.workOrderId = childObj.value("id").toString();
											workOrder.name = childObj.value("name").toString();
											
											if (childObj.contains("processList") && childObj["processList"].isArray()) {
												QJsonArray processListArray = childObj["processList"].toArray();
												for (int i = 0; i < processListArray.size(); i++) {
													if (processListArray[i].isObject()) {
														QJsonObject childObj = processListArray[i].toObject();
														WorkProcess workProcess;
														workProcess.processId = childObj.value("processId").toString();
														workProcess.name = childObj.value("processName").toString();
														workOrder.vecProcessId.push_back(workProcess);
													}
												}
											}
											workOrderList.vecWorkOrder.push_back(workOrder);
										}
									}
								}
								pdList.workOrderList = workOrderList;
							}
							pdLineList.vecPdList.push_back(pdList);
						}
				    }
					
					m_vecPdLineList.push_back(pdLineList);
				}
			}
		}
	}
	else {
		m_lastmsg = "QueryWorkOrder reply no content...";
		return false;
	}

	for (int i = 0; i < m_vecPdLineList.size(); i++) {
		for (int j = 0; j < m_vecPdLineList[i].vecPdList.size(); j++) {
			for (int k = 0; k < m_vecPdLineList[i].vecPdList[j].workOrderList.vecWorkOrder.size(); k++) {
				m_vecWorkOrder.push_back(&m_vecPdLineList[i].vecPdList[j].workOrderList.vecWorkOrder[k]);
			}
		}
	}

	return true;
}

bool MesHttpPost::ReplyJsonFromQueryDeviceProcess(QJsonObject & jsonObject)
{
	QString logtxt = "ReplyJsonFromQueryDeviceProcess:\n" + QJsonDocument(jsonObject).toJson();
	SaveTestLog(logtxt.toLocal8Bit());
	QString value = jsonObject.value("status").toString();
	if (value != "PASS") {
		m_lastmsg = jsonObject.value("message").toString();
		return false;
	}

	if (jsonObject.contains("content") && jsonObject["content"].isObject()) {
		QJsonObject subObj = jsonObject["content"].toObject();
		//工序列表
		if (subObj.contains("processList") && subObj["processList"].isArray()) {
			QJsonArray processListArray = subObj["processList"].toArray();
			for (int i = 0; i < processListArray.size(); i++) {
				if (processListArray[i].isObject()) {
					QJsonObject childObj = processListArray[i].toObject();
					WorkProcess workProcess;
					workProcess.processId = childObj.value("processId").toString();
					workProcess.name = childObj.value("processName").toString();	
					m_vecWorkOrder[m_queryWorkProcess]->vecProcessId.push_back(workProcess);
				}
			}
		}	
	}

	return true;
}

bool MesHttpPost::ReplyJsonFromCheckUserInfoAuth(QJsonObject & jsonObject)
{
	QString logtxt = "ReplyJsonFromCheckUserInfoAuth:\n" + QJsonDocument(jsonObject).toJson();
	SaveTestLog(logtxt.toLocal8Bit());
	QString value = jsonObject.value("status").toString();
	if (value != "PASS") {
		m_lastmsg = jsonObject.value("message").toString();
		return false;
	}

	if (jsonObject.contains("content") && jsonObject["content"].isObject()) {
		QJsonObject subObj = jsonObject["content"].toObject();
		if (!subObj.contains("IOVTOKEN")) {
			m_lastmsg = jsonObject.value("message").toString();
			return false;
		}
		m_iovtoken =subObj.value("IOVTOKEN").toString();
	}

	return true;
}

bool MesHttpPost::ReplyJsonFromConfigProcess(QJsonObject & jsonObject)
{
	QString logtxt = "ReplyJsonFromConfigProcess:\n" + QJsonDocument(jsonObject).toJson();
	SaveTestLog(logtxt.toLocal8Bit());
	QString value = jsonObject.value("status").toString();
	if (value != "PASS") {
		m_lastmsg = jsonObject.value("message").toString();
		return false;
	}

	if (jsonObject.contains("content") && jsonObject["content"].isObject()) {
		QJsonObject subObj = jsonObject["content"].toObject();
		if (subObj.contains("processKey")) {
			m_processKey = subObj.value("processKey").toString();
		}
	}

	return true;
}

bool MesHttpPost::ReplyJsonFromCheckValidateNumber(QJsonObject & jsonObject)
{
	QString logtxt = "ReplyJsonFromCheckValidateNumber:\n" + QJsonDocument(jsonObject).toJson();
	SaveTestLog(logtxt.toLocal8Bit());
	QString value = jsonObject.value("status").toString();
	if (value != "PASS") {
		m_lastmsg = jsonObject.value("message").toString();
		return false;
	}

	/*if (jsonObject.contains("content") && jsonObject["content"].isObject()) {
		QJsonObject subObj = jsonObject["content"].toObject();
		m_processKey = subObj.value("processKey").toString();
	}*/

	return true;
}

bool MesHttpPost::ReplyJsonFromSaveCalibrationResult(QJsonObject & jsonObject)
{
	QString logtxt = "ReplyJsonFromSaveCalibrationResult:\n" + QJsonDocument(jsonObject).toJson();
	SaveTestLog(logtxt.toLocal8Bit());
	QString value = jsonObject.value("status").toString();
	if (value != "PASS") {
		m_lastmsg = jsonObject.value("message").toString();
		return false;
	}

	return true;
}

bool MesHttpPost::ReplyJsonFromSaveProductTestResult(QJsonObject & jsonObject)
{
	QString logtxt = "ReplyJsonFromSaveProductTestResult:\n" + QJsonDocument(jsonObject).toJson();
	SaveTestLog(logtxt.toLocal8Bit());
	QString value = jsonObject.value("status").toString();
	if (value != "PASS") {
		m_lastmsg = jsonObject.value("message").toString();
		return false;
	}
	if (jsonObject.contains("content") && jsonObject["content"].isObject()) {
		QJsonObject subObj = jsonObject["content"].toObject();
		m_testMainId = subObj.value("testMainId").toString();
	}

	return true;
}

bool MesHttpPost::ReplyJsonFromSaveCompleteTask(QJsonObject & jsonObject)
{
	QString logtxt = "ReplyJsonFromSaveCompleteTask:\n" + QJsonDocument(jsonObject).toJson();
	SaveTestLog(logtxt.toLocal8Bit());
	QString value = jsonObject.value("status").toString();
	if (value != "PASS") {
		m_lastmsg = jsonObject.value("message").toString();
		return false;
	}

	return true;
}

bool MesHttpPost::QueryListDeviceProcessInfo()
{
	if (m_queryWorkProcess >= m_vecWorkOrder.size()) {
		m_queryWorkProcess = 0;
		return true;
	}
	QueryDeviceProcessInfo(m_vecWorkOrder[m_queryWorkProcess]->workOrderId);

	return false;
}
void  MesHttpPost::SaveTestLog(QByteArray&bytedata)
{
	QDateTime  current_time = QDateTime::currentDateTime();
	QString current_date = current_time.toString("yyyy-MM-dd");
	QString filename = QCoreApplication::applicationDirPath() + "./LOG/MES-" + current_date + ".txt";
	QFile file(filename);
	current_date = current_time.toString("yyyy-MM-dd hh:mm:ss.zzz");
	QString sninfo;
	if (g_sn.isEmpty())
		sninfo = g_sn;
	else 
		sninfo = " " + g_sn + "-";
	if (file.open(QIODevice::WriteOnly | QIODevice::Text| QIODevice::Append)){
		file.write(current_date.toLocal8Bit(), current_date.length());
		file.write(sninfo.toLocal8Bit(), sninfo.length());
		file.write(bytedata.data(), bytedata.length());
	}
	file.close();
}


