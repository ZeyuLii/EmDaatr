#include "main.h"
#include "link.h"
#include "spectrum.h"
using namespace std;
struct Event
{
    int node;   // 节点编号
    int status; // 状态: 1表示上链, 0表示下链
};
struct TimeStep
{
    double time;               // 时间节点 (毫秒)
    std::vector<Event> events; // 在该时间节点上发生的事件
};
struct ParsedData
{
    float time;
    std::vector<int> drop_nodes;
    std::vector<int> up_nodes;
};
std::vector<TimeStep> timeSteps;
void printTimeSteps(const std::vector<TimeStep> &timeSteps)
{
    for (const auto &timeStep : timeSteps)
    {
        std::cout << "时间节点: " << timeStep.time << " 秒" << std::endl;
        std::cout << "事件: " << std::endl;
        for (const auto &event : timeStep.events)
        {
            std::cout << "  节点: " << event.node << ", 状态: " << event.status << std::endl;
        }
    }
}

ParsedData parseLine(const std::string &line)
{
    ParsedData data = {0.0, {}, {}};

    // 使用正则表达式解析时间节点，可以是整数或小数
    std::regex time_regex(R"((\d+(\.\d+)?))");
    std::smatch match;
    if (std::regex_search(line, match, time_regex))
    {
        data.time = std::stof(match[1]);
    }

    std::regex drop_regex(R"(drop:((\d+\s*)+))");
    if (std::regex_search(line, match, drop_regex))
    {
        std::stringstream ss(match[1]);
        int node;
        while (ss >> node)
        {
            data.drop_nodes.push_back(node);
        }
    }

    // 使用正则表达式解析up节点
    std::regex up_regex(R"(up:((\d+\s*)+))");
    if (std::regex_search(line, match, up_regex))
    {
        std::stringstream ss(match[1]);
        int node;
        while (ss >> node)
        {
            data.up_nodes.push_back(node);
        }
    }

    return data;
}

bool link_init()
{
    extern std::string in_subnet_id_to_ip[21];
    extern int sock_fd;
    for (int i = 1; i <= 20; ++i)
    {
        in_subnet_id_to_ip[i] = "192.168.50." + std::to_string(i + 1); // 从192.168.50.2开始
    }
    int id = 0;
    std::string IP = "192.168.50.21";

    int IF_NOT_BLOCKED = 0;
    sock_fd = macDaatrCreateUDPSocket(IP, 8102, IF_NOT_BLOCKED);

    std::ifstream file("link.txt");
    if (!file.is_open())
    {
        std::cerr << "无法打开文件" << std::endl;
        return 0;
    }

    double probability;
    std::string line;
    if (std::getline(file, line))
    {
        probability = std::stod(line);
    }
    std::cout << "概率值: " << probability << std::endl;
    write_spectrum_to_file(probability);
    float time_temp = -1;
    while (std::getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }
        ParsedData data = parseLine(line);
        TimeStep step_temp;
        step_temp.time = data.time;
        if (data.time <= time_temp)
        {
            cout << "请按顺序输入时间节点" << endl;
            file.close();
            return 0;
        }
        // std::cout << "时间节点: " << data.time << std::endl;
        // std::cout << "drop节点: ";
        for (int node : data.drop_nodes)
        {
            // std::cout << node << " ";
            Event even_temp;
            even_temp.node = node;
            even_temp.status = 0;
            step_temp.events.push_back(even_temp);
        }
        // std::cout << std::endl;

        std::cout << "up节点: ";
        for (int node : data.up_nodes)
        {
            // std::cout << node << " ";
            Event even_temp;
            even_temp.node = node;
            even_temp.status = 1;
            step_temp.events.push_back(even_temp);
        }
        // std::cout << std::endl;
        timeSteps.push_back(step_temp);
    }

    printTimeSteps(timeSteps);

    std::cout << std::endl;
    file.close();
    return 1;
}

void printCurrentTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    // 计算当前时间的毫秒部分
    long milliseconds = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    // 打印当前时间
    printf("当前时间: %ld 毫秒\n", milliseconds);
}

void drop_link_process()
{
    char *Chain_drop = "掉网";
    char *Chain_up = "入网";
    printf("开始掉链流程\n");
    struct timespec startTime, currentTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);
    printCurrentTime();
    for (const auto &timeStep : timeSteps)
    {
        double waitTimeSec = timeStep.time;
        while (1)
        {
            clock_gettime(CLOCK_MONOTONIC, &currentTime);
            double elapsedSec = (currentTime.tv_sec - startTime.tv_sec) +
                                (currentTime.tv_nsec - startTime.tv_nsec) / 1e9;
            // printf("%lld   %lld", currentTime.tv_sec, currentTime.tv_nsec);
            if (waitTimeSec <= elapsedSec)
            {
                break; // 达到指定时间，退出循环
            }
            usleep(1000); // 每毫秒检查一次
        }
        cout << "**************drop control************" << endl;
        printCurrentTime();
        for (const auto &event : timeStep.events)
        {
            uint8_t *message = (event.status == 1) ? (uint8_t *)Chain_up : (uint8_t *)Chain_drop;
            size_t length = 7; // 消息长度

            macDaatrSocketLowFreq_Send(message, length, event.node);
            printf("节点 %d   %s\n", event.node, message);
            cout << "*************************************" << endl;
        }
    }
}