#ifndef WidgetTrayState_H
#define WidgetTrayState_H

#include <QWidget>
#include <QTimer>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include "ParamDef.h"

namespace Ui {
class WidgetTrayState;
}

class QCustomItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    QCustomItemDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};


class WidgetTrayState : public QWidget
{
    Q_OBJECT

public:
    explicit WidgetTrayState(QWidget *parent = nullptr);
    ~WidgetTrayState();

public slots:
    //flag  0 更新料盘大小 1 更新料盘1状态 2 更新料盘2状态 3 更新NG料盘状态左列 3 更新NG料盘状态右列
    int event_TrayUpdate(int flag, bool exits);
    //trayIndex  0 上料盘1 1 上料盘2 2 NG料盘  blankFlag true 下料模组 ok 测试成功 number 数量 text 异常文本
    int event_TrayHoleUpdate(TrayType type, bool blankFlag, bool ok, int number, QString text);
    //加载托盘码 0 三角料盘左 1 三角料盘右  2 下料机料盘
    int event_ShowTayBarcode(TrayType type, QString barcode);

private:
    void Init();
    //表格大小
    void TabelInit();

private:
    Ui::WidgetTrayState *ui;
};

#endif // WidgetTrayState_H
