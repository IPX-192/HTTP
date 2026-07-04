#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <QObject>
#include <future>
#include <QMap>
#include "hthreadpool.h"
#include "VisAppTool_global.h"

#define  GlobalThreadPool (VisAppThreadPool::instance())
#define  ExceptionErr  999

class ThreadPoolPriv;
class VISAPPTOOL_EXPORT VisAppThreadPool : public QObject
{
    Q_OBJECT
public:
    static VisAppThreadPool* instance();
    static void exitInstance();

protected:
    explicit VisAppThreadPool(QObject *parent = nullptr);
    ~VisAppThreadPool();

public:
    //清空任务
    void Clear();
    //提交任务(函数返回值为int,需手动WaitTask)
	template<class Fn, class ...Args>
	void  Commit_Topic(QString topic, Fn&& fn, Args && ...args){
		std::future<int>task = m_threadpool->commit(fn, std::forward<Args>(args)...);
        m_mutexTask.lock();
		m_task[topic].push_back(std::move(task));
        m_mutexTask.unlock();
	};
	//等待任务完成
    int  WaitTask(QString topic);
	//不用管执行结果
	template<class Fn, class ...Args>
	void  Commit(Fn&& fn, Args && ...args){
		m_threadpool->commit(fn, std::forward<Args>(args)...);
	};

protected:
    static VisAppThreadPool* s_pInstance;
    static std::mutex s_mutex;
	HThreadPool* m_threadpool = nullptr;
    std::map<QString, std::vector<std::future<int>>>m_task;
    std::mutex m_mutexTask;
    std::thread::id m_initThreadID;

signals:

public slots:
};

#endif // THREADPOOL_H
