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

int main(int argc, char *argv[])
{
    uint8_t buf[] = "End Simulation";

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
        thread highSendThread(&MacDaatr::highFreqSendThread, &daatr_str);

        for (int i = 0; i < 13000; i++)
        {
            macDaatrControlThread(0, 0, 0);
            usleep(1e3);
        }

        end_simulation = true;
        cout << "main end_simulation" << endl;
        this_thread::sleep_for(chrono::seconds(1));
        daatr_str.highThread_condition_variable.notify_one();
        // daatr_str.macDaatrSocketLowFreq_Send(buf, 15);
        highRecvThread.join();
        highSendThread.join();
    }
    return 0;

    // extern int over_count;
    // extern int PPS_overcount;
    // extern int fd_irq;

    // int id = atoi(argv[1]);

    // daatr_str.macParameterInitialization(id); // mac层参数初始化
    // if (daatr_str.nodeId == 1)
    // {
    //     daatr_str.local_node_position_info.positionX = 1;
    //     daatr_str.local_node_position_info.positionY = 2;
    //     daatr_str.local_node_position_info.positionZ = 3;
    // }
    // else if (daatr_str.nodeId == 2)
    // {
    //     daatr_str.local_node_position_info.positionX = 4;
    //     daatr_str.local_node_position_info.positionY = 5;
    //     daatr_str.local_node_position_info.positionZ = 6;
    // }
    // else if (daatr_str.nodeId == 3)
    // {
    //     daatr_str.local_node_position_info.positionX = 7;
    //     daatr_str.local_node_position_info.positionY = 8;
    //     daatr_str.local_node_position_info.positionZ = 9;
    // }

    // thread t1(&MacDaatr::macDaatrSocketLowFreq_Recv, &daatr_str, false);
    // thread t2(lowFreqSendThread);

    // thread highRecvThread(&MacDaatr::macDaatrSocketHighFreq_Recv, &daatr_str, false);
    // thread highSendThread(&MacDaatr::highFreqSendThread, &daatr_str);

    // timeInit();

    // cout << "等待同步信号" << endl;

    // t1.join();
    // t2.join();
    // highRecvThread.join();
    // highSendThread.join();

    // cout << simInfoPosition << "    " << over_count << "    " << PPS_overcount << endl;

    // // 将仿真数据写入文件
    // char filePath[50];
    // sprintf(filePath, "./res/NODE%d.txt", id);
    // int file = open(filePath, O_CREAT | O_TRUNC | O_WRONLY);

    // if (!file)
    //     cout << "文件打开错误" << endl;

    // for (int j = 0; j < simInfoPosition; j++)
    // {
    //     write(file, simInfo[j], strlen(simInfo[j]));
    // }

    // close(file);
    // close(fd_irq);

    // system("rmmod irq_start.ko");
    // system("rmmod irq.ko");

    // return 0;
}
