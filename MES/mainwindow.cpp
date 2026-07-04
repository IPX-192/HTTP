#pragma execution_character_set("utf-8")
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QFile>
#include <QDir>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_waitMessageBox(new QMessageBox(this))
{
    ui->setupUi(this);

    QString g_appPath = QCoreApplication::applicationDirPath();
    QString filePath = g_appPath + "/url.txt";
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString line = file.readLine().trimmed();
        QStringList ipPort = line.split(" ");
        ipPort.removeAll("");
        if (ipPort.size() == 2)
        {
            QString ip = ipPort[0];
            QString port = ipPort[1];
            MesHttpPost::Instance(this)->SetMesIpInfo(ip, port);
            setWindowTitle("MES-" + ip + ":" + port);
        }
        file.close();
    }

    // 等待弹窗初始化
    m_waitMessageBox->setWindowTitle(("提示信息"));
    m_waitMessageBox->setStandardButtons(QMessageBox::NoButton);

    // 初始化缓存数据
    m_allPdLineData.clear();
    m_heartTimer.setInterval(HEARTBEAT_INTERVAL);
    connect(&m_heartTimer, &QTimer::timeout, this, &MainWindow::SlotPostHeartBeatTimeout);

    // ===================== 模拟假测试数据 =====================

    PdLineList fakeLine;
    fakeLine.id = "LINE_001";
    fakeLine.name = "一号产线";

    PdList fakeProduct;
    fakeProduct.id = "PROD_001";
    fakeProduct.name = "产品A-型号001";

    // 工单1
    WorkOrder wo1;
    wo1.workOrderId = "WO20260702001";
    wo1.name = "20260702-A批次工单";
    // 工单1绑定两道工序
    WorkProcess proc1;
    proc1.processId = "PROC_001";
    proc1.name = "贴片工序";
    WorkProcess proc2;
    proc2.processId = "PROC_002";
    proc2.name = "检测工序";
    wo1.vecProcessId << proc1 << proc2;

    // 工单2
    WorkOrder wo2;
    wo2.workOrderId = "WO20260702002";
    wo2.name = "20260702-B批次工单";
    WorkProcess proc3;
    proc3.processId = "PROC_003";
    proc3.name = "组装工序";
    wo2.vecProcessId << proc3;

    // 工单列表挂载到产品
    fakeProduct.workOrderList.vecWorkOrder << wo1 << wo2;
    // 产品挂载到产线
    fakeLine.vecPdList << fakeProduct;
    // 存入全局缓存
    m_allPdLineData << fakeLine;

    // 自动填充产品线下拉
    ui->comboBox_pdLineList->clear();
    ui->comboBox_pdLineList->addItem(fakeLine.name);
    // 触发下拉联动，自动填充产品、工单、工序
    on_comboBox_pdLineList_currentIndexChanged(0);

    ui->lineEdit_user->setText("admin");
    ui->lineEdit_passwd->setText("123456");
}

MainWindow::~MainWindow()
{
    delete ui;
}

// 统一弹窗提示
void MainWindow::ShowMsg(const QString& title, const QString& text)
{
    //QMessageBox::information(this, title, text);
}

// 界面锁定解锁
void MainWindow::SetUiLock(bool lock)
{
    this->setEnabled(!lock);
    this->setCursor(lock ? Qt::WaitCursor : Qt::ArrowCursor);
}

// 加载全量产品线、产品、工单数据，填充产品线下拉
bool MainWindow::LoadAllWorkOrderData(QString& errMsg)
{
    errMsg.clear();
    m_waitMessageBox->setText(("获取产品工单信息中,请稍候..."));
    m_waitMessageBox->show();

    //先获取当前可生产的产品和工单信息
    QVector<PdLineList> outData;
    errMsg = MesHttpPost::Instance()->QueryWorkOrderInfo(outData);

    m_waitMessageBox->hide();
    if (!errMsg.isEmpty())
        return false;

    // 缓存全局数据
    m_allPdLineData = outData;
    // 清空所有下拉
    ui->comboBox_pdLineList->clear();
    ui->comboBox_pdList->clear();
    ui->comboBox_workOrderList->clear();
    ui->comboBox_workProcess->clear();

    // 填充产品线下拉
    for (const auto& line : m_allPdLineData)
    {
        ui->comboBox_pdLineList->addItem(line.name);    //填充产品线名
    }
    if (!m_allPdLineData.isEmpty())
        on_comboBox_pdLineList_currentIndexChanged(0);

    return true;
}

// 根据工单ID刷新工序下拉
bool MainWindow::RefreshProcessCombo(const QString& workOrderId, QString& errMsg)
{
    errMsg.clear();
    QList<DeviceProcessItem> processList;
    errMsg = MesHttpPost::Instance()->QueryDeviceProcessInfo("",workOrderId, processList);
    if (!errMsg.isEmpty())
        return false;

    ui->comboBox_workProcess->clear();
    for (const auto& proc : processList)
    {
        //添加工序
        ui->comboBox_workProcess->addItem(proc.processName, proc.processId);
    }
    return true;
}

bool MainWindow::DoFullLoginFlow(QString& errMsg)
{
    errMsg.clear();
    // 1. 读取界面输入
    QString user = ui->lineEdit_user->text().trimmed();
    QString pwd = ui->lineEdit_passwd->text().trimmed();
    QString workOrderId = ui->comboBox_workOrderList->currentData().toString();
    QString processId = ui->comboBox_workProcess->currentData().toString();

    // 基础输入校验
    if (user.isEmpty() || pwd.isEmpty())
    {
        errMsg = ("用户名/密码不能为空");
        return false;
    }
    if (workOrderId.isEmpty() || processId.isEmpty())
    {
        errMsg = ("请先选择工单和工序");
        return false;
    }

    // 2. 用户登录鉴权 UserInfoAuth
    AuthUserInfo userInfo;
    errMsg = MesHttpPost::Instance()->UserInfoAuth(user, pwd, workOrderId, processId, userInfo);
    if (!errMsg.isEmpty())
        return false;

    // 3. 设置工序，获取processKey
    QString processKey;
    errMsg = MesHttpPost::Instance()->ConfigProcess(processId, processKey);
    if (!errMsg.isEmpty())
        return false;

    // 4. 获取生产信息
    ProductionInfo prodInfo;
    errMsg = MesHttpPost::Instance()->QueryProductionInfo(prodInfo);
    if (!errMsg.isEmpty())
        return false;

    // 5. 启动周期心跳
    m_heartTimer.start();

    //6.通知治具检查和自检
    emit SignalCheckFixtureNeedAndSelfCheck();

    // 7. 读取注册表启动标定软件   (之前的逻辑)
    QSettings reg("HKEY_CURRENT_USER\\Software\\ViSensing\\FisheyeCalib_ME\\applicationPath", QSettings::NativeFormat);
    QString apppath = reg.value("applicationPath").toString();
    if (apppath.isEmpty())
    {
        errMsg = ("无标定软件配置信息");
        return false;
    }
    if (!QFile::exists(apppath))
    {
        errMsg = ("标定软件不存在");
        return false;
    }

    // 启动标定程序
    QProcess *process = new QProcess(this);
    process->startDetached(apppath);
    g_success = true;
    return true;
}

// 产品线下拉切换
void MainWindow::on_comboBox_pdLineList_currentIndexChanged(int index)
{
    if (m_allPdLineData.isEmpty() || index < 0 || index >= m_allPdLineData.size())
        return;

    const auto& lineData = m_allPdLineData[index];
    ui->comboBox_pdList->clear();
    for (const auto& product : lineData.vecPdList)
    {
        ui->comboBox_pdList->addItem(product.name);
    }
    if (!lineData.vecPdList.isEmpty())
        on_comboBox_pdList_currentIndexChanged(0);
}

// 产品下拉切换
void MainWindow::on_comboBox_pdList_currentIndexChanged(int index)
{
    int lineIdx = ui->comboBox_pdLineList->currentIndex();
    if (m_allPdLineData.isEmpty() || lineIdx < 0 || index < 0)
        return;

    const auto& lineData = m_allPdLineData[lineIdx];
    if (index >= lineData.vecPdList.size())
        return;

    const auto& prodData = lineData.vecPdList[index];
    ui->comboBox_workOrderList->clear();
    for (const auto& workOrder : prodData.workOrderList.vecWorkOrder)
    {
        ui->comboBox_workOrderList->addItem(workOrder.name, workOrder.workOrderId);
    }
    if (!prodData.workOrderList.vecWorkOrder.isEmpty())
        on_comboBox_workOrderList_currentIndexChanged(0);
}

void MainWindow::on_comboBox_workOrderList_currentIndexChanged(int index)
{
    ui->comboBox_workProcess->clear();
    QString workOrderId = ui->comboBox_workOrderList->currentData().toString();
    if (workOrderId.isEmpty())
        return;

    // 1. 先从本地缓存m_allPdLineData查找对应工单的工序,假数据
    int lineIdx = ui->comboBox_pdLineList->currentIndex();
    int prodIdx = ui->comboBox_pdList->currentIndex();
    if (lineIdx >=0 && prodIdx >=0 && lineIdx < m_allPdLineData.size())
    {
        const PdLineList& line = m_allPdLineData[lineIdx];
        if (prodIdx < line.vecPdList.size())
        {
            const PdList& prod = line.vecPdList[prodIdx];
            // 遍历工单匹配ID
            for (const WorkOrder& wo : prod.workOrderList.vecWorkOrder)
            {
                if (wo.workOrderId == workOrderId)
                {
                    // 填充工序下拉，不走网络
                    for (const WorkProcess& proc : wo.vecProcessId)
                    {
                        ui->comboBox_workProcess->addItem(proc.name, proc.processId);
                    }
                    return;
                }
            }
        }
    }

    // 2. 本地缓存无数据，才调用真实接口
    QString err;
    RefreshProcessCombo(workOrderId, err);
    if (!err.isEmpty())
        ShowMsg("加载工序失败", err);
}

// 登录按钮点击
void MainWindow::on_pushButton_login_clicked()
{
    SetUiLock(true);
    QString errMsg;
    bool loginOk = DoFullLoginFlow(errMsg);
    SetUiLock(false);

    if (!loginOk)
    {
        ShowMsg(("登录失败"), errMsg);
        return;
    }
    ShowMsg(("登录成功"), ("标定程序已启动"));
}

void MainWindow::SlotPostHeartBeatTimeout()
{
    QString heartBeatMsg =  MesHttpPost::Instance()->StationHeartbeat();
    if(heartBeatMsg.isEmpty())
    {
        return;
    }
    ui->label_heartMsg->setText(heartBeatMsg);
}
