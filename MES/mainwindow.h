#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QVector>
#include <QTimer>
#include "MesHttpPost.h"
#include "ControlCenterHttpApi.h"

namespace Ui {
class MainWindow;
}

const int HEARTBEAT_INTERVAL = 5000;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public:
    bool LoadAllWorkOrderData(QString& errMsg);


signals:
    //通知其他类进行治具检测和自检
    void SignalCheckFixtureNeedAndSelfCheck();


private slots:
    void on_comboBox_pdLineList_currentIndexChanged(int index);
    void on_comboBox_pdList_currentIndexChanged(int index);
    void on_comboBox_workOrderList_currentIndexChanged(int index);
    void on_pushButton_login_clicked();

private slots:
    void SlotPostHeartBeatTimeout();

private:
    // 工具函数
    void ShowMsg(const QString& title, const QString& text);
    void SetUiLock(bool lock);

    bool RefreshProcessCombo(const QString& workOrderId, QString& errMsg);
    bool DoFullLoginFlow(QString& errMsg);

private:
    Ui::MainWindow *ui;
    QMessageBox* m_waitMessageBox;
    QVector<PdLineList> m_allPdLineData;
    bool g_success = false;
    QTimer m_heartTimer;         //发送心跳定时器
};

#endif // MAINWINDOW_H
