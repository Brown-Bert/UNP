#include "utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <iomanip>
#include <mutex>

#include "log.hpp"

/**
  @brief 把时间格式转换成字符串格式，方便在网络中传输
  @param timPoint 时间格式
  @return std::string 返回字符串
*/
std::string timeToStr(std::chrono::system_clock::time_point timePoint) {
  // 将时间点转换为时间结构
  std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
  std::tm timeInfo;
  localtime_r(&time, &timeInfo);

  // 获取毫秒和微秒级时间间隔
  auto duration = timePoint.time_since_epoch();
  auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration) % 1000;
  auto microseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(duration) % 1000000;

  // 格式化时间结构为字符串
  char buffer[80];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);

  // 将毫秒和微秒追加到字符串
  std::stringstream ss;
  ss << buffer << '.' << std::setfill('0') << std::setw(3)
     << milliseconds.count() << std::setw(6) << microseconds.count();
  std::string result = ss.str();

  return result;
}

/**
  @brief 把字符串转换成时间格式
  @param timStr 字符串格式的时间
  @return std::chrono::system_clock::time_point 时间格式
*/
std::chrono::system_clock::time_point strToTime(const std::string &timeStr) {
  std::tm timeInfo = {};
  std::istringstream ss(timeStr);
  ss >> std::get_time(&timeInfo, "%Y-%m-%d %H:%M:%S");

  // 提取毫秒和微秒字段
  std::string subSeconds =
      timeStr.substr(19);  // 提取秒及后面的小数点及毫秒微秒部分
  size_t dotPos = subSeconds.find('.');
  size_t length = subSeconds.length();
  int milliseconds = 0;
  int microseconds = 0;
  if (dotPos != std::string::npos) {
    std::string millisecondsStr =
        subSeconds.substr(dotPos + 1, 3);  // 提取毫秒部分
    std::string microsecondsStr =
        subSeconds.substr(dotPos + 4, 3);  // 提取微秒部分
    milliseconds = std::stoi(millisecondsStr);
    microseconds = std::stoi(microsecondsStr);
  }

  // 设置时间结构的毫秒和微秒字段
  timeInfo.tm_sec += milliseconds / 1000;     // 将毫秒转换为秒
  timeInfo.tm_gmtoff += microseconds * 1000;  // 将微秒转换为纳秒

  // 将时间结构转换为时间点
  std::time_t time = std::mktime(&timeInfo);
  std::chrono::system_clock::time_point timePoint =
      std::chrono::system_clock::from_time_t(time);

  return timePoint;
}

/**
  @brief 序列化结构体为字节流
  @param message 结构体格式的消息
  @param buffer 序列化之后的消息存入buffer缓冲区中
*/
void serializeStruct(const Message &message, char *buffer) {
  // 序列化发送方ip字符串成员的长度和字符数据
  int messageLength = message.sourceIp.size();
  memcpy(buffer, &messageLength, sizeof(my_int));
  buffer += sizeof(my_int);
  memcpy(buffer, message.sourceIp.c_str(), messageLength);
  buffer += messageLength;

  // 序列化发送方port
  memcpy(buffer, &message.sourcePort, sizeof(my_int));
  buffer += sizeof(my_int);

  // 序列化接收方ip字符串成员的长度和字符数据
  messageLength = message.desIp.size();
  memcpy(buffer, &messageLength, sizeof(my_int));
  buffer += sizeof(my_int);
  memcpy(buffer, message.desIp.c_str(), messageLength);
  buffer += messageLength;

  // 序列化发送方port
  memcpy(buffer, &message.desPort, sizeof(my_int));
  buffer += sizeof(my_int);

  // 序列化包的大小
  memcpy(buffer, &message.packageSize, sizeof(my_int));
  buffer += sizeof(my_int);

  // 序列化包的编号
  memcpy(buffer, &message.packageNum, sizeof(my_int));
  buffer += sizeof(int);

  // 序列化时间消息
  messageLength = message.timestr.size();
  memcpy(buffer, &messageLength, sizeof(my_int));
  buffer += sizeof(my_int);
  memcpy(buffer, message.timestr.c_str(), messageLength);
  buffer += messageLength;

  // 序列化具体的消息
  messageLength = message.message.size();
  memcpy(buffer, &messageLength, sizeof(my_int));
  buffer += sizeof(my_int);
  memcpy(buffer, message.message.c_str(), messageLength);
  buffer += messageLength;
}

/**
  @brief 反序列化字节流为结构体
  @param tmp 需要反序列化的字符数组
  @param message 反序列化之后消息存入的结构体
*/
// std::mutex mutex_test;
void deserializeStruct(const char *tmp, Message &message) {
  {
    // std::unique_lock<std::mutex> lock(mutex_test);
    // std::cout << "start" << std::endl;
    // 反序列化发送方字符串成员的长度和字符数据
    int messageLength;
    memcpy(&messageLength, tmp, sizeof(my_int));
    tmp += sizeof(my_int);
    // std::cout << "messageLength = " << messageLength << std::endl;
    message.sourceIp.assign(tmp, messageLength);
    tmp += messageLength;
    // std::cout << "sourceIp = " << message.sourceIp << std::endl;

    // 反序列化发送方的port
    memcpy(&message.sourcePort, tmp, sizeof(my_int));
    tmp += sizeof(my_int);
    // std::cout << "sourcePort = " << message.sourcePort << std::endl;

    // 反序列化接收方字符串成员的长度和字符数据
    memcpy(&messageLength, tmp, sizeof(my_int));
    tmp += sizeof(my_int);
    // std::cout << "messageLength = " << messageLength << std::endl;
    message.desIp.assign(tmp, messageLength);
    tmp += messageLength;
    // std::cout << "desIp = " << message.desIp << std::endl;

    // 反序列化接收方的port
    memcpy(&message.desPort, tmp, sizeof(my_int));
    tmp += sizeof(my_int);
    // std::cout << "desPort = " << message.desPort << std::endl;

    // 反序列化接收方的包大小
    memcpy(&message.packageSize, tmp, sizeof(my_int));
    tmp += sizeof(my_int);
    // std::cout << "packageSize = " << message.packageSize << std::endl;

    // 反序列化接收方的包编号
    memcpy(&message.packageNum, tmp, sizeof(my_int));
    tmp += sizeof(int);
    // std::cout << "packageNum = " << message.packageNum << std::endl;

    // 反序列化时间消息
    memcpy(&messageLength, tmp, sizeof(my_int));
    tmp += sizeof(my_int);
    // std::cout << "messageLength = " << messageLength << std::endl;
    message.timestr.assign(tmp, messageLength);
    tmp += messageLength;
    // std::cout << "timestr = " << message.timestr << std::endl;

    // 反序列化具体消息
    memcpy(&messageLength, tmp, sizeof(my_int));
    tmp += sizeof(my_int);
    // std::cout << "messageLength = " << messageLength << std::endl;
    message.message.assign(tmp, messageLength);
    tmp += messageLength;
    // std::cout << "message = " << message.message << std::endl;
    // std::cout << "end" << std::endl;
  }
}

/**
  @brief 中继服务器使用的发送消息的工具函数
  @param socket_d 要发送数据的套接字描述符
  @param mt 要发送的结构体消息
*/
my_int sendMessage(my_int socket_d, Message &mt) {
  // 如果消息够长，则会造成缓冲区溢出，而且自己定义的数据结构在接收方并不能按照预计的那样去读取，
  // 所以需要将具体的消息手动切片或者补齐
  // 发送信息
  Message message;
  message.sourceIp = mt.sourceIp;
  message.sourcePort = mt.sourcePort;
  message.desIp = mt.desIp;
  message.desPort = mt.desPort;
  message.packageSize = mt.message.size();
  message.timestr = mt.timestr;
  auto msg = mt.message;
  int LEN =
      BUFSIZE - (sizeof(my_int) + mt.sourceIp.size() + sizeof(mt.sourcePort) +
                 sizeof(my_int) + mt.desIp.size() + sizeof(mt.desPort) +
                 sizeof(message.packageSize) + sizeof(message.packageNum) +
                 sizeof(my_int) + message.timestr.size() + sizeof(my_int));
  int count = 0;
  while (true) {
    if (msg.size() > LEN) {
      message.packageNum = count;
      count++;
      message.message = msg.substr(0, LEN);
      msg = msg.substr(LEN);
      // 将结构体序列化
      char buf[BUFSIZE] = {'\0'};
      serializeStruct(message, buf);
      errno = 0;
      my_int len = 0;
      {
        while (len < BUFSIZE) {
          my_int n = write(socket_d, buf + len, BUFSIZE - len);
          if (n < 0) {
            if (errno == EWOULDBLOCK || EAGAIN) {
              continue;
            } else {
              std::cout << "写入错误" << std::endl;
              perror("write");
              exit(-1);
            }
          }
          len += n;
        }
      }
    } else {
      message.message = msg;
      message.packageNum = count;
      // 将结构体序列化
      char buf[BUFSIZE] = {'\0'};
      serializeStruct(message, buf);
      errno = 0;
      my_int len = 0;
      {
        while (len < BUFSIZE) {
          my_int n = write(socket_d, buf + len, BUFSIZE - len);
          if (n < 0) {
            if (errno == EWOULDBLOCK || EAGAIN) {
              if (len == 0)
                return len;
              else
                continue;
            } else {
              std::cout << "写入错误" << std::endl;
              perror("write");
              exit(-1);
            }
          }
          len += n;
        }
      }
      break;
    }
  }
  return BUFSIZE;
}

/**
  @brief 解析信息
*/
void analysis(char *buffer, my_int bytesRead) {
  std::map<std::string, std::vector<int>> receivedMap;
  // 将字节流反序列化为 map
  std::string serializedMap(buffer, bytesRead);
  size_t startPos = 0;
  size_t endPos = serializedMap.find(";");
  while (endPos != std::string::npos) {
    std::string keyValue = serializedMap.substr(startPos, endPos - startPos);

    // 解析键
    size_t colonPos = keyValue.find(":");
    std::string key = keyValue.substr(0, colonPos);

    // 解析向量长度
    size_t commaPos = keyValue.find(",", colonPos);
    int vectorSize =
        std::stoi(keyValue.substr(colonPos + 1, commaPos - colonPos - 1));

    // 解析向量元素
    std::vector<int> vector;
    size_t elementPos = commaPos + 1;
    for (int i = 0; i < vectorSize; ++i) {
      size_t nextCommaPos = keyValue.find(",", elementPos);
      int element =
          std::stoi(keyValue.substr(elementPos, nextCommaPos - elementPos));
      vector.push_back(element);
      elementPos = nextCommaPos + 1;
    }

    // 将键值对添加到 map 中
    receivedMap[key] = vector;

    // 继续解析下一个键值对
    startPos = endPos + 1;
    endPos = serializedMap.find(";", startPos);
  }

  // 输出接收到的 map
  for (const auto &pair : receivedMap) {
    std::cout << pair.first << ": ";
    for (const auto &element : pair.second) {
      std::cout << element << " ";
    }
    std::cout << std::endl;
  }
}

/**
  @brief 查询servers的程序，输出servers的信息
*/
void searchClient() {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, SERVERIP, &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(searchPort);

  int socket_d = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  // setsockopt(socket_d, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));

  if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    perror("connect()");
    exit(-1);
  }

  char *resN = NULL;
  char buf[BUFSIZE] = {'\0'};
  // shutdown(socket_d, SHUT_RD);
  while ((resN = (fgets(buf, BUFSIZE, stdin))) != NULL) {
    // 去除换行符
    buf[strcspn(buf, "\n")] = '\0';
    std::string strs(buf);
    if (strs == "search") {
      write(socket_d, buf, sizeof(buf));
      int res = -1;
      res = read(socket_d, buf, BUFSIZE);
      analysis(buf, res);
    } else if (strs == "close") {
      break;
    } else {
      std::cout << "输入有误" << std::endl;
    }
  }
  close(socket_d);
}

/**
  @brief 中继器自己杀死查询线程
*/
void killThread() {
  struct sockaddr_in raddr;
  raddr.sin_family = AF_INET;
  inet_pton(AF_INET, SERVERIP, &raddr.sin_addr.s_addr);
  raddr.sin_port = htons(searchPort);

  int socket_d = socket(AF_INET, SOCK_STREAM, 0);

  int val = 1;
  setsockopt(socket_d, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  if (connect(socket_d, (const struct sockaddr *)&raddr, sizeof(raddr)) < 0) {
    perror("connect()");
    exit(-1);
  }

  char buf[6] = {'c', 'l', 'o', 's', 'e', '\0'};
  write(socket_d, buf, 6);
  close(socket_d);
}