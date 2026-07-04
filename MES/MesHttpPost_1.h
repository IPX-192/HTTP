#ifndef MESHTTPPOST_H
#define MESHTTPPOST_H

#include <QObject>
#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "mainwindow.h"


//工序信息
struct WorkProcess {
	QString processId;
	QString name;
};


//工单信息
struct WorkOrder{
	QString workOrderId;
	QString name;
	QVector<WorkProcess> vecProcessId;
};


//工单列表
struct WorkOrderList {
	QVector<WorkOrder>vecWorkOrder;
};

//产品信息
struct PdList {
	QString id;
	QString name;
	WorkOrderList workOrderList;
};

//产品线列表
struct PdLineList {
	QString id;
	QString name;
	QVector<PdList>vecPdList;
};

enum ReplyStatus {
	replyNone,
	replyQueryWorkOrder,
	replyQueryDeviceProcess,
    replyCheckUserInfoAuth,
    replyConfigProcess,
    replyCheckValidateNumber,
    replySaveCalibrationResult,
    replySaveProductTestResult,
    replySaveCompleteTask
};

class MesHttpPost : public QObject
{
    Q_OBJECT

public:
	static MesHttpPost* Instance(QObject *parent = nullptr) {
		if (m_instance == nullptr) {
			m_instance = new MesHttpPost(parent);
		}
		return m_instance;
	}
	static void Uninstance() {
		if (m_instance != nullptr) {
			delete m_instance;
			m_instance = nullptr;
		}
	}

protected:
    explicit MesHttpPost(QObject *parent = nullptr);
	~MesHttpPost();

public:
	void  SetMesIpInfo(QString ip, QString port);
    void  QueryWorkOrderInfo();                                    //查询工单信息
    void  QueryDeviceProcessInfo(QString workOrderId);             //查询工序信息
    void  CheckUserInfoAuth(QString user, QString passwd, 
		      QString workOrderId, QString processId);             //校验用户信息
    void  ConfigProcess();                                         //设置工序
    void  CheckValidateNumber(QString sn);                         //条码校验
    void  SaveCalibrationResultFileContent(QByteArray&dataArray);   //保存标定结果
	void  SaveProductTestResult(QByteArray&dataArray);
	void  SaveCompleteTask(QByteArray&dataArray);
	QVector<PdLineList>  GetPdLineListInfo() {
		return m_vecPdLineList;
	}
	void  SetMaterialInfo(QString material) {
		m_Material = material;
	}
	QString  GetMaterialInfo() {
		MainWindow*widget = (MainWindow*)parent();
		widget->setCursor(Qt::ArrowCursor);
		widget->setEnabled(true);
		widget->m_waitMessageBox->setEnabled(true);
		widget->m_waitMessageBox->hide();
		widget->hide();
		return m_Material;
	}
	bool  QueryListDeviceProcessInfo();

public:
	bool  ReplyJsonFromQueryWorkOrder(QJsonObject& jsonObject);
	bool  ReplyJsonFromQueryDeviceProcess(QJsonObject& jsonObject);
	bool  ReplyJsonFromCheckUserInfoAuth(QJsonObject& jsonObject);
	bool  ReplyJsonFromConfigProcess(QJsonObject& jsonObject);
	bool  ReplyJsonFromCheckValidateNumber(QJsonObject& jsonObject);
	bool  ReplyJsonFromSaveCalibrationResult(QJsonObject& jsonObject);
	bool  ReplyJsonFromSaveProductTestResult(QJsonObject & jsonObject);
	bool  ReplyJsonFromSaveCompleteTask(QJsonObject & jsonObject);

protected:
	void  SaveTestLog(QByteArray&bytedata);


protected:
	static MesHttpPost*  m_instance;
	QString             MesServerIp;
	QString             MesServerPort;
    QNetworkAccessManager m_httpPost;
    QNetworkRequest       m_netRequest;
	QNetworkReply*        m_replyQueryWorkOrder;
	QNetworkReply*        m_replyQueryDeviceProcess;
	QNetworkReply*        m_replyCheckUserInfoAuth;
	QNetworkReply*        m_replyConfigProcess;
	QNetworkReply*        m_replyCheckValidateNumber;
	QNetworkReply*        m_replySaveCalibrationResult;
	QNetworkReply*        m_replySaveProductTestResult;
	QNetworkReply*        m_replySaveCompleteTask;
	ReplyStatus           m_replyStatus = replyNone;

    QString               m_ipInfo;
	QVector<PdLineList>   m_vecPdLineList;
	QString               m_processId;          //工序id
	QString               m_iovtoken;           //MES系统认证token
	QString               m_processKey;         //工序操作key
	QString               m_Material;           //物料信息
	QString               m_lastmsg;

	QVector<WorkOrder*>   m_vecWorkOrder;
	int                   m_queryWorkProcess = 0;      //当前工序查询序号

	QString               m_testMainId;                //保存终端测试结果返回值


signals:
	void   emitQueryWorkOrderResult(bool bErr, QString message);
	void   emitQueryWorkProcessResult(bool bErr, QString message);
    void   emitCheckUserInfoAuthResult(bool bErr, QString message);
	void   emitConfigProcessResult(bool bErr, QString message);


public slots:
    void   slotPostFinished(QNetworkReply*);
};

#endif // MESHTTPPOST_H
