/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-10-20 18:23:08
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-11-01 02:33:25
 * @FilePath: /EmDaatr/upper_cmp/sources/main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "socket_control.h"
#include "spectrum.h"
#include "main.h"
#include "link.h"
using namespace std;

int sock_fd;
std::string in_subnet_id_to_ip[21];

int main()
{
    if (!link_init())
        return 0;

    printf("等待节点启动\n");
    char recv_buf[60000];
    struct sockaddr_in addr_client;
    socklen_t len = sizeof(sockaddr_in);
    while (1)
    {
        int recv_num = recvfrom(sock_fd, recv_buf, sizeof(recv_buf), 0,
                                (struct sockaddr *)&addr_client, (socklen_t *)&len);
        recv_buf[recv_num] = '\0';
        if (!strcmp("start", recv_buf))
        {
            printf("收到开始信息\n");
            break;
        }
    }

    thread dropLink(drop_link_process);
    // thread linkDateGet(link_data_print);
    // linkDateGet.join();
    dropLink.join();

    return 0;
}
