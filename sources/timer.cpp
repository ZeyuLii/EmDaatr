#include "timer.h"

#define USBCOM "/dev/ttyPS1"
#define EVENT_MAX_NUMBER 25 // 定时器最大数量

// 示例
//  extern uint16_t event_number_now;
//  for(int i = 0;i < 10;i++)
//  {
//      insertEventTimer_ms(1,hello);
//      eventTimerUpdate();
//      cout << "定时器现有数据：" << event_number_now << endl;
//  }
//  for(int i = 0;i < 12;i++)
//  {
//      eventTimerUpdate();
//  }

extern MacDaatr daatr_str; // mac层协议类
extern bool end_simulation;

char simInfo[12000][200] = {0};
int simInfoPosition = 0;

mutex mtxWritePos;
mutex mtxWriteStr;

int PPS_overcount = 0;
int fd_irq = 0;
int pid = 0;
struct timespec tpstart = {0};
unsigned long long idleTime = 0;
unsigned long long totalTime = 0;

myevent event_list[EVENT_MAX_NUMBER];
uint16_t event_number_now = 0;

/**
 * @brief 插入一个微秒级别的事件定时器
 *
 * @param time_del 事件触发前的延迟时间，单位为微秒
 * @param event_func 事件触发时要执行的函数指针
 * @return true 成功插入事件定时器
 * @return false 事件列表已满，无法插入新的定时器
 */
bool insertEventTimer_us(uint64_t time_del, event_function event_func)
{
    if (event_number_now >= EVENT_MAX_NUMBER)
        return false;
    for (int i = 0; i < EVENT_MAX_NUMBER; i++)
    {
        if (event_list[i].if_used == false)
        { // 是否是空闲
            event_list[i].if_used = true;
            event_list[i].time_delay = time_del;
            event_list[i].func = event_func;
            event_list[i].time_has_passed = 0;
            return true;
        }
    }
    return false;
}

/**
 * @brief 插入一个毫秒级别的事件定时器
 *
 * @param time_del 事件触发前的延迟时间，单位为微秒
 * @param event_func 事件触发时要执行的函数指针
 * @return true 成功插入事件定时器
 * @return false 事件列表已满，无法插入新的定时器
 */
bool insertEventTimer_ms(double time_del, event_function event_func)
{
    time_del *= 1000; // 转化成us
    for (int i = 0; i < EVENT_MAX_NUMBER; i++)
    {
        if (event_list[i].if_used == false)
        { // 是否是空闲
            event_list[i].if_used = true;
            event_list[i].time_delay = time_del;
            event_list[i].func = event_func;
            event_list[i].time_has_passed = 0;
            event_number_now += 1;
            return true;
        }
    }
    cout << "事件队列已满！！！" << endl;
    return false;
}

void eventTimerUpdate()
{
    if (event_number_now == 0)
        return;
    for (int i = 0; i < EVENT_MAX_NUMBER; i++)
    {
        if (event_list[i].if_used == true)
        { // 是否是空闲
            event_list[i].time_has_passed += TIME_PRECISION;
            if (event_list[i].time_has_passed >= event_list[i].time_delay)
            {
                event_list[i].func();
                event_list[i].if_used = false;
                event_number_now -= 1;
            }
        }
    }
}

int utcToint(const char *str, int find)
{

    printf("time is :%s\n", str);
    char time[8];

    for (int i = find + 7; i < find + 13; i++)
        time[i - find - 7] = str[i];

    time[8] = '\0';
    printf("time is %d\n", atoi(time));

    return atoi(time);
}

int utcGet()
{

    int fd = open(USBCOM, O_RDONLY);
    if (fd == -1)
    {
        perror("open error!");
        exit(-1);
    }

    // set attr
    // 1.save the old data of attr  oldtms
    struct termios newtms, oldtms;
    bzero(&newtms, sizeof(struct termios));
    if (tcgetattr(fd, &oldtms) == -1)
    {
        perror("tegetattr error!");
        exit(-1);
    }

    // 2.set CREAD and CLOCAL
    newtms.c_cflag |= CREAD;
    newtms.c_cflag |= CLOCAL;

    // 3.set bound
    if (cfsetispeed(&newtms, B115200) == -1)
    {
        perror("cfsetispeed error!");
        exit(-1);
    }
    if (cfsetospeed(&newtms, B115200) == -1)
    {
        perror("cfsetospeed error!");
        exit(-1);
    }
    // 4.set CSIZE
    newtms.c_cflag &= ~CSIZE;
    newtms.c_cflag |= CS8;

    // 5.set PARENB
    newtms.c_cflag &= ~PARENB;

    // 6.set CSTOPB
    newtms.c_cflag &= ~CSTOPB;

    // 7.set wait time and min of char
    newtms.c_cc[VTIME] = 0;
    newtms.c_cc[VMIN] = 1;

    // 8.引用对象
    if (tcflush(fd, TCIOFLUSH) == -1)
    {
        perror("tcflush error!");
        exit(-1);
    }

    // 9.激活设置
    if (tcsetattr(fd, TCSANOW, &newtms) == -1)
    {
        perror("tcsetattr error!");
        exit(-1);
    }

    char buf[512];
    int ir = 0;
    int count = 0;
    char *find;
    int res = 0;
    bzero(&buf, 512);
    while (1)
    {
        // 清除buf中数据，包括'\0'
        ir = read(fd, buf + count, 25);
        if (ir > 0)
        {
            count = count + ir;
            find = strstr(buf, "$GNRMC,");
            if (find != NULL)
            {
                res = utcToint(buf, find - buf);
                if (res)
                {
                    close(fd);
                    return res;
                }
                count = 0;
                bzero(&buf, 512);
            }
        }
    }
    return 0;
}

void microsecondIRQ()
{
    const char *dev_name = "/dev/mydev_clk";
    extern void macDaatrControlThread(int signum, siginfo_t *info, void *context);

    int count = 0;
    pid = getpid();

    // 打开GPIO
    if ((fd_irq = open(dev_name, O_RDWR | O_NDELAY)) < 0)
    {
        printf("open dev failed! \n");
        return;
    }

    printf("open dev success! \n");

    // 注册信号处理函数
    struct sigaction sc;
    sigemptyset(&sc.sa_mask);
    sc.sa_sigaction = &macDaatrControlThread;
    sc.sa_flags = SA_SIGINFO;

    sigaction(35, &sc, NULL);

    // set PID
}

void ppsIRQ()
{
    const char *dev_name_pps = "/dev/mydev";
    int fd_irq_pps = 0;
    int pid_pps = getpid();

    // 打开GPIO
    if ((fd_irq_pps = open(dev_name_pps, O_RDWR | O_NDELAY)) < 0)
    {
        printf("open dev failed! \n");
        return;
    }

    printf("open dev success! \n");

    // 注册信号处理函数
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = &ppsIRQHandle;

    sa.sa_flags = SA_SIGINFO;

    sigaction(36, &sa, NULL);

    // set PID
    // printf("call ioctl. pid = %d \n", pid);
    ioctl(fd_irq_pps, 100, &pid_pps);
    close(fd_irq_pps);
}

void ppsIRQHandle(int signum, siginfo_t *info, void *context)
{

    if (!PPS_overcount)
    {
        printf("开始仿真\n\n");
        extern int fd_irq;
        extern int pid;
        // signal(36, SIG_IGN);
        int stim = SIMULATION_TIME;
        clock_gettime(CLOCK_MONOTONIC, &tpstart);
        ioctl(fd_irq, 102, &stim);
        ioctl(fd_irq, 101, &pid);
        PPS_overcount++;
        daatr_str.lowthreadcondition_variable.notify_one();
        parseCpuTimes();
    }
    else
        PPS_overcount++;
    return;
}

// 解析 /proc/stat 文件中的 CPU 时间
void parseCpuTimes()
{
    // system("cat /proc/stat");
    ifstream file("/proc/stat");
    string line;

    if (file.is_open())
    {
        getline(file, line); // 读取第一行 (以 "cpu" 开头)
        file.close();

        string cpu;
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;

        istringstream ss(line);
        ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

        idleTime = idle + iowait;                                                 // 空闲时间
        totalTime = user + nice + system + idle + iowait + irq + softirq + steal; // 总时间
        // printf(" \nhhhhhh%lld %lld\n", (long long)idleTime, (long long)totalTime);
        // cout << "  " << cpu << "  " << user << "  " << nice << "  " << system << "  " << endl;
        // cout << "  " << idle << "  " << iowait << "  " << irq << "  " << softirq << "  " << steal << endl;
    }
    else
    {
        cerr << "无法打开 /proc/stat 文件" << endl;
    }
}
// 计算 CPU 占用率
double calculateCpuUsage()
{
    unsigned long long newIdleTime, newTotalTime;
    // system("cat /proc/stat");
    //  读取初始 CPU 时间
    ifstream file("/proc/stat");
    string line;

    if (file.is_open())
    {
        getline(file, line); // 读取第一行 (以 "cpu" 开头)
        file.close();

        string cpu;
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;

        istringstream ss(line);
        ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

        newIdleTime = idle + iowait;                                                 // 空闲时间
        newTotalTime = user + nice + system + idle + iowait + irq + softirq + steal; // 总时间
        // printf(" \njisuan%lld %lld\n", (long long)newIdleTime, (long long)newTotalTime);
        // cout << "  " << cpu << "  " << user << "  " << nice << "  " << system << "  " << endl;
        // cout << "  " << idle << "  " << iowait << "  " << irq << "  " << softirq << "  " << steal << endl;
    }
    else
    {
        cerr << "无法打开 /proc/stat 文件" << endl;
    }

    // 计算时间差
    unsigned long long idleDelta = newIdleTime - idleTime;
    unsigned long long totalDelta = newTotalTime - totalTime;
    // printf("res is %lld %lld %lld %lld %lf\n", idleTime, totalTime, idleDelta, totalDelta, ((double)idleDelta / (double)totalDelta));
    //  计算 CPU 使用率
    double cpuUsage = (double)idleDelta / (double)totalDelta;

    return cpuUsage;
}

void _writeInfo(bool addTimeHeadFlag, const char *fmt, ...)
{

    static char sprint_buf[2048];
    struct timespec tptemp;
    char temp_str[200] = {0};
    va_list args;

    mtxWriteStr.lock();
    va_start(args, fmt);
    vsprintf(sprint_buf, fmt, args);
    va_end(args);

    if (!addTimeHeadFlag)
    {
        sprintf(simInfo[simInfoPosition++],
                "%s\n",
                sprint_buf);
        mtxWriteStr.unlock();
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &tptemp);

    long long temp = tptemp.tv_nsec +
                     (long long)(tptemp.tv_sec - tpstart.tv_sec) * 1e9 - tpstart.tv_nsec;

    long long temp_sec = temp / 1e9;
    temp = temp - temp_sec * 1e9;
    long long temp_msec = temp / 1e6;
    temp = temp - temp_msec * 1e6;
    long long temp_usec = temp / 1000;
    long long temp_nsec = temp % 1000;

    // mtxWritePos.lock();
    sprintf(simInfo[simInfoPosition++],
            "[ %3d ,  %3lld %3lld %3lld] %s\n",
            temp_sec, temp_msec, temp_usec, temp_nsec, sprint_buf);
    // mtxWritePos.unlock();
    mtxWriteStr.unlock();
}

void timeInit()
{
    system("echo system_wrapper.bin > /sys/class/fpga_manager/fpga0/firmware");
    system("echo spi32766.0 > /sys/bus/spi/drivers/ads7846/unbind");
    system("echo 960 > /sys/class/gpio/export");
    system("echo in > /sys/class/gpio/gpio960/direction");
    system("echo 961 > /sys/class/gpio/export");
    system("echo in > /sys/class/gpio/gpio961/direction");
    system("echo 962 > /sys/class/gpio/export");
    system("echo in > /sys/class/gpio/gpio962/direction");
    system("insmod irq_start.ko");
    system("insmod irq.ko");

    if (daatr_str.nodeId == daatr_str.mana_node)
    {
        while (!daatr_str.start_irq)
            ;

        char *send = "start";
        daatr_str.macDaatrSocketLowFreq_Send((uint8_t *)send, 6);
        printf("发送开始信号\n");
    }
    else
    {
        char send[15] = {0};
        sprintf(send, "ready NODE%02d", daatr_str.nodeId);
        while (!daatr_str.init_send)
        {
            daatr_str.macDaatrSocketLowFreq_Send((uint8_t *)send, 13);
            printf("NODE %2d send\n", daatr_str.nodeId);
            sleep(1);
        }
        printf("等待开始信号\n");
        while (!daatr_str.start_irq)
            ;
    }

    ppsIRQ();
    microsecondIRQ();
}
