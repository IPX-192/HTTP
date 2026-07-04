#ifndef CALIBTCPTRANS_H
#define CALIBTCPTRANS_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QByteArray>

// 端口常量
constexpr quint16 TCP_DEV_PORT = 8385;

// 指令枚举
enum TcpCmdType
{
    Material,     //物料
    SN,           //SN校验
    CalibData,    //标定数据
    CalibResult,  //标定结果
    TaskResult    //工作站过站结果
};

// TCP包头结构体
#pragma pack(push, 1)
struct TcpCommand {
    int nType;    //指令类型
    int nLenth;   //后续数据长度
};
#pragma pack(pop)

// 单客户端独立缓存上下文（解决多客户端数据串扰）
struct TcpClientCtx
{
    QTcpSocket* socket = nullptr;
    int m_nCurType = 0;
    int m_nDataSize = 0;
    QByteArray m_DataArray;
};

class CalibTcpTrans : public QObject
{
    Q_OBJECT
public:
    // 单例
    static CalibTcpTrans* Instance(QObject *parent = nullptr)
    {
        if (m_instance == nullptr) {
            m_instance = new CalibTcpTrans(parent);
        }
        return m_instance;
    }
    static void Uninstance()
    {
        if (m_instance != nullptr) {
            delete m_instance;
            m_instance = nullptr;
        }
    }

protected:
    explicit CalibTcpTrans(QObject *parent = nullptr);
    ~CalibTcpTrans() override;

public:
    // 对外发送响应接口，指定目标socket
    void SendMesResult(QTcpSocket* targetSocket, int nType, bool bPass, QString& message);

protected:
    // 移除断开的客户端上下文
    void EraseClientCtx(QTcpSocket* socket);

protected slots:
    void newClientConnect();
    void disConnect();
    void tcpReceive();

protected:
    static CalibTcpTrans* m_instance;
    QTcpServer* m_tcpServer = nullptr;
    // 支持多客户端同时连接
    QList<TcpClientCtx> m_clientList;
};

#endif // CALIBTCPTRANS_H
