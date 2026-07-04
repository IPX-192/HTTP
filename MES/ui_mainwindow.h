/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.13.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralWidget;
    QLabel *label;
    QComboBox *comboBox_pdLineList;
    QComboBox *comboBox_pdList;
    QLabel *label_2;
    QComboBox *comboBox_workOrderList;
    QLabel *label_3;
    QLabel *label_4;
    QComboBox *comboBox_workProcess;
    QLabel *label_5;
    QLineEdit *lineEdit_user;
    QLineEdit *lineEdit_passwd;
    QLabel *label_6;
    QPushButton *pushButton_login;
    QWidget *widget;
    QHBoxLayout *horizontalLayout;
    QLabel *label_8;
    QLabel *label_heartMsg;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(820, 597);
        QFont font;
        font.setPointSize(15);
        MainWindow->setFont(font);
        MainWindow->setStyleSheet(QString::fromUtf8(""));
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        centralWidget->setStyleSheet(QString::fromUtf8("\n"
"background-color: qlineargradient(spread:pad, x1:0.5, y1:0, x2:0.5, y2:1, stop:0 rgba(188, 231, 251, 255), stop:1 rgba(255, 255, 255, 255))\n"
""));
        label = new QLabel(centralWidget);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(70, 30, 81, 31));
        QFont font1;
        font1.setPointSize(12);
        label->setFont(font1);
        label->setStyleSheet(QString::fromUtf8("background:transparent;"));
        comboBox_pdLineList = new QComboBox(centralWidget);
        comboBox_pdLineList->setObjectName(QString::fromUtf8("comboBox_pdLineList"));
        comboBox_pdLineList->setGeometry(QRect(160, 30, 441, 31));
        QFont font2;
        font2.setPointSize(10);
        comboBox_pdLineList->setFont(font2);
        comboBox_pdLineList->setStyleSheet(QString::fromUtf8(""));
        comboBox_pdList = new QComboBox(centralWidget);
        comboBox_pdList->setObjectName(QString::fromUtf8("comboBox_pdList"));
        comboBox_pdList->setGeometry(QRect(160, 80, 441, 31));
        comboBox_pdList->setFont(font2);
        comboBox_pdList->setStyleSheet(QString::fromUtf8(""));
        label_2 = new QLabel(centralWidget);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setGeometry(QRect(70, 80, 71, 31));
        label_2->setFont(font1);
        label_2->setStyleSheet(QString::fromUtf8("background:transparent;"));
        comboBox_workOrderList = new QComboBox(centralWidget);
        comboBox_workOrderList->setObjectName(QString::fromUtf8("comboBox_workOrderList"));
        comboBox_workOrderList->setGeometry(QRect(160, 130, 441, 31));
        comboBox_workOrderList->setFont(font2);
        comboBox_workOrderList->setStyleSheet(QString::fromUtf8(""));
        label_3 = new QLabel(centralWidget);
        label_3->setObjectName(QString::fromUtf8("label_3"));
        label_3->setGeometry(QRect(70, 130, 81, 31));
        label_3->setFont(font1);
        label_3->setStyleSheet(QString::fromUtf8("background:transparent;"));
        label_4 = new QLabel(centralWidget);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setGeometry(QRect(70, 180, 71, 31));
        label_4->setFont(font1);
        label_4->setStyleSheet(QString::fromUtf8("background:transparent;"));
        comboBox_workProcess = new QComboBox(centralWidget);
        comboBox_workProcess->setObjectName(QString::fromUtf8("comboBox_workProcess"));
        comboBox_workProcess->setGeometry(QRect(160, 180, 441, 31));
        comboBox_workProcess->setFont(font2);
        comboBox_workProcess->setStyleSheet(QString::fromUtf8(""));
        label_5 = new QLabel(centralWidget);
        label_5->setObjectName(QString::fromUtf8("label_5"));
        label_5->setGeometry(QRect(180, 250, 81, 31));
        label_5->setFont(font1);
        label_5->setStyleSheet(QString::fromUtf8("background:transparent;"));
        lineEdit_user = new QLineEdit(centralWidget);
        lineEdit_user->setObjectName(QString::fromUtf8("lineEdit_user"));
        lineEdit_user->setGeometry(QRect(260, 249, 251, 31));
        lineEdit_user->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);"));
        lineEdit_passwd = new QLineEdit(centralWidget);
        lineEdit_passwd->setObjectName(QString::fromUtf8("lineEdit_passwd"));
        lineEdit_passwd->setGeometry(QRect(260, 299, 251, 31));
        lineEdit_passwd->setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);"));
        lineEdit_passwd->setEchoMode(QLineEdit::Password);
        label_6 = new QLabel(centralWidget);
        label_6->setObjectName(QString::fromUtf8("label_6"));
        label_6->setGeometry(QRect(180, 300, 81, 31));
        label_6->setFont(font1);
        label_6->setStyleSheet(QString::fromUtf8("background:transparent;"));
        pushButton_login = new QPushButton(centralWidget);
        pushButton_login->setObjectName(QString::fromUtf8("pushButton_login"));
        pushButton_login->setGeometry(QRect(180, 350, 331, 41));
        QFont font3;
        font3.setPointSize(19);
        pushButton_login->setFont(font3);
        widget = new QWidget(centralWidget);
        widget->setObjectName(QString::fromUtf8("widget"));
        widget->setGeometry(QRect(30, 500, 361, 31));
        horizontalLayout = new QHBoxLayout(widget);
        horizontalLayout->setSpacing(6);
        horizontalLayout->setContentsMargins(11, 11, 11, 11);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        label_8 = new QLabel(widget);
        label_8->setObjectName(QString::fromUtf8("label_8"));

        horizontalLayout->addWidget(label_8);

        label_heartMsg = new QLabel(widget);
        label_heartMsg->setObjectName(QString::fromUtf8("label_heartMsg"));

        horizontalLayout->addWidget(label_heartMsg);

        horizontalLayout->setStretch(0, 1);
        horizontalLayout->setStretch(1, 3);
        MainWindow->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(MainWindow);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        MainWindow->setStatusBar(statusBar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MES", nullptr));
        label->setText(QCoreApplication::translate("MainWindow", "\344\272\247\345\223\201\347\272\277\357\274\232", nullptr));
        label_2->setText(QCoreApplication::translate("MainWindow", "\344\272\247  \345\223\201\357\274\232", nullptr));
        label_3->setText(QCoreApplication::translate("MainWindow", "\345\267\245  \345\215\225\357\274\232", nullptr));
        label_4->setText(QCoreApplication::translate("MainWindow", "\345\267\245  \345\272\217\357\274\232", nullptr));
        label_5->setText(QCoreApplication::translate("MainWindow", "\347\224\250\346\210\267\345\220\215\357\274\232", nullptr));
        lineEdit_passwd->setText(QString());
        label_6->setText(QCoreApplication::translate("MainWindow", "\345\257\206  \347\240\201\357\274\232", nullptr));
        pushButton_login->setText(QCoreApplication::translate("MainWindow", "\347\231\273 \345\275\225", nullptr));
        label_8->setText(QCoreApplication::translate("MainWindow", "MES\350\277\236\346\216\245\347\212\266\346\200\201\357\274\232", nullptr));
        label_heartMsg->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
