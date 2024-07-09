#include <fstream>
#include <sstream>
#include <thread>

#include "timer.h"
#include "macdaatr.h"
#include "common_struct.h"
#include "daatr_control.h"
#include "socket_control.h"
#include "low_freq_channel.h"
#include "high_freq_channel.h"

using namespace std;

bool end_simulation = false;    // 结束仿真标志
MacDaatr daatr_str;             // mac层daatr协议信息保存类
ringBuffer RoutingTomac_Buffer; // 网络层->MAC缓存队列
ringBuffer macToRouting_Buffer; // MAC->网络层缓存队列

void hello()
{
    cout << "hello return func!!!" << endl;
}

int main()
{
    cout << "hello world" << endl;
    cout << "hi hi hi hi" << endl;

    macParameterInitialization(); // mac层参数初始化

    thread macControl(macDaatrControlThread); // daatr_control.cpp 总控线程主函数
    macControl.join();

    // thread macLowFreq(macDaatrSocketLowFreq_Recv);
    // thread macHighFreq(macDaatrSocketHighFreq_Recv);

    return 0;
}