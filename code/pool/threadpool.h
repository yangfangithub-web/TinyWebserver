#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
#include <assert.h>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8):pool_(std::make_shared<Pool>()){
        assert(threadCount >0);
        for (size_t i = 0;i < threadCount; i++) {
            std::thread([pool = pool_] {
                std::unique_lock<std::mutex> locker(pool->mtx);
                while(true) {
                        if(!pool->tasks.empty()) {
                            auto task = std::move(pool->tasks.front());//取出第一个task
                            pool->tasks.pop();
                            locker.unlock();//已经把任务取出来了，所以可以提前解锁了
                            task();//执行task
                            locker.lock();//马上又要取任务了,上锁
                        } 
                        else if(pool->isClosed) break;//task为空时，如果有停止标志，退出循环
                        else pool->cond.wait(locker);
                }
            }).detach();
        }
    }
    

    ThreadPool()=default;

    ThreadPool(ThreadPool&&) = default;

    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->isClosed=true;
        }
        pool_->cond.notify_all();
    }

    template<class F>
    void AddTask(F&& task) {
        {
            
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();
        
    }

private:
    
    struct Pool {
        std::mutex mtx;
        std::condition_variable cond;
        bool isClosed;
        std::queue<std::function<void()>> tasks;//由任务函数组成的队列，函数类型为void()
    };
    std::shared_ptr<Pool> pool_;

    
};




#endif