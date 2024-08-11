#include "macdaatr.h"
#include "highFreqToolFunc.h"
#include "main.h"
#include "timer.h"
using namespace std;

/**********************此文件定义了总控线程相关内容************************/

int over_count = 0;

// 总控线程主函数
// 每 100 us 调用一次这个函数 就可以实现时间的流动
void macDaatrControlThread(int signum, siginfo_t *info, void *context)
{
    extern ringBuffer RoutingTomac_Buffer;
    extern MacDaatr daatr_str;
    uint8_t rBuffer_mac[MAX_DATA_LEN];
    extern bool end_simulation;

    // 线程结束标志(以什么样的条件终止线程)
    if (end_simulation)
    {
        over_count++;
        return;
    }

    daatr_str.time += TIME_PRECISION;         // 经过100us
    double time_ms = daatr_str.time / 1000.0; // 得到ms单位时间

    if (time_ms == SIMULATION_TIME * 1000)
        end_simulation = true; // 在本轮发送完后结束仿真

    // 判断当前节点所处阶段
    if (daatr_str.state_now == Mac_Initialization && time_ms >= END_LINK_BUILD_TIME - 1)
    { // 结束建链，进入执行阶段
        daatr_str.state_now = Mac_Execution;
        cout << endl;
        cout << endl;
        cout << "Node " << daatr_str.nodeId << " 进入执行阶段" << endl;
        cout << endl;
    }

    // 节点掉网测试
    // if (time_ms == 5000 && daatr_str.nodeId == 3)
    // {
    //     daatr_str.state_now = Mac_Access;
    //     daatr_str.access_state = DAATR_NEED_ACCESS;
    //     writeInfo("NODE %2d 掉链", daatr_str.nodeId);
    // }

    // 调整阶段测试
    if (time_ms == 2900 && daatr_str.nodeId == 1)
    {
        int linkNumTest = 437;
        LinkAssignment *linkAssList = (LinkAssignment *)malloc(linkNumTest * sizeof(LinkAssignment));
        linkNumTest = Generate_LinkAssignment_Stage_1(linkAssList);
        unsigned int fullPacketSize = sizeof(LinkAssignmentHeader) + linkNumTest * sizeof(LinkAssignment);
        char *pktStart = (char *)malloc(fullPacketSize);

        LinkAssignmentHeader *linkheader = (LinkAssignmentHeader *)pktStart;
        linkheader->linkNum = linkNumTest;
        LinkAssignment *linkAssPtr = (LinkAssignment *)(pktStart + sizeof(LinkAssignmentHeader));
        memcpy(linkAssPtr, linkAssList, linkNumTest * sizeof(LinkAssignment));

        uint8_t *ret = (uint8_t *)malloc(fullPacketSize + 3);
        memset(ret, 0, sizeof(ret)); // 清零
        ret[0] = 0x0B;
        // memcpy((ret + 1), &len, 2); // 小端序
        // 大端序
        ret[1] |= ((fullPacketSize >> 8) & 0xff);
        ret[2] |= (fullPacketSize & 0xff);
        memcpy((ret + 3), pktStart, fullPacketSize);

        extern ringBuffer RoutingTomac_Buffer;
        RoutingTomac_Buffer.ringBuffer_put(ret, fullPacketSize + 3);
    }

    daatr_str.clock_trigger += 1;
    if (daatr_str.clock_trigger % (int)HIGH_FREQ_CHANNEL_TRIGGER_LEN == 0) // 25
    {
        // 业务信道线程触发 (节点间的收发线程)
        daatr_str.highThread_condition_variable.notify_one();                   // 唤醒阻塞在发送线程 highFreqSendThread() 的 wait()
        daatr_str.networkToMacBufferReadThread_condition_variable.notify_one(); // 唤醒缓存区读函数

        if (daatr_str.clock_trigger % (int)LOW_FREQ_CHANNEL_TRIGGER_LEN == 0) // 200
        {
            // 网管信道线程触发(网管信道 节点间 收发线程)
            daatr_str.clock_trigger = 0; // 每次网管信道发送时为一个周期，清空计数
            daatr_str.lowthreadcondition_variable.notify_one();
        }
    }

    // 线程结束标志(以什么样的条件终止线程)
    if (end_simulation)
    {
        sleep(1);
        daatr_str.highThread_condition_variable.notify_one();                   // 唤醒阻塞在发送线程 highFreqSendThread() 的 wait()
        daatr_str.networkToMacBufferReadThread_condition_variable.notify_one(); // 唤醒缓存区读函数
        daatr_str.macDaatrSocketHighFreq_Send((uint8_t *)send, 13, daatr_str.nodeId);
        daatr_str.lowthreadcondition_variable.notify_one();
        if (daatr_str.nodeId == 1)
        {
            char *send = "仿真结束";
            daatr_str.macDaatrSocketLowFreq_Send((uint8_t *)send, 13);
        }
    }
}

void MacDaatr::networkToMacBufferReadThread()
{

    extern ringBuffer RoutingTomac_Buffer;
    uint8_t rBuffer_mac[MAX_DATA_LEN];
    extern bool end_simulation;
    unique_lock<mutex> nTmBufferReadlock(nTmBufferReadmutex);

    while (1)
    {
        networkToMacBufferReadThread_condition_variable.wait(nTmBufferReadlock);

        if (end_simulation)
        {
            printf("NODE %2d BufferRead Thread is Over\n", nodeId);
            break;
        }

        while (RoutingTomac_Buffer.recvFrmNum != 0)
        {
            RoutingTomac_Buffer.ringBuffer_get(rBuffer_mac);
            networkToMacBufferHandle(rBuffer_mac); // 处理接收到的序列（包括读取业务种类，业务长度，然后做对应处理）
        }
    }
}
