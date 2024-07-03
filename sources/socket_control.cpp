#include "macdaatr.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <fstream>
using namespace std;
#define SEND_BUFFER_SIZE 65536     // 发送缓冲区大小
#define RECV_BUFFER_SIZE 65536     // 接收缓冲区大小
#define HIGH_FREQ_SOCKET_PORT 8000 // 高频信道端口号
#define LOW_FREQ_SOCKET_PORT 8100  // 低频信道端口号

/********************************此文件包含对于socket处理相关函数*****************************************/
/* 初始化节点的IP地址和端口信息 */
/* 参数: */
/*   receiver - 接收者地址结构体 */
/*   nodeId - 节点ID，用于索引IP地址 */
/*   port - 端口号，用于设置服务端口 */
void initializeNodeIP(sockaddr_in &receiver, uint16_t nodeId, int port)
{
    /* 引入mac层数据结构类，用于获取节点IP地址 */
    extern MacDaatr daatr_str; // mac层数据结构信息保存类
    // struct sockaddr_in addr_serv; // 服务端地址结构体
    string ip = daatr_str.in_subnet_id_to_ip[nodeId]; // 根据节点ID获取对应的IP地址

    /* 初始化服务端地址结构体 */
    memset(&receiver, 0, sizeof(struct sockaddr_in)); // 清零地址结构体
    receiver.sin_family = AF_INET;                    // 使用IPv4地址
    receiver.sin_port = htons(port);                  // 设置端口号，转换为网络字节序
    /* 将IP地址字符串转换为二进制格式，并赋值给服务端地址结构体 */
    receiver.sin_addr.s_addr = inet_addr(ip.c_str()); // 设置IP地址
}

/* 创建一个UDP套接字并绑定到指定的IP和端口 */
/* 参数: */
/*   ip: 要绑定的IP地址字符串 */
/*   port: 要绑定的端口号 */
/*   if_not_block: 指示套接字是否应该被设置为非阻塞模式 */
/* 返回值: */
/*   成功创建并绑定套接字后返回套接字文件描述符，否则退出程序 */
int macDaatrCreateUDPSocket(string ip, int port, bool if_not_block)
{
    /* 创建一个IPv4地址族的UDP套接字 */
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0)
    {
        perror("socket");
        return -1;
    }
    // 设置发送/接收缓冲区大小
    uint32_t send_buf_size = SEND_BUFFER_SIZE; // 设置发送缓冲区大小(64kb)
    uint32_t recv_buf_size = RECV_BUFFER_SIZE; // 设置接收缓冲区大小(64kb)
    setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &send_buf_size, sizeof(send_buf_size));
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size));
    /* 根据if_block参数决定是否将套接字设置为非阻塞模式 */
    if (if_not_block)
    {
        /* 非阻塞模式 */
        int flags = fcntl(sock_fd, F_GETFL, 0);
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
    }
    /* 初始化服务器地址结构体 */
    /* 将套接字和IP、端口绑定 */
    struct sockaddr_in addr_serv;
    socklen_t len;
    memset(&addr_serv, 0, sizeof(struct sockaddr_in)); // 每个字节都用0填充
    addr_serv.sin_family = AF_INET;                    // 使用IPV4地址
    addr_serv.sin_port = htons(port);                  // 端口,转换为大端序
    /* 将IP地址字符串转换为二进制格式并赋值给服务器地址结构体 */
    /* INADDR_ANY表示不管是哪个网卡接收到数据，只要目的端口是SERV_PORT，就会被该应用程序接收到 */
    addr_serv.sin_addr.s_addr = inet_addr(ip.c_str()); // 自动获取IP地址
    len = sizeof(addr_serv);
    /* 将套接字绑定到指定的IP地址和端口 */
    /* 绑定socket */
    if (bind(sock_fd, (struct sockaddr *)&addr_serv, len) < 0)
    {
        perror("bind error:");
        return -1;
    }
    /* 返回绑定成功的套接字文件描述符 */
    return sock_fd;
}
/**
 * 创建一个用于MAC DAATR协议高频信道的UDP socket线程 ，此线程抛出去以后开启默认开启接收功能
 *
 *   ip: 要绑定的IP地址字符串
 *   port: 要绑定的端口号
 *   IF_NOT_BLOCKED: 指示套接字是否应该被设置为非阻塞模式
 **/
void macDaatrSocketHighFreqThread(bool IF_NOT_BLOCKED = false)
{
    extern bool end_simulation;
    extern MacDaatr daatr_str; // mac层协议类
    string IP = daatr_str.in_subnet_id_to_ip[daatr_str.nodeId];
    int sock_fd = macDaatrCreateUDPSocket(IP, HIGH_FREQ_SOCKET_PORT, IF_NOT_BLOCKED);
    daatr_str.mac_daatr_high_freq_socket_fid = sock_fd;
    int recv_num;
    int send_num;
    char send_buf[1500] = "i am server!";
    char recv_buf[1500];
    struct sockaddr_in addr_client;
    socklen_t len = sizeof(struct sockaddr_in);
    if (!IF_NOT_BLOCKED)
    { // 阻塞模式
        while (1)
        {
            printf("server wait:\n");
            recv_num = recvfrom(sock_fd, recv_buf, sizeof(recv_buf), 0,
                                (struct sockaddr *)&addr_client, (socklen_t *)&len);
            recv_buf[recv_num] = '\0';
            printf("server receive %d bytes: %s\n", recv_num, recv_buf);

            if (end_simulation)
                break;
        }
    }
    else
    { // 非阻塞模式
        while (1)
        {
            printf("server wait:\n");

            recv_num = recvfrom(sock_fd, recv_buf, sizeof(recv_buf), 0,
                                (struct sockaddr *)&addr_client, (socklen_t *)&len);
            if (recv_num < 0 && errno == EWOULDBLOCK)
            {
                // perror("recvfrom error:");
                // exit(1);
                cout << "服务器未收到消息" << endl;
                this_thread::sleep_for(chrono::seconds(4));
            }
            else
            {
                recv_buf[recv_num] = '\0';
                printf("server receive %d bytes: %s\n", recv_num, recv_buf);

                send_num = sendto(sock_fd, send_buf, recv_num, 0, (struct sockaddr *)&addr_client, len);

                if (send_num < 0)
                {
                    perror("sendto error:");
                    exit(1);
                }
            }
            if (end_simulation)
                break;
        }
    }
    close(sock_fd);
}

/**
 * 创建一个用于MAC DAATR协议低频信道的UDP socket线程 ，此线程抛出去以后开启默认开启接收功能
 *
 *   ip: 要绑定的IP地址字符串
 *   port: 要绑定的端口号
 *   IF_NOT_BLOCKED: 指示套接字是否应该被设置为非阻塞模式
 **/
void macDaatrSocketLowFreqThread(bool IF_NOT_BLOCKED = false)
{
    extern bool end_simulation;
    extern MacDaatr daatr_str; // mac层协议类
    string IP = daatr_str.in_subnet_id_to_ip[daatr_str.nodeId];
    int sock_fd = macDaatrCreateUDPSocket(IP, LOW_FREQ_SOCKET_PORT, IF_NOT_BLOCKED);
    daatr_str.mac_daatr_low_freq_socket_fid = sock_fd;
    int recv_num;
    int send_num;
    char send_buf[1500] = "i am server!";
    char recv_buf[1500];
    struct sockaddr_in addr_client;
    socklen_t len = sizeof(struct sockaddr_in);
    if (!IF_NOT_BLOCKED)
    { // 阻塞模式
        while (1)
        {
            printf("server wait:\n");
            recv_num = recvfrom(sock_fd, recv_buf, sizeof(recv_buf), 0,
                                (struct sockaddr *)&addr_client, (socklen_t *)&len);
            recv_buf[recv_num] = '\0';
            printf("server receive %d bytes: %s\n", recv_num, recv_buf);

            if (end_simulation)
                break;
        }
    }
    else
    { // 非阻塞模式
        while (1)
        {
            printf("server wait:\n");

            recv_num = recvfrom(sock_fd, recv_buf, sizeof(recv_buf), 0,
                                (struct sockaddr *)&addr_client, (socklen_t *)&len);
            if (recv_num < 0 && errno == EWOULDBLOCK)
            {
                // perror("recvfrom error:");
                // exit(1);
                cout << "服务器未收到消息" << endl;
                this_thread::sleep_for(chrono::seconds(4));
            }
            else
            {
                recv_buf[recv_num] = '\0';
                printf("server receive %d bytes: %s\n", recv_num, recv_buf);

                send_num = sendto(sock_fd, send_buf, recv_num, 0, (struct sockaddr *)&addr_client, len);

                if (send_num < 0)
                {
                    perror("sendto error:");
                    exit(1);
                }
            }
            if (end_simulation)
                break;
        }
    }
    close(sock_fd);
}

/**
 * @brief 通过高频socket发送数据
 *
 * 本函数用于通过已创建的高频socket向指定接收者发送数据。它首先检查socket文件描述符是否有效，
 * 然后尝试使用sendto函数发送数据。如果发送失败，将输出错误信息；如果未创建socket，则同样输出错误信息。
 *
 * @param data 发送的数据指针
 * @param len 发送的数据长度
 * @param nodeId 目的节点ID
 */
void macDaatrHighFreqSocketSend(uint8_t *data, uint32_t len, uint16_t nodeId)
{
    sockaddr_in recever; // 接收端地址
    initializeNodeIP(recever, nodeId, HIGH_FREQ_SOCKET_PORT);
    // 引入外部定义的MacDaatr类实例，用于获取socket文件描述符
    extern MacDaatr daatr_str;
    // 获取mac_daatr_high_freq_socket_fid的值，用于后续的发送操作
    int sock_fd = daatr_str.mac_daatr_high_freq_socket_fid;
    int send_num = 0;
    // 检查socket文件描述符是否大于等于0，即检查socket是否已成功创建
    if (sock_fd >= 0)
    {
        // 尝试发送数据到指定接收者，sendto函数用于向特定地址发送数据
        send_num = sendto(sock_fd, data, len, 0, (struct sockaddr *)&recever, sizeof(recever));
        // 如果发送字节数小于0，说明发送失败，输出错误信息
        if (send_num < 0)
        {
            cout << "高频信道线程发送失败!!!!!!!" << endl;
        }
    }
    else
    {
        // 如果socket文件描述符小于0，说明socket未成功创建，输出错误信息
        cout << "未创建高频信道线程，无法发送!!!!!!" << endl;
    }
}

/**
 * @brief 通过低频socket发送数据
 *
 * 本函数用于通过已创建的低频socket向指定接收者发送数据。它首先检查socket文件描述符是否有效，
 * 然后尝试使用sendto函数广播发送数据。如果发送失败，将输出错误信息；如果未创建socket，则同样输出错误信息。
 *
 * @param data 发送的数据指针
 * @param len 发送的数据长度
 */
void macDaatrLowFreqSocketSend(uint8_t *data, uint32_t len)
{
    sockaddr_in recever; // 接收端地址
    // 引入外部定义的MacDaatr类实例，用于获取socket文件描述符
    extern MacDaatr daatr_str;
    // 获取mac_daatr_low_freq_socket_fid的值，用于后续的发送操作
    int sock_fd = daatr_str.mac_daatr_low_freq_socket_fid;
    int send_num = 0;
    // 检查socket文件描述符是否大于等于0，即检查socket是否已成功创建
    if (sock_fd >= 0)
    {
        for (const auto &sendid : daatr_str.in_subnet_id_to_ip)
        {
            if (sendid.first == daatr_str.nodeId)
                continue;                                                  // 跳过本节点
            initializeNodeIP(recever, sendid.first, LOW_FREQ_SOCKET_PORT); // 初始化接收端地址
            // 尝试发送数据到指定接收者，sendto函数用于向特定地址发送数据
            send_num = sendto(sock_fd, data, len, 0, (struct sockaddr *)&recever, sizeof(recever));
            // 如果发送字节数小于0，说明发送失败，输出错误信息
            if (send_num < 0)
            {
                cout << "低频信道线程发送失败!!!!!!!" << endl;
            }
        }
    }
    else
    {
        // 如果socket文件描述符小于0，说明socket未成功创建，输出错误信息
        cout << "未创建低频信道线程，无法发送!!!!!!" << endl;
    }
}