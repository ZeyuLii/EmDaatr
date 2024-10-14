#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/syscall.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <cmath>

#include "main.h"
#include "common_struct.h"
#include "network.h"
#include "routing_mmanet.h"
#include "macdaatr.h"
using namespace std;

// 新加，测试
extern MMANETData *mmanet;

#define LINK_MAX_IN_MSGFROMCONTROL 3

// 新增 链路构建需求保存于txt并发送给网络状态视图
#define LINK_ASSIGNMENT_FILE "linkAssignment.txt"

extern LinkConfigData *linkConfigPtr;
extern IdentityData *IdentityPtr;
extern NodeNotification *nodeNotificationPtr;
extern NetViewData *netViewPtr; // 用于维护网络状态视图数据结构
// network模块所需的循环缓冲区
extern ringBuffer svcToNet_buffer;
extern ringBuffer macToNet_buffer;
extern ringBuffer routingToNet_buffer;
extern ringBuffer netToRouting_buffer;
extern ringBuffer RoutingTomac_Buffer;
extern bool end_simulation;

void *NetReceiveFromSvc(void *arg)
{
    // int id = ((int *)arg)[0];

    // // printf("%d \n", id);
    //  注册SIGUSR2的信号处理函数
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO; // 即使在信号处理函数执行期间，其他同类型的信号也会继续被递送。
    sa.sa_sigaction = timer_callbackUser2;
    sigemptyset(&sa.sa_mask); // 清空信号屏蔽字
    sigaction(SIGUSR2, &sa, NULL);

    NetUpdateAndMaintenanceInit();
    LinkConfigInit(linkConfigPtr);
    uint8_t net_buffer_svc[MAX_DATA_LEN]; // 将从循环缓冲区读取出来的数据(含包头)存放于此

    FlightStatus *flightPtr11 = nullptr;
    while (!end_simulation)
    {
        // 接收数据
        memset(net_buffer_svc, 0, MAX_DATA_LEN);
        svcToNet_buffer.ringBuffer_get(net_buffer_svc);

        // 解析数据
        uint8_t type = net_buffer_svc[0];
        uint16_t len = (net_buffer_svc[1] << 8) | net_buffer_svc[2];

        vector<uint8_t> dataPacket_svc(&net_buffer_svc[3], &net_buffer_svc[3] + len);

        switch (type)
        {
        case 0: // 无数据接收
            break;

        case 0x01: // 本地飞行状态信息
            for (int i = 0; i < sizeof(FlightStatus *); i++)
            {
                cout << (int)dataPacket_svc[i] << " i " << i;
            }
            cout << endl;
            cout << "网络层已接收Svc本地飞行状态信息" << endl;
            flightPtr11 = (FlightStatus *)&dataPacket_svc[0];
            // FlightStatus *flightPtr11 = (FlightStatus *)&dataPacket_svc[0];
            cout << "net work *flightPtr  " << flightPtr11 << endl;
            HandleLocalFlightState(dataPacket_svc);
            break;

        case 0x06: // 底层传输需求
            cout << "网络层已接收Svc底层传输需求" << endl;
            HandleReceivedTaskInstruct(dataPacket_svc);
            break;

        default:
            cout << "不是NetReceiveFromSvc应该接收的数据类型:" << (int)type << endl;
            break;
        }

        // 每间隔100us读取一次数据
        usleep(100);
    }

    return NULL;
}

void *NetReceiveFromRouting(void *arg)
{
    uint8_t net_buffer_routing[MAX_DATA_LEN]; // 将从循环缓冲区读取出来的数据(含包头)存放于此

    while (!end_simulation)
    {
        // 接收数据
        memset(net_buffer_routing, 0, MAX_DATA_LEN);
        routingToNet_buffer.ringBuffer_get(net_buffer_routing);
        // 解析数据
        uint8_t type = net_buffer_routing[0];
        uint16_t len = (net_buffer_routing[1] << 8) | net_buffer_routing[2];
        vector<uint8_t> dataPacket_svc(&net_buffer_routing[3], &net_buffer_routing[3] + len);

        // 层间的type待定
        switch (type)
        {
        case 0: // 无数据接收
            break;

        case 0x1B: // 路由信息
            cout << "网络层已接收Routing路由信息" << endl;
            HandleReceivedRTtable(dataPacket_svc);
            break;

        case 0x1C: // 拓扑感知信息
            cout << "网络层已接收Routing拓扑感知信息" << endl;
            HandleReceivedTopology(dataPacket_svc);
            break;

        case 0x1D: // 毁伤感知信息
            cout << "网络层已接收Routing毁伤感知信息" << endl;
            ReceiveDamagedNodesmsg(dataPacket_svc);
            break;

        case 0x1E: // 管控开销消息
            cout << "网络层已接收Routing管控开销消息" << endl;
            HandleControlMsgFromRoute(dataPacket_svc);
            break;

        default:
            cout << "不是NetReceiveFromRouting应该接收的数据类型:" << (int)type << endl;
            break;
        }

        // 每间隔100us读取一次数据
        usleep(100);
    }

    return NULL;
}

void *NetReceiveFromMac(void *arg)
{
    uint8_t net_buffer_mac[MAX_DATA_LEN]; // 将从循环缓冲区读取出来的数据(含包头)存放于此

    while (!end_simulation)
    {
        // 接收数据
        memset(net_buffer_mac, 0, MAX_DATA_LEN);
        macToNet_buffer.ringBuffer_get(net_buffer_mac);
        // 解析数据
        uint8_t type = net_buffer_mac[0];
        uint16_t len = (net_buffer_mac[1] << 8) | net_buffer_mac[2];
        vector<uint8_t> dataPacket_svc(&net_buffer_mac[3], &net_buffer_mac[3] + len);

        switch (type)
        {
        case 0: // 无数据接收
            break;

        case 0x0C: // 其他节点飞行状态信息
            cout << "网络层已接收Mac其他节点飞行状态信息" << endl;
            HandleReceivedNetPos(dataPacket_svc);
            break;

        case 0X17: // 链路运行状态
            cout << "网络层已接收Mac链路运行状态" << endl;
            UpdateLinkStatus(dataPacket_svc);
            break;

        case 0x18: // 身份配置信息发送指示
            cout << "网络层已接收Mac身份配置信息发送指示" << endl;
            ControlNodeDealWithIdentityStatus(dataPacket_svc);
            break;

        default:
            cout << "不是NetReceiveFromMac应该接收的数据类型:" << (int)type << endl;
            break;
        }

        // 每间隔100us读取一次数据
        usleep(100);
    }

    return NULL;
}

// 将要发送的数据(结构体)封装成层间接口数据帧，以便进行发送
void PackRingBuffer(uint8_t buffer[], void *data, uint16_t size, uint8_t type)
{
    // 封装数据类型字段
    buffer[0] = type;

    // 封装数据长度字段
    buffer[1] = (size >> 8) & 0xFF;
    buffer[2] = size & 0xFF;

    memcpy(&buffer[3], data, size);
}

// 将接收的指针解析出来
uint64_t analysisPtr(const vector<uint8_t> &msgPtr)
{
    if (msgPtr.size() == 8)
    {
        uint64_t ptr = ((uint64_t)msgPtr[7] << 56) | ((uint64_t)msgPtr[6] << 48) | ((uint64_t)msgPtr[5] << 40) | ((uint64_t)msgPtr[4] << 32) | ((uint64_t)msgPtr[3] << 24) | ((uint64_t)msgPtr[2] << 16) | ((uint64_t)msgPtr[1] << 8) | ((uint64_t)msgPtr[0]);
    }
    else
    {
        cout << "传入的结构体指针不正确!" << endl;
    }
}

// 设置一个定时器，最后一个参数为信号发送时携带的指针信息
// ptr:结构体指针, structSize:结构体大小
void SetTimerUser2(long sc, long ms, int number, void *ptr, int structSize)
{
    // cout << "设置定时器" << endl;
    // 创建定时器
    timer_t timerid;
    struct sigevent sevp;
    sevp.sigev_notify = SIGEV_SIGNAL | SIGEV_THREAD_ID;
    sevp._sigev_un._tid = syscall(SYS_gettid); // 获取LWP，标识线程身份，交给cpu使用
    sevp.sigev_signo = SIGUSR2;                // 超时发送SIGUSR2信号

    // sigev_valuel类型为联合体，要传结构体只能使用sival_ptr，将信息传给信号处理函数
    char *signPtr = (char *)malloc(sizeof(int) + structSize); // 信号处理函数使用完后free
    int *timerNum = (int *)signPtr;
    *timerNum = number;                             // 定时器编号, 后面跟要传的结构体内容
    memcpy(signPtr + sizeof(int), ptr, structSize); // 携带的结构体内容
    sevp.sigev_value.sival_ptr = signPtr;

    timer_create(CLOCK_REALTIME, &sevp, &timerid);

    // 设置定时器的超时时间和间隔
    struct itimerspec its;
    its.it_value.tv_sec = sc;            // 初次超时时间秒数
    its.it_value.tv_nsec = ms * 1000000; // 初次超时时间纳秒数
    its.it_interval.tv_sec = 0;          // 间隔时间：0秒
    its.it_interval.tv_nsec = 0;
    timer_settime(timerid, 0, &its, NULL);
}

// SIGUSR2信号的回调函数
void timer_callbackUser2(int signum, siginfo_t *si, void *uc)
{
    // cout << "定时器触发" << endl;
    char *timerMsg = (char *)si->si_value.sival_ptr;
    int *timerNum = (int *)timerMsg;

    //*timerNum为设置定时器时设置的定时器编号，再次用于区分不同定时器的功能
    if (*timerNum == 6)
    {
        // 到期重新检查自己的任务列表, 到期总分控制节点给MAC发，其余节点给控制节点发
        CheckTaskListTimeout();
    }
    else if (*timerNum == 7)
    {
        // 到期重新触发网络视图数据结构的身份更新
        HandleIdentityUpdate((NodeNotification *)(timerMsg + sizeof(int)));
    }
    else if (*timerNum == 8)
    {
        // 下发身份的定时器
        HandleIdentityBroadcastTiming();
    }
    else if (*timerNum == 9)
    {
        // 控制节点处理自身打包的身份广播包
        cout << "控制节点处理自身打包的身份广播包" << endl;
        ReceiveAMsgFromOtherNodes((timerMsg + sizeof(int)));
    }
    else if (*timerNum == 10)
    {
        // 通知本节点其他模块身份状态信息
        InformIdentityToOtherModules();
    }
    else if (*timerNum == 11)
    {
        // 待处理毁伤的时钟到期，从待处理毁伤列表中删除
        CheckDamagedNodeClock((DamagedNode *)(timerMsg + sizeof(int)));
    }
    else
    {
        cout << "定时器类型错误" << endl;
    }
    free(timerMsg);
}

// 链路层定义的函数
bool MacDaatr_judge_if_could_send_config_or_LinkRequist(int interfaceIndex)
{
    // 未定义的引用
    return true;
}

/*------------------------network_translinkconfig.cpp------------------------------------*/
void LinkConfigInit(LinkConfigData *linkConfigPtr)
{
    // 节点id如何获取？
    linkConfigPtr->nodeId = 1;
    // 判断本地节点的身份
    if (nodeNotificationPtr->nodeResponsibility == INTRAGROUPCONTROL)
    {
        //**************************控制节点初始化链路运行状态*************
        linkConfigPtr->linkStatus = 0; // 初始化为初始建链阶段
        // 控制节点初始化链路构建需求存储文件
        FileInit(LINK_ASSIGNMENT_FILE);
        //*****************************************************************
        // 控制节点设置周期性下发给链路层
        SetTimerUser2(14, 999, 6, NULL, 0); // 6为定时器编号
    }
    else
    {
        // 其余为普通节点，设置周期性转发给控制节点
        SetTimerUser2(11, 999, 6, NULL, 0); // 6为定时器编号
    }

    //************************************测试发送链路状态**********
    // LinkSetTimer(node, NULL, 0, MSG_NETWORK_LINKCONFIG_TestSendLinkStatus, 18 * SECOND);
}

// 控制节点初始化链路构建需求存储的文件
void FileInit(const string filename)
{
    ifstream fileIn((LINK_ASSIGNMENT_FILE));
    bool fileExists = fileIn.good();
    if (fileExists)
    {
        // 文件存在
        // 判断文件是否为空
        fileIn.seekg(0, ios::end);
        streampos fileSize = fileIn.tellg();
        bool isEmpty = ((int)fileSize == 0);

        if (!isEmpty)
        {
            // 文件非空，清空文件内容
            ofstream fileOut(filename, ios::trunc);
            if (fileOut.is_open())
            {
                fileOut.close();
                cout << "文件已存在且非空，已清空文件内容。" << endl;
            }
            else
            {
                cerr << "无法清空文件内容。" << endl;
            }
        }
        else
        {
            // 文件存在且为空
            cout << "文件已存在且为空" << endl;
        }
    }
    else
    {
        // 文件不存在，创建文件
        ofstream fileOut(filename);
        if (fileOut.is_open())
        {
            fileOut.close();
            cout << "文件不存在，已创建文件。" << endl;
        }
        else
        {
            cerr << "无法创建文件。" << endl;
        }
    }
}

// 处理收到的全局路由表
void HandleReceivedRTtable(const vector<uint8_t> &NetRT_EntryMsgPtr)
{
    cout << "节点" << linkConfigPtr->nodeId << " 收到路由表！" << endl;
    NetRT_EntryMsg *rtMsgPtr = (NetRT_EntryMsg *)&NetRT_EntryMsgPtr[0];
    // NetRT_EntryMsg *rtMsgPtr = (NetRT_EntryMsg *)analysisPtr(NetRT_EntryMsgPtr);
    // 先释放上次接收的全网路由表
    CleanRTtabFromRout(linkConfigPtr->netRTtablePtr);
    // 更新自己数据结构里的指针
    linkConfigPtr->netRTtablePtr = rtMsgPtr->netRTtablePtr;
    PrintRTTable(linkConfigPtr->netRTtablePtr);
    cout << "123282147389859" << endl;
    if (rtMsgPtr->netRTtablePtr->size() == 0) // 路由表为空
    {
        // 节点脱网，触发身份配置模块修改身份
        cout << "路由模块发送的全网路由表为空，节点脱网 !" << endl;
        // 身份配置模块处理函数在此处运行
        HandleNodeOffline();
    }
    cout << "34745909606700" << endl;
    // 获取全网所有节点
    vector<NodeAddress> *netNodesPtr = new vector<NodeAddress>;
    for (int i = 0; i < linkConfigPtr->netRTtablePtr->size(); i++)
    {
        NodeAddress newNode = (*(linkConfigPtr->netRTtablePtr))[i][0]->source;
        netNodesPtr->push_back(newNode);
    }
    // 身份配置模块存储新的路由表成员
    // cout<<"节点"<< IdentityPtr->nodeId<<" 身份配置模块收到路由表成员！"<<endl;
    // 释放上一次存储的路由表
    if (IdentityPtr->netNodes)
    {
        delete IdentityPtr->netNodes;
    }
    IdentityPtr->netNodes = netNodesPtr;

    cout << "全网路由表处理结束" << endl;
}

// 释放上次接收的全网路由表
void CleanRTtabFromRout(vector<vector<nodeNetForwardingTable *>> *rtTablePtr)
{
    if (linkConfigPtr->netRTtablePtr != nullptr)
    {
        vector<vector<nodeNetForwardingTable *>>::iterator it1;
        vector<nodeNetForwardingTable *>::iterator it2;
        for (it1 = linkConfigPtr->netRTtablePtr->begin(); it1 != linkConfigPtr->netRTtablePtr->end(); it1++)
        {
            for (it2 = it1->begin(); it2 != it1->begin(); it2++)
            {
                delete *it2;
                *it2 = NULL;
            }
            it1->clear();
        }
        delete linkConfigPtr->netRTtablePtr;
        // cout << "路由表清空完成! " << endl;
    }
}

// 打印接收到的全网路由表
void PrintRTTable(vector<vector<nodeNetForwardingTable *>> *rTtablePtr)
{
    if (rTtablePtr)
    {
        vector<vector<nodeNetForwardingTable *>>::iterator i;
        vector<nodeNetForwardingTable *>::iterator j;
        vector<uint16_t>::iterator k;
        for (i = rTtablePtr->begin(); i != rTtablePtr->end(); i++)
        {
            //*i是vector<nodeNetForwardingTable*>
            for (j = i->begin(); j != i->end(); j++)
            {
                //*j是nodeNetForwardingTable*
                // // printf("src:%u dest:%u \n",(*j)->source, (*j)->dest);
                cout << "src: " << (*j)->source << " dest: " << (*j)->dest
                     << " hop: " << (*j)->hop << " nexthop: " << (*j)->nexthop
                     << " delay: " << (*j)->delay << endl;
                cout << "Relaynodes is:  ";
                for (k = (*j)->Relaynodes.begin(); k != (*j)->Relaynodes.end(); k++)
                {
                    cout << (*k) << "  ";
                }
                cout << endl;
            }
        }
    }
}

// 接收任务指令（应用层）
void HandleReceivedTaskInstruct(const vector<uint8_t> &task_MsgPtr)
{
    // 任何节点收到后都会进行查表操作
    // 将链路拆解后存入自己的taskList中

    // 传入的是指针？还是结构体内容
    cout << "收到传输任务指令！" << endl;
    cout << "收到底层传输需求" << endl;
    TaskAssignment *task_Msg = (TaskAssignment *)&task_MsgPtr[0];
    // TaskAssignment* task_Msg = (TaskAssignment*)analysisPtr(task_MsgPtr);

    NodeAddress sourceAddr = task_Msg->begin_node;
    NodeAddress destAddr = task_Msg->end_node;
    // 查自己的路由表
    // 查找结果，默认为false
    bool isFind = false;
    if (linkConfigPtr->netRTtablePtr) // 保护机制，路由表为空跳出
    {
        if (linkConfigPtr->netRTtablePtr->empty())
        {
            cout << "路由表内容为空 !" << endl;
            return;
        }

        vector<vector<nodeNetForwardingTable *>>::iterator j;
        vector<nodeNetForwardingTable *>::iterator k;
        for (j = linkConfigPtr->netRTtablePtr->begin(); j != linkConfigPtr->netRTtablePtr->end(); j++)
        {
            NodeAddress sourceInTable = (*(j->begin()))->source;
            if (sourceInTable == sourceAddr)
            {
                // 源地址相同
                for (k = j->begin(); k != j->end(); k++)
                {
                    //*j是nodeNetForwardingTable*
                    if ((*k)->dest == destAddr)
                    {
                        // 目的地址也相同，即找到了
                        // 此处只打印，组包待补充
                        isFind = true;
                        PrintFullRTPath(&((*k)->Relaynodes));
                        BreakFullRoute(&((*k)->Relaynodes), *task_Msg);
                        break;
                    }
                    else if (destAddr < (*k)->dest)
                    {
                        // 待找目的地小于此时表中目的地，直接跳出
                        break;
                    }
                }
            }
            else if (sourceAddr < sourceInTable)
            {
                // 待找源地址小于此时表中的源地址，直接跳出
                break;
            }
            else
            {
                // 待找源地址大于此时表中的源地址，继续查找
                continue;
            }

            // 此处为源地址相同处理后的位置，感觉暂时到不了这里，存疑
            break;
        }
    }
    else
    {
        cout << "路由表指针为空，无法拆解链路，请先发送路由表！" << endl;
        return;
    }

    // 查询路由表完成后输出查询结果
    if (isFind)
    {
        // printf("Find path \n");
    }
    else
    {
        // printf("Doesn't find path \n");
    }

    // PrintTaskAssignment(task_Msg);
    // printf("Node task list\n");
    PrintLinkAssList(&(linkConfigPtr->linkAllocList));

    // 判断自己是否是控制节点，不是的话给控制节点的对端模块进行汇总
    // 发送前先判断有无控制节点，没有直接跳出
    if (nodeNotificationPtr->intragroupcontrolNodeId)
    {
        cout << "判断有控制节点" << endl;
        cout << "nodeNotificationPtr->nodeResponsibility " << nodeNotificationPtr->nodeResponsibility << endl;
        if (nodeNotificationPtr->nodeResponsibility != MASTERCONTROL &&
            nodeNotificationPtr->nodeResponsibility != INTRAGROUPCONTROL &&
            linkConfigPtr->linkAllocList.size() >= LINK_MAX_IN_MSGFROMCONTROL)
        {
            cout << "自己是控制节点" << endl;
            unsigned int actualLinkNum = linkConfigPtr->linkAllocList.size();
            unsigned int payLoadSize = sizeof(LinkAssignmentHeader) + actualLinkNum * sizeof(LinkAssignment);
            // 申请内存，大小为header+多个LA，注意申请完不释放，读取处理后释放
            char *pktStart = (char *)malloc(payLoadSize);
            LinkAssignmentHeader *laHeaderPtr = (LinkAssignmentHeader *)pktStart;
            laHeaderPtr->linkNum = actualLinkNum;
            // 指针指向第一个LA
            LinkAssignment *linkPtr = (LinkAssignment *)(pktStart + sizeof(LinkAssignmentHeader));
            list<LinkAssignment *>::iterator linkIter = linkConfigPtr->linkAllocList.begin();
            // 对每一个LA进行赋值
            for (int i = 0; i < actualLinkNum; i++)
            {
                //*linkIter是LinkAssignment*
                linkPtr[i] = *(*linkIter);
                linkIter++;
            }
            NodeAddress destId = nodeNotificationPtr->intragroupcontrolNodeId;
            vector<uint8_t> *v1_controlmsg = LinkConfigAddHeader_controlmsg(destId, pktStart, payLoadSize, 0, 0);

            // 将封装后的业务数据包封装成层间接口数据帧发给路由模块
            uint8_t wBufferToRouting[MAX_DATA_LEN];                // 将要写入循环缓冲区的数据(含包头)存放于此
            PackRingBuffer(wBufferToRouting, v1_controlmsg, 0x1E); // 0x1E管控开销消息
            netToRouting_buffer.ringBuffer_put(wBufferToRouting, sizeof(wBufferToRouting));
            cout << "自身不是控制节点，链路构建需求发送给路由模块完成" << endl;
            delete v1_controlmsg;
            // 清空原list
            ClearTaskList(&(linkConfigPtr->linkAllocList));
            linkConfigPtr->linkAllocList.clear();
        }
    }
    else
    {
        cout << "无法汇总给控制节点，请等待身份配置完成 !" << endl;
        return;
    }
}

// 打印一条路由的完整路径
void PrintFullRTPath(vector<uint16_t> *relayNodesPtr)
{

    if (relayNodesPtr)
    {
        vector<uint16_t>::iterator i;
        // printf("Find route full path: ");
        for (i = relayNodesPtr->begin(); i != relayNodesPtr->end(); i++)
        {
            // printf("%u, ", *i);
        }
        // printf("\n");
    }
}

// 将完整链路打散
void BreakFullRoute(vector<uint16_t> *relayNodesPtr, TaskAssignment origTask)
{
    // 完整路径节点数
    unsigned int fullNodeNum = relayNodesPtr->size();
    // 直连链路条数=节点数-1
    unsigned int fullLinkNum = fullNodeNum - 1;
    vector<uint16_t>::iterator node1Ptr = relayNodesPtr->begin();
    vector<uint16_t>::iterator node2Ptr = ++node1Ptr;
    node1Ptr--;
    // 这样node1Ptr指向路径的第一个节点，node2Ptr指向下一个节点
    // // printf("Node break full path\n");
    for (int i = 0; i < fullLinkNum; i++)
    {
        unsigned short begin_node = *node1Ptr;
        unsigned short end_node = *node2Ptr;
        // PrintLinkAssignment(&partLink);
        // 找自己的链路列表中是否存过相同链路（源节点和目的节点相同）
        LinkAssignment *resultLinkPtr = FindSameLinkInList(&(linkConfigPtr->linkAllocList), begin_node, end_node);
        if (resultLinkPtr)
        {
            // 非空，找到了
            // printf("Find path %u to %u in list\n", begin_node, end_node);
            // 不满的情况
            if (resultLinkPtr->listNumber < BUSSINE_TYPE_NUM)
            {
                resultLinkPtr->type[resultLinkPtr->listNumber] = origTask.type;
                resultLinkPtr->frequency[resultLinkPtr->listNumber] = origTask.frequency;
                resultLinkPtr->priority[resultLinkPtr->listNumber] = origTask.priority;
                resultLinkPtr->QOS[resultLinkPtr->listNumber] = origTask.QOS;
                resultLinkPtr->size[resultLinkPtr->listNumber] = origTask.size;
                resultLinkPtr->begin_time[resultLinkPtr->listNumber][0] = origTask.begin_time[0];
                resultLinkPtr->begin_time[resultLinkPtr->listNumber][1] = origTask.begin_time[1];
                resultLinkPtr->begin_time[resultLinkPtr->listNumber][2] = origTask.begin_time[2];
                resultLinkPtr->end_time[resultLinkPtr->listNumber][0] = origTask.end_time[0];
                resultLinkPtr->end_time[resultLinkPtr->listNumber][1] = origTask.end_time[1];
                resultLinkPtr->end_time[resultLinkPtr->listNumber][2] = origTask.end_time[2];
                resultLinkPtr->listNumber++;
            }
        }
        else
        {
            // 没有相同的，需新建
            // printf("Not find path %u to %u in list\n", begin_node, end_node);
            LinkAssignment *newLink = new LinkAssignment;
            newLink->begin_node = begin_node;
            newLink->end_node = end_node;
            newLink->listNumber = 1;
            newLink->frequency[0] = origTask.frequency;
            newLink->priority[0] = origTask.priority;
            newLink->QOS[0] = origTask.QOS;
            newLink->size[0] = origTask.size;
            newLink->type[0] = origTask.type;
            newLink->begin_time[0][0] = origTask.begin_time[0];
            newLink->begin_time[0][1] = origTask.begin_time[1];
            newLink->begin_time[0][2] = origTask.begin_time[2];
            newLink->end_time[0][0] = origTask.end_time[0];
            newLink->end_time[0][1] = origTask.end_time[1];
            newLink->end_time[0][2] = origTask.end_time[2];
            linkConfigPtr->linkAllocList.push_back(newLink);
        }

        // 最后节点后移
        node1Ptr++;
        node2Ptr++;
    }
}

// 打印一条链路构建要求
void PrintLinkAssignment(LinkAssignment *taskPtr)
{
    int listNum = taskPtr->listNumber;
    for (int i = 0; i < listNum; i++)
    {
        // printf("Begin: %u, end: %u, type: %u, pri: %u, size: %.4f QOS: %u ",
        // taskPtr->begin_node, taskPtr->end_node, taskPtr->type[i], taskPtr->priority[i], taskPtr->size[i], taskPtr->QOS[i]);
        // printf("begin_time: %uh  %um  %us end_time: %uh  %um  %us\n",
        //    taskPtr->begin_time[i][0], taskPtr->begin_time[i][1], taskPtr->begin_time[i][2],
        //    taskPtr->end_time[i][0], taskPtr->end_time[i][1], taskPtr->end_time[i][2]);
    }
}

// 根据给定链路在链路列表里找是否有相同段
LinkAssignment *FindSameLinkInList(list<LinkAssignment *> *linkListPtr, unsigned short begin_node, unsigned short end_node)
{
    if (linkListPtr)
    {
        list<LinkAssignment *>::iterator linkIter;
        for (linkIter = linkListPtr->begin(); linkIter != linkListPtr->end(); linkIter++)
        {
            //*linkIter是LinkAssignment*
            if ((*linkIter)->begin_node == begin_node && (*linkIter)->end_node == end_node)
            {
                // 找到相等的链路了
                return *linkIter;
            }
        }
    }
    else
    {
        cout << "链路列表为空，找不到该链路 " << endl;
    }

    // 最后也没找到
    return NULL;
}

// 打印服务层传来的任务传输指令
void PrintTaskAssignment(TaskAssignment *taskPtr)
{
    // printf("Begin: %u, end: %u, type: %u, pri: %u, size: %.4f QOS: %u ",
    //     taskPtr->begin_node, taskPtr->end_node, taskPtr->type, taskPtr->priority, taskPtr->size, taskPtr->QOS);
    // printf("begin_time: %uh  %um  %us end_time: %uh  %um  %us\n",
    //            taskPtr->begin_time[0], taskPtr->begin_time[1], taskPtr->begin_time[2],
    //            taskPtr->end_time[0], taskPtr->end_time[1], taskPtr->end_time[2]);
}

// 打印链路构建要求列表
void PrintLinkAssList(list<LinkAssignment *> *linkListPtr)
{
    if (linkListPtr)
    {
        list<LinkAssignment *>::iterator i;
        for (i = linkListPtr->begin(); i != linkListPtr->end(); i++)
        {
            /*// printf("Link begin: %u, end: %u, size: %f, beginTime: %uh %um %us\n",
            (*i)->begin_node, (*i)->end_node, (*i)->size, (*i)->begin_time[0], (*i)->begin_time[1], (*i)->begin_time[2]);*/
            PrintLinkAssignment((*i));
        }
    }
}

// 给链路构建需求添加包头并组成msgFromControl
vector<uint8_t> *LinkConfigAddHeader_controlmsg(NodeAddress destId, char *pktStart, int payLoadSize, int moduleName, int eventType)
{
    // DataManager* dataManagerPtr = (DataManager*)node->networkData.networkVar->DataManager;
    // 总大小为 msgID + MessageDiversionHeader+原大小(LAheader+多个LA)
    unsigned int fullpacketSize = sizeof(msgID) + sizeof(MessageDiversionHeader) + payLoadSize;
    // 申请内存
    char *msgToSendPtr = (char *)malloc(fullpacketSize);
    // 对消息标识赋值 业务信道加消息标识
    msgID *msgIdPtr = (msgID *)msgToSendPtr;
    msgIdPtr->msgid = 28; // 北航填28
    msgIdPtr->msgsub = 5; // 消息子标识，标志传输链路配置模块
    // 对分流头赋值
    MessageDiversionHeader *diversionPtr = (MessageDiversionHeader *)(msgToSendPtr + sizeof(msgID));
    diversionPtr->moduleName = moduleName;
    diversionPtr->eventType = eventType;
    // 其次对负载进行赋值（LAHeader+多个LA），因为前面赋过值，此处直接复制
    memcpy(msgToSendPtr + sizeof(msgID) + sizeof(MessageDiversionHeader), pktStart, payLoadSize);

    // 将要发送的msgfromcontrol中的具体内容（给控制节点汇总的链路构建需求）转成vector<uint8_t>的数组
    vector<uint8_t> *msgToSend_data = PackDataOfMsgFromControl(msgToSendPtr);
    free(msgToSendPtr); // 实际数据内容已msgToSend_data经写入vector数组，释放掉申请的内存

    // 封装msgFromControl包
    msgFromControl controlmsg;
    controlmsg.data = (char *)msgToSend_data;
    controlmsg.packetLength = 8 + msgToSend_data->size() + 2; // 包长度是 8 + data的长度 + CRC校验的2
    controlmsg.srcAddr = linkConfigPtr->nodeId;               // 源地址为本节点
    controlmsg.destAddr = destId;
    controlmsg.priority = 0;    // 网管消息优先级为0
    controlmsg.backup = 0;      // 备用字段为0
    controlmsg.msgType = 4;     // 网管消息走业务信道
    controlmsg.repeat = 0;      // 非重传包
    controlmsg.fragmentNum = 0; // 无效值
    controlmsg.fragmentSeq = 0; // 分片序号，无效值
    // 序列号待补充

    // 将controlmsg转成8位数组方便CRC校验
    vector<uint8_t> *v1_controlmsg = PackMsgFromControl(&controlmsg);
    // 发端成包处统计开销 业务信道链路层包头25
    // dataManagerPtr->Manager_cost = dataManagerPtr->Manager_cost + v1_controlmsg->size() + 25;
    // cout << "节点 " << linkConfigPtr->nodeId << " 向节点 " << destId << "汇总链路构建需求， 开销为 " << v1_controlmsg->size() + 25 << "目前总开销为 " << dataManagerPtr->Manager_cost <<endl;
    return v1_controlmsg;
}

// 将要发送的msgfromcontrol中char*指针的内容转成vector<>的数组
vector<uint8_t> *PackDataOfMsgFromControl(char *payloadStart)
{
    vector<uint8_t> *cur_data = new vector<uint8_t>;
    // 封消息标识
    msgID *msgIdPtr = (msgID *)payloadStart;
    cur_data->push_back((msgIdPtr->msgid << 3) | (msgIdPtr->msgsub));
    // 封分流头
    MessageDiversionHeader *diversionPtr = (MessageDiversionHeader *)(payloadStart + sizeof(msgID));
    cur_data->push_back((diversionPtr->moduleName >> 8) & 0xFF); // 取高8位
    cur_data->push_back((diversionPtr->moduleName) & 0xFF);      // 取低8位
    cur_data->push_back((diversionPtr->eventType >> 8) & 0xFF);  // 取高8位
    cur_data->push_back((diversionPtr->eventType) & 0xFF);       // 取低8位

    // 封LinkAssignmentHeader
    LinkAssignmentHeader *laHeader = (LinkAssignmentHeader *)(payloadStart + sizeof(msgID) + sizeof(MessageDiversionHeader));
    cur_data->push_back((laHeader->linkNum >> 8) & 0xFF); // 取高8位
    cur_data->push_back((laHeader->linkNum) & 0xFF);      // 取低8位
    // 封LinkAssignment的具体内容
    unsigned int linkNumInMsg = laHeader->linkNum;
    // 此时指向第一个LA
    LinkAssignment *linkPtr = (LinkAssignment *)(payloadStart + sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(LinkAssignmentHeader));
    for (int i = 0; i < linkNumInMsg; i++)
    {
        cur_data->push_back((linkPtr[i].listNumber >> 8) & 0xFF); // 取高8位
        cur_data->push_back((linkPtr[i].listNumber) & 0xFF);      // 取低8位
        unsigned short listNum = linkPtr[i].listNumber;
        cur_data->push_back((linkPtr[i].begin_node >> 8) & 0xFF); // 取高8位
        cur_data->push_back((linkPtr[i].begin_node) & 0xFF);      // 取低8位
        cur_data->push_back((linkPtr[i].end_node >> 8) & 0xFF);   // 取高8位
        cur_data->push_back((linkPtr[i].end_node) & 0xFF);        // 取低8位
        for (int j = 0; j < listNum; j++)
        {
            cur_data->push_back((linkPtr[i].type[j] >> 8) & 0xFF);     // 取高8位
            cur_data->push_back((linkPtr[i].type[j]) & 0xFF);          // 取低8位
            cur_data->push_back((linkPtr[i].priority[j] >> 8) & 0xFF); // 取高8位
            cur_data->push_back((linkPtr[i].priority[j]) & 0xFF);      // 取低8位
            TurnFloatToUint(linkPtr[i].size[j], cur_data);
            cur_data->push_back((linkPtr[i].QOS[j] >> 8) & 0xFF);           // 取高8位
            cur_data->push_back((linkPtr[i].QOS[j]) & 0xFF);                // 取低8位
            cur_data->push_back((linkPtr[i].begin_time[j][0] >> 8) & 0xFF); // 取高8位
            cur_data->push_back((linkPtr[i].begin_time[j][0]) & 0xFF);      // 取低8位
            cur_data->push_back((linkPtr[i].begin_time[j][1] >> 8) & 0xFF); // 取高8位
            cur_data->push_back((linkPtr[i].begin_time[j][1]) & 0xFF);      // 取低8位
            cur_data->push_back((linkPtr[i].begin_time[j][2] >> 8) & 0xFF); // 取高8位
            cur_data->push_back((linkPtr[i].begin_time[j][2]) & 0xFF);      // 取低8位
            cur_data->push_back((linkPtr[i].end_time[j][0] >> 8) & 0xFF);   // 取高8位
            cur_data->push_back((linkPtr[i].end_time[j][0]) & 0xFF);        // 取低8位
            cur_data->push_back((linkPtr[i].end_time[j][1] >> 8) & 0xFF);   // 取高8位
            cur_data->push_back((linkPtr[i].end_time[j][1]) & 0xFF);        // 取低8位
            cur_data->push_back((linkPtr[i].end_time[j][2] >> 8) & 0xFF);   // 取高8位
            cur_data->push_back((linkPtr[i].end_time[j][2]) & 0xFF);        // 取低8位
            cur_data->push_back((linkPtr[i].frequency[j] >> 8) & 0xFF);     // 取高8位
            cur_data->push_back((linkPtr[i].frequency[j]) & 0xFF);          // 取低8位
        }
    }
    return cur_data;
}

// 将float数据存入vector<uint8_t>数组中
void TurnFloatToUint(float size, vector<uint8_t> *buffer)
{
    buffer->push_back(((int)size >> 24) & 0xFF);
    buffer->push_back(((int)size >> 16) & 0xFF);
    buffer->push_back(((int)size >> 8) & 0xFF);
    buffer->push_back((int)size & 0xFF);
}

// 清空任务列表
void ClearTaskList(list<LinkAssignment *> *taskListPtr)
{
    list<LinkAssignment *>::iterator i;
    for (i = taskListPtr->begin(); i != taskListPtr->end(); i++)
    {
        //*i是LinkAssignment*
        delete *i;
    }
}

// 定期检查自己的任务列表, 到期总分控制节点给MAC发，其余节点给控制节点发
void CheckTaskListTimeout()
{
    unsigned int actualLinkNum = linkConfigPtr->linkAllocList.size();
    if (actualLinkNum)
    {
        // 不为0，有链路才进行发送
        unsigned int payLoadSize = sizeof(LinkAssignmentHeader) + actualLinkNum * sizeof(LinkAssignment);
        // 申请内存，大小为header+多个LA，注意申请完不释放，读取处理后释放
        char *pktStart = (char *)malloc(payLoadSize);
        LinkAssignmentHeader *laHeaderPtr = (LinkAssignmentHeader *)pktStart;
        laHeaderPtr->linkNum = actualLinkNum;
        // 指针指向第一个LA
        LinkAssignment *linkPtr = (LinkAssignment *)(pktStart + sizeof(LinkAssignmentHeader));
        list<LinkAssignment *>::iterator linkIter = linkConfigPtr->linkAllocList.begin();
        // 对每一个LA进行赋值
        for (int i = 0; i < actualLinkNum; i++)
        {
            //*linkIter是LinkAssignment*
            linkPtr[i] = *(*linkIter);
            // 发之前打印一下
            // PrintLinkAssignment(&(linkPtr[i]));
            linkIter++;
        }
        // 根据节点身份采取不同动作
        if (nodeNotificationPtr->nodeResponsibility == MASTERCONTROL || nodeNotificationPtr->nodeResponsibility == INTRAGROUPCONTROL)
        {
            // 如果是总/分控制节点，给链路层发
            //**************新增链路运行状态判断*****************************
            bool flag = MacDaatr_judge_if_could_send_config_or_LinkRequist(0);
            if (!flag)
            {
                // 如果不是任务执行阶段
                cout << "当前链路状态不在任务执行阶段，链路需求暂不下发 " << endl;
                // 设置定时器，500ms后重复检查
                SetTimerUser2(0, 500, 6, NULL, 0); // 6为定时器编号
                return;
            }
            //****************************************************************

            //******************新增:下发清空前，发给网络状态视图并输出到txt****
            OutputTaskListToTxt(&(linkConfigPtr->linkAllocList), LINK_ASSIGNMENT_FILE); // 输出到文件
            // 输出到网络状态视图
            //***************此处新增为网络状态视图处理链路构建需求消息*************************
            // 网络状态视图根据 链路构建需求消息计算节点繁忙度并填入网络状态视图中
            // 繁忙度 = （所有起点/终点是该节点的需求）大小 * 频率之和吗 繁忙度计算方法待定！！！
            //**********************************************************************************
            cout << "网络状态视图模块收到链路构建需求消息，以计算节点繁忙度! " << endl;

            //******************************************************************

            cout << "Node LinkConfig has " << linkConfigPtr->linkAllocList.size() << " links to send to MAC" << endl;
            PrintLinkAssList(&(linkConfigPtr->linkAllocList));
            // 给链路层发送链路构建需求
            uint8_t wBufferToMac[MAX_DATA_LEN];                        // 将要写入循环缓冲区的数据(含包头)存放于此
            PackRingBuffer(wBufferToMac, pktStart, payLoadSize, 0x0B); // 链路构建要求
            RoutingTomac_Buffer.ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
            // MAC_Daatr_Receive->ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
            free(pktStart);
            cout << "控制节点给链路层发送链路构建需求完成!" << endl;

            // 清空原list
            ClearTaskList(&(linkConfigPtr->linkAllocList));
            linkConfigPtr->linkAllocList.clear();
        }
        else
        {
            // 如果是其他节点，通过路由模块透传发给控制节点汇总
            NodeAddress destId = nodeNotificationPtr->intragroupcontrolNodeId;
            vector<uint8_t> *v1_controlmsg = LinkConfigAddHeader_controlmsg(destId, pktStart, payLoadSize, 0, 0);

            // 将封装后的业务数据包封装成层间接口数据帧发给路由模块
            uint8_t wBufferToRouting[MAX_DATA_LEN];                // 将要写入循环缓冲区的数据(含包头)存放于此
            PackRingBuffer(wBufferToRouting, v1_controlmsg, 0x1E); // 0x1E管控开销消息
            netToRouting_buffer.ringBuffer_put(wBufferToRouting, sizeof(wBufferToRouting));
            delete v1_controlmsg;
            cout << "普通节点给路由模块发送链路构建需求完成!" << endl;

            // 清空原list
            ClearTaskList(&(linkConfigPtr->linkAllocList));
            linkConfigPtr->linkAllocList.clear();
        }

        if (nodeNotificationPtr->nodeResponsibility == MASTERCONTROL || nodeNotificationPtr->nodeResponsibility == INTRAGROUPCONTROL)
        {
            // 控制节点下发链路构建需求周期为10s
            SetTimerUser2(10, 0, 6, NULL, 0); // 6为定时器编号
        }
        else
        {
            // 普通节点汇聚链路构建需求周期为5s
            SetTimerUser2(5, 0, 6, NULL, 0); // 6为定时器编号
        }
    }
}

// 将链路构建需求输出到txt
void OutputTaskListToTxt(list<LinkAssignment *> *linkListPtr, const string filename)
{
    // 打开一个文本文件用于写入
    std::ofstream outFile(filename, ios::app); // 在文件尾追加
    // 检查文件是否成功打开
    if (!outFile.is_open())
    {
        cerr << "无法打开文件." << endl;
        return;
    }

    // 向文件写入 TaskList
    if (linkListPtr) // 当链路构建需求列表指针不为空
    {
        list<LinkAssignment *>::iterator i;
        for (i = linkListPtr->begin(); i != linkListPtr->end(); i++)
        {
            int listNum = (*i)->listNumber;
            for (int j = 0; j < listNum; j++)
            {
                outFile << "Begin: " << (*i)->begin_node << "  end: " << (*i)->end_node << " type: " << (*i)->type[j] << " pri: ";
                outFile << (*i)->priority[j] << " size: " << (*i)->size[j] << " QOS: " << (*i)->QOS[j];
                outFile << " begin_time: " << (*i)->begin_time[j][0] << " h ";
                outFile << " begin_time: " << (*i)->begin_time[j][2] << " m ";
                outFile << " begin_time: " << (*i)->begin_time[j][1] << " s ";
                outFile << " end_time: " << (*i)->end_time[j][0] << " h ";
                outFile << " end_time: " << (*i)->end_time[j][1] << " m ";
                outFile << " end_time: " << (*i)->end_time[j][2] << " s " << endl;
            }
        }
    }
    // 关闭文件
    outFile.close();
}

// 处理收到的管控开销消息（西电）
void HandleControlMsgFromRoute(const vector<uint8_t> &control_MsgPtr)
{
    // 处理从路由模块发来的管控消息
    // 主要执行分流操作
    // 此时msg携带一个msgFromControl
    vector<uint8_t> curPktPayload(control_MsgPtr.begin(), control_MsgPtr.end());
    vector<uint8_t> *v_controlmsg = &curPktPayload;

    msgFromControl *controlPtr = AnalysisLayer2MsgFromControl(v_controlmsg);
    // 收到管控包进行crc校验，创建一个新vector<uint8_t>来重算CRC
    vector<uint8_t> *cur_PktPayload = new vector<uint8_t>();
    cur_PktPayload->insert(cur_PktPayload->begin(), v_controlmsg->begin(), v_controlmsg->begin() + controlPtr->packetLength - 2);
    uint16_t crc = CRC16(cur_PktPayload);
    delete cur_PktPayload;

    if (crc == controlPtr->CRC) // CRC校验通过，做分流操作
    {
        // 只需解出MessageDiversionHeader
        // controlPtr->data目前是一个vector<uint8_t>的数组 第一位是msgId，第二位开始是模块名和事件名
        vector<uint8_t> *data_control = (vector<uint8_t> *)controlPtr->data;
        unsigned int moduleName = ((*data_control)[1] << 8) | (*data_control)[2];
        unsigned int eventType = ((*data_control)[3] << 8) | (*data_control)[4];
        if (eventType == 0)
        {
            // 处理其他节点同层模块的传输任务任务指令
            HandleTaskListFromOther(controlPtr);
        }
        else if (eventType == 1)
        {
            // 其他节点处理控制节点发来的身份广播包
            ReceiveAMsgFromOtherNodes((char *)controlPtr);
        }
        else if (eventType == 2)
        {
            // 控制节点&控制备份 处理其他节点汇总过来的毁伤信息
            ControlNodeDealWithDamage(controlPtr);
        }
    }
}

// 处理其他节点同层模块的传输任务任务指令
void HandleTaskListFromOther(msgFromControl *controlMsgPtr)
{
    // 比对后，放入/新建存入自己的taskList
    cout << "收到其他节点的任务传输指令" << endl;
    // 将controlPtr->data解析出来
    char *payloadStart = AnalysisDataInMsgFromControl((vector<uint8_t> *)controlMsgPtr->data);
    // 释放数组data里的内容
    free((vector<uint8_t> *)controlMsgPtr->data);
    // 跳过MessageDiversionHeader，指向LAHeader
    LinkAssignmentHeader *laHeader = (LinkAssignmentHeader *)(payloadStart + sizeof(msgID) + sizeof(MessageDiversionHeader));

    if (nodeNotificationPtr->nodeResponsibility == INTRAGROUPCONTROL || nodeNotificationPtr->nodeResponsibility == MASTERCONTROL)
    {
        // 主、分控制节点才会接受其他节点传来的任务列表
        unsigned int linkNumInMsg = laHeader->linkNum;
        // 此时指向第一个LA
        LinkAssignment *linkPtr = (LinkAssignment *)(payloadStart + sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(LinkAssignmentHeader));

        for (int i = 0; i < linkNumInMsg; i++)
        {
            LinkAssignment *resultLinkPtr = FindSameLinkInList(&(linkConfigPtr->linkAllocList), linkPtr[i].begin_node, linkPtr[i].end_node);
            PrintLinkAssignment(&(linkPtr[i]));
            if (resultLinkPtr)
            {
                // 非空，找到了，再想想//其他节点发来的linkAssingment可能也是个数组了
                // 写个循环把数组里的这一些些都赋值过来
                if (resultLinkPtr->listNumber < BUSSINE_TYPE_NUM)
                {
                    for (int j = 0; j < linkPtr[i].listNumber; j++)
                    {
                        resultLinkPtr->type[resultLinkPtr->listNumber] = linkPtr[i].type[j];
                        resultLinkPtr->frequency[resultLinkPtr->listNumber] = linkPtr[i].frequency[j];
                        resultLinkPtr->priority[resultLinkPtr->listNumber] = linkPtr[i].priority[j];
                        resultLinkPtr->QOS[resultLinkPtr->listNumber] = linkPtr[i].QOS[j];
                        resultLinkPtr->size[resultLinkPtr->listNumber] = linkPtr[i].size[j];
                        resultLinkPtr->begin_time[resultLinkPtr->listNumber][0] = linkPtr[i].begin_time[j][0];
                        resultLinkPtr->begin_time[resultLinkPtr->listNumber][1] = linkPtr[i].begin_time[j][1];
                        resultLinkPtr->begin_time[resultLinkPtr->listNumber][2] = linkPtr[i].begin_time[j][2];
                        resultLinkPtr->end_time[resultLinkPtr->listNumber][0] = linkPtr[i].begin_time[j][0];
                        resultLinkPtr->end_time[resultLinkPtr->listNumber][1] = linkPtr[i].begin_time[j][1];
                        resultLinkPtr->end_time[resultLinkPtr->listNumber][2] = linkPtr[i].begin_time[j][2];
                        resultLinkPtr->listNumber++;
                        if (resultLinkPtr->listNumber >= BUSSINE_TYPE_NUM)
                            break;
                    }
                }
            }
            else
            {
                // 没有相同的，需新建
                LinkAssignment *newLink = new LinkAssignment;
                *newLink = linkPtr[i];
                // newLink->listNumber = 1;
                linkConfigPtr->linkAllocList.push_back(newLink);
            }
        }
    }
    // 最后释放发送方申请的内存
    free(payloadStart);
}

// 将vector<uint8_t>中的数据解析出来
char *AnalysisDataInMsgFromControl(vector<uint8_t> *dataPacket)
{
    // 先把存了多少条linkAssignment找出来
    unsigned int linknum = ((*dataPacket)[5] << 8) | (*dataPacket)[6];
    unsigned int fullPacketSize = sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(LinkAssignmentHeader) + linknum * sizeof(LinkAssignment);
    char *payload = (char *)malloc(fullPacketSize);
    // 解出msgID
    msgID *msgIdPtr = (msgID *)payload;
    msgIdPtr->msgid = ((*dataPacket)[0] >> 3) & 0x1F; // 取消息标识
    msgIdPtr->msgsub = ((*dataPacket)[0] & 0x07);     // 取消息子标识

    // 将分流头解出来
    MessageDiversionHeader *diversionPtr = (MessageDiversionHeader *)(payload + sizeof(msgID));
    diversionPtr->moduleName = ((*dataPacket)[1] << 8) | (*dataPacket)[2];
    diversionPtr->eventType = ((*dataPacket)[3] << 8) | (*dataPacket)[4];

    LinkAssignmentHeader *linkheader = (LinkAssignmentHeader *)(payload + sizeof(msgID) + sizeof(MessageDiversionHeader));
    linkheader->linkNum = linknum;
    LinkAssignment *linkAssPtr = (LinkAssignment *)(payload + sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(LinkAssignmentHeader));
    unsigned short servicesNum = 0;
    int cnt = 7; // cnt记录LinkAssignment从数组多少位开始取
    for (int i = 0; i < linkheader->linkNum; i++)
    {
        linkAssPtr[i].listNumber = ((*dataPacket)[cnt + servicesNum] << 8) | (*dataPacket)[cnt + 1 + servicesNum];
        // 第一次循环，从第6位开始，之后每循环一次，往后多读 6 + listNumber * 24 位
        // servicesNum += 6 + linkAssPtr[i].listNumber * 24;
        linkAssPtr[i].begin_node = ((*dataPacket)[cnt + 2 + servicesNum] << 8) | (*dataPacket)[cnt + 3 + servicesNum];
        linkAssPtr[i].end_node = ((*dataPacket)[cnt + 4 + servicesNum] << 8) | (*dataPacket)[cnt + 5 + servicesNum];
        // listNumber指示数组位有多少位有效，每一位有效 需要24个
        for (int j = 0; j < linkAssPtr[i].listNumber; j++)
        {
            linkAssPtr[i].type[j] = ((*dataPacket)[cnt + 6 + j * 24 + servicesNum] << 8) | (*dataPacket)[cnt + 7 + j * 24 + servicesNum];
            linkAssPtr[i].priority[j] = ((*dataPacket)[cnt + 8 + j * 24 + servicesNum] << 8) | (*dataPacket)[cnt + 9 + j * 24 + servicesNum];
            linkAssPtr[i].size[j] = ((*dataPacket)[cnt + 10 + j * 24 + servicesNum] << 24) | ((*dataPacket)[cnt + 11 + j * 24 + servicesNum] << 16) | ((*dataPacket)[cnt + 12 + j * 24 + servicesNum] << 8) | (*dataPacket)[cnt + 13 + j * 24 + servicesNum];
            linkAssPtr[i].QOS[j] = ((*dataPacket)[cnt + 14 + j * 24 + servicesNum] << 8) | (*dataPacket)[cnt + 15 + j * 24 + servicesNum];
            linkAssPtr[i].begin_time[j][0] = ((*dataPacket)[cnt + 16 + j * 24 + servicesNum] << 8) | (*dataPacket)[cnt + 17 + j * 24 + servicesNum];
            linkAssPtr[i].begin_time[j][1] = ((*dataPacket)[cnt + 18 + j * 24 + servicesNum] << 8) | (*dataPacket)[cnt + 19 + j * 24 + servicesNum];
            linkAssPtr[i].begin_time[j][2] = ((*dataPacket)[cnt + 20 + j * 24 + servicesNum] << 8) | (*dataPacket)[cnt + 21 + j * 24 + servicesNum];
            linkAssPtr[i].end_time[j][0] = ((*dataPacket)[cnt + 22 + j * 24 + servicesNum] << 8) | (*dataPacket)[cnt + 23 + j * 24 + servicesNum];
            linkAssPtr[i].end_time[j][1] = ((*dataPacket)[cnt + 24 + j * 24 + servicesNum] << 8) | (*dataPacket)[cnt + 25 + j * 24 + servicesNum];
            linkAssPtr[i].end_time[j][2] = ((*dataPacket)[cnt + 26 + j * 24 + servicesNum] << 8) | (*dataPacket)[cnt + 27 + j * 24 + servicesNum];
            linkAssPtr[i].frequency[j] = ((*dataPacket)[cnt + 28 + j * 24 + servicesNum] << 8) | (*dataPacket)[cnt + 29 + j * 24 + servicesNum];
        }
        servicesNum += 6 + linkAssPtr[i].listNumber * 24;
    }

    return payload;
}

// 处理收到的链路层链路运行状态，进行更新
void UpdateLinkStatus(const vector<uint8_t> &linkStatusPtr)
{
    LinkStatus *linkStatus_Msg = (LinkStatus *)&linkStatusPtr[0];
    linkConfigPtr->linkStatus = linkStatus_Msg->linkStatus; // 更新链路状态
}
/*------------------------network_translinkconfig.cpp------------------------------------*/

/*------------------------network_netview.cpp------------------------------------*/
// 网络视图数据结构初始化
void NetViewInit()
{
    // 节点id如何初始化

    netViewPtr->nodeId = mmanet->nodeAddr;
}

// 处理路由模块传来的网络拓扑（西电）
void HandleReceivedTopology(const vector<uint8_t> &TopologyMsgPtr)
{
    // printf("node received topology\n");
    // 处理从路由模块传来的网络拓扑
    NetworkTopologyMsg *topologyMsgPtr = (NetworkTopologyMsg *)&TopologyMsgPtr[0];
    vector<nodeNetNeighList *> *topologyPtr = topologyMsgPtr->netNetghPtr;
    // i是节点
    vector<nodeNetNeighList *>::iterator i;
    // j是i的邻居
    vector<nodeLocalNeighList *>::iterator j;
    NodeAddress nodeAddr;

    if (topologyPtr->size() != 0)
    {
        // 先清掉旧表
        CleanNetViewTopology(&(netViewPtr->netViewList));
        for (i = topologyPtr->begin(); i != topologyPtr->end(); i++)
        {
            // 遍历节点列表
            NodeCondition *newNode = new NodeCondition((*i)->nodeAddr);
            // 其他变量待补充

            for (j = (*i)->localNeighList.begin(); j != (*i)->localNeighList.end(); j++)
            {
                // 遍历i节点的邻居节点
                LinkQuality *newLink = new LinkQuality((*j)->nodeAddr);
                // newLink->linkDelay = (*j)->Delay;
                // 除了delay，其他指标待补充
                newNode->neighborList.push_back(newLink);
            }

            // 插完所有邻居，按id排一下序
            if (newNode->neighborList.size() > 1)
            {
                newNode->neighborList.sort(sortNeighborByNodeId);
            }

            // 把新节点插入到网络视图中
            netViewPtr->netViewList.push_back(newNode);
        }

        // 插完所有节点，按id排一下序
        netViewPtr->netViewList.sort(sortByNodeId);
        // 打印一下
        PrintNodeListAndNeiborWithId(&(netViewPtr->netViewList));
    }
    else
    {
        cout << "路由模块发送的高速拓扑表为空,节点高速不在网！ " << endl;
        // 节点高速不在网，网管模块将其身份置为普通
        nodeNotificationPtr->nodeResponsibility = NODENORMAL;
    }

    // 将发送方new出来的空间进行释放
    vector<nodeNetNeighList *>::iterator it1;
    vector<nodeLocalNeighList *>::iterator it2;
    for (it1 = topologyPtr->begin(); it1 != topologyPtr->end(); it1++)
    {
        for (it2 = (*it1)->localNeighList.begin(); it2 != (*it1)->localNeighList.end(); it2++)
        {
            delete *it2;
            *it2 = NULL;
        }

        delete *it1;
        *it1 = NULL;
    }
    delete topologyPtr;
}

// 清空网络状态视图
void CleanNetViewTopology(list<NodeCondition *> *nodeList)
{
    if (nodeList)
    {
        list<NodeCondition *>::iterator i;
        list<LinkQuality *>::iterator j;
        for (i = nodeList->begin(); i != nodeList->end(); i++)
        {
            // 顶点表的一个节点，*i是NodeCondition*
            for (j = (*i)->neighborList.begin(); j != (*i)->neighborList.end(); j++)
            {
                // 一个节点的一个邻居，*j是LinkQuality*
                delete *j;
                *j = NULL;
            }
            // 删除完所有邻居，清空邻居列表
            (*i)->neighborList.clear();
            // 再释放节点
            delete *i;
            *i = NULL;
        }
        // 删除完所有顶点，清空顶点表
        nodeList->clear();
    }
    cout << "nodeList length: " << nodeList->size() << ", 旧网络状态视图清空完成" << endl;
}

// 所有邻居，按id排序
bool sortNeighborByNodeId(LinkQuality *neighbor1, LinkQuality *neighbor2)
{
    return neighbor1->nodeId < neighbor2->nodeId;
}

// 将群内网络状态视图按id排序
bool sortByNodeId(NodeCondition *node1, NodeCondition *node2)
{
    return node1->nodeId < node2->nodeId; // 升序排列
}

// 打印网络状态视图
void PrintNodeListAndNeiborWithId(list<NodeCondition *> *nodeListPtr)
{
    if (nodeListPtr)
    {
        list<NodeCondition *>::iterator i;
        for (i = nodeListPtr->begin(); i != nodeListPtr->end(); i++)
        {
            // printf("NodeId: %u, PosX: %.2f, PosY: %.2f  PosZ: %.2f", (*i)->nodeId, (*i)->thisPos.positionX, (*i)->thisPos.positionY, (*i)->thisPos.positionZ);
            cout << "  nodeResponse: " << (*i)->nodeType << endl;
            if ((*i)->neighborList.size() != 0)
            {
                // printf("Neighbor: ");
                list<LinkQuality *> neighborPtrList = (*i)->neighborList;
                list<LinkQuality *>::iterator neighborPtr;
                for (neighborPtr = neighborPtrList.begin(); neighborPtr != neighborPtrList.end();
                     neighborPtr++)
                {
                    // printf(" %u (%.4f),  ", (*neighborPtr)->nodeId, (float)(*i)->nodeId / ((*i)->nodeId + (*neighborPtr)->nodeId));
                }
            }
            // printf("\n");
        }
        // printf("Net View Finished! \n");
    }
    else
    {
        cout << "网络视图为空 !" << endl;
    }
}

// 处理链路层发来的其他节点飞行状态信息（成电）
void HandleReceivedNetPos(const vector<uint8_t> &NetPosMsgPtr)
{
    // 主要是在自己的结构里存储该指针
    char *pktStart = (char *)&NetPosMsgPtr[0];
    NetFlightStateMsg *nodePosMsg = (NetFlightStateMsg *)pktStart;
    netViewPtr->nodeNum = nodePosMsg->nodeNum;
    // netViewPtr->netPosPtr = nodePosMsg->nodePosPtr;
    if (netViewPtr->netPosPtr)
    {
        // 非空，已存储节点飞行状态，释放内存
        // printf("netPosPtr isn't empty\n");
        free(netViewPtr->netPosPtr);
    }

    unsigned int flightStateFullSize = nodePosMsg->nodeNum * sizeof(FlightStatus);
    // 因为是按值传递，所以要自行申请内存存起来
    netViewPtr->netPosPtr = (FlightStatus *)malloc(flightStateFullSize);
    memcpy(netViewPtr->netPosPtr, pktStart + sizeof(NetFlightStateMsg), flightStateFullSize);

    FlightStatus *nodePosList = netViewPtr->netPosPtr;
    // for(int i = 0; i < netViewPtr->nodeNum; i++) {
    //     cout << "nodeid = " << nodePosList[i].nodeId << endl;
    //     cout << nodePosList[i].accelerateX << endl;
    //     cout << nodePosList[i].accelerateY << endl;
    //     cout << nodePosList[i].accelerateZ << endl;
    //     cout << nodePosList[i].length << endl;
    //     cout << nodePosList[i].pitchAngle << endl;
    //     cout << nodePosList[i].positionX << endl;
    //     cout << nodePosList[i].positionY << endl;
    //     cout << nodePosList[i].positionZ << endl;
    //     cout << nodePosList[i].rollAngle << endl;
    //     cout << nodePosList[i].speedX << endl;
    //     cout << nodePosList[i].speedY << endl;
    //     cout << nodePosList[i].speedZ << endl;
    //     cout << nodePosList[i].timeStemp << endl;
    //     cout << nodePosList[i].yawAngle << endl;
    // }

    // 给自己的网络视图里读取
    if (netViewPtr->netViewList.size() != 0)
    {
        ReadNetPos();
        cout << "ReadNetPos!!!" << endl;

        // 进行节点身份正常更新，只在控制节点上进行
        if (nodeNotificationPtr->nodeResponsibility == INTRAGROUPCONTROL || nodeNotificationPtr->nodeResponsibility == MASTERCONTROL)
        {
            // SpacebasedUpdate();
            IntraGroupUpdate();
        }
    }
    else
    {
        cout << "未收到路由模块拓扑信息，暂不填入位置信息 !" << endl;
    }
}

// 处理从链路模块传来的节点位置
void ReadNetPos()
{
    unsigned int nodeNum = netViewPtr->nodeNum;
    FlightStatus *nodePosList = netViewPtr->netPosPtr;
    int i;
    // printf("Receive %u nodes flight state\n", nodeNum);

    for (i = 0; i < nodeNum; i++)
    {
        NodeAddress nodeId = nodePosList[i].nodeId;
        // 在网络视图中根据id找出该节点
        NodeCondition *findNode = GetANodeConditionById(&(netViewPtr->netViewList), nodeId);

        if (findNode)
        {
            // // printf("Find Node %u posX: %f posY: %f posZ: %f\n", findNode->nodeId, );
            // 找到了
            // 找到了将经纬高转为xyz//
            float fJ = nodePosList[i].positionX;
            float fW = nodePosList[i].positionY;
            float fZ = nodePosList[i].positionZ;

            float fx;
            float fy;
            float fz;
            float sinLat_tmp, cosLat_tmp, sinLon_tmp, cosLon_tmp, p;
            // fx = degToRad(fJ);
            // fy = degToRad(fW);
            fx = fJ * MY_PI / 180;
            fy = fW * MY_PI / 180;
            sinLat_tmp = sin(fx);
            cosLat_tmp = cos(fx);
            sinLon_tmp = sin(fy);
            cosLon_tmp = cos(fy);
            p = A / sqrt(1 - E2 * sinLon_tmp * sinLon_tmp);
            fx = (p + Daita_h) * cosLon_tmp * cosLat_tmp;
            fy = (p + Daita_h) * cosLon_tmp * sinLat_tmp;
            fz = (p * (1 - E2) + Daita_h) * sinLon_tmp;
            // 写入位置//
            findNode->thisPos.positionX = fx;
            findNode->thisPos.positionY = fy;
            findNode->thisPos.positionZ = fz;

            // 写入位置//
            // findNode->thisPos.positionX = nodePosList[i].positionX;
            // findNode->thisPos.positionY = nodePosList[i].positionY;
            // findNode->thisPos.positionZ = nodePosList[i].positionZ;

            // 写入速度（待补充）
            findNode->thisSpeed.speedX = nodePosList[i].speedX;
            findNode->thisSpeed.speedY = nodePosList[i].speedY;
            findNode->thisSpeed.speedZ = nodePosList[i].speedZ;
        }
        else
        {
            // printf("Not find node %u posX: %f posY: %f posZ: %f\n", nodeId, nodePosList[i].positionX, nodePosList[i].positionY, nodePosList[i].positionZ);
        }
    }

    // 打印一下
    // cout << "node " << netViewPtr->nodeId << "的网络视图为 ： " <<endl;
    // PrintNodeListAndNeiborWithId(&(netViewPtr->netViewList));
}

// 根据节点id在nodecondition中（按Id升序排列）找到对应节点
NodeCondition *GetANodeConditionById(list<NodeCondition *> *nodeList, unsigned int nodeId)
{
    if (nodeId == 0)
    {
        return NULL;
    }
    if (nodeList)
    {
        list<NodeCondition *>::iterator i;
        for (i = (*nodeList).begin(); i != (*nodeList).end(); i++)
        {
            // // printf("Enter circle %u\n", nodeId);
            if (nodeId < (*i)->nodeId)
            {
                // // printf("Not find to find: %u current: %u\n",nodeId, (*i)->nodeId);
                return NULL;
            }
            else if (nodeId == (*i)->nodeId)
            {
                // // printf("Find it!to find: %u current: %u\n",nodeId, (*i)->nodeId);
                return (*i);
            }
            // nodeId > i->nodeId, 什么都不做，直接后移
        }
    }
    // 最后一个都不是，即比最后一个还大，证明没找着
    return NULL;
}

// 处理服务层传来的本地飞行信息（应用层）
void HandleLocalFlightState(const vector<uint8_t> &LocalFlightStateMsgPtr)
{
    FlightStatus *flightPtr = (FlightStatus *)&LocalFlightStateMsgPtr[0];
    // 注意深浅拷贝的问题
    FlightStatus flightToTrans = *flightPtr;
    cout << "处理服务层传来的本地飞行信息（应用层）" << flightPtr << endl;
    cout << "节点 " << flightToTrans.nodeId << " 收到飞行状态，位置为 " << flightToTrans.positionX << " " << flightToTrans.positionY << " " << flightToTrans.positionZ << endl;

    // 发送给链路层
    uint8_t wBufferToMac[MAX_DATA_LEN];                                       // 将要写入循环缓冲区的数据(含包头)存放于此
    PackRingBuffer(wBufferToMac, &flightToTrans, sizeof(FlightStatus), 0x01); // 本地飞行状态信息
    // MAC_Daatr_Receive->ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
    RoutingTomac_Buffer.ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
    // printf("Node send local flight state to mac\n");
    // printf("处理服务层传来的本地飞行信息，已发送给链路层\n");
}

// 处理身份配置模块传来的身份配置信息，即将身份填入网络状态视图
void HandleIdentityUpdate(NodeNotification *nodeReponseMsgPtr)
{
    if (netViewPtr->nodeId == nodeReponseMsgPtr->intragroupcontrolNodeId) // 判断是自己是控制节点
    {
        if (netViewPtr->netViewList.size() != 0) // 网络状态视图不为空
        {
            // 所有节点身份初始化为普通节点，现填入关键节点身份
            // 群内控制节点
            NodeCondition *findNode = GetANodeConditionById(&(netViewPtr->netViewList), nodeReponseMsgPtr->intragroupcontrolNodeId);
            if (findNode)
            {
                findNode->nodeType = INTRAGROUPCONTROL;
            }

            // 备用控制
            findNode = GetANodeConditionById(&(netViewPtr->netViewList), nodeReponseMsgPtr->reserveintracontrolNodeId);
            if (findNode)
            {
                findNode->nodeType = RESERVEINTRAGROUPCONTROL;
            }

            // 其他关键节点身份待补充
            PrintNodeListAndNeiborWithId(&(netViewPtr->netViewList));
        }
        else
        {
            // 等待一段时间重新触发
            SetTimerUser2(1, 0, 7, nodeReponseMsgPtr, sizeof(nodeReponseMsgPtr));
        }
    }
}
/*------------------------network_netview.cpp------------------------------------*/

/*------------------------network_identityUpdateAndMaintenance.cpp------------------------------------*/
// 天基网关新加
list<SatebasedUpd_Factor> covertimelist;
int running_cnt = 0;

// 节点更新与维护初始, 初始化读入单独的地面选点cpp中计算出的各点身份的txt
void NetUpdateAndMaintenanceInit()
{
    extern MacDaatr daatr_str;
    // 初始化IdentityData，节点id如何获取？
    IdentityPtr->nodeId = daatr_str.nodeId;

    // 读取txt给NodeNotification初始化
    // printf("Node NodeNotification Init\n");
    ReadNetResponseFromText();
    PrintIdentity();
    cout << "Node:" << nodeNotificationPtr->nodeIIndex << "  Init response: " << nodeNotificationPtr->nodeResponsibility << endl;

    // 初始化统计变量

    // 初始化下发身份的定时器
    SetTimerUser2(1, 0, 8, NULL, 0);
}

// 读取txt给NodeNotification结构体赋值
void ReadNetResponseFromText()
{
    // printf("Read node information from txt\n");
    ifstream ifs;
    ifs.open(INPUTNODEFILE, ios::in);
    if (!ifs.is_open())
    {
        // printf("File not open!\n");
    }
    NodeNotification *resultPtr = nodeNotificationPtr; // 读入位置

    if (resultPtr) // 身份结构体指针不为空
    {
        int nodeIIndex;
        int groupID;
        int nodeResponsibility; // 定义临时的读入变量
        while (ifs >> nodeIIndex && ifs >> groupID && ifs >> nodeResponsibility)
        {
            if (nodeIIndex == IdentityPtr->nodeId) // 自己的身份
            {
                resultPtr->nodeIIndex = nodeIIndex;
                resultPtr->groupID = groupID;
                resultPtr->nodeResponsibility = nodeResponsibility;
            }
            if (nodeResponsibility != NODENORMAL) // 读入关键节点身份
            {
                switch (nodeResponsibility)
                {
                case MASTERCONTROL:
                    resultPtr->mastercontrolNodeId = nodeIIndex;
                    break; // 总控制
                case INTRAGROUPCONTROL:
                    resultPtr->intragroupcontrolNodeId = nodeIIndex;
                    break; // 群内控制
                case RESEVERMASTERCONTROL:
                    resultPtr->reservemastercontrolNodeId = nodeIIndex;
                    break; // 总控制备份
                case RESERVEINTRAGROUPCONTROL:
                    resultPtr->reserveintracontrolNodeId = nodeIIndex;
                    break; // 群内控制备份
                case TIMEREFERENCE:
                    resultPtr->timereferenceNodeId = nodeIIndex;
                    break; // 时间基准
                case INTERGROUPGATEWAY:
                    resultPtr->intergroupgatewayNodeId = nodeIIndex;
                    break; // 群间网关
                case RESERVETIMEREFERENCE:
                    resultPtr->reservetimereferenceNodeId = nodeIIndex;
                    break; // 时间基准备份
                case RESERVEINTERGROUPFATEWAY:
                    resultPtr->reserveintergroupgatewayNodeId = nodeIIndex;
                    break; // 群间网关备份
                case SATELLITEBASEDGATEWAY:
                    resultPtr->satellitebasedgatewayNodeId = nodeIIndex;
                    break; // 天基网关
                case RESERVESATELLITEBASEDGATEWAY:
                    resultPtr->reservesatellitebasedgatewayNodeId = nodeIIndex;
                    break; // 天基网关备份
                default:
                    break;
                }
            }
        }
    }
    else
    {
        cout << "error! 未初始化身份结构体 !" << endl;
    }

    ifs.close();
}

// 打印NodeNotification结构体中部分属性
void PrintIdentity()
{
    NodeNotification *resultPtr = nodeNotificationPtr;
    cout << "node: " << resultPtr->nodeIIndex << "  groupID: " << resultPtr->groupID << "  response: " << resultPtr->nodeResponsibility << endl;
}

// 路由表为空处理节点脱网, 高低速都不在网
void HandleNodeOffline()
{
    // 将自身身份置为脱网节点
    nodeNotificationPtr->nodeResponsibility = ISOLATION;
    // 清除对群内关键节点身份认知状态
    nodeNotificationPtr->mastercontrolNodeId = 0;
    nodeNotificationPtr->reservemastercontrolNodeId = 0;
    nodeNotificationPtr->intragroupcontrolNodeId = 0;
    nodeNotificationPtr->reserveintracontrolNodeId = 0;
    nodeNotificationPtr->timereferenceNodeId = 0;
    nodeNotificationPtr->reservetimereferenceNodeId = 0;
    nodeNotificationPtr->intergroupgatewayNodeId = 0;
    nodeNotificationPtr->reserveintergroupgatewayNodeId = 0;
    nodeNotificationPtr->satellitebasedgatewayNodeId = 0;
    nodeNotificationPtr->reservesatellitebasedgatewayNodeId = 0;

    // 告知链路层进入随遇接入状态
    NodeNotification *nodeIdentityPtr = new NodeNotification;
    nodeIdentityPtr->groupID = nodeNotificationPtr->groupID;                                                       // 群Id
    nodeIdentityPtr->nodeIIndex = nodeNotificationPtr->nodeIIndex;                                                 // 本节点id
    nodeIdentityPtr->nodeResponsibility = nodeNotificationPtr->nodeResponsibility;                                 // 本节点身份
    nodeIdentityPtr->intragroupcontrolNodeId = nodeNotificationPtr->intragroupcontrolNodeId;                       // 群控制节点
    nodeIdentityPtr->reserveintracontrolNodeId = nodeNotificationPtr->reserveintracontrolNodeId;                   // 群控制备份
    nodeIdentityPtr->satellitebasedgatewayNodeId = nodeNotificationPtr->satellitebasedgatewayNodeId;               // 星基网关
    nodeIdentityPtr->reservesatellitebasedgatewayNodeId = nodeNotificationPtr->reservesatellitebasedgatewayNodeId; // 备份星基
    // 其他关键节点身份信息待补充
    nodeIdentityPtr->intergroupgatewayNodeId = nodeNotificationPtr->intergroupgatewayNodeId;
    nodeIdentityPtr->reserveintergroupgatewayNodeId = nodeNotificationPtr->reserveintergroupgatewayNodeId;

    // 发送给链路层
    cout << "通知链路层进入随遇接入状态!" << endl;
    uint8_t wBufferToMac[MAX_DATA_LEN];                                            // 将要写入循环缓冲区的数据(含包头)存放于此
    PackRingBuffer(wBufferToMac, nodeIdentityPtr, sizeof(NodeNotification), 0x0A); // 身份配置信息
    // MAC_Daatr_Receive->ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
    RoutingTomac_Buffer.ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
    delete nodeIdentityPtr;
}

// 全网身份更新函数
void IntraGroupUpdate()
{
    if (IdentityPtr->IsBroadcast)
    {
        cout << "开始计算身份!" << endl;
        // 统计触发身份更新开始的时间 clocktype delay_IdentityUpdate
        IdentityPtr->Identity_Trigger = getCurrentTime() / 1000; // 换算成毫秒
        running_cnt += 1;
        cout << "当前是第" << running_cnt << "次身份计算" << endl;
        IdentityPtr->netViewList = netViewPtr->netViewList;
        //****************************新增根据网络视图确认群内节点数目***********
        IdentityPtr->nodeNumInGroup = IdentityPtr->netViewList.size();
        //***********************************************************************

        //******************************计算新的身份*****************************
        if (IdentityPtr->nodeId == nodeNotificationPtr->intragroupcontrolNodeId)
        {
            IntraGroupControlUpdate(&(IdentityPtr->netViewList));

            cout << "群内控制节点为  " << IdentityPtr->intragroupcontrol_Temp << endl;
            cout << "群内备份控制为  " << IdentityPtr->reserveintracontrol_Temp << endl;
            // 完成计算身份
            IdentityPtr->If_calculated = true;
        }
    }
}

// 群内控制节点更新函数
void IntraGroupControlUpdate(list<NodeCondition *> *ptr)
{
    //***************一次尝试 不维护listTmpPtr而是维护身份全局变量****************************
    list<UpdateFactor> *listUpdateFactorPtr = new list<UpdateFactor>;
    list<UpdateFactor> listUpdateFactor = *listUpdateFactorPtr;
    list<NodeCondition *>::iterator myitr;
    NodeCondition *nodeControlConditon;

    //*****************************判断新增********************************
    unsigned int pastControl_ID;
    unsigned int pastreserveControl_ID;
    pastControl_ID = nodeNotificationPtr->intragroupcontrolNodeId;
    // 3节点测试条件下，现在没有备用控制节点 显示ID为0
    pastreserveControl_ID = nodeNotificationPtr->reserveintracontrolNodeId;
    //*********************************************************************

    NodeCondition *tmp;
    float maxDistance = 0.0;
    float distanceTmp = 0.0;
    int ttt = 0;

    for (myitr = ptr->begin(); myitr != ptr->end(); ++myitr)
    {
        tmp = *myitr;
        distanceTmp = distanceAll(tmp, ptr);
        if (distanceTmp > maxDistance)
        {
            maxDistance = distanceTmp;
        }
    } // 该循环算出该群内的最大距离和

    for (myitr = ptr->begin(); myitr != ptr->end(); ++myitr)
    {
        ttt++;
        tmp = *myitr;

        UpdateFactor UpdateFactorTmp = UpdateFactor();
        UpdateFactorTmp.nodeId = tmp->nodeId;
        distanceTmp = distanceAll(tmp, ptr);
        UpdateFactorTmp.nodeDistance = distanceTmp; // 距离
        // UpdateFactorTmp.nodeDistance=distanceAll(tmp,ptr);//时延//新加一句
        UpdateFactorTmp.nodeSpeed = speedAll(tmp, ptr); // 速度
        // UpdateFactorTmp.nodePayLoad = nodePayLoadAll(tmp,ptr);//负载
        UpdateFactorTmp.nodePriorityFactor = evaluatePriorityFactor(UpdateFactorTmp.nodeDistance, UpdateFactorTmp.nodeSpeed, UpdateFactorTmp.nodePayLoad, maxDistance);
        listUpdateFactor.push_back(UpdateFactorTmp);
    } // 计算更新因子
    listUpdateFactor.sort(sortByNodePriorityFactor); // 更新因子排序

    // 新增判断阈值
    float mindistance;
    float mindistance_2;
    float past_resControl_distance;
    float pastControl_distance;
    float deviation;
    int cnt = 0;
    unsigned int connectivity_1 = 0, connectivity_2 = 0, connectivity_p = 0, connectivity_pr = 0;
    // 链表存储，从头遍历  根据updatefactor大小排序选出控制和备用控制
    ///@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@解决备用控制与控制可能重合的问题@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%选举控制节点%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    UpdateFactor UfTmp = *listUpdateFactor.begin(); // 获取排名第一的UpdateFactor
    //********************************新增判断*********************************
    mindistance = UfTmp.nodeDistance;
    // 获取排名第一节点的连接度
    for (list<NodeCondition *>::iterator it = ptr->begin(); it != ptr->end(); it++)
    {
        if (UfTmp.nodeId == (*it)->nodeId)
        {
            connectivity_1 = (*it)->neighborList.size();
            break;
        }
    }

    // 获取原控制节点的连接度
    for (list<NodeCondition *>::iterator it = ptr->begin(); it != ptr->end(); it++)
    {
        if ((*it)->nodeId == pastControl_ID)
        {
            connectivity_p = (*it)->neighborList.size();
            break;
        }
    }

    // 进行比较，参考因素为相对距离差&&连接度
    for (list<UpdateFactor>::iterator in_iter = listUpdateFactor.begin(); in_iter != listUpdateFactor.end(); ++in_iter)
    {
        if ((*in_iter).nodeId == pastControl_ID) // 找到上一轮控制节点
        {
            UpdateFactor Ufpast_control = *in_iter;
            pastControl_distance = Ufpast_control.nodeDistance;
            deviation = (pastControl_distance - mindistance) / pastControl_distance;
            if (deviation < 0.1 && connectivity_p >= connectivity_1)
            {
                // cout << Ufpast_control.nodeId <<endl;
                IdentityPtr->intragroupcontrol_Temp = Ufpast_control.nodeId; // 原控制节点仍满足，保持原控制节点
            }
            else
            {
                IdentityPtr->intragroupcontrol_Temp = UfTmp.nodeId; // 不满足，换成新的控制节点
            }
            break;
        }
    }

    if (!(IdentityPtr->intragroupcontrol_Temp > 0 && IdentityPtr->intragroupcontrol_Temp < (NODENUMINGROUP + 1))) // 新增保护
    {
        IdentityPtr->intragroupcontrol_Temp = UfTmp.nodeId;
    }
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%选举备用控制节点%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    for (list<UpdateFactor>::iterator iter = listUpdateFactor.begin(); iter != listUpdateFactor.end(); iter++)
    {
        // 遍历找出除本轮控制节点外排名最靠前的UpdateFactor
        if ((*iter).nodeId != IdentityPtr->intragroupcontrol_Temp) // 按顺序排列了，即找到控制节点后一位节点，则为第二合适的节点
        {
            UfTmp = *iter; // 获取本轮待定备份更新因子
            break;
        }
    }
    mindistance_2 = UfTmp.nodeDistance;

    // 获取本轮待定备份的节点连接度
    for (list<NodeCondition *>::iterator it = ptr->begin(); it != ptr->end(); it++)
    {
        if (UfTmp.nodeId == (*it)->nodeId)
        {
            connectivity_2 = (*it)->neighborList.size();
            break;
        }
    }

    // 获取原备用控制节点的连接度
    for (list<NodeCondition *>::iterator it = ptr->begin(); it != ptr->end(); it++)
    {
        if ((*it)->nodeId == pastreserveControl_ID)
        {
            connectivity_pr = (*it)->neighborList.size();
            break;
        }
    }

    for (list<UpdateFactor>::iterator in_iter = listUpdateFactor.begin(); in_iter != listUpdateFactor.end(); ++in_iter)
    {

        if ((*in_iter).nodeId == pastreserveControl_ID)
        {
            UpdateFactor Ufpast_rescontrol = *in_iter;
            past_resControl_distance = Ufpast_rescontrol.nodeDistance;
            deviation = (past_resControl_distance - mindistance_2) / past_resControl_distance;
            if (deviation < 0.1 && connectivity_pr >= connectivity_2)
            {
                IdentityPtr->reserveintracontrol_Temp = Ufpast_rescontrol.nodeId; // 原备份仍满足，保留
            }
            else
            {
                IdentityPtr->reserveintracontrol_Temp = UfTmp.nodeId; // 原备份不满足，更新新的备份
            }
            break;
        }
    }

    // 保护机制-----对于未选出身份的情况
    if (!(IdentityPtr->reserveintracontrol_Temp > 0 && IdentityPtr->reserveintracontrol_Temp < (NODENUMINGROUP + 1)))
    {
        IdentityPtr->reserveintracontrol_Temp = UfTmp.nodeId;
    }
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    // 选完了，释放listUpdateFactorPtr
    listUpdateFactorPtr->clear();
    delete listUpdateFactorPtr;
}

// 用于计算群内某点到群内其他点的距离之和
float distanceAll(NodeCondition *NodePtr, list<NodeCondition *> *ptr)
{
    float distance = 0.0;
    list<NodeCondition *>::iterator myitr;
    NodeCondition *tmp;
    for (myitr = ptr->begin(); myitr != ptr->end(); ++myitr)
    {
        tmp = *myitr;
        if (tmp->nodeId == NodePtr->nodeId)
        {
            continue;
        }
        else
        {
            // tmp->thisPos.positionX;
            // NodePtr->thisPos.positionX;
            distance += (float)sqrt(pow(tmp->thisPos.positionX - NodePtr->thisPos.positionX, 2) +
                                    pow(tmp->thisPos.positionY - NodePtr->thisPos.positionY, 2) +
                                    pow(tmp->thisPos.positionZ - NodePtr->thisPos.positionZ, 2));
        }
    }
    return distance;
}

// 用于计算群内某点相对于群平均速度的偏离程度
float speedAll(NodeCondition *NodePtr, list<NodeCondition *> *ptr)
{
    float vx = 0.0;
    float vy = 0.0;
    float vz = 0.0;
    list<NodeCondition *>::iterator myitr;
    NodeCondition *tmp;
    int cnt = 0;
    for (myitr = ptr->begin(); myitr != ptr->end(); ++myitr)
    {
        cnt++;
        tmp = *myitr;
        vx += tmp->thisSpeed.speedX;
        vy += tmp->thisSpeed.speedY;
        vz += tmp->thisSpeed.speedZ;
    }
    vx /= cnt;
    vy /= cnt;
    vz /= cnt;
    NodePtr->thisSpeed.speedX;
    return (float)sqrt(pow(vx - NodePtr->thisSpeed.speedX, 2) + pow(vy - NodePtr->thisSpeed.speedY, 2) + pow(vz - NodePtr->thisSpeed.speedZ, 2));
}

// 用于计算群内某点负载，由节点排队率和链路占用率计算
float nodePayLoadAll(NodeCondition *NodePtr, list<NodeCondition *> *ptr)
{
    float NodequeueLoadRate = NodePtr->queueLoadRate; // 节点排队率
    list<LinkQuality *> listTmp = NodePtr->neighborList;
    list<LinkQuality *>::iterator myitr;
    int cnt = 0;
    float linkUsageRateAll = 0.0;
    LinkQuality *linkTmp;
    for (myitr = listTmp.begin(); myitr != listTmp.end(); ++myitr)
    {
        cnt++;
        linkTmp = *myitr;
        linkUsageRateAll += linkTmp->linkUsageRate;
    }
    linkUsageRateAll /= cnt; // 链路占用率
    cout << "lianluzhanyonglv:::" << linkUsageRateAll << endl;
    return NodequeueLoadRate / 2.0 + linkUsageRateAll / 2.0;
}

// 计算更新计算各节点优先级
float evaluatePriorityFactor(float nodeDistance, float nodeSpeed, float nodePayLoad, float maxDistance)
{
    // 具体采取该方式，各权值都可以修改，目前其他外协没有传过来payload,故该部分为0
    float Mi = nodeDistance / maxDistance;
    float Fi = exp(-1 / (nodeSpeed * nodeSpeed));
    return 0.8 * Mi + 0.2 * Fi + 0.0 * nodePayLoad;
}

// 计算因子按照优先级排序
bool sortByNodePriorityFactor(UpdateFactor node1, UpdateFactor node2)
{
    return node1.nodePriorityFactor < node2.nodePriorityFactor; // 升序排列
}

// 定时下发身份广播包的函数
void HandleIdentityBroadcastTiming()
{
    // cout << "触发HandleIdentityBroadcastTiming" << endl;
    if (nodeNotificationPtr->nodeResponsibility == INTRAGROUPCONTROL || nodeNotificationPtr->nodeResponsibility == MASTERCONTROL)
    { // 是总控制或群内控制
        if (IdentityPtr->If_calculated)
        {
            // 发广播包函数
            NodeBroadcast *nodeBroadcastPtr = (NodeBroadcast *)malloc(sizeof(NodeBroadcast));
            nodeBroadcastPtr->groupID = IdentityPtr->groupID_Temp;
            nodeBroadcastPtr->intragroupcontrolNodeId = IdentityPtr->intragroupcontrol_Temp;                       // 控制
            nodeBroadcastPtr->reserveintracontrolNodeId = IdentityPtr->reserveintracontrol_Temp;                   // 备用控制
            nodeBroadcastPtr->intergroupgatewayNodeId = IdentityPtr->intergroupgateway_Temp;                       // 群间网关
            nodeBroadcastPtr->reserveintergroupgatewayNodeId = IdentityPtr->reserveintergroupgateway_Temp;         // 群间网关备份
            nodeBroadcastPtr->satellitebasedgatewayNodeId = IdentityPtr->satellitebasedgateway_Temp;               // 天基
            nodeBroadcastPtr->reservesatellitebasedgatewayNodeId = IdentityPtr->reservesatellitebasedgateway_Temp; // 天基备份
            // 其他变量待补充

            // 判断关键节点身份是否发生变化，给一个总标志位
            IdentityPtr->Is_IdentityUpdate = IsIdentityUpdate(nodeBroadcastPtr);

            // 添加分流头
            unsigned int fullpacketSize = sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(NodeBroadcast);
            char *msgToSendPtr = (char *)malloc(fullpacketSize);
            // 对消息标识赋值 业务信道加消息标识
            msgID *msgIdPtr = (msgID *)msgToSendPtr;
            msgIdPtr->msgid = 28; // 北航填28
            msgIdPtr->msgsub = 3; // 消息子标识，标志身份配置模块
            // 首先对分流头进行赋值
            MessageDiversionHeader *diversionPtr = (MessageDiversionHeader *)(msgToSendPtr + sizeof(msgID));
            diversionPtr->moduleName = 1;
            diversionPtr->eventType = 1;
            // 之后对负载进行赋值
            memcpy(msgToSendPtr + sizeof(msgID) + sizeof(MessageDiversionHeader), nodeBroadcastPtr, sizeof(NodeBroadcast));

            //***************************~广播 目的地址怎么填 补充IdentityData内的广播包序列号***************************
            //***************************此处暂写成多个一对一，只是包格式改为广播包****************
            vector<uint8_t> *msg_tempPtr = TransPacketToVec_Broadcast(msgToSendPtr); // 应该将传的结构体改为八位数组形式
            // 封装msgFromControl包
            msgFromControl controlmsg;
            controlmsg.data = (char *)msg_tempPtr;
            controlmsg.packetLength = 8 + msg_tempPtr->size() + 2; // 包长度
            controlmsg.srcAddr = IdentityPtr->nodeId;              // 源地址为本节点
            controlmsg.destAddr = 0x3FF;
            controlmsg.priority = 0;    // 网管消息优先级为0
            controlmsg.backup = 0;      // 备用字段为0
            controlmsg.msgType = 1;     // 网管消息走网管信道
            controlmsg.repeat = 0;      // 非重传包
            controlmsg.fragmentNum = 0; // 无效值
            controlmsg.fragmentSeq = 0; // 分片序号，无效值
            // 序列号待补充

            IdentityPtr->SerialNum = controlmsg.seq;                          // 记录序列号
            vector<uint8_t> *v1_controlmsg = PackMsgFromControl(&controlmsg); // msgFromControl整体转为八位数组
            // 将封装后的业务数据包封装成层间接口数据帧发给路由模块
            uint8_t wBufferToRouting[MAX_DATA_LEN];                // 将要写入循环缓冲区的数据(含包头)存放于此
            PackRingBuffer(wBufferToRouting, v1_controlmsg, 0x1E); // 0x1E管控开销消息
            netToRouting_buffer.ringBuffer_put(wBufferToRouting, sizeof(wBufferToRouting));
            cout << "控制节点下发身份广播包完毕!" << endl;

            free(msgToSendPtr); // 实际数据内容已msgToSend_data经写入vector数组，释放掉申请的内存

            // 新一轮身份计算完成，清空待处理毁伤列表和已处理毁伤列表
            IdentityPtr->damaged_Pending.clear();
            IdentityPtr->damaged_Processed.clear();
            // 重新置位 是否计算身份
            IdentityPtr->If_calculated = false;
            // 置位广播包是否成功的标志位，开始广播
            IdentityPtr->IsBroadcast = false;
            // 计算统计变量 计算网管模块cost 发送端统计 包大小+链路层包头网管信道17

            delete v1_controlmsg;
        }
    }

    // 设置新的定时器
    SetTimerUser2(1, 0, 8, NULL, 0);
}

// 判断是否本次关键节点身份是否发生变化
bool IsIdentityUpdate(NodeBroadcast *contentPtr)
{
    bool flag = true;
    // 新的控制等于原控制，新的备份控制等于原备份控制时，身份没有更新
    // 其他关键节点身份待补充
    if (contentPtr->intragroupcontrolNodeId == nodeNotificationPtr->intragroupcontrolNodeId && contentPtr->reserveintracontrolNodeId == nodeNotificationPtr->reserveintracontrolNodeId)
    {
        flag = false;
    }
    return flag;
}

// 应该身份广播结构体改为八位数组形式
vector<uint8_t> *TransPacketToVec_Broadcast(char *MsgToSendPtr)
{
    vector<uint8_t> *buffer = new vector<uint8_t>;
    // 封消息标识
    msgID *msgIdPtr = (msgID *)MsgToSendPtr;
    buffer->push_back((msgIdPtr->msgid << 3) | (msgIdPtr->msgsub));
    MessageDiversionHeader *HeaderPtr = (MessageDiversionHeader *)(MsgToSendPtr + sizeof(msgID));
    NodeBroadcast *NotifyPtr = (NodeBroadcast *)(MsgToSendPtr + sizeof(msgID) + sizeof(MessageDiversionHeader));
    buffer->push_back((HeaderPtr->moduleName >> 8) & 0xFF);
    buffer->push_back((HeaderPtr->moduleName) & 0xFF);
    buffer->push_back((HeaderPtr->eventType >> 8) & 0xFF);
    buffer->push_back((HeaderPtr->eventType) & 0xFF);
    TurnUnsignedToUint(NotifyPtr->groupID, buffer);                            // 群id
    TurnUnsignedToUint(NotifyPtr->intragroupcontrolNodeId, buffer);            // 控制
    TurnUnsignedToUint(NotifyPtr->reserveintracontrolNodeId, buffer);          // 控制备份
    TurnUnsignedToUint(NotifyPtr->intergroupgatewayNodeId, buffer);            // 网关
    TurnUnsignedToUint(NotifyPtr->reserveintergroupgatewayNodeId, buffer);     // 网关备份
    TurnUnsignedToUint(NotifyPtr->satellitebasedgatewayNodeId, buffer);        // 天基
    TurnUnsignedToUint(NotifyPtr->reservesatellitebasedgatewayNodeId, buffer); // 天基备份
    return buffer;
}

// 将unsigned int存入vector<uint8_t>中
void TurnUnsignedToUint(unsigned int member, vector<uint8_t> *buffer)
{
    buffer->push_back((member >> 24) & 0xFF);
    buffer->push_back((member >> 16) & 0xFF);
    buffer->push_back((member >> 8) & 0xFF);
    buffer->push_back(member & 0xFF);
}

// 处理链路层传来的身份配置广播状态（只有控制节点会收到）
void ControlNodeDealWithIdentityStatus(const vector<uint8_t> &IdentityStatusMsgPtr)
{
    cout << "身份包已经开始广播，稍后更新身份 " << endl;
    IdentityStatus *identityStatus_Msg = (IdentityStatus *)&IdentityStatusMsgPtr[0];
    if (identityStatus_Msg->isBroadcast && identityStatus_Msg->seq == IdentityPtr->SerialNum) // 广播成功 且 传回序列号是否与本轮待广播序列号相同
    {
        // 等待一段时间通知链路层和路由
        // 包一个广播包样结构体更新自身身份
        NodeBroadcast *nodeBroadcastPtr = (NodeBroadcast *)malloc(sizeof(NodeBroadcast));
        nodeBroadcastPtr->groupID = IdentityPtr->groupID_Temp;
        nodeBroadcastPtr->intragroupcontrolNodeId = IdentityPtr->intragroupcontrol_Temp;                       // 控制
        nodeBroadcastPtr->reserveintracontrolNodeId = IdentityPtr->reserveintracontrol_Temp;                   // 备用控制
        nodeBroadcastPtr->intergroupgatewayNodeId = IdentityPtr->intergroupgateway_Temp;                       // 群间网关
        nodeBroadcastPtr->reserveintergroupgatewayNodeId = IdentityPtr->reserveintergroupgateway_Temp;         // 群间网关备份
        nodeBroadcastPtr->satellitebasedgatewayNodeId = IdentityPtr->satellitebasedgateway_Temp;               // 天基
        nodeBroadcastPtr->reservesatellitebasedgatewayNodeId = IdentityPtr->reservesatellitebasedgateway_Temp; // 天基备份
        IdentityPtr->IsBroadcast = true;                                                                       // 更新广播成功标志位

        // 10ms后，调用ReceiveAMsgFromOtherNodes函数，使控制节点处理自身打包的身份广播包
        // 触发这个函数说明链路层已经成功将身份包广播出去了，此时控制节点需要等待一个经验性的广播时延，再更新自己的身份表
        SetTimerUser2(0, 10, 9, nodeBroadcastPtr, sizeof(nodeBroadcastPtr));
        // 此处没有delete nodeBroadcastPtr，因为不知道会不会有影响(感觉会出问题)
    }
}

// 节点处理收到的身份广播包
void ReceiveAMsgFromOtherNodes(char *msgPtr)
{
    //****************************此处有修改***********************************//
    // 此函数为节点接收广播包进行处理更新自身身份的函数
    // 节点身份为网管节点时，收到的是自身打包的NodeBroadcast
    // 节点身份为其他节点时，收到的是网管节点发送的广播包msgFromControl广播包，应分别进行处理
    //************************************************************************//

    NodeBroadcast *contentPtr;
    char *payloadStart;
    bool flag = false;
    if (nodeNotificationPtr->nodeResponsibility == INTRAGROUPCONTROL) // 群内控制
    {
        // 控制节点收到的是自身打包的NodeBroadcast
        contentPtr = (NodeBroadcast *)msgPtr;
        cout << " 控制节点开始更新自身身份表 !" << endl;
        // 控制节点开始更新身份表，身份更新完成，记录身份更新完成的时间
        IdentityPtr->delay_IdentityUpdate = getCurrentTime() / 1000 - IdentityPtr->Identity_Trigger;
        if (IdentityPtr->Is_IdentityUpdate) // 如果群内关键节点身份发生变化 即身份更新了，则触发上报身份更新时间及群内新的身份
        {
            // 上报身份更新完成时间
            cout << "身份更新时间为 " << IdentityPtr->delay_IdentityUpdate << "ms!" << endl;

            // 上报状态信息 1网络分布式管控 1单子网 5指标类型为身份更新时间
            // 240531改 改为针对不同的身份类型上报身份更新时间
            // 此处上报数据源代码在network_identityUpdateAndMaintenance.cpp中682-702行，后续再进行移植
        }
    }
    else
    {
        // 其他节点收到的是msgFromControl广播包
        flag = true;
        msgFromControl *controlMsgPtr = (msgFromControl *)msgPtr;
        vector<uint8_t> *vec1 = (vector<uint8_t> *)controlMsgPtr->data;
        payloadStart = TransVecToPacket_Broadcast(vec1);
        // 释放掉数组的内存
        free((vector<uint8_t> *)controlMsgPtr->data);
        contentPtr = (NodeBroadcast *)(payloadStart + sizeof(msgID) + sizeof(MessageDiversionHeader));
        cout << "Node 收到身份广播信息 ! " << endl;
    }

    // 判断是否需要通知其他模块
    bool is_Inform = IsIdentityUpdate(contentPtr);
    // 需要记录一下上一轮的节点身份，若节点身份发生变更，则触发上报新的节点身份
    unsigned int NodeReponse_last = nodeNotificationPtr->nodeResponsibility;
    // 赋值节点身份
    if (IdentityPtr->nodeId == contentPtr->intragroupcontrolNodeId) // nodeId = 群内控制
    {
        nodeNotificationPtr->nodeResponsibility = INTRAGROUPCONTROL;
    }
    else if (IdentityPtr->nodeId == contentPtr->reserveintracontrolNodeId) // nodeId = 群内备用控制
    {
        nodeNotificationPtr->nodeResponsibility = RESERVEINTRAGROUPCONTROL;
    }
    else if (IdentityPtr->nodeId == contentPtr->intergroupgatewayNodeId) // nodeId = 群间网关
    {
        nodeNotificationPtr->nodeResponsibility = INTERGROUPGATEWAY;
    }
    else if (IdentityPtr->nodeId == contentPtr->reserveintergroupgatewayNodeId) // nodeId = 群间网关备份
    {
        nodeNotificationPtr->nodeResponsibility = RESERVEINTERGROUPFATEWAY;
    }
    else if (IdentityPtr->nodeId == contentPtr->satellitebasedgatewayNodeId) // nodeId = 天基网关
    {
        nodeNotificationPtr->nodeResponsibility = SATELLITEBASEDGATEWAY;
    }
    else if (IdentityPtr->nodeId == contentPtr->reservesatellitebasedgatewayNodeId) // nodeId = 天基网关备份
    {
        nodeNotificationPtr->nodeResponsibility = RESERVESATELLITEBASEDGATEWAY;
    }
    else
    {
        nodeNotificationPtr->nodeResponsibility = NODENORMAL;
    }

    // 赋值群内关键节点
    nodeNotificationPtr->groupID = contentPtr->groupID;                                                       // 群号
    nodeNotificationPtr->intragroupcontrolNodeId = contentPtr->intragroupcontrolNodeId;                       // 控制节点
    nodeNotificationPtr->reserveintracontrolNodeId = contentPtr->reserveintracontrolNodeId;                   // 备用控制
    nodeNotificationPtr->intergroupgatewayNodeId = contentPtr->intergroupgatewayNodeId;                       // 群间网关
    nodeNotificationPtr->reserveintergroupgatewayNodeId = contentPtr->reserveintergroupgatewayNodeId;         // 备用网关
    nodeNotificationPtr->satellitebasedgatewayNodeId = contentPtr->satellitebasedgatewayNodeId;               // 星基网关
    nodeNotificationPtr->reservesatellitebasedgatewayNodeId = contentPtr->reservesatellitebasedgatewayNodeId; // 备用星基

    // 打印验证
    cout << "groupID: " << nodeNotificationPtr->groupID << endl;
    cout << "nodeIIndex: " << nodeNotificationPtr->nodeIIndex << endl;
    cout << "nodeResponsibility: " << nodeNotificationPtr->nodeResponsibility << endl;
    cout << "intragroupcontrolNodeId: " << nodeNotificationPtr->intragroupcontrolNodeId << endl;
    cout << "reserverdintracontrolNodeId: " << nodeNotificationPtr->reserveintracontrolNodeId << endl;
    // printf("Receive msg Finished! \n");

    if (flag == true)
    {
        // 释放payloadStart
        free(payloadStart);
    }

    // 群内关键节点身份发生变化则触发通知本节点其他模块（链路层和路由模块）更新身份
    if (is_Inform)
    {
        InformIdentityToOtherModules();
    }
}

// 将转成vector的广播包复原
char *TransVecToPacket_Broadcast(vector<uint8_t> *vect)
{
    unsigned int fullpacketSize = sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(NodeBroadcast);
    char *msgToSendPtr = (char *)malloc(fullpacketSize);
    // 解出msgID
    msgID *msgIdPtr = (msgID *)msgToSendPtr;
    msgIdPtr->msgid = ((*vect)[0] >> 3) & 0x1F; // 取消息标识
    msgIdPtr->msgsub = ((*vect)[0] & 0x07);     // 取消息子标识
    MessageDiversionHeader *HeaderPtr = (MessageDiversionHeader *)(msgToSendPtr + sizeof(msgID));
    NodeBroadcast *NotifyPtr = (NodeBroadcast *)(msgToSendPtr + sizeof(msgID) + sizeof(MessageDiversionHeader));
    HeaderPtr->moduleName = (unsigned short)(((*vect)[1] << 8) | (*vect)[2]);
    HeaderPtr->eventType = (unsigned short)(((*vect)[3] << 8) | (*vect)[4]);
    NotifyPtr->groupID = TurnUintToUnsigned(vect, 5);                             // 群id
    NotifyPtr->intragroupcontrolNodeId = TurnUintToUnsigned(vect, 9);             // 控制
    NotifyPtr->reserveintracontrolNodeId = TurnUintToUnsigned(vect, 13);          // 控制备份
    NotifyPtr->intergroupgatewayNodeId = TurnUintToUnsigned(vect, 17);            // 网关
    NotifyPtr->reserveintergroupgatewayNodeId = TurnUintToUnsigned(vect, 21);     // 网关备份
    NotifyPtr->satellitebasedgatewayNodeId = TurnUintToUnsigned(vect, 25);        // 天基
    NotifyPtr->reservesatellitebasedgatewayNodeId = TurnUintToUnsigned(vect, 29); // 天基备份
    return msgToSendPtr;
}

// 将vector<uint8_t>中存储的unsigned int还原
unsigned int TurnUintToUnsigned(vector<uint8_t> *vect, int index)
{
    return (unsigned int)(((*vect)[index] << 24) | ((*vect)[index + 1] << 16) | ((*vect)[index + 2] << 8) | (*vect)[index + 3]);
}

// 通知其他模块（链路层和路由模块）身份配置信息
void InformIdentityToOtherModules()
{
    bool flag = MacDaatr_judge_if_could_send_config_or_LinkRequist(0);
    if (flag)
    {
        NodeNotification *nodeIdentityPtr = new NodeNotification;
        nodeIdentityPtr->groupID = nodeNotificationPtr->groupID;                                                       // 群Id
        nodeIdentityPtr->nodeIIndex = nodeNotificationPtr->nodeIIndex;                                                 // 本节点id
        nodeIdentityPtr->nodeResponsibility = nodeNotificationPtr->nodeResponsibility;                                 // 本节点身份
        nodeIdentityPtr->intragroupcontrolNodeId = nodeNotificationPtr->intragroupcontrolNodeId;                       // 群控制节点
        nodeIdentityPtr->reserveintracontrolNodeId = nodeNotificationPtr->reserveintracontrolNodeId;                   // 群控制备份
        nodeIdentityPtr->satellitebasedgatewayNodeId = nodeNotificationPtr->satellitebasedgatewayNodeId;               // 星基网关
        nodeIdentityPtr->reservesatellitebasedgatewayNodeId = nodeNotificationPtr->reservesatellitebasedgatewayNodeId; // 备份星基
        // 其他关键节点身份信息待补充
        nodeIdentityPtr->intergroupgatewayNodeId = nodeNotificationPtr->intergroupgatewayNodeId;
        nodeIdentityPtr->reserveintergroupgatewayNodeId = nodeNotificationPtr->reserveintergroupgatewayNodeId;

        cout << "节点开始通知其他模块身份 " << endl;
        // 发送给链路层
        uint8_t wBufferToMac[MAX_DATA_LEN];                                            // 将要写入循环缓冲区的数据(含包头)存放于此
        PackRingBuffer(wBufferToMac, nodeIdentityPtr, sizeof(NodeNotification), 0x0A); // 身份配置信息
        // MAC_Daatr_Receive->ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
        RoutingTomac_Buffer.ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
        // 判断 如果是控制节点的话，通知给网络视图模块)
        if (nodeIdentityPtr->nodeResponsibility == INTRAGROUPCONTROL) // 如果是群内控制
        {
            HandleIdentityUpdate(nodeIdentityPtr);
        }
        delete nodeIdentityPtr;
    }
    else
    {
        // 过500ms重新调用该函数
        SetTimerUser2(0, 500, 10, NULL, 0);
    }
}

// 处理路由模块传来的毁伤信息
void ReceiveDamagedNodesmsg(const vector<uint8_t> &DamagedNodeMsgPtr)
{
    // 接收传过来的损毁节点信息并在当前节点进行删除无身份节点并向新的控制节点传送该消息
    cout << "节点收到毁伤消息!" << endl;
    DamagedNodes_Msg *DamagedPtr = (DamagedNodes_Msg *)&DamagedNodeMsgPtr[0];
    if (DamagedPtr->DamagedNodes != nullptr)
    {
        vector<NodeAddress> damagedAllNodes = *(DamagedPtr->DamagedNodes);
        vector<NodeAddress>::iterator myitr;

        for (myitr = damagedAllNodes.begin(); myitr != damagedAllNodes.end(); myitr++)
        {
            // 判断是否是关键节点损毁, 把传过来的vector中普通节点id删掉，只留下有身份的节点
            if ((*myitr) == nodeNotificationPtr->intragroupcontrolNodeId ||
                (*myitr) == nodeNotificationPtr->mastercontrolNodeId ||
                (*myitr) == nodeNotificationPtr->reservemastercontrolNodeId ||      // 2
                (*myitr) == nodeNotificationPtr->reserveintracontrolNodeId ||       // 3
                (*myitr) == nodeNotificationPtr->timereferenceNodeId ||             // 4
                (*myitr) == nodeNotificationPtr->intergroupgatewayNodeId ||         // 5
                (*myitr) == nodeNotificationPtr->reservetimereferenceNodeId ||      // 6
                (*myitr) == nodeNotificationPtr->reserveintergroupgatewayNodeId ||  // 7
                (*myitr) == nodeNotificationPtr->satellitebasedgatewayNodeId ||     // 8
                (*myitr) == nodeNotificationPtr->reservesatellitebasedgatewayNodeId // 9
            )
            {
                cout << "损毁的有身份的节点为： " << (*myitr) << endl;
                continue;
            }
            else
            {
                damagedAllNodes.erase(myitr--); // 删除损毁节点列表中的普通节点
            }
        }

        vector<NodeAddress> *damagedAllNodesPtr = new vector<NodeAddress>;
        for (myitr = damagedAllNodes.begin(); myitr != damagedAllNodes.end(); myitr++)
        {
            damagedAllNodesPtr->emplace_back((*myitr));
        }

        //******************新增修改 毁伤汇总改为传值***************************//
        for (myitr = damagedAllNodesPtr->begin(); myitr != damagedAllNodesPtr->end(); myitr++)
        {
            // 遍历有身份的损毁信息列表, 进行相应处理
            if ((*myitr) == nodeNotificationPtr->intragroupcontrolNodeId)
            {
                // 如果该损毁节点是分控制节点,则该损毁消息会被传送给备用控制节点（其实所有的损毁消息都同时传给控制节点和备用控制节点）
                // 群内控制毁伤 目的节点为备用控制
                NodeAddress destId = nodeNotificationPtr->reserveintracontrolNodeId;
                vector<uint8_t> *v1_controlmsg = MsgFromControl_ConvertDamage(destId, damagedAllNodesPtr, 1, 2);

                // 如果是控制毁伤且自身不是备用控制，发给西电；如果自身是备用控制，直接发给自己
                if (IdentityPtr->nodeId == destId)
                {
                    // 如果自身是备份, 直接处理
                    HandleControlMsgFromRoute(*v1_controlmsg);
                }
                else
                {
                    // 发给西电
                    uint8_t wBufferToRouting[MAX_DATA_LEN];                // 将要写入循环缓冲区的数据(含包头)存放于此
                    PackRingBuffer(wBufferToRouting, v1_controlmsg, 0x1E); // 0x1E管控开销消息
                    netToRouting_buffer.ringBuffer_put(wBufferToRouting, sizeof(wBufferToRouting));
                }
                delete v1_controlmsg;
            }
            else if ((*myitr) == nodeNotificationPtr->mastercontrolNodeId)
            {
                // 总控制节点毁伤 目的节点为备用总控制
                NodeAddress destId = nodeNotificationPtr->reservemastercontrolNodeId;
                vector<uint8_t> *v1_controlmsg = MsgFromControl_ConvertDamage(destId, damagedAllNodesPtr, 1, 2);

                if (IdentityPtr->nodeId == destId)
                {
                    // 如果自身是备份, 直接处理
                    HandleControlMsgFromRoute(*v1_controlmsg);
                }
                else
                {
                    // 发给西电
                    uint8_t wBufferToRouting[MAX_DATA_LEN];                // 将要写入循环缓冲区的数据(含包头)存放于此
                    PackRingBuffer(wBufferToRouting, v1_controlmsg, 0x1E); // 0x1E管控开销消息
                    netToRouting_buffer.ringBuffer_put(wBufferToRouting, sizeof(wBufferToRouting));
                }
                delete v1_controlmsg;
            }
            else if ((*myitr) == nodeNotificationPtr->reservemastercontrolNodeId ||      // 2
                     (*myitr) == nodeNotificationPtr->reserveintracontrolNodeId ||       // 3
                     (*myitr) == nodeNotificationPtr->timereferenceNodeId ||             // 4
                     (*myitr) == nodeNotificationPtr->intergroupgatewayNodeId ||         // 5
                     (*myitr) == nodeNotificationPtr->reservetimereferenceNodeId ||      // 6
                     (*myitr) == nodeNotificationPtr->reserveintergroupgatewayNodeId ||  // 7
                     (*myitr) == nodeNotificationPtr->satellitebasedgatewayNodeId ||     // 8
                     (*myitr) == nodeNotificationPtr->reservesatellitebasedgatewayNodeId // 9
            )
            {
                // 损毁节点是其他有身份节点，则该消息传给控制节点，该消息同时传给备用控制节点
                // （因当前邻居不一定可以判定控制是否失效）控制节点更新当前有身份节点，控制节点在下个阶段更新有身份节点的备份节点
                // 目的节点为控制节点
                NodeAddress destId = nodeNotificationPtr->intragroupcontrolNodeId;
                vector<uint8_t> *v1_controlmsg = MsgFromControl_ConvertDamage(destId, damagedAllNodesPtr, 1, 2);
                if (IdentityPtr->nodeId == destId)
                {
                    // 如果自身是控制, 直接处理
                    HandleControlMsgFromRoute(*v1_controlmsg);
                }
                else
                {
                    // 发给西电
                    uint8_t wBufferToRouting[MAX_DATA_LEN];                // 将要写入循环缓冲区的数据(含包头)存放于此
                    PackRingBuffer(wBufferToRouting, v1_controlmsg, 0x1E); // 0x1E管控开销消息
                    netToRouting_buffer.ringBuffer_put(wBufferToRouting, sizeof(wBufferToRouting));
                }
                delete v1_controlmsg;

                // 目的节点为备用控制
                NodeAddress destId1 = nodeNotificationPtr->reserveintracontrolNodeId;
                vector<uint8_t> *v1_controlmsg_1 = MsgFromControl_ConvertDamage(destId1, damagedAllNodesPtr, 1, 2);
                if (IdentityPtr->nodeId == destId)
                {
                    // 如果自身是备用控制, 直接处理
                    HandleControlMsgFromRoute(*v1_controlmsg_1);
                }
                else
                {
                    // 发给西电
                    uint8_t wBufferToRouting[MAX_DATA_LEN];                  // 将要写入循环缓冲区的数据(含包头)存放于此
                    PackRingBuffer(wBufferToRouting, v1_controlmsg_1, 0x1E); // 0x1E管控开销消息
                    netToRouting_buffer.ringBuffer_put(wBufferToRouting, sizeof(wBufferToRouting));
                }
                delete v1_controlmsg_1;
            }

            cout << "毁伤节点为：" << (*myitr) << "  ";
        }
        cout << endl;
        free(damagedAllNodesPtr); // 由于传值 释放damagedAllNodesPtr
    }
    else
    {
        cout << "node has no damagedNodes" << endl;
    }

    // 释放路由模块发送的毁伤表, （会不会导致数据丢失）
    delete DamagedPtr->DamagedNodes;
}

// 毁伤消息汇聚时组msgFromControl包
vector<uint8_t> *MsgFromControl_ConvertDamage(NodeAddress destId, void *data, int moduleName, int eventType)
{
    //***********改成传值********************//
    int damaged_Num = ((vector<NodeAddress> *)data)->size();
    unsigned int fullpacketSize = sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(DamagedHeader) + damaged_Num * sizeof(DamagedNode);
    char *msgToSendPtr = (char *)malloc(fullpacketSize);

    // 对消息标识赋值 业务信道加消息标识
    msgID *msgIdPtr = (msgID *)msgToSendPtr;
    msgIdPtr->msgid = 28; // 北航填28
    msgIdPtr->msgsub = 3; // 消息子标识

    // 对分流头赋值
    MessageDiversionHeader *diversionPtr = (MessageDiversionHeader *)(msgToSendPtr + sizeof(msgID));
    diversionPtr->moduleName = moduleName;
    diversionPtr->eventType = eventType;

    // 对DamagedHeader进行赋值
    DamagedHeader *damagedHeaderPtr = (DamagedHeader *)(msgToSendPtr + sizeof(msgID) + sizeof(MessageDiversionHeader));
    damagedHeaderPtr->damagedNum = damaged_Num;

    // 对DamagedNode进行赋值
    for (int i = 0; i < damaged_Num; i++)
    {
        DamagedNode *damagedNodePtr = (DamagedNode *)(msgToSendPtr + sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(DamagedHeader) + i * sizeof(DamagedNode));
        damagedNodePtr->damagedNode = (*((vector<NodeAddress> *)data))[i];
    }

    // 对负载进行赋值
    vector<uint8_t> *msgToSend_data = TransDamageToVec_1(msgToSendPtr); // 应该将传的结构体改为八位数组形式
    free(msgToSendPtr);                                                 // 实际数据内容已msgToSend_data经写入vector数组，释放掉申请的内存

    // 封装msgFromControl包
    msgFromControl controlmsg;
    controlmsg.data = (char *)msgToSend_data;
    controlmsg.packetLength = 8 + msgToSend_data->size() + 2; // 包长度是 8 + data的长度 + CRC校验的2
    controlmsg.srcAddr = IdentityPtr->nodeId;                 // 源地址为本节点
    controlmsg.destAddr = destId;
    controlmsg.priority = 0;    // 网管消息优先级为0
    controlmsg.backup = 0;      // 备用字段为0
    controlmsg.msgType = 4;     // 网管消息走业务信道
    controlmsg.repeat = 0;      // 非重传包
    controlmsg.fragmentNum = 0; // 无效值
    controlmsg.fragmentSeq = 0; // 分片序号，无效值
    // 序列号待补充
    // 将controlmsg转成8位数组方便CRC校验
    vector<uint8_t> *v1_controlmsg = PackMsgFromControl(&controlmsg);

    // 发端成包处统计开销 业务信道链路层包头25

    return v1_controlmsg;
}

// 将毁伤节点数据转成vector<uint8_t>数组
vector<uint8_t> *TransDamageToVec_1(char *MsgToSendPtr) // 传值
{
    vector<uint8_t> *buffer = new vector<uint8_t>;
    msgID *msgIdPtr = (msgID *)MsgToSendPtr;
    MessageDiversionHeader *HeaderPtr = (MessageDiversionHeader *)(MsgToSendPtr + sizeof(msgID));
    DamagedHeader *DamageHeader_Ptr = (DamagedHeader *)(MsgToSendPtr + sizeof(msgID) + sizeof(MessageDiversionHeader));
    buffer->push_back((msgIdPtr->msgid << 3) | (msgIdPtr->msgsub));
    buffer->push_back((HeaderPtr->moduleName >> 8) & 0xFF);
    buffer->push_back((HeaderPtr->moduleName) & 0xFF);
    buffer->push_back((HeaderPtr->eventType >> 8) & 0xFF);
    buffer->push_back((HeaderPtr->eventType) & 0xFF);
    buffer->push_back((DamageHeader_Ptr->damagedNum >> 8) & 0xFF);
    buffer->push_back((DamageHeader_Ptr->damagedNum) & 0xFF);
    for (int i = 0; i < DamageHeader_Ptr->damagedNum; i++)
    {
        DamagedNode *damagedNodePtr = (DamagedNode *)(MsgToSendPtr + sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(DamagedHeader) + i * sizeof(DamagedNode));
        TurnAddrToUint(damagedNodePtr->damagedNode, buffer);
    }
    return buffer;
}

// 将损毁节点地址转化为数组形式
void TurnAddrToUint(NodeAddress address, vector<uint8_t> *buffer)
{
    buffer->push_back((address >> 24) & 0xFF);
    buffer->push_back((address >> 16) & 0xFF);
    buffer->push_back((address >> 8) & 0xFF);
    buffer->push_back(address & 0xFF);
}

// 控制节点&控制备份 处理其他节点汇总过来的毁伤信息
void ControlNodeDealWithDamage(msgFromControl *controlMsgPtr)
{
    // 新的控制节点接收到整理后的消息，进行一个去重复和将备用节点提为正式身份节点的操作
    // 并将最后更正后的向群内其他节点发送
    vector<uint8_t> *vec1 = (vector<uint8_t> *)controlMsgPtr->data; // controlPtr->data目前是一个vector<uint8_t>的数组
    char *payloadStart = TransVecToDamage_1(vec1);
    // 释放掉数组的内存
    free(vec1);
    DamagedHeader *damagedHeaderPtr = (DamagedHeader *)(payloadStart + sizeof(msgID) + sizeof(MessageDiversionHeader)); // 获取毁伤节点数目

    // 遍历毁伤列表
    for (int i = 0; i < damagedHeaderPtr->damagedNum; i++)
    {
        DamagedNode *damagedNodePtr = (DamagedNode *)(payloadStart + sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(DamagedHeader) + i * sizeof(DamagedNode));
        // 查询是否在已处理毁伤列表中
        bool isFind_Proccessed = IsInDamagedList(&(IdentityPtr->damaged_Processed), damagedNodePtr->damagedNode);
        if (!isFind_Proccessed) // 不在已处理毁伤列表
        {
            // 查看是否在待处理毁伤列表
            bool isFind_Pending = IsInDamagedList(&(IdentityPtr->damaged_Pending), damagedNodePtr->damagedNode);
            if (!isFind_Pending) // 如果不在待处理毁伤列表
            {
                // 添加到待处理毁伤列表并设置定期清除
                IdentityPtr->damaged_Pending.push_back(damagedNodePtr->damagedNode);
                // 设置定时器定时清除
                SetTimerUser2(1, 0, 11, damagedNodePtr, sizeof(damagedNodePtr));
            }
            else
            {
                // 如果在待处理毁伤列表，立即触发毁伤处理操作
                HandleDamageMsg(damagedNodePtr->damagedNode);
                // 将节点添加到已处理毁伤列表中
                IdentityPtr->damaged_Processed.push_back(damagedNodePtr->damagedNode);
            }
        }
        else // 在已处理毁伤列表，什么都不用做，打印已处理消息
        {
            cout << "node " << damagedNodePtr->damagedNode << "DamagedMsg 已处理！" << endl;
        }
    }
}

// 将存储在vector<uint8_t>数组中的毁伤数据还原
char *TransVecToDamage_1(vector<uint8_t> *vect) // 传值
{
    // msgID: 1  MessageDiversionHeader: 4 ,DamagedNum从 5 开始索引
    int Damaged_Num = (unsigned short)(((*vect)[5] << 8) | (*vect)[6]);
    unsigned int fullpacketSize = sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(DamagedHeader) + Damaged_Num * sizeof(DamagedNode);
    char *msgToSendPtr = (char *)malloc(fullpacketSize);
    msgID *msgIdPtr = (msgID *)msgToSendPtr;
    MessageDiversionHeader *HeaderPtr = (MessageDiversionHeader *)(msgToSendPtr + sizeof(msgID));
    DamagedHeader *Damage_Ptr = (DamagedHeader *)(msgToSendPtr + sizeof(msgID) + sizeof(MessageDiversionHeader));
    msgIdPtr->msgid = ((*vect)[0] >> 3) & 0x1F; // 取消息标识
    msgIdPtr->msgsub = ((*vect)[0] & 0x07);     // 取消息子标识
    HeaderPtr->moduleName = (unsigned short)(((*vect)[1] << 8) | (*vect)[2]);
    HeaderPtr->eventType = (unsigned short)(((*vect)[3] << 8) | (*vect)[4]);
    int index = 7;
    Damage_Ptr->damagedNum = Damaged_Num;
    for (int i = 0; i < Damaged_Num; i++)
    {
        DamagedNode *damagedNodePtr = (DamagedNode *)(msgToSendPtr + sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(DamagedHeader) + i * sizeof(DamagedNode));
        damagedNodePtr->damagedNode = TurnUintToUnsigned(vect, index);
        index += 4;
    }
    return msgToSendPtr;
}

// 判断毁伤列表中有无某节点
bool IsInDamagedList(list<NodeAddress> *damaged_List, NodeAddress nodeId)
{
    if (damaged_List->size() != 0) // 列表不为空
    {
        for (list<NodeAddress>::iterator it = damaged_List->begin(); it != damaged_List->end(); it++)
        {
            if ((*it) == nodeId)
            {
                return true;
            }
        }
    }
    return false;
}

// 待处理毁伤的时钟到期，从待处理毁伤列表中删除
void CheckDamagedNodeClock(DamagedNode *damagedNodePtr)
{
    cout << "毁伤时钟超时 ，将删除待处理毁伤消息 !" << endl;
    NodeAddress damagedNode = damagedNodePtr->damagedNode;
    IdentityPtr->damaged_Pending.remove(damagedNode);
}

// 毁伤消息处理, 判断毁伤节点身份，做出身份更替，并通知其他节点
void HandleDamageMsg(NodeAddress damaged_node)
{
    // 如果备份发现控制节点毁伤，先把自身身份表里的身份改为控制，立刻通知其他节点
    // 如果发现其他节点毁伤，先改IdentityData里的身份，等待控制节点广播
    // 先判断自己身份,再判断毁伤节点身份
    if (IdentityPtr->nodeId == nodeNotificationPtr->reserveintracontrolNodeId) // 备份控制
    {
        // 处理控制节点毁伤, 立刻升自己身份为新的控制
        if (damaged_node == nodeNotificationPtr->intragroupcontrolNodeId)
        {
            nodeNotificationPtr->nodeResponsibility = INTRAGROUPCONTROL; // 改自身身份是控制
            IdentityPtr->intragroupcontrol_Temp = IdentityPtr->nodeId;   // 改自己为控制
            IdentityPtr->reserveintracontrol_Temp = 0;                   // 原备份改为0
            IdentityPtr->Identity_Trigger = getCurrentTime() / 1000;     // 微秒换算成毫秒
        }
    }
    else if (IdentityPtr->nodeId == nodeNotificationPtr->reservemastercontrolNodeId) // 备份总控
    {
        if (damaged_node == nodeNotificationPtr->mastercontrolNodeId)
        {
            nodeNotificationPtr->nodeResponsibility = MASTERCONTROL; // 改自身身份是总控制
            IdentityPtr->mastercontrol_Temp = IdentityPtr->nodeId;   // 改自己为总控制
            IdentityPtr->reservemastercontrol_Temp = 0;              // 原备份改为0
            IdentityPtr->Identity_Trigger = getCurrentTime() / 1000; // 微秒换算成毫秒
        }
    }

    // 统一处理其他节点毁伤
    if (IdentityPtr->nodeId == nodeNotificationPtr->mastercontrolNodeId || IdentityPtr->nodeId == nodeNotificationPtr->reservemastercontrolNodeId || IdentityPtr->nodeId == nodeNotificationPtr->intragroupcontrolNodeId || IdentityPtr->nodeId == nodeNotificationPtr->reserveintracontrolNodeId)
    {
        if (damaged_node == nodeNotificationPtr->timereferenceNodeId) // 如果是时间基准节点损坏
        {
            if (nodeNotificationPtr->reservetimereferenceNodeId == 0)
            {
                // 且备份损坏，不做处理
                return;
            }
            IdentityPtr->timereference_Temp = nodeNotificationPtr->reservetimereferenceNodeId;
            IdentityPtr->reservetimereference_Temp = 0;
        }
        else if (damaged_node == nodeNotificationPtr->intergroupgatewayNodeId) // 如果是群间网关节点损坏
        {
            if (nodeNotificationPtr->reserveintergroupgatewayNodeId == 0)
            {
                // 且备份损坏，不做处理
                return;
            }
            IdentityPtr->intergroupgateway_Temp = nodeNotificationPtr->reserveintergroupgatewayNodeId;
            IdentityPtr->reserveintergroupgateway_Temp = 0;
        }
        else if (damaged_node == nodeNotificationPtr->satellitebasedgatewayNodeId) // 如果是星基网关节点损坏
        {
            if (nodeNotificationPtr->reservesatellitebasedgatewayNodeId == 0)
            {
                // 且备份损坏，不做处理
                return;
            }
            IdentityPtr->satellitebasedgateway_Temp = nodeNotificationPtr->reservesatellitebasedgatewayNodeId;
            IdentityPtr->reservesatellitebasedgateway_Temp = 0;
        }
        //***********************关于备份节点损坏************************//
        //*************暂且备份节点置0，等待下一轮节点选取***************//
        else if (damaged_node == nodeNotificationPtr->reserveintracontrolNodeId) // 备用控制损坏
        {
            IdentityPtr->reserveintracontrol_Temp = 0;
        }
        else if (damaged_node == nodeNotificationPtr->reservetimereferenceNodeId) // 如果是备用时间基准节点损坏
        {
            IdentityPtr->reservetimereference_Temp = 0;
        }
        else if (damaged_node == nodeNotificationPtr->reserveintergroupgatewayNodeId) // 如果是备用群间网关节点
        {
            IdentityPtr->reserveintergroupgateway_Temp = 0;
        }
        else if (damaged_node == nodeNotificationPtr->reservesatellitebasedgatewayNodeId) // 如果是备用星基网关节点
        {
            IdentityPtr->reservesatellitebasedgateway_Temp = 0;
        }
    }

    // 通知其他节点新的身份
    //*************走管控*******************//
    //********************************身份更新************************************
    if (nodeNotificationPtr->nodeResponsibility == MASTERCONTROL || nodeNotificationPtr->nodeResponsibility == INTRAGROUPCONTROL)
    {
        NodeBroadcast *nodeBroadcastPtr = (NodeBroadcast *)malloc(sizeof(NodeBroadcast));
        nodeBroadcastPtr->groupID = IdentityPtr->groupID_Temp;
        nodeBroadcastPtr->intragroupcontrolNodeId = IdentityPtr->intragroupcontrol_Temp;                       // 控制
        nodeBroadcastPtr->reserveintracontrolNodeId = IdentityPtr->reserveintracontrol_Temp;                   // 备用控制
        nodeBroadcastPtr->intergroupgatewayNodeId = IdentityPtr->intergroupgateway_Temp;                       // 群间网关
        nodeBroadcastPtr->reserveintergroupgatewayNodeId = IdentityPtr->reserveintergroupgateway_Temp;         // 群间网关备份
        nodeBroadcastPtr->satellitebasedgatewayNodeId = IdentityPtr->satellitebasedgateway_Temp;               // 天基
        nodeBroadcastPtr->reservesatellitebasedgatewayNodeId = IdentityPtr->reservesatellitebasedgateway_Temp; // 天基备份
        // 其他变量待补充

        // 添加分流头
        unsigned int fullpacketSize = sizeof(msgID) + sizeof(MessageDiversionHeader) + sizeof(NodeBroadcast);
        char *msgToSendPtr = (char *)malloc(fullpacketSize);

        // 对消息标识赋值 业务信道加消息标识
        msgID *msgIdPtr = (msgID *)msgToSendPtr;
        msgIdPtr->msgid = 28; // 北航填28
        msgIdPtr->msgsub = 3; // 消息子标识，标志身份配置模块

        // 首先对分流头进行赋值
        MessageDiversionHeader *diversionPtr = (MessageDiversionHeader *)(msgToSendPtr + sizeof(msgID));
        diversionPtr->moduleName = 1;
        diversionPtr->eventType = 1;

        // 之后对负载进行赋值
        memcpy(msgToSendPtr + sizeof(msgID) + sizeof(MessageDiversionHeader), nodeBroadcastPtr, sizeof(NodeBroadcast));
        vector<uint8_t> *msg_tempPtr = TransPacketToVec_Broadcast(msgToSendPtr); // 应该将传的结构体改为八位数组形式

        // 封装msgFromControl包
        msgFromControl controlmsg;
        controlmsg.data = (char *)msg_tempPtr;
        controlmsg.packetLength = 8 + msg_tempPtr->size() + 2; // 包长度
        controlmsg.srcAddr = IdentityPtr->nodeId;              // 源地址为本节点
        controlmsg.destAddr = 0x3FF;
        controlmsg.priority = 0;    // 网管消息优先级为0
        controlmsg.backup = 0;      // 备用字段为0
        controlmsg.msgType = 1;     // 网管消息走网管信道
        controlmsg.repeat = 0;      // 非重传包
        controlmsg.fragmentNum = 0; // 无效值
        controlmsg.fragmentSeq = 0; // 分片序号，无效值
        // 序列号待补充

        vector<uint8_t> *v1_controlmsg = PackMsgFromControl(&controlmsg); // msgFromControl整体转为八位数组
        // 存储序列号
        IdentityPtr->SerialNum = controlmsg.seq;

        // 统计开销

        // 发给路由模块
        uint8_t wBufferToRouting[MAX_DATA_LEN];                // 将要写入循环缓冲区的数据(含包头)存放于此
        PackRingBuffer(wBufferToRouting, v1_controlmsg, 0x1E); // 0x1E管控开销消息
        netToRouting_buffer.ringBuffer_put(wBufferToRouting, sizeof(wBufferToRouting));

        free(msgToSendPtr); // 实际数据内容已msgToSend_data经写入vector数组，释放掉申请的内存
    }
}

/*------------------------network_identityUpdateAndMaintenance.cpp------------------------------------*/