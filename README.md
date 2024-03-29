# 第一章 简介
## 1.1 
        包裹函数：是unp作者自定义的，就是将传统的一些固定的多条语句封装成一个方法，例如open打开之后的检查描述符的几条语句就可以封装成一条语句，这就是包裹函数名字的由来（用一条语句代替（包裹）多条语句），在unp中包裹函数不只是包裹检查出错的语句，还包括调用函数，例如把open函数以及检查open返回值的语句全部包裹在Open中，一般包裹函数都是原函数的首字母大写。
# 第二章 传输层：TCP、UDP和SCTP
## SCTP
        SCTP中的关联取代TCP中的连接，TCP中的连接涉及的同一时间只能一个客户端和一个服务器进行通信，而SCTP的关联指的是同一时间一个客户端可以和多个服务器建立连接并通信。（多宿的特点）（关联：多组IP使用同一个端口）
        SCTP面向的是消息，发送端会把每个记录的长度随数据一起发送给接收方。
        SCTP还可以在同一个客户端与服务器之间建立多个流，每个流各自**可靠**且**按序**投递消息。
## TCP的TIME_WAIT状态
        time_wait状态只存在于主动关闭方：
        原因一：主动要求关闭的一方最终会发送ACK确认给对方，为了防止ACK丢失，主动关闭方必须在time_wait状态等待2MSL的时间，以确保对方收到了自己发送的ACK。
        原因二：当连接双方都关闭通道之后，很可能在2MSL时间之内再次建立一个一模一样的通信连接，如果建立成功，此时在2MSL时间内，上一个关闭的通道内很可能仍然存在着数据包，所以很大概率会把上一个销毁的通道中残存的数据包传到新建立的一模一样的通道里面，所以为了避免这样的情况出现，最初设计的时候就设置time_wait状态必须等待2MSL时间之后才能建立一模一样的通信通道 。
# 第三章 套接口编程简介
        虽然多数结构体不要求sin_zero这个成员为0，但当捆绑一个非通配的IPv4的时候，此成员必须为0（为什么？）

        sockaddr_storage代替sockaddr：
        原因一：如果系统支持的任何套接口地址结构有对齐的需求，那么sockaddr_storage能满足最苛刻的对齐要求。
        原因二：sockaddr_storage足够大，能容纳系统支持的任何套接口地址结构。

        inet_aton和inet_addr都是把点分十进制的字符串转换成32位的网络字节序二进制值。inet_addr出错的时候返回一个32位均为1的值，与255.255.255.255冲突。
        inet_ntoa将一个32位的网络字节序二进制转换成点分十进制的字符串。
        inet_pton和inet_ntop对IPv4和IPv6都可以处理。
# 第四章 基本TCP套接口编程
        TCP的listen：内核为一个给定的套接口维护两个队列，一个是未完成连接队列，但是已经收到了来自客户端的SYN分节，另一个是已完成连接队列，表示服务器和客户端已经完成了三次握手建立了连接，可以进行通信。（两个队列的和不能超过listen的第二个参数的大小，也就是连接池大小，具体系统具体讨论也不一定是这个 设置）
# 第五章 TCP客户/服务器程序例子
        SIGPIPE信号：当一个进程向某个已经收到RST的套接口执行写的操作时，内核向该进程发送一个SIGPIPE信号，该信号的缺省行为是终止当前进程。（无论是捕获该信号，并从信号处理函数返回，还是简单的忽略该信号，写操作都将返回一个EPIPE的错误）(问题：当收到RST信号的时候不应该马上终止进程吗，怎么会存在上述说的情况呢)
# 第六章 I/O复用：select和poll函数
        问题1：p153拒绝服务攻击，为什么会阻塞与第二个read系统调用？
        问题2：poll的底层机制是什么？
        问题3：read读取套接字的时候，是会冲刷缓冲区吗还是不会冲刷？
        问题4：setsockopt中设置缓冲区大小SO_SNDBUF，当设置为0的时候表示的是什么？是立即发送吗？
        问题5：SHUT_RD到底是怎么样的

## epoll两种工作模式
### LT模式（水平触发，缺省模式）
        假设检测的是套接口的读事件，当缓冲区来了数据或者数据没有读取完，epoll就会一直通知用户。（这种模式同时支持阻塞和非阻塞）
### ET模式（边沿触发）
        假设检测的是套接口的读事件，当缓冲区来了数据时会通知用户一次，然后就不管了，读没读完也不会再次通知。（支持非阻塞）
        epoll不也是会阻塞在IO上吗
# 第七章 套接口选项
        需要用到的时候查表可知
# 第十一章 名字与地址转换
# 第二十六章 线程
## 概述