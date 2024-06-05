#ifndef UTILS_H_
#define UTILS_H_

#include "base.hpp"
#include "macro.hpp"

std::string timeToStr(std::chrono::system_clock::time_point
                          timePoint);  // 时间格式转换成字符串格式

std::chrono::system_clock::time_point strToTime(
    const std::string& timeStr);  // 把字符串转换成时间格式

void serializeStruct(const Message& message,
                     char* buffer);  // 序列化结构体为字节流

void deserializeStruct(const char* tmp,
                       Message& message);  // 反序列化字节流为结构体

my_int sendMessage(my_int socket_d,
                   Message& mt);  // 中继服务器使用的发送消息的工具函数

void analysis(char* buffer, my_int bytesRead);  // 解析信息

void searchClient();  // 查询servers的程序，输出servers的信息
void killThread();    // 中继器自己杀死这个分离的线程

#endif