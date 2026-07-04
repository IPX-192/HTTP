#ifndef CONTROLCENTERHTTPAPI_H
#define CONTROLCENTERHTTPAPI_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QTimer>
#include <QEventLoop>
#include <QDateTime>

// 业务数据结构体
// 自检单项明细
struct SelfCheckResultItem
{
    QString tiId;
    QString priData;
    QString priResult;
    QString tiName;
    QString tiLower;
    QString tiUpper;
    QString tiStandard;
};

// 换型任务信息
struct DeviceTaskInfo
{
    QString workOrderNum;
    QString processName;
    int workOrderTotal;
    QString token;
    QString processKey;
    QString productionNum;
    QString programName;
    // 默认构造清空脏数据
    DeviceTaskInfo(){
        workOrderNum.clear();
        processName.clear();
        workOrderTotal = 0;
        token.clear();
        processKey.clear();
        productionNum.clear();
        programName.clear();
    }
};

// 设备指令返回
struct DeviceExecCommand
{
    QString command;
    DeviceExecCommand(){ command = "NONE"; }
};

// 网络请求上下文
struct CtrlRequestContext
{
    int reqType;
    QNetworkReply* reply;
    QJsonObject reqBody;
    QByteArray respData;
};

// 接口类型枚举
enum CtrlReqType
{
    ctrlNone = 0,
    ctrlDeviceSelfCheckStatus,        // 设备自检状态上报
    ctrlDeviceSelfCheckResult,        // 设备自检最终结果上报
    ctrlDeviceSelfCheckResultExt,     // 设备自检完整明细上报
    ctrlDeviceAlarmEvent,             // 设备告警上报
    ctrlDeviceStatus,                 // 设备状态上传
    ctrlGetDeviceExecCommands,        // 获取设备执行指令
    ctrlGetDeviceTaskInfo,            // 获取当前生产任务
    ctrlSaveDeviceCmdExecResult,      // 指令执行结果上报
    ctrlSaveDeviceInstructionFeedback // 指令执行反馈上传
};

const int CTRL_NET_REQUEST_TIMEOUT_MS = 30000;

class ControlCenterHttpApi : public QObject
{
    Q_OBJECT
public:
    // 单例
    static ControlCenterHttpApi* Instance(QObject *parent = nullptr);
    static void Uninstance();

protected:
    explicit ControlCenterHttpApi(QObject *parent = nullptr);
    ~ControlCenterHttpApi();

public:
    // 服务地址配置
    void SetCtrlIpInfo(const QString& ip, const QString& port);
    QString GetCtrlBaseUrl() const;

    // 上报类POST（无结构体输出，仅返回错误）
    QString UploadDeviceSelfCheckStatus(const QString& deviceCode, const QString& deviceIp, const QString& channel,
                                        const QString& result, const QJsonArray& productResultItem);
    QString UploadDeviceSelfCheckResult(const QString& deviceCode, const QString& deviceIp, const QString& result);
    QString UploadDeviceSelfCheckResultExt(const QString& deviceCode, const QString& deviceIp, const QString& channel,
                                           const QString& result, const QJsonArray& productResultItem);
    QString UploadDeviceAlarmEvent(const QString& deviceCode, const QString& deviceIp,
                                   const QString& sourceAlarmEvent, const QDateTime& alarmTime);
    QString UploadDeviceStatus(const QString& deviceCode, const QString& deviceIp, const QString& status);

    // 查询类GET（带结构体输出）
    QString GetDeviceExecCommands(const QString& deviceCode, const QString& ip, DeviceExecCommand& outCmd);
    QString GetDeviceTaskInfo(const QString& deviceCode, const QString& ip, DeviceTaskInfo& outTask);

    // 反馈上报GET（无结构体输出）
    QString SaveDeviceCmdExecResult(const QString& deviceCode, const QString& ip, const QString& command, const QString& result);
    QString SaveDeviceInstructionFeedback(const QString& deviceCode, const QString& ip, const QString& command,
                                          const QString& result, const QString& message);

    // 带输出结构体
    template<typename T>
    QString SendCtrlRequest(int reqType, const QString &apiPath, bool isGet, const QJsonObject &body, T& outData)
    {
        return SendCtrlRequestImpl(reqType, apiPath, isGet, body, &outData);
    }
    // 无输出结构体
    QString SendCtrlRequest(int reqType, const QString &apiPath, bool isGet, const QJsonObject &body)
    {
        return SendCtrlRequestImpl(reqType, apiPath, isGet, body, nullptr);
    }

protected:
    // 私有底层实现（仅内部使用void*裸指针）
    QString SendCtrlRequestImpl(int reqType, const QString &apiPath, bool isGet, const QJsonObject &body, void* outData);

    bool WaitRequestFinish(QNetworkReply* reply, int timeoutMs = CTRL_NET_REQUEST_TIMEOUT_MS);
    void ClearRequestContext(QNetworkReply* reply);
    void HandleRequestError(QNetworkReply* reply, int reqType);
    void SaveCtrlLog(const QByteArray& data);

    // 报文解析统一函数,错误存入静态map
    bool ParseCommonResponse(const QJsonObject& jsonObj, int reqType);
    bool ParseExecCommandResp(const QJsonObject& jsonObj, DeviceExecCommand& outCmd);
    bool ParseTaskInfoResp(const QJsonObject& jsonObj, DeviceTaskInfo& outTask);

protected:
    static ControlCenterHttpApi* m_instance;
    // 静态存储解析错误
    static QMap<int, QString> m_parseErrMap;

    QString m_ctrlIp;
    QString m_ctrlPort;
    QString m_localIp;

    QNetworkAccessManager m_netMgr;
    QMap<QNetworkReply*, CtrlRequestContext> m_reqMap;
    QMap<int, bool> m_reqRunning;

public slots:
    void slotCtrlRequestFinished(QNetworkReply* reply);
};

#endif // CONTROLCENTERHTTPAPI
