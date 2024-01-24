#ifndef SOCKET_H_
#define SOCKET_H_

#define my_size_t int

#define BUFSIZE 1024

class Socket
{
public:
    // Socket();
    // ~Socket();
    // virtual void mySocket(); // 创建套接字
    void myBind(); // 绑定本地地址
    void mySetsockopt(); // 设置服务器的地址是可以重复使用
public:
    my_size_t socket_d; // 描述符字
    my_size_t port;
};

void sendData(my_size_t fd, const char* strs, my_size_t flag); // 发送数据
my_size_t readData(my_size_t fd, char *buf); // 读取数据

class Client : public Socket
{
public:
    Client(my_size_t port);
    ~Client();
    void mySocket();
    void myConnect(); // 客户端连接服务器
};

class Server : public Socket
{
public:
    Server(my_size_t port);
    ~Server();
    void mySocket();
    void myListen(); // 服务器监听
    void myAccept(); // 接受来自客户端的连接请求
};

#endif