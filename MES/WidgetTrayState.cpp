#include "WidgetTrayState.h"
#include "ui_WidgetTrayState.h"
#include "ParamManager.h"
#include <QPainter>

QCustomItemDelegate::QCustomItemDelegate(QObject* parent)
    :QStyledItemDelegate(parent)
{

}

void QCustomItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    opt.rect.adjust(2, 2, -2, -2);
    QColor color = index.data(Qt::BackgroundColorRole).value<QColor>();
    painter->fillRect(opt.rect, color);
    QStyledItemDelegate::paint(painter, opt, index);
}

WidgetTrayState::WidgetTrayState(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WidgetTrayState)
{
    ui->setupUi(this);

    VisAppBus::subscibeEvent(this, "TrayUpdate");
    VisAppBus::subscibeEvent(this, "TrayHoleUpdate");
    VisAppBus::subscibeEvent(this, "ShowTayBarcode");

    Init();
}

WidgetTrayState::~WidgetTrayState()
{
    delete ui;
}

int WidgetTrayState::event_TrayUpdate(int flag, bool exits)
{
    QTableWidget *widget = ui->tableWidgetOKTray1;
    QLabel *label = ui->labelOKTray1;
    if (flag == 0)
    {
        TabelInit();
    }
    else if (flag == 1)
    {
        widget = ui->tableWidgetOKTray1;
        label = ui->labelOKTray1;
    }
    else if (flag == 2)
    {
        widget = ui->tableWidgetOKTray2;
        label = ui->labelOKTray2;
    }
    else if (flag == 3 || flag == 4)
    {
        widget = ui->tableWidgetNgBlankTray;
        label = ui->labelNgBlankTray;
    }
    if (flag > 0)
    {
        for (int i = 0; i < widget->rowCount(); i++)
        {
            for (int j = 0; j < widget->columnCount(); j++)
            {
                QTableWidgetItem *item = widget->item(i, j);
                item->setText(QString::number(i * GlobalParam->recipeTray.trayColsHolder + j + 1));

                if (exits)
                {
                    if (flag == 3)
                        continue;
                    label->setText(QString(u8"料盘%1(有盘)").arg(flag));
                    label->setStyleSheet("background-color: rgb(0, 255, 0);");
                    item->setBackgroundColor(QColor(0, 255, 0));
                }
                else
                {
                    if (flag != 3)
                    {
                        label->setText(QString(u8"料盘%1(无盘)").arg(flag));
                        label->setStyleSheet("background-color: rgb(219, 219, 219);");
                    }
                    item->setBackgroundColor(QColor(125, 125, 125));
                }
            }
        }
    }
    return 0;
}

int WidgetTrayState::event_TrayHoleUpdate(TrayType type, bool blankFlag, bool ok, int number, QString text)
{
    int row = 0, col = 0;
    if (type < NGTray)
    {
        QTableWidget *tableWidget = ui->tableWidgetOKTray1;
        if (type == OKTray)
            tableWidget = ui->tableWidgetOKTray2;
        row = number / GlobalParam->recipeTray.trayRowsHolder;
        col = number % GlobalParam->recipeTray.trayColsHolder;
        if (!blankFlag)
        {
            if (ok)
            {
                tableWidget->item(row, col)->setBackgroundColor(QColor(125, 125, 125));
                tableWidget->item(row, col)->setText(QString::number(number + 1));
            }
            else
            {
                tableWidget->item(row, col)->setText(text);
                tableWidget->item(row, col)->setBackgroundColor(QColor(255, 0, 0));
            }
        }
        else
        {
            tableWidget->item(row, col)->setBackgroundColor(QColor(255, 170, 0));
            tableWidget->item(row, col)->setText(QString::number(number + 1));
        }
    }
    else
    {
        /* row = number / GlobalParam->recipeTray.ngTrayCols;
        col = number % GlobalParam->recipeTray.ngTrayCols;*/
        ui->tableWidgetNgBlankTray->item(row, col)->setBackgroundColor(QColor(255, 0, 0));
        ui->tableWidgetNgBlankTray->item(row, col)->setText(text);
    }
    return 0;
}

int WidgetTrayState::event_ShowTayBarcode(TrayType type, QString barcode)
{
    if (type==OKTray)
        ui->labelOKTray2->setText(ui->labelOKTray1->text() + barcode);
    else
        ui->labelOKTray1->setText(ui->labelOKTray1->text() + barcode);

    return 0;
}

void WidgetTrayState::Init()
{
    TabelInit();
}

void WidgetTrayState::TabelInit()
{
    auto delegate = new QCustomItemDelegate(this);
    //OK上料盘
    for (int index = 0; index < 2; index++)
    {
        QTableWidget *widget = ui->tableWidgetOKTray1;
        if (index == 1)
            widget = ui->tableWidgetOKTray2;

        widget->clear();
        widget->setItemDelegate(delegate);
        /* widget->setRowCount(GlobalParam->recipeTray.testTrayRows);
        widget->setColumnCount(GlobalParam->recipeTray.testTrayCols);*/
        widget->verticalHeader()->hide();
        widget->horizontalHeader()->hide();
        widget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        widget->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        widget->setWordWrap(true);
        for (int i = 0; i < widget->rowCount(); i++)
        {
            for (int j = 0; j < widget->columnCount(); j++)
            {
                QTableWidgetItem *item = new QTableWidgetItem();
                widget->setItem(i, j, item);
                item->setFont(QFont("song", 12));
                item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                item->setFlags(Qt::NoItemFlags);
                // int indexTable = i * GlobalParam->recipeTray.testTrayCols + j + 1;
                //item->setText(QString::number(indexTable));
                item->setBackgroundColor(QColor(125, 125, 125));
                item->setTextColor(QColor(0, 0, 0));
            }
        }
    }

    //NG料盘
    ui->tableWidgetNgBlankTray->clear();
    ui->tableWidgetNgBlankTray->setItemDelegate(delegate);
    /*ui->tableWidgetNgBlankTray->setRowCount(GlobalParam->recipeTray.ngTrayRows);
    ui->tableWidgetNgBlankTray->setColumnCount(GlobalParam->recipeTray.ngTrayCols);*/
    ui->tableWidgetNgBlankTray->verticalHeader()->hide();
    ui->tableWidgetNgBlankTray->horizontalHeader()->hide();
    ui->tableWidgetNgBlankTray->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidgetNgBlankTray->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidgetNgBlankTray->setWordWrap(true);
    for (int i = 0; i < ui->tableWidgetNgBlankTray->rowCount(); i++)
    {
        for (int j = 0; j < ui->tableWidgetNgBlankTray->columnCount(); j++)
        {
            QTableWidgetItem *item = new QTableWidgetItem();
            ui->tableWidgetNgBlankTray->setItem(i, j, item);
            item->setFont(QFont("song", 11));
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            item->setFlags(Qt::NoItemFlags);
            //item->setText(QString::number(i * GlobalParam->recipeTray.ngTrayCols + j + 1));
            item->setBackgroundColor(QColor(125, 125, 125));
            item->setTextColor(QColor(0, 0, 0));
        }
    }
}
