#include "http_conn.h"

// 所有的socket都被注册到同一个epoll对象中,初始化
int http_conn:: m_epollfd=-1;
// 统计用户的数量，初始化
int http_conn::m_user_count=0;

// 设置文件描述符为非阻塞
void setnonblocking(int fd){
    int old_flag = fcntl(fd,F_GETFL);
    int new_flag = old_flag|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_flag);
}


// 添加文件描述符到epoll中,具体实现
void addfd(int epollfd,int fd, bool one_shot){
    epoll_event event;
    event.data.fd=fd;
    event.events = EPOLLIN | EPOLLRDHUP|EPOLLET;
    if(one_shot){
        event.events|EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);

    // 设置文件描述符非阻塞
    setnonblocking(fd);
}

// 从epoll中删除文件描述符,具体实现
void removefd(int epollfd,int fd){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

// 修改epoll中的文件描述符
void modfd(int epollfd,int fd, int ev){
    epoll_event event;
    event.data.fd=fd;
    event.events = ev|EPOLLONESHOT|EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

// 初始化新接受到的客户端的信息
void http_conn::init(int sockfd, const sockaddr_in &addr){
    m_sockfd = sockfd;
    m_address = addr;

    // 设置端口复用
    int reuse=1;
    setsockopt(m_sockfd, SOL_SOCKET,SO_REUSEADDR,&reuse, sizeof(reuse));

    // 添加到epoll对象中，就不用再main中写了
    addfd(m_epollfd,m_sockfd,true);
    m_user_count++; // 用户数目加1
    init();
}

// 初始化连接其余的数据
void http_conn::init(){
    m_check_state = CHECK_STATE_REQUESTLINE; // 主状态机的初始化状态为解析请求首行
    m_checked_index = 0; // 当前正在解析的行的起始位置
    m_start_line = 0;

    m_read_index = 0;
    m_method  =GET;
    m_url=0;
    m_version=0;
    m_linger=false;
    m_host=0;
    bzero(m_read_buf,READ_BUFFER_SIZE);
}


// 关闭连接
void http_conn::close_conn(){
    if(m_sockfd!=-1){
        removefd(m_epollfd,m_sockfd);
        m_sockfd=-1;
        m_user_count--;
    }
}


// 非阻塞读取数据
bool http_conn::read(){
    if(m_read_index>=READ_BUFFER_SIZE){
        return false;
    }
    
    // 读取到的字节
    int bytes_read=0;
    while(true){
        bytes_read = recv(m_sockfd, m_read_buf+m_read_index, READ_BUFFER_SIZE-m_read_index, 0);
        if(bytes_read==-1){
            if(errno==EAGAIN||errno==EWOULDBLOCK){
                // 没有数据
                break;
            }
            return false;
        }else if(bytes_read==0){
            // 对方关闭连接
            return false;
        }
        m_read_index+=bytes_read;
    }
    printf("读取到了数据:%s\n",m_read_buf);
    return true;
}
// 非阻塞写数据
bool http_conn::write(){
    printf("一次性写完了数据\n");
    return true;
}

// 响应并处理客户端的请求,处理http请求的入口函数
void http_conn::process(){
    // 解析http请求
    HTTP_CODE read_ret = process_read();
    if(read_ret==NO_REQUEST){
        modfd(m_epollfd,m_sockfd,EPOLLIN);
        return;
    }
    printf("HTTP请求解析...\n");
    // 生成响应
}

// 解析请求信息
http_conn::HTTP_CODE http_conn::process_read(){
    // 主状态机
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;
    // 一行一行的解析
    while(((m_start_line==CHECK_STSTE_CONTENT)&& (line_status==LINE_OK))||((line_status=parse_line())==LINE_OK)){
        text = get_line(); // 获取一行数据
        m_start_line = m_checked_index;
        printf("解析到一行HTTP信息: %s\n",text);
        switch(m_check_state){
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line(text);
                if(ret==BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STSTE_HEADER:
            {
                ret = parse_request_headers(text);
                if(ret==BAD_REQUEST){
                    return BAD_REQUEST;
                }else if(ret==GET_REQUEST){
                    return do_request(text); // 真正去解析请求
                }
                break;
            }
            case CHECK_STSTE_CONTENT:
            {
                ret = parse_request_content(text);
                if(ret==GET_REQUEST){
                    return do_request(text);
                }
                line_status = LINE_OPEN;
                break;

            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
        return NO_REQUEST;
    }
    

}

// 解析请求首行,最终要得到请求的方法和请求的目标url,HTTP的版本
http_conn::HTTP_CODE http_conn::parse_request_line(char* text){
    // 数据长这样：
    // GET /index.html HTTP/1.1
    m_url = strpbrk(text," \t");

    *m_url++='\0';

    char* method  =text;
    if(strcasecmp(method,"GET")==0){
        m_method=GET;
    }else{
        return BAD_REQUEST;
    }

    m_version = strpbrk(m_url," \t");
    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++='\0';

    if(strcasecmp(m_version,"HTTP/1.1")!=0){
        return BAD_REQUEST;
    }

    if(strncasecmp(m_url,"http://",7)==0){
        m_url+=7;
        m_url = strchr(m_url,'/');
    }
    if(!m_url||m_url[0]!='/'){
        return BAD_REQUEST;
    }
    m_check_state = CHECK_STSTE_HEADER; // 请求首行解析完毕，将状态机状态设置为解析请求头

    return NO_REQUEST;
} 

// 解析请求头
http_conn::HTTP_CODE http_conn::parse_request_headers(char* text){

} 

// 解析请求请求体
http_conn::HTTP_CODE http_conn::parse_request_content(char* text){

} 

// 获取具体的一行数据之后交给上面解析的函数
http_conn::LINE_STATUS http_conn::parse_line(){
    // 通过\r\n特殊标记来区分一行
    char temp;
    for(;m_checked_index<m_read_index;++m_checked_index){
        temp = m_read_buf[m_checked_index];
        if(temp=='\r'){
            if (m_checked_index+1==m_read_index){
                return LINE_OPEN;
            }else if(m_read_buf[m_checked_index+1]=='\n'){
                m_read_buf[m_checked_index++]='\0';
                m_read_buf[m_checked_index++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }else if(temp=='\n'){
            if((m_checked_index>1)&&(m_read_buf[m_checked_index-1]=='\r')){
                m_read_buf[m_checked_index-1]='\0';
                m_read_buf[m_checked_index++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        return LINE_OPEN;
    }
} 

// 具体的解析某一句
 http_conn::HTTP_CODE http_conn::do_request(char* text){

 }