#include "macdaatr.h"
#include "highFreqToolFunc.h"
#include "main.h"
#include "timer.h"
using namespace std;

/**********************此文件定义了总控线程相关内容************************/

int over_count = 0;
bool firstExce = 1;
// 总控线程主函数
// 每 100 us 调用一次这个函数 就可以实现时间的流动
void macDaatrControlThread(int signum, siginfo_t *info, void *context)
{
    extern MacDaatr daatr_str;
    extern bool end_simulation;

    // 线程结束标志(以什么样的条件终止线程)
    if (end_simulation)
    {
        over_count++;
        return;
    }

    daatr_str.time += TIME_PRECISION;         // 经过500us
    double time_ms = daatr_str.time / 1000.0; // 得到ms单位时间

    if (time_ms == SIMULATION_TIME * 1000)
        end_simulation = true; // 在本轮发送完后结束仿真

    // 判断当前节点所处阶段
    if (daatr_str.state_now == Mac_Initialization && time_ms == END_LINK_BUILD_TIME)
    { // 结束建链，进入执行阶段
        daatr_str.state_now = Mac_Execution;
        cout << endl;
        cout << endl;
        cout << "Node " << daatr_str.nodeId << " 进入执行阶段  ";
        printTime_ms();
        cout << endl;
    }

    // 会出现第一个时隙网管信道不触发的情况

    daatr_str.clock_trigger += 1;
    if (daatr_str.clock_trigger % (int)HIGH_FREQ_CHANNEL_TRIGGER_LEN == 0) // 25
    {
        // 业务信道线程触发 (节点间的收发线程)
        if (end_simulation == true)
        {
            cout << "Node send end_simulation" << endl;
        }
        daatr_str.highThread_condition_variable.notify_one();                   // 唤醒阻塞在发送线程 highFreqSendThread() 的 wait()
        daatr_str.networkToMacBufferReadThread_condition_variable.notify_one(); // 唤醒缓存区读函数

        if (daatr_str.clock_trigger % (int)LOW_FREQ_CHANNEL_TRIGGER_LEN == 0) // 200
        {
            // 网管信道线程触发(网管信道 节点间 收发线程)
            daatr_str.clock_trigger = 0; // 每次网管信道发送时为一个周期，清空计数
            daatr_str.lowthreadcondition_variable.notify_one();
            // parseCpuTimes();
        }
    }

    // 线程结束标志(以什么样的条件终止线程)
    if (end_simulation)
    {
        sleep(1);
        daatr_str.macDaatrSocketHighFreq_Send((uint8_t *)send, 13, daatr_str.nodeId);
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

        while (RoutingTomac_Buffer.recvFrmNum != 0)
        {
            RoutingTomac_Buffer.ringBuffer_get(rBuffer_mac);
            networkToMacBufferHandle(rBuffer_mac); // 处理接收到的序列（包括读取业务种类，业务长度，然后做对应处理）
        }

        if (end_simulation)
        {
            sleep(1);
            printf("Node %2d networkToMacBufferReadThread is Over\n", nodeId);
            break;
        }
    }
}
