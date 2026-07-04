#ifndef HV_THREAD_H_
#define HV_THREAD_H_

#ifdef __cplusplus
/************************************************
 * HThread
 * Status: STOP,RUNNING,PAUSE
 * Control: start,stop,pause,resume
 * first-level virtual: doTask
 * second-level virtual: run
************************************************/
#include <thread>
#include <atomic>
#include <chrono>
#include <qDebug>

class HThread {
public:
    enum Status {
        STOP,
        RUNNING,
        PAUSE,
    };

    enum SleepPolicy {
        YIELD,
        SLEEP_FOR,
        SLEEP_UNTIL,
        NO_SLEEP,
    };

    HThread() {
        status = STOP;
        status_changed = false;
        dotask_cnt = 0;
        sleep_policy = YIELD;
        sleep_ms = 0;
    }

    virtual ~HThread() {}

    void setStatus(Status stat) {
        status_changed = true;
        status = stat;
    }

    void setSleepPolicy(SleepPolicy policy, uint32_t ms = 0) {
        sleep_policy = policy;
        sleep_ms = ms;
        setStatus(status);
    }

    virtual int start() {
        if (status == STOP) {
			if (thread.joinable())
				thread.detach();
            thread = std::thread([this] {
                if (!doPrepare()) return;
                setStatus(RUNNING);
                run();
                setStatus(STOP);
                if (!doFinish()) return;
            });
        }
		else {
			resume();
		}
		
        return 0;
    }

    virtual int stop() {
        if (status != STOP) {
            setStatus(STOP);
        }
        if (thread.joinable()) {
            thread.join();  // wait thread exit
        }
        return 0;
    }

    virtual int pause() {
        if (status == RUNNING) {
            setStatus(PAUSE);
        }
        return 0;
    }

    virtual int resume() {
        if (status == PAUSE) {
            setStatus(RUNNING);
        }
        return 0;
    }

	bool isRun() { 
		return (status == RUNNING);
	}

    virtual void run() {
        while (status != STOP) {
            while (status == PAUSE) {
                std::this_thread::yield();
            }
            if (status != STOP)
                if (!doTask()) {
                    pause();
                }
            ++dotask_cnt;
            HThread::sleep();
        }
    }

    virtual bool doPrepare() {return true;}
    virtual bool doTask()    {return true;}
    virtual bool doFinish() {return true;}

    std::thread thread;
    std::atomic<Status> status;
    uint32_t dotask_cnt;
protected:
    void sleep() {
        switch (sleep_policy) {
        case YIELD:
            std::this_thread::yield();
            break;
        case SLEEP_FOR:
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            break;
        case SLEEP_UNTIL: {
            if (status_changed) {
                status_changed = false;
                base_tp = std::chrono::system_clock::now();
            }
            base_tp += std::chrono::milliseconds(sleep_ms);
            std::this_thread::sleep_until(base_tp);
        }
            break;
        default:    // donothing, go all out.
            break;
        }
    }

    SleepPolicy sleep_policy;
    uint32_t    sleep_ms;
    // for SLEEP_UNTIL
    std::atomic<bool> status_changed;
    std::chrono::system_clock::time_point base_tp;
};
#endif

#endif // HV_THREAD_H_
