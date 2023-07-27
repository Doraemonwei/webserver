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
#include<errno.h>
#include"locker.h"
#include<sys/uio.h>
#include<string.h>

class http_conn{
public:
    // 所有的socket都被注册到同一个epoll对象中
    static int m_epollfd;
    // 统计用户的数量
    static int m_user_count;
    // 读取数据的缓冲的大小
    static const int READ_BUFFER_SIZE=2048;
    // 写数据的缓冲的大小
    static const int WRITRE_NUFFER_SIZE=2048;


    // 有限状态机的状态，响应的状态信息
    // HTTP请求方法，该项目只支持GET请求
    enum METHOD{GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT};

    /* 解析客户端发来的请求时主机自己的状态
    CHECK_STATE_REQUESTLINE:当前正在分析请求
    CHECK_STSTE_HEADER: 当前正在分析头部数据
    CHECK_STSTE_CONTENT: 当前正在解析请求体
    */
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE=0,CHECK_STSTE_HEADER,CHECK_STSTE_CONTENT};

    /*服务器处理HTTP请求的可能结果
    NO_REQUEST: 请求不完整，需要继续读取客户端发来的数据
    GET_REQUEST: 获得了一个用户发来的完整的请求
    BAD_REQUEST: 用户请求的语法错误
    NO_RESOURCE:  服务器没有用户请求的资源
    FORBIDDEN_REQUEST: 用户没有该资源的访问权限
    FILE_REQUEST: 获取请求的文件成功
    INTERNAL_ERROR: 服务器内部出错
    CLOSED_CONNECTION: 客户端已经关闭连接
    */
    enum HTTP_CODE {NO_REQUEST=0,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION};

    // 从状态机的三种可能状态，行的读取状态
    // 1、读取到一个完整的行 2、行出错 3、行数据尚不完整
    enum LINE_STATUS{LINE_OK=0, LINE_BAD, LINE_OPEN};



    // 构造与析构函数
    http_conn(){};
    ~http_conn(){};
    // 响应并处理客户端的请求
    void process();

    // 初始化新接受到的客户端的信息
    void init(int sockfd, const sockaddr_in &addr);

    // 关闭连接'
    void close_conn();

    // 非阻塞读取数据
    bool read();
    // 非阻塞写数据
    bool write();

    // 解析请求信息
    HTTP_CODE process_read();
    HTTP_CODE parse_request_line(char* text); // 解析请求首行
    HTTP_CODE parse_request_headers(char* text); // 解析请求头
    HTTP_CODE parse_request_content(char* text); // 解析请求请求体

    LINE_STATUS parse_line(); // 获取具体的一行数据之后交给上面解析的函数

    

private:
    int m_sockfd; // 该http连接的socket
    sockaddr_in m_address; // 通信的socket地址
    char m_read_buf[READ_BUFFER_SIZE];  // 读缓冲区
    int m_read_index; // 读缓冲区中已经读取进来的最后一个字节的下标，因为可能一次读不完，缓冲区装不下，需要来回运
    char m_write_buf[WRITRE_NUFFER_SIZE];

    int m_checked_index; // 当前正在读的字符在缓冲区中的位置
    int m_start_line; // 当前正在解析的行的起始位置

    char* m_url;  // 请求的目标文件的文件名
    char* m_version; // 请求的http版本
    METHOD m_method; // 请求方法
    char* m_host; // 主机名
    bool m_linger; //是否保持连接
    




    CHECK_STATE m_check_state;  // 主状态机当前的状态

    void init();// 初始化连接其余的数据

    // 或许接下来要读取的一行数据
    char* get_line(){return m_read_buf+m_start_line;}

    // 具体处理数据
    HTTP_CODE do_request(char* text);

};

#endif