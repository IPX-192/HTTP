/*
Author : Wangwei
Data    : 2025-04-22
Version : 1.0.1
*/

#ifndef VISAPPBUS_H
#define VISAPPBUS_H

#include <QObject>
#include <QByteArray>
#include <QVariant>
#include <QSharedPointer>
#include "VisAppTool_global.h"

class VisAppBusPrivate;
class VISAPPTOOL_EXPORT VisAppBus : public QObject
{
    Q_OBJECT
public:
    //须连接加密狗
    static bool install();

    //阻塞调用
    template<class ...Args>
    static int sendEvent_Topic(QString topic,QByteArray eventName, Args&& ... args) {
        if (topic.isEmpty()) return -10000;
        if (eventName.isEmpty()) return -10001;
        auto argsNum = sizeof...(args);
        Q_ASSERT_X(argsNum <= 10, "publishEvent", "publishEvent argv number not greater than 10");

        QList<QGenericArgument> list;
        QList<QByteArray> typeNames;
        std::initializer_list<int32_t> {(getList(typeNames, list, args), 0)...};
        return sendReq(topic,eventName, list, typeNames);
    }

    //异步调用
    template<class ...Args>
    static void postEvent_Topic(QString topic,QByteArray eventName, Args&& ... args) {
        if (eventName.isEmpty()) return;
        auto argsNum = sizeof...(args);
        Q_ASSERT_X(argsNum <= 10, "postEvent", "publishEvent argv number not greater than 10");
        QList< QSharedPointer<QVariant> > list;
        QList<QByteArray> typeNames;
        std::initializer_list<int32_t> {(getList(typeNames, list, args), 0)...};
        postReq(topic, eventName, list, typeNames);
    }


    //阻塞调用
    template<class ...Args>
    static int sendEvent(QByteArray eventName, Args&& ... args) {
        if (eventName.isEmpty()) return -10001;
        auto argsNum = sizeof...(args);
        Q_ASSERT_X(argsNum <= 10, "publishEvent", "publishEvent argv number not greater than 10");

        QList<QGenericArgument> list;
        QList<QByteArray> typeNames;
        std::initializer_list<int32_t> {(getList(typeNames, list, args), 0)...};
        return sendReq(eventName, list, typeNames);
    }

    //直接调用
    template<class ...Args>
    static int sendEventDirect(QByteArray eventName, Args&& ... args) {
        if (eventName.isEmpty()) return -10001;
        auto argsNum = sizeof...(args);
        Q_ASSERT_X(argsNum <= 10, "publishEvent", "publishEvent argv number not greater than 10");

        QList<QGenericArgument> list;
        QList<QByteArray> typeNames;
        std::initializer_list<int32_t> {(getList(typeNames, list, args), 0)...};
        return sendReqDirect(eventName, list, typeNames);
    }

    //异步调用
    template<class ...Args>
    static void postEvent(QByteArray eventName, Args&& ... args) {
        if (eventName.isEmpty()) return;
        auto argsNum = sizeof...(args);
        Q_ASSERT_X(argsNum <= 10, "postEvent", "publishEvent argv number not greater than 10");
        QList< QSharedPointer<QVariant> > list;
        QList<QByteArray> typeNames;
        std::initializer_list<int32_t> {(getList(typeNames, list, args), 0)...};
        postReq(eventName, list, typeNames);
    }

    //订阅事件: 主题+事件名+对象
    static bool subscibeEvent_Topic(QString topic, QByteArray eventName, QObject* listener);
    static bool UnsubscribeEvent_Topic(QString topic, QByteArray eventName,QObject* listener);
    static bool UnsubscribeEvent_Topic(QString topic, QObject* listener);
    //订阅事件: 对象+事件名
    static bool subscibeEvent(
        QObject* listener, QByteArray eventName);
    static bool UnsubscribeEvent(QObject* listener, QByteArray eventName);
    static bool UnsubscribeEvent(QObject* listener);
    static bool show_detail;

private:
    template<class T>
    static void getList(QList<QByteArray> &typeNames,QList<QGenericArgument> &list, T &&t) {
        typeNames << QByteArray(typeid(t).name());
        list << Q_ARG(T, t);
    }
    template<class T>
    static void getList(QList<QByteArray>& typeNames,QList< QSharedPointer<QVariant> >& list, T&& t) {
        typeNames << QByteArray(typeid(t).name());
        QSharedPointer<QVariant> ptr(new QVariant());
        ptr->setValue(t);
        list << ptr;
    }
    static int   sendReq(QString topic, QByteArray eventName, QList<QGenericArgument>& list, QList<QByteArray>& typeNames);
    static void  postReq(QString topic, QByteArray eventName, QList<QSharedPointer<QVariant>>& list, QList<QByteArray>& typeNames);
    static int   sendReq(QByteArray eventName, QList<QGenericArgument>& list, QList<QByteArray>& typeNames);
    static void  postReq(QByteArray eventName, QList<QSharedPointer<QVariant>>&list, QList<QByteArray>& typeNames);
    static int   sendReqDirect(QByteArray eventName, QList<QGenericArgument>& list, QList<QByteArray>& typeNames);

protected:
    static QScopedPointer<VisAppBusPrivate>d_ptr;

signals:

public slots:
};

#endif // VISAPPBUS_H
