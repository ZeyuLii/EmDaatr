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
    system("clear");

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

    timeInit();

    cout << "等待同步信号" << endl;

    lowRecvThread.join();
    lowSendThread.join();
    BufferReadThread.join();
    highRecvThread.join();
    highSendThread.join();

    sleep(1);
    cout << simInfoPosition << "    " << over_count << "    " << PPS_overcount << endl;

    if (daatr_str.nodeId == 1)
    {
        for (int j = 0; j < 3; j++)
        {
            _writeInfo(0, "网管节点收到 NODE %2d 频谱感知结果为:\n", j + 1);
            char temp[TOTAL_FREQ_POINT];
            int count = 0;
            for (int i = 1; i < TOTAL_FREQ_POINT + 1; i++)
            {
                count += sprintf(temp + count, "%d", daatr_str.spectrum_sensing_sum[j][i - 1]);
                if (i % 100 == 0 || i == TOTAL_FREQ_POINT)
                {
                    sprintf(temp + count, "\n");
                    _writeInfo(0, temp);
                    memset(temp, 0, TOTAL_FREQ_POINT);
                    count = 0;
                }
            }
        }
    }

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
