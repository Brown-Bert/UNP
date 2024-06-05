/**
 * @file macro.h
 * @brief 各种宏变量以及系统需要调参的变量文件
 */

#ifndef MACRO_H_
#define MACRO_H_

#define my_int int32_t  // 自定义int类型
// #define SERVERIP "192.168.1.89"  // 中继器ip
#define SERVERIP "192.168.1.89"  // 中继器ip
// #define SERVERIP "127.0.0.1"  // 中继器ip(本地测试)
#define SERVERPORT 8888           // 中继器端口
#define REVENTSSIZE 10240         // 监听事件的最大数量
#define BUFSIZE 2048              // 缓冲区的大小
#define serverPortStart 40000     // 服务器起始端口
#define serverNum 1               // 开启100台服务器
#define serverIp "192.168.1.236"  // 暂时只考虑所有服务器的ip相同
// #define serverIp "127.0.0.1" // 本地测试
#define searchPort 9999
#define FLAG \
  true  // true : 允许服务器接收来自多个客户端的连接请求 false :
        // 不允许服务器接收多个客户端的请求

#endif