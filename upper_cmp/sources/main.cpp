#include "socket_control.h"

using namespace std;
#define subnet_node_num 3
#define SEND_BUFFER_SIZE 65536     // 发送缓冲区大小
#define RECV_BUFFER_SIZE 65536     // 接收缓冲区大小
#define HIGH_FREQ_SOCKET_PORT 8000 // 高频信道端口号
#define LOW_FREQ_SOCKET_PORT 8100  // 低频信道端口号
#define TOTAL_FREQ_POINT 501
#define SPECTRUM_SENSING_TIME 6

std::string in_subnet_id_to_ip[subnet_node_num];

static bool Randomly_Generate_0_1(double one_probability)
{
    // 以one_probability大小的概率生成1
    bool t = 0;
    // srand(time(NULL));
    double random_number = (double)rand() / RAND_MAX;
    // cout << random_number;
    if (random_number < one_probability)
    {
        t = 1;
    }
    else
    {
        t = 0;
    }
    return t;
}
int main(int argc, char *argv[])
{
    in_subnet_id_to_ip[1] = "192.168.50.2";
    in_subnet_id_to_ip[2] = "192.168.50.3";
    in_subnet_id_to_ip[3] = "192.168.50.4";
    int id = 0;
    std::string IP = "192.168.93.128";
    int sock_fd;
    int IF_NOT_BLOCKED = 0;
    char Spectrum[TOTAL_FREQ_POINT] = {0};
    sock_fd = macDaatrCreateUDPSocket(IP, LOW_FREQ_SOCKET_PORT, IF_NOT_BLOCKED);
    // 生成频谱感知序列
    float probability = atof(argv[1]);
    char filePath[100];
    for (int id = 1; id < subnet_node_num + 1; id++)
    {
        for (int times = 0; times < SPECTRUM_SENSING_TIME; times++)
        {
            sprintf(filePath, "/home/cyh/project/nfs_share/4/Spectrum_sensing/NODE%d/time%d.txt", id, times);
            int file = open(filePath, O_CREAT | O_TRUNC | O_WRONLY);

            if (!file)
                printf("文件打开错误");
            for (int i = 0; i < TOTAL_FREQ_POINT; i++)
            {
                if (Randomly_Generate_0_1(probability))
                    Spectrum[i] = '1';
                else
                    Spectrum[i] = '0';
            }

            write(file, Spectrum, TOTAL_FREQ_POINT);
            close(file);
            sprintf(filePath, "chmod 777 /home/cyh/project/nfs_share/4/Spectrum_sensing/NODE%d/time%d.txt", id, times);
            system(filePath);
            memset(filePath, 0, 100);
        }
    }

    // 掉链
    printf("请输入想要掉链的节点ID\n");

    int dropID = 0;
    scanf("%d", &dropID);
    if (dropID == 0)
    {
        printf("输入失败\n");
    }
    else if (dropID > 20 || dropID < 1)
    {
        printf("输入错误,ID应该在1-20内\n");
    }
    // printf("%d\n", dropID);
    char *Chain_drop = "掉链";
    macDaatrSocketLowFreq_Send((uint8_t *)Chain_drop, 7, dropID);
    sleep(1);
    Chain_drop = "上链";
    macDaatrSocketLowFreq_Send((uint8_t *)Chain_drop, 7, dropID);

    return 0;
}
