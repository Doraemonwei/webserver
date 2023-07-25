#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<error.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/epoll.h>
#include<signal.h>
#include"locker.h"
#include"threadpool.h"
#include"http_conn.h"

// 能保留的最大文件描述符数量，其实就是最大并发
#define MAX_FD 65535
// 一次监听的最大连接数
#define MAX_EVENT_NUMBER 10000 

// 添加信号捕捉
void addsig(int sig, void(handler)(int)){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,NULL);
}

// 添加文件描述符到epoll中,具体实现在http_conn.cpp中
extern void addfd(int epollfd,int fd, bool oneshot);
// 从epoll中删除文件描述符,具体实现在http_conn.cpp中
extern void removefd(int epollfd,int fd, bool oneshot);
// 修改epoll中的文件描述符
extern void modfd(int epollfd,int fd, int ev);

int main(int argc, char* argv[]){
    if(argc<=1){
        printf("请按照如下格式运行:\n");
        printf("%s port_number\n",argv[0]);
        exit(-1);
    }
    // 获取端口号
    int port =atoi(argv[1]);

    // 对SIGPIPE信号进行处理
    addsig(SIGPIPE,SIG_IGN);

    // 创建并初始化线程池
    threadpool<http_conn>* pool=NULL;
    try{
        pool=new threadpool<http_conn>;
    } catch(...){
        exit(-1);
    }

    // 创建数组用于保存所有的客户端信息
    http_conn* users = new http_conn[MAX_FD];

    // 创建监听套接字
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    // 设置端口复用
    int reuse=1;
    setsockopt(listenfd, SOL_SOCKET,SO_REUSEADDR,&reuse, sizeof(reuse));
    // 绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    bind(listenfd,(struct sockaddr*)& address, sizeof(address));

    // 监听
    listen(listenfd,5);

    // 使用epol实现多路复用
    // 创建epoll对象
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd=epoll_create1(5);
    // 将监听的文件描述符添加到epoll中
    addfd(epollfd,listenfd,false);
    http_conn::m_epollfd = epollfd;
    while(true){
        int num = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if((num<0)&&(errno!=EINTR)){
            printf("epoll failure\n");
            break;
        }

        // 循环遍历事件数组
        for(int i=0;i<num;i++){
            int sockfd=events[i].data.fd;
            if(sockfd==listenfd){
                // 有客户端链接进来了
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd,(struct sockaddr*)&client_address, &client_addrlen);
                if(http_conn::m_user_count>=MAX_FD){
                    // 连接数已经满了，告诉客户端服务器正忙
                    close(connfd);
                    continue;
                }
                // 将新的客户的数据初始化并放入数组中
                users[connfd].init(connfd,client_address);
            } else if(events[i].events&(EPOLLHUP|EPOLLRDHUP|EPOLLERR)){
                // 对方异常断开等错误时间，直接关闭连接
                users[sockfd].close_conn();  
            } else if(events[i].events&EPOLLIN){
                // 有信息被写入
                if(users[sockfd].read()){
                    // 一次性把数据都读取成功
                    pool->append(users+sockfd);
                }else{
                    // 读失败了
                    users[sockfd].close_conn();
                }
            } else if(events[i].events&EPOLLOUT){
                // 如果有写事件产生
                if(!users[sockfd].write()){
                    // 一次性写数据失败
                    users[sockfd].close_conn();
                }
            }
        }


    }

    close(epollfd);
    close(listenfd);
    delete[] users;
    delete[] pool;
    return 0;
}