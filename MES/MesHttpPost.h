#ifndef MESHTTPPOST_H
#define MESHTTPPOST_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QMap>
#include <QTimer>
#include <QEventLoop>
#include <QDateTime>
#include <QFile>

// 工单工序
struct WorkProcess {
    QString processId;
    QString name;
};

// 用户认证返回的用户信息
struct AuthUserInfo
{
    QString deptName;
    QString updateDate;
    QString mobilePhone;
    QString phone;
    QString name;
    QString roleName;
    QString id;
    QString jobNumber;
    QString email;
    QString createDate;
    QString username;
};

// 生产信息单条字段
struct ProductionInfoItem
{
    QString name;
    QString value;
    QString key;
};

// 完整生产信息缓存
struct ProductionInfo
{
    QString pdPartNumber;   //产品料号
    QString pdName;         //产品名称
    QString pdCode;         //产品ERP
    QString bomVersion;     //BOM版本
    QString workOrderNum;   //工单号
    QString workOrderTotal; //工单总数
    QString batchNum;       //生产批次
    QString partNo;         //客户零件号
    QString processName;    //工序名称
    QString processSuccess; //合格数量
    QString processFail;    //失败数量
    QList<ProductionInfoItem> rawList; //原始数组备份
    ProductionInfo(){
        pdPartNumber.clear();
        pdName.clear();
        pdCode.clear();
        bomVersion.clear();
        workOrderNum.clear();
        workOrderTotal.clear();
        batchNum.clear();
        partNo.clear();
        processName.clear();
        processSuccess.clear();
        processFail.clear();
        rawList.clear();
    }
};

// 单条工序工作站信息
struct DeviceProcessItem
{
    QString workstationCode;
    QString workCenterCode;
    QString processId;
    QString processName;
    QString workstationName;
    QString deviceCode;
    QString workCenterName;
    QString deviceName;
};

// 工单
struct WorkOrder{
    QString workOrderId;
    QString name;
    QVector<WorkProcess> vecProcessId;
};

struct WorkOrderList {
    QVector<WorkOrder> vecWorkOrder;
};

// 产品/产线（登录工单查询缓存用）
struct PdList {
    QString id;
    QString name;
    WorkOrderList workOrderList;
};

struct PdLineList {
    QString id;
    QString name;
    QVector<PdList> vecPdList;
};

// 心跳上报报文
struct StationHeartBeat {
    QString stationId;
    QString workOrderId;
    QString processId;
    QString status;
    QDateTime beatTime;
};

// 治具寿命消耗单条
struct FixtureLifeItem
{
    QString number;
    qreal totalLife;
    qreal residueLife;
    int consumption;
};

struct FixtureConsumeResult
{
    bool warnFlag;
    QString warnMessage;
    QVector<FixtureLifeItem> fixtureList;
    FixtureConsumeResult(){
        warnFlag = false;
        warnMessage.clear();
        fixtureList.clear();
    }
};

// 文件网关上传固定请求头结构体
struct GatewayUploadHeader
{
    QString user;        // 操作员工号
    QString userName;    // 操作人员姓名（未编码原始中文）
    QString appCode;     // 应用编码
    QString appSecretKey;// 应用密钥
};


// 文件上传返回
struct UploadFileResp {
    QString fileId;
    QString fullPath;
    QString filePId;
    QString createDirsRaw; // 目录数组原始json字符串
    QString contentMsg;    // content.message
    QString contentStatus;  // content.status
    QString version;
    UploadFileResp(){
        fileId.clear();
        fullPath.clear();
        filePId.clear();
        createDirsRaw.clear();
        contentMsg.clear();
        contentStatus.clear();
        version.clear();
    }
};

//接口枚举
enum ReplyStatus {
    replyNone = 0,
    replyQueryWorkOrderInfo,
    replyUserInfoAuth,
    replyConfigProcess,
    replyQueryProductionInfo,
    replyValidateNumber,
    replySaveProcessOpResult,
    replyCompleteTask,
    replyQueryDeviceProcessInfo,
    replyStationHeartbeat,
    replyValidateDeviceUseFixture,
    replyBindFixtureChannel,
    replyConsumeFixtureLife,
    replyValidateStandardElementNumber,
    replyUploadSingle,
    replySaveProductFilePath
};

// 请求上下文
struct MesRequestContext {
    ReplyStatus reqType;
    QNetworkReply* reply;
    QJsonObject reqBody;
    QByteArray respData;
};

// 文件上传网关固定常量
const QString MES_UPLOAD_BOUNDARY = "----MES_UPLOAD_BOUNDARY_123456789";
const int UPLOAD_ERR_TRUNCATE_LEN = 200;
const int UPLOAD_JSON_ERR_TRUNCATE_LEN = 500;

const int   MES_NET_REQUEST_TIMEOUT_MS = 30000;

class MesHttpPost : public QObject
{
    Q_OBJECT
public:
    // 单例
    static MesHttpPost* Instance(QObject *parent = nullptr) {
        if (!m_instance)
            m_instance = new MesHttpPost(parent);
        return m_instance;
    }
    static void Uninstance();

protected:
    explicit MesHttpPost(QObject *parent = nullptr);
    ~MesHttpPost();

public:
    // 服务地址配置
    void SetMesIpInfo(QString ip, QString port);
    QString GetMesBaseUrl() const;
    QString GetStorageGatewayUrl() const;

    // 登录流程
    QString QueryWorkOrderInfo(QVector<PdLineList>& outPdLineList);
    QString UserInfoAuth(const QString& user, const QString& passwd, const QString& workOrderId, const QString& processId, AuthUserInfo& outUserInfo);
    QString ConfigProcess(const QString& processId, QString& outProcessKey);
    QString QueryProductionInfo(ProductionInfo& outProdInfo);
    QString QueryDeviceProcessInfo(const QString& deviceIp, const QString& workOrderId, QList<DeviceProcessItem>& outDeviceList);
    QString StationHeartbeat();
    QString ValidateDeviceUseFixture(bool& outNeedFixture);
    QString BindFixtureChannel(const QString& fixtureSn, const QString& channel, FixtureConsumeResult& outResult);

    // 自动生产流程
    QString ConsumeFixtureLife(const QString& fixtureSn, int consumeNum, FixtureConsumeResult& outResult);
    QString ValidateStandardElementNumber(const QString& sn,bool& outStandard);
    QString ValidateNumber(const QString& sn,bool& outValidate);
    QString SaveProcessOpResult(const QString& sn, int opResult, const QJsonArray& detailArr, QString& outMainId);
    //CompleteTask参数resultMainId是否使用待确定
    QString CompleteTask(const QString& sn, bool isSuccess, const QString& errCode, const QString& errInfo, bool bindMat, bool& outTaskResult);
    QString UploadSingle(const GatewayUploadHeader& headerInfo,const QString& fileName, const QByteArray& fileBin, const QString& filePid, const QString& nsId, UploadFileResp& outUploadResp);
    QString SaveProductFilePath(const QString& sn, const QString& fileId, const QString& type, const QString& nsId, const QString& fileName,bool& outSave);

    // 解析函数错误信息存入静态map
    bool ReplyJsonFromQueryWorkOrderInfo(QJsonObject& jsonObject, QVector<PdLineList>& outPdLineList);
    bool ReplyJsonFromUserInfoAuth(QJsonObject& jsonObject, AuthUserInfo& outUserInfo);
    bool ReplyJsonFromConfigProcess(QJsonObject& jsonObject,QString& outProcessKey);
    bool ReplyJsonFromQueryProductionInfo(QJsonObject& jsonObject, ProductionInfo& outProdInfo);
    bool ReplyJsonFromValidateNumber(QJsonObject& jsonObject,bool& outValidate);
    bool ReplyJsonFromSaveProcessOpResult(QJsonObject& jsonObject, QString& outMainId);
    bool ReplyJsonFromCompleteTask(QJsonObject& jsonObject, bool& outTaskResult);
    bool ReplyJsonFromQueryDeviceProcessInfo(QJsonObject& jsonObject, QList<DeviceProcessItem>& outDeviceList);
    bool ReplyJsonFromStationHeartbeat(QJsonObject& jsonObject);
    bool ReplyJsonFromValidateDeviceUseFixture(QJsonObject& jsonObject, bool& outNeedFixture);
    bool ReplyJsonFromBindFixtureChannel(QJsonObject& jsonObject, FixtureConsumeResult& outResult);
    bool ReplyJsonFromConsumeFixtureLife(QJsonObject& jsonObject, FixtureConsumeResult& outResult);       //注意：这里文档上出参"residuleLife"，应为"residueLife"
    bool ReplyJsonFromValidateStandardElementNumber(QJsonObject& jsonObject,bool& outStandard);
    bool ReplyJsonFromUploadSingle(QJsonObject& jsonObject, UploadFileResp& outUploadResp);
    bool ReplyJsonFromSaveProductFilePath(QJsonObject& jsonObject,bool& outSave);

public:
    // 有返回结构体，传引用
    template<typename T>
    QString SendMesPostRequest(ReplyStatus reqType, const QString& apiPath, const QJsonObject& body, T& outData)
    {
        // 内部调用私有void*底层，自动取地址
        return SendMesPostRequestImpl(reqType, apiPath, body, &outData);
    }

    // 无返回结构体（心跳、单纯校验接口）
    QString SendMesPostRequest(ReplyStatus reqType, const QString& apiPath, const QJsonObject& body)
    {
        // 传空指针
        return SendMesPostRequestImpl(reqType, apiPath, body, nullptr);
    }

protected:
    // JSON通用POST底层
    QString SendMesPostRequestImpl(ReplyStatus reqType, const QString& apiPath, const QJsonObject& body, void* outData);
    // 文件上传独立底层 multipart/form-data

    QString SendMultipartUploadRequest(const GatewayUploadHeader& headerInfo, const QString& fileName, const QByteArray& fileBin, const QString& filePid, const QString& nsId, UploadFileResp& outUploadResp);

    bool WaitRequestFinish(QNetworkReply* reply, int timeoutMs = MES_NET_REQUEST_TIMEOUT_MS);
    void SaveTestLog(const QByteArray& bytedata);
    QString EncodeWholePath(const QString& path);
    void HandleRequestError(QNetworkReply* reply, ReplyStatus reqType);
    void ClearRequestContext(QNetworkReply* reply);

protected:
    static MesHttpPost* m_instance;
    // 全局静态存储解析错误
    static QMap<QString, QString> m_parseErrMap;

    QString MesServerIp;
    QString MesServerPort;
    QString m_iovtoken;
    QString m_processKey;
    QString m_ipInfo;

    QNetworkAccessManager m_httpPost;
    QMap<QNetworkReply*, MesRequestContext> m_reqMap;
    QMap<ReplyStatus, bool> m_reqRunning;

    QString m_processId;

public slots:
    void slotPostFinished(QNetworkReply* reply);
};

#endif // MESHTTPPOST
