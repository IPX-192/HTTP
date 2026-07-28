#pragma execution_character_set("utf-8")
#include "CalibTcpTrans.h"
#include "MesHttpPost.h"
#include <QDebug>
#include <cstring>
#include <QHostAddress>

CalibTcpTrans* CalibTcpTrans::m_instance = nullptr;

CalibTcpTrans::CalibTcpTrans(QObject *parent) : QObject(parent)
{
    m_tcpServer = new QTcpServer(this);
    bool listenRet = m_tcpServer->listen(QHostAddress::Any, TCP_DEV_PORT);
    if (!listenRet)
    {
        qCritical() << "TCP服务启动失败！端口:" << TCP_DEV_PORT << " 错误:" << m_tcpServer->errorString();
    }
    else
    {
        qInfo() << "TCP服务启动成功，监听端口:" << TCP_DEV_PORT;
    }

    // 新式信号槽绑定
    connect(m_tcpServer, &QTcpServer::newConnection, this, &CalibTcpTrans::newClientConnect);
}

CalibTcpTrans::~CalibTcpTrans()
{
    // 关闭所有客户端连接，释放socket
    for (auto& ctx : m_clientList)
    {
        if (ctx.socket)
        {
            ctx.socket->disconnectFromHost();
            ctx.socket->deleteLater();
        }
    }
    m_clientList.clear();
}

void CalibTcpTrans::EraseClientCtx(QTcpSocket *socket)
{
    for (auto iter = m_clientList.begin(); iter != m_clientList.end();)
    {
        if (iter->socket == socket)
        {
            iter = m_clientList.erase(iter);
            break;
        }
        ++iter;
    }
}

void CalibTcpTrans::SendMesResult(QTcpSocket *targetSocket, int nType, bool bPass, QString &message)
{
    // 连接状态校验
    if (!targetSocket || targetSocket->state() != QTcpSocket::ConnectedState)
    {
        qWarning() << "TCP响应发送失败，客户端未连接";
        return;
    }

    QByteArray buf;
    // 1. 指令类型
    buf.append((char*)&nType, sizeof(int));
    // 2. 结果标志（单独1字节，规避bool内存对齐跨平台bug）
    char flag = bPass ? 1 : 0;
    buf.append(&flag, sizeof(char));
    // 3. 字符串长度
    std::string strInfo = message.toStdString();
    int length = static_cast<int>(strInfo.size());
    buf.append((char*)&length, sizeof(int));
    // 4. 消息内容
    if (length > 0)
    {
        buf.append(strInfo.data(), length);
    }

    qint64 nWrite = targetSocket->write(buf);
    if (nWrite != buf.size())
    {
        qWarning() << "TCP发送数据不完整，预期字节:" << buf.size() << " 实际写入:" << nWrite;
    }
    targetSocket->flush();
}

void CalibTcpTrans::newClientConnect()
{
    QTcpSocket* newSocket = m_tcpServer->nextPendingConnection();
    qInfo() << "新客户端接入：" << newSocket->peerAddress().toString() << ":" << newSocket->peerPort();

    // 新建客户端独立缓存
    TcpClientCtx newCtx;
    newCtx.socket = newSocket;
    m_clientList.append(newCtx);

    // 绑定客户端信号
    connect(newSocket, &QTcpSocket::readyRead, this, &CalibTcpTrans::tcpReceive);
    connect(newSocket, &QTcpSocket::disconnected, this, &CalibTcpTrans::disConnect);
}

void CalibTcpTrans::disConnect()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;
    qInfo() << "客户端断开连接：" << socket->peerAddress().toString();
    // 清理上下文
    EraseClientCtx(socket);
    socket->deleteLater();
}

void CalibTcpTrans::tcpReceive()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    // 查找当前客户端独立缓存
    TcpClientCtx* curCtx = nullptr;
    for (auto& ctx : m_clientList)
    {
        if (ctx.socket == socket)
        {
            curCtx = &ctx;
            break;
        }
    }
    if (!curCtx)
        return;

    // 第一步：读取包头
    if (curCtx->m_nDataSize == 0)
    {
        qint64 nAvail = socket->bytesAvailable();
        if (nAvail < sizeof(TcpCommand))
            return;
        QByteArray buf = socket->read(sizeof(TcpCommand));
        TcpCommand pCommand;
        std::memcpy(&pCommand, buf.data(), sizeof(TcpCommand));
        curCtx->m_nCurType = pCommand.nType;
        curCtx->m_nDataSize = pCommand.nLenth;
        curCtx->m_DataArray.clear();
    }

    bool bContinue = false;
    QByteArray buf;
    qint64 nAvail = socket->bytesAvailable();
    if (nAvail <= curCtx->m_nDataSize)
    {
        buf = socket->read(nAvail);
    }
    else
    {
        buf = socket->read(curCtx->m_nDataSize);
        bContinue = true;
    }
    curCtx->m_DataArray.append(buf);
    curCtx->m_nDataSize -= buf.size();
    if (curCtx->m_nDataSize > 0)
        return;

    // 完整数据包接收完成，业务分发（已适配新版同步 MesHttpPost）
    if (Material == curCtx->m_nCurType)
    {
        // 新版 MesHttpPost 无 GetMaterialInfo 接口，暂保留
    }
    else if (SN == curCtx->m_nCurType)
    {
        QString sn = QString::fromUtf8(curCtx->m_DataArray);
        bool outValidate = false;
        QString err = MesHttpPost::Instance()->ValidateNumber(sn, outValidate);
        QString resultMsg = err.isEmpty() ? "校验通过" : err;
        SendMesResult(socket, SN, err.isEmpty(), resultMsg);
    }
    else if (CalibData == curCtx->m_nCurType)
    {
        // 新版 MesHttpPost 无 SaveCalibrationResultFileContent 接口，暂保留
    }
    else if (CalibResult == curCtx->m_nCurType)
    {
        // 新版 MesHttpPost 无 SaveProductTestResult 接口，暂保留
    }
    else if (TaskResult == curCtx->m_nCurType)
    {
        // 新版 MesHttpPost 无 SaveCompleteTask 接口，暂保留
    }

    // 清空当前客户端缓存
    curCtx->m_DataArray.clear();
    curCtx->m_nDataSize = 0;
    // 缓冲区还有剩余数据，递归继续解析粘包
    if (bContinue)
        tcpReceive();
}
