#include "macdaatr.h"
#include "main.h"

using namespace std;

/**********************此文件定义了总控线程相关内容************************/

// 总控线程主函数
// 每 100 us 调用一次这个函数 就可以实现时间的流动
void MacDaatr::macDaatrControlThread()
{
    extern ringBuffer RoutingTomac_Buffer;
    // extern condition_variable lowthreadcondition_variable;
    uint8_t rBuffer_mac[MAX_DATA_LEN];
    extern bool end_simulation;

    // 互斥锁创建一个 unique_lock 对象，确保线程安全
    //-- unique_lock<mutex> uni_lock_central_control_thread(lock_central_control_thread); // 管控线程unique_lock

    //-- while (1)
    //-- {
    //-- central_control_thread_var.wait(uni_lock_central_control_thread); // 等待中断时钟沿触发
    time += TIME_PRECISION;         // 经过100us
    double time_ms = time / 1000.0; // 得到ms单位时间

    if (time_ms == SIMULATION_TIME * 1000)
        end_simulation = true; // 在本轮发送完后结束仿真

    // 判断当前节点所处阶段
    if (state_now == Mac_Initialization && time_ms >= END_LINK_BUILD_TIME - 1)
    { // 结束建链，进入执行阶段
        state_now = Mac_Execution;
    }

    clock_trigger += 1;
    // if (nodeId == 1)
    // {
    //     cout << "time_ms: " << time_ms << " ms  ";
    //     cout << "clock_trigger: " << (int)clock_trigger << endl;
    // }

    if (clock_trigger % (int)HIGH_FREQ_CHANNEL_TRIGGER_LEN == 0) // 25
    {

        // 业务信道线程触发(节点间的收发线程)
        // highThread_condition_variable.notify_one(); // 唤醒阻塞在发送线程的wait
        thread highSendThread(&MacDaatr::highFreqSendThread, this);
        highSendThread.join();

        if (clock_trigger % (int)LOW_FREQ_CHANNEL_TRIGGER_LEN == 0) // 200
        {
            // 网管信道线程触发(网管信道 节点间 收发线程)
            clock_trigger = 0; // 每次网管信道发送时为一个周期，清空计数}
        }
    }
    else
    {
        // 如果本次触发业务和网管信道都不需要发送，则进行 节点内 上下层间 信息的接收
        // 从网络-macFIFO队列接收业务
        while (RoutingTomac_Buffer.recvFrmNum != 0)
        {
            RoutingTomac_Buffer.ringBuffer_get(rBuffer_mac);
            // networkToMacBufferHandle(rBuffer_mac)//处理接收到的序列（包括读取业务种类，业务长度，然后做对应处理）
        }
    }

    // 线程结束标志(以什么样的条件终止线程)
    //-- if (end_simulation)
    //--     break;
    //-- } // --endwhile
}
