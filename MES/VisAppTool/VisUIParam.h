#ifndef VISUIPARAM
#define VISUIPARAM

#include <QObject>
#include <QSettings>
#include "VisAppTool_global.h"

#define ComboBoxDynamics  "comboBoxDynamics"

class VISAPPTOOL_EXPORT VisUIParam : public QObject
{
     Q_OBJECT
public:
    explicit VisUIParam(QObject *parent = nullptr);

    //更新UI数据到参数结构体，并保存到ini文件
    static void SaveUIToIni(QString filename,QObject*uiObj, QObject *paramObj);
    //更新UI数据到参数结构体，并保存到ini文件
    static void SaveUIToIni(QSettings* settings,QObject*uiObj, QObject *paramObj);
    //读取ini文件，更新到UI和参数结构体
    static void LoadIniToUI(QString filename, QObject* uiObj, QObject* paramObj);
    //读取ini文件，更新到UI和参数结构体
    static void LoadIniToUI(QSettings* settings, QObject* uiObj, QObject* paramObj);
    //更新参数结构体数据到UI
    static void UpdateParamToUI(QObject* paramObj, QObject* uiObj);
    //实现QObject子类间属性赋值构造
    static void QObjectCopy(QObject* src, QObject* dst);

signals:

public slots:
};

#endif // VISUIPARAM
