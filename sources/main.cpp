#include <fstream>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

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

int main()
{
    cout << "======== Simulation Start ========" << endl;
    uint32_t idx = 1;

    // 创建新的进程以模拟多节点
    pid_t pid;
    for (int i = 1; i < 6; i++)
    {
        pid = fork();
        if (pid == 0)
            break;

        idx++; // 标记进程的序号
    }
    if (pid > 0) // Parent PID
    {
        while (true)
        {
            int s;
            int ret = waitpid(-1, &s, 0); // 阻塞所有子进程，直至全部运行完成
            if (ret == -1)
                break;
        }
    }
    else if (pid == 0)
    {
        daatr_str.macParameterInitialization(idx); // mac层参数初始化
        thread highRecvThread(&MacDaatr::macDaatrSocketHighFreq_Recv, &daatr_str, false);

        // if (idx == 1)
        // {
        for (int i = 0; i < 10100; i++)
        {
            daatr_str.macDaatrControlThread();
            usleep(1e3);
        }
        // }

        highRecvThread.join();
    }

    return 0;
}