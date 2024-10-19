#include <fstream>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <pthread.h>

#include "timer.h"
#include "macdaatr.h"
#include "common_struct.h"
#include "daatr_control.h"
#include "socket_control.h"
#include "low_freq_channel.h"
#include "high_freq_channel.h"
#include "routing_mmanet.h"
//
#include "routing_mmanet.h"
#include "network.h"

using namespace std;

bool end_simulation = false;    // 结束仿真标志
MacDaatr daatr_str;             // mac层daatr协议信息保存类
ringBuffer RoutingTomac_Buffer; // 网络层->MAC缓存队列

// routing_mmanet模块所需的循环缓冲区
ringBuffer macToRouting_buffer; // MAC->网络层缓存队列
ringBuffer netToRouting_buffer;
ringBuffer svcToRouting_buffer;
// network模块所需的循环缓冲区
ringBuffer svcToNet_buffer;
ringBuffer macToNet_buffer; // MAC->路由缓存队列
ringBuffer routingToNet_buffer;

MMANETData *mmanet;
LinkConfigData *linkConfigPtr;
IdentityData *IdentityPtr;             // 用于维护IdentityData结构体
NodeNotification *nodeNotificationPtr; // 用于存储节点内向其他层传播的节点身份
NetViewData *netViewPtr;               // 用于维护网络状态视图数据结构

int main(int argc, char *argv[])
{
    system("clear");
    system("source ./rm.sh");

    extern int over_count;
    extern int PPS_overcount;
    extern int fd_irq;

    int id = atoi(argv[1]);

    daatr_str.macParameterInitialization(id); // mac层参数初始化
    thread lowRecvThread(&MacDaatr::macDaatrSocketLowFreq_Recv, &daatr_str, false);
    thread lowSendThread(lowFreqSendThread);
    thread BufferReadThread(&MacDaatr::networkToMacBufferReadThread, &daatr_str);

    thread highRecvThread(&MacDaatr::macDaatrSocketHighFreq_Recv, &daatr_str, false);
    thread highSendThread(&MacDaatr::highFreqSendThread, &daatr_str);

    // 将主线程优先级设高
    // pid_t pid = getpid();
    // struct sched_param param;
    // param.sched_priority = sched_get_priority_max(SCHED_FIFO); // 也可用SCHED_RR
    // if (sched_setscheduler(pid, SCHED_FIFO, &param) == -1)
    // {
    //     perror("sched_setscheduler");
    //     return 1;
    // }

    pthread_t tid = pthread_self();
    int policy;
    struct sched_param param;
    policy = SCHED_RR;         // 使用实时调度策略
    param.sched_priority = 20; // 设置优先级，值越高优先级越高
    if (pthread_setschedparam(tid, policy, &param) != 0)
    {
        perror("pthread_setschedparam");
        exit(EXIT_FAILURE);
    }

    // 创建routing_mmanet模块所需的数据类型结构体指针,并进行初始化
    mmanet = new MMANETData();
    MMANETInit(mmanet);
    mmanet->nodeAddr = id;
    mmanet->groupID = 1;
    mmanet->intragroupcontrolNodeId = daatr_str.mana_node;
    //  创建network模块所需的数据类型结构体指针,并进行初始化
    linkConfigPtr = new LinkConfigData(); // 在对应的线程中进行初始化
    IdentityPtr = new IdentityData();
    nodeNotificationPtr = new NodeNotification(); // 在对应的线程中进行初始化
    netViewPtr = new NetViewData();
    NetViewInit();

    // 创建routing_mmanet模块多线程
    pthread_t routingReceiveFromSvc, routingReceiveFromNet, routingReceiveFromMac;
    pthread_create(&routingReceiveFromMac, NULL, RoutingReceiveFromMac, NULL);
    pthread_create(&routingReceiveFromNet, NULL, RoutingReceiveFromNet, NULL);
    pthread_create(&routingReceiveFromSvc, NULL, RoutingReceiveFromSvc, NULL);

    // 创建network模块多线程
    pthread_t netReceiveFromSvc, netReceiveFromRouting, netReceiveFromMac;
    pthread_create(&netReceiveFromMac, NULL, NetReceiveFromMac, NULL);
    pthread_create(&netReceiveFromRouting, NULL, NetReceiveFromRouting, NULL);
    pthread_create(&netReceiveFromSvc, NULL, NetReceiveFromSvc, NULL);

    timeInit();
    cout << "等待同步信号" << endl;

    // SetTimer(0, 1, 1);
    lowRecvThread.join();
    lowSendThread.join();
    BufferReadThread.join();
    highRecvThread.join();
    highSendThread.join();

    pthread_join(routingReceiveFromSvc, NULL);
    pthread_join(routingReceiveFromNet, NULL);
    pthread_join(routingReceiveFromMac, NULL);
    pthread_join(netReceiveFromSvc, NULL);
    pthread_join(netReceiveFromRouting, NULL);
    pthread_join(netReceiveFromMac, NULL);

    delete mmanet;
    delete linkConfigPtr;
    delete IdentityPtr;
    delete nodeNotificationPtr;
    delete netViewPtr;

    sleep(1);
    cout << simInfoPosition << "    " << over_count << "    " << PPS_overcount << endl;

    // 将仿真数据写入文件
    char filePath[50];
    sprintf(filePath, "./res/NODE%d.txt", id);
    int file = open(filePath, O_CREAT | O_TRUNC | O_WRONLY);

    if (!file)
        cout << "文件打开错误" << endl;

    for (int j = 0; j < simInfoPosition; j++)
    {
        write(file, simInfo[j], strlen(simInfo[j]));
    }

    close(file);
    close(fd_irq);

    system("rmmod irq_start.ko");
    system("rmmod irq.ko");

    return 0;
}
