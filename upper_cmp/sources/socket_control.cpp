
#include "socket_control.h"
using namespace std;
using namespace std;
#define subnet_node_num 20
#define SEND_BUFFER_SIZE 65536     // 发送缓冲区大小
#define RECV_BUFFER_SIZE 65536     // 接收缓冲区大小
#define HIGH_FREQ_SOCKET_PORT 8000 // 高频信道端口号
#define LOW_FREQ_SOCKET_PORT 8100  // 低频信道端口号
int mac_daatr_low_freq_socket_fid = 0;
/********************************此文件包含对于socket处理相关函数*****************************************/

/**
 *   @brief 初始化节点的IP地址和端口信息
 *   @param receiver 接收者地址结构体
 *   @param dest_node 节点ID，用于索引IP地址
 *   @param port 端口号，用于设置服务端口
 **/
void initializeNodeIP(sockaddr_in &receiver, uint16_t dest_node, int port)
{
    extern string in_subnet_id_to_ip[subnet_node_num];
    string ip = in_subnet_id_to_ip[dest_node]; // 根据节点ID获取对应的IP地址
    printf("%s  %d\n", ip.c_str(), port);
    /* 初始化服务端地址结构体 */
    memset(&receiver, 0, sizeof(sockaddr_in)); // 清零地址结构体
    receiver.sin_family = AF_INET;             // 使用IPv4地址
    receiver.sin_port = htons(port);           // 设置端口号，转换为网络字节序
    /* 将IP地址字符串转换为二进制格式，并赋值给服务端地址结构体 */
    receiver.sin_addr.s_addr = inet_addr(ip.c_str()); // 设置IP地址
}

/**
 *   @brief 创建一个UDP套接字并绑定到指定的IP和端口
 *   @param ip 要绑定的IP地址字符串
 *   @param port 要绑定的端口号
 *   @param if_not_block: 指示套接字是否应该被设置为非阻塞模式
 *
 *   @return 成功创建并绑定套接字后返回套接字文件描述符，否则退出程序
 **/
int macDaatrCreateUDPSocket(string ip, int port, bool if_not_block)
{
    /* 创建一个IPv4地址族的UDP套接字 */
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0); // 建立套接字
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
    sockaddr_in addr_serv;
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
    mac_daatr_low_freq_socket_fid = sock_fd;
    return sock_fd;
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
void macDaatrSocketLowFreq_Send(uint8_t *data, uint32_t len, int id)
{
    extern string in_subnet_id_to_ip[subnet_node_num];
    sockaddr_in recever; // 接收端地址
    // 获取mac_daatr_low_freq_socket_fid的值，用于后续的发送操作
    int sock_fd = mac_daatr_low_freq_socket_fid;
    int send_num = 0;
    // 检查socket文件描述符是否大于等于0，即检查socket是否已成功创建
    if (sock_fd >= 0)
    {
        initializeNodeIP(recever, id, LOW_FREQ_SOCKET_PORT); // 初始化接收端地址

        // 尝试发送数据到指定接收者，sendto函数用于向特定地址发送数据
        send_num = sendto(sock_fd, data, len, 0, (struct sockaddr *)&recever, sizeof(recever));
        // printf("send_num : %d ", send_num);
        //  如果发送字节数小于0，说明发送失败，输出错误信息
        if (send_num <= 0)
        {
            printf("send_num : %d ", send_num);
            perror("sendto");
            printf("发送失败\n");
        }
    }
    else
    {
        printf("未创建低频信道线程，无法发送!!!!!!\n");
    }
}
void link_data_print()
{
    extern int sock_fd;
    char recv_buf[60000];
    struct sockaddr_in addr_client;
    socklen_t len = sizeof(sockaddr_in);
    int last_segment;
    while (1)
    {
        int recv_num = recvfrom(sock_fd, recv_buf, sizeof(recv_buf), 0,
                                (struct sockaddr *)&addr_client, (socklen_t *)&len);
        recv_buf[recv_num] = '\0';
        char *ip_str = inet_ntoa(addr_client.sin_addr);
        // std::cout << "Client IP: " << ip_str << std::endl;

        // 使用 strtok 分离最后两段 IP 地址
        char *token = std::strtok(ip_str, ".");
        int segments[4], i = 0;
        while (token != nullptr && i < 4)
        {
            segments[i++] = std::stoi(token);
            token = std::strtok(nullptr, ".");
        }

        if (i == 4)
        {
            last_segment = segments[3];
        }
        else
        {
            std::cerr << "Failed to parse IP address\n";
        }

        // cout << "===========NODE " << last_segment - 1 << " data come=========" << endl;
        // cout << recv_buf;
        // cout << "===========NODE " << last_segment - 1 << " data over=========" << endl
        //      << endl;
    }
}