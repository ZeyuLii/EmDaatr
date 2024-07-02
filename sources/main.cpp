#include "macdaatr.h"
#include "common_struct.h"
#include "daatr_control.h"
#include "fstream"
#include <sstream>
#include "timer.h"
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
    cout << "hihihihi" << endl;

    mac_paramater_initialization(); // mac层参数初始化
    return 0;
}