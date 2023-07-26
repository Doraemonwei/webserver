#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include<sys/epoll.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<stdarg.h>
#include<error.h>
#include"locker.h"
#include<sys/uio.h>

class http_conn{
public:
    // 所有的socket都被注册到同一个epoll对象中
    static int m_epollfd;
    // 统计用户的数量
    static int m_user_count;

    // 构造与析构函数
    http_conn(){};
    ~http_conn(){};
    // 响应并处理客户端的请求
    void process();

    // 初始化新接受到的客户端的信息
    void init(int sockfd, const sockaddr_in &addr);

    // 关闭连接
    void close_conn();

    // 非阻塞读取数据
    bool read();
    // 非阻塞写数据
    bool write();

private:
    int m_sockfd; // 该http连接的socket
    sockaddr_in m_address; // 通信的socket地址
};

#endif