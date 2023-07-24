#ifndef THREADPOOL_H
#define TTHREADPOOL_H

#include<pthread.h>
#include<list>
#include "locker.h"
#include<exception>
#include<cstdio>

// 线程池类，定义成模板类更加的通用
template<typename T>
class threadpool{
public:
    threadpool(int thread_number=8,int max_requests=10000);
    ~threadpool();
    bool append(T * request);     

private:
    // 线程的数量
    int m_thread_number;
    // 使用数组装线程
    pthread_t *m_threads;

    // 请求队列中最多允许的等待请求的数量
    int m_max_requests;

    // 请求队列
    std::list< T* > m_workqueue;

    // 互斥锁
    locker m_queuelocker;

    // 信号量,用来判断是否有任务需要处理
    sem m_queuestat;

    // 是否结束线程
    bool m_stop;
};

// 具体的实现
// 构造函数
template<typename T>
threadpool<T>::threadpool(int thread_number=8,int max_requests=10000) :
    m_thread_number(thread_number),m_max_requests(max_requests),
    m_stop(false),m_thread(NULL){
        if((thread_number<=0)||(max_requests<=0)){
            throw std::exception();
        }
        m_threads = new pthread_t[m_thread_number]
        // 数组创建成功
        if(!m_threads){
            throw std::exception();
        }
        // 创建thread_number个线程，并将他们设置为线程脱离
        for(int i=0;i<thread_number;i++){
            printf("正在创建第%d个线程\n",i);
            if(pthread_create(m_threads+i,NULL,worker,NULL)!=0){
                delete [] m_threads;
                throw std::exception();
            }
            // 设置线程分离
            pthread_detach(m_threads[i]){
                delete [] m_threads;
                throw std::exception();
            }   
        }
    }

// 析构函数




#endif