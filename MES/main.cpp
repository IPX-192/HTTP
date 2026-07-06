#include <windows.h>
#include "mainwindow.h"
#include "ModelChangeManager.h"
#include "ProductionTestManager.h"
#include <QApplication>
#include "MesHttpPost_1.h"
#include "CalibTcpTrans.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QMessageBox>
#include <QSettings>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QString appName = QApplication::applicationName();//程序名称
    QString appPath = QApplication::applicationFilePath();// 程序路径
    appPath = appPath.replace("/", "\\");
    QSettings *reg = new QSettings(
                "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                QSettings::NativeFormat);

    QString val = reg->value(appName).toString();// 如果此键不存在，则返回的是空字符串
    if (val != appPath)
        reg->remove(appName);
    //reg->setValue(appName, appPath);// 如果移除的话，reg->remove(applicationName);

    HANDLE m_hMutex = CreateMutex(nullptr, FALSE, L"MES");
    //  检查错误代码
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        //  如果已有互斥量存在则释放句柄并复位互斥量
        CloseHandle(m_hMutex);
        m_hMutex = nullptr;
        QMessageBox::information(nullptr, QString::fromLocal8Bit("提示信息"), QString::fromLocal8Bit("请勿重复打开软件"));
        //  程序退出
        return 0;
    }

    MainWindow w;


    ModelChangeManager model;
    CalibTcpTrans::Instance(&w);
    MesHttpPost::Instance(&w);
    w.show();
     ProductionTestManager p;
    QString str;
    w.LoadAllWorkOrderData(str);


    a.exec();
    return CloseHandle(m_hMutex);
}
