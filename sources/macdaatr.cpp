#include <iostream>
#include "macdaatr.h"
#include "main.h"
#include "fstream"
#include <sstream>
using namespace std;

/*****************************此文件主要包含对本层协议结构体控制相关函数******************************/
/**
 * 获取当前时间
 *
 * 该函数通过访问外部变量daatr_str来获取当前的时间值。
 * 时间值以uint64_t类型表示，具体的时间单位由外部变量的定义决定。
 * 单位为us,精确度100us
 *
 * @return 返回当前协议栈的绝对时间，单位取决于外部变量daatr_str的定义。
 */

uint64_t getTime()
{
    extern MacDaatr daatr_str;
    return daatr_str.time;
}

/**
 * @brief 打印时间（以毫秒为单位）
 *
 * 该函数从外部定义的MacDaatr变量中获取时间值，并将其转换为毫秒单位进行打印。
 * 这对于需要知道操作耗时的应用场景非常有用，比如性能测试或时间戳记录。
 *
 * 注意：该函数依赖于外部定义的daatr_str变量，该变量应事先被正确初始化和更新。
 */
void printTime_ms()
{
    extern MacDaatr daatr_str;
    double time_ms = daatr_str.time / 1000.0;
    cout << "time:" << time_ms << " ms" << endl;
}

/**
 * @brief 打印时间（以微秒为单位）
 *
 * 该函数从外部定义的MacDaatr变量中获取时间值，并将其转换为微秒单位进行打印。
 * 这对于需要知道操作耗时的应用场景非常有用，比如性能测试或时间戳记录。
 *
 * 注意：该函数依赖于外部定义的daatr_str变量，该变量应事先被正确初始化和更新。
 */
void printTime_us()
{
    extern MacDaatr daatr_str;
    cout << "time:" << daatr_str.time << " us" << endl;
}

/**
 * @brief 打印时间（以秒为单位）
 *
 * 该函数从外部定义的MacDaatr变量中获取时间值，并将其转换为秒单位进行打印。
 * 这对于需要知道操作耗时的应用场景非常有用，比如性能测试或时间戳记录。
 *
 * 注意：该函数依赖于外部定义的daatr_str变量，该变量应事先被正确初始化和更新。
 */
void printTime_s()
{
    extern MacDaatr daatr_str;
    double time_ms = daatr_str.time / 1000000.0;
    cout << "time:" << time_ms << " s" << endl;
}

/*******************************MacDaatr类函数********************************************************/

/**
 * @brief 构造函数MacDaatr
 *
 * 该构造函数用于初始化MacDaatr类的实例。
 * 它设置了节点ID和子网ID，同时初始化了时钟触发器和两个MAC DAATR套接字的文件描述符。
 *
 * @param nodeId 节点ID，用于标识设备在子网中的唯一位置。
 * @param subnetId 子网ID，用于标识设备所属的子网。
 */
MacDaatr::MacDaatr(uint16_t nodeId_, uint16_t subnetId_)
{
    // 初始化时钟触发器为0，表示尚未触发。
    clock_trigger = 0;

    // 初始化高频率MAC DAATR套接字文件描述符为-1，表示尚未打开套接字。
    mac_daatr_high_freq_socket_fid = -1;

    // 初始化低频率MAC DAATR套接字文件描述符为-1，表示尚未打开套接字。
    mac_daatr_low_freq_socket_fid = -1;

    // 初始化节点ID和子网ID
    nodeId = nodeId_;
    subnetId = subnetId_;

    low_slottable_should_read = 0;
    state_now = Mac_Initialization; // 初试为建链阶段
    time = 0;                       // 绝对时间初始化为0
}

MacDaatr::MacDaatr()
{
    // 初始化时钟触发器为0，表示尚未触发。
    clock_trigger = 0;

    // 初始化高频率MAC DAATR套接字文件描述符为-1，表示尚未打开套接字。
    mac_daatr_high_freq_socket_fid = -1;

    // 初始化低频率MAC DAATR套接字文件描述符为-1，表示尚未打开套接字。
    mac_daatr_low_freq_socket_fid = -1;

    low_slottable_should_read = 0;
    state_now = Mac_Initialization;         // 初试为建链阶段
    time = 0;                               // 绝对时间初始化为0
    access_state = DAATR_NO_NEED_TO_ACCESS; // 默认不需要接入
    access_backoff_number = 0;              // 接入时隙
}

msgFromControl MacDaatr::getBusinessFromHighChannel()
{
    lock_buss_channel.lock();
    // 获取业务

    lock_buss_channel.unlock();
}

/***********************************************buffer缓冲区写函数************************************************/
/**
 * 将输入结构体转换为缓冲区帧并插入
 *
 * @param data 数据
 * @param type 数据类型
 * @param len 数据长度
 * @return 返回一个指向转换后缓冲区的指针
 *
 * 该函数首先对输入数据进行设定帧格式转换
 * 具体的转换流程是，对数据添加帧头，第一位为数据类型，第二、三位为数据长度（采用大端），第四位开始为数据
 * 然后将转换后的数据插入缓冲区
 */
void macToNetworkBufferHandle(void *data, uint8_t type, uint16_t len)
{
    extern MacDaatr daatr_str; // mac层协议类
    extern ringBuffer macToRouting_Buffer;
    uint8_t *ret = NULL; // 返回值
    ret = (uint8_t *)malloc(len + 3);
    memset(ret, 0, sizeof(ret)); // 清零
    ret[0] = type;
    // memcpy((ret+1),&len,2);//小端序
    // 大端序
    ret[1] |= ((len >> 8) & 0xff);
    ret[2] |= (len & 0xff);
    memcpy((ret + 3), data, len);
    // 写进Mac->Net缓冲区
    daatr_str.lock_Mac_to_Net.lock();
    macToRouting_Buffer.ringBuffer_put(ret, len + 3);
    daatr_str.lock_Mac_to_Net.unlock();
    free(ret);
}

/**
 * net->MAC缓冲区数据处理函数
 *
 * @param rBuffer_mac net->MAC缓冲区取得的数据，注意，这只为一行数据，大小最大
 *
 * 该函数首先解析MAC缓冲区中的数据类型和长度字段，然后根据数据类型来处理相应的数据。
 * 目前支持的数据类型包括飞行状态信息、身份配置信息、链路构建需求、转发表和网络数据包。
 * 对于每种数据类型，函数会在switch语句中根据类型值进行分支，并在每个分支中提供一个处理逻辑的框架。
 *
 * 注意：当前函数实现中，各个分支内的处理逻辑尚未实现，需要根据具体需求进行补充。
 */
void networkToMacBufferHandle(uint8_t *rBuffer_mac)
{
    extern MacDaatr daatr_str;     // mac层协议类
    uint8_t type = rBuffer_mac[0]; // 数据类型
    uint16_t len = 0;              // 数据长度（单位：字节）
    len |= rBuffer_mac[1];
    len << 8;
    len |= rBuffer_mac[2];
    uint8_t *data = &rBuffer_mac[3]; // 数据启示指针
    switch (type)
    {
    case 0x01:
    { // 飞行状态信息
        FlightStatus *temp = (FlightStatus *)data;
        daatr_str.local_node_position_info = *temp;
        break;
    }
    case 0x0a:
    { // 身份配置信息

        break;
    }
    case 0x0b:
    { // 链路构建需求

        break;
    }
    case 0x0e:
    { // 转发表

        break;
    }
    case 0x10:
    { // 网络数据包（业务+信令）

        break;
    }
    }
}

void macParameterInitialization()
{
    extern MacDaatr daatr_str; // mac层协议类
    string filePath = "../config/daatr_config.txt";
    // 打开文件
    ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "!!!!无法打开文件: " << filePath << std::endl;
    }
    // 读取文件内容
    string line;
    while (getline(file, line))
    {                            //
        istringstream iss(line); // 将读取的行放入字符串流中以便进一步分割
        vector<string> items;
        string item;
        while (getline(iss, item, ' '))
        {                          // 使用空格分割每一项
            items.push_back(item); // 将分割出的项存入vector中
        }
        string type = items[0];
        items.erase(items.begin()); // 删除数据类型
        if (type == "SUBNET")
        { // 子网号
            daatr_str.subnetId = stoi(items[0]);
        }
        else if (type == "NODEID")
        { // 节点号
            daatr_str.nodeId = stoi(items[0]);
        }
        else if (type == "SUBNET_NODE_NUM")
        { // 子网节点数量
            daatr_str.subnet_node_num = stoi(items[0]);
        }
        else if (type == "IMPORTANT_NODE")
        {                                                    // 子网重要节点
            daatr_str.mana_node = stoi(items[0]);            // n网管节点
            daatr_str.gateway_node = stoi(items[1]);         // 网关节点
            daatr_str.standby_gateway_node = stoi(items[2]); // 备份网关节点
        }
        else if (type == "NODE_TYPE")
        { // 节点身份
            if (items[0] == "ORDINARY")
            { // 普通节点
                daatr_str.node_type = Node_Ordinary;
            }
            else if (items[0] == "MANAGEMENT")
            { // 网管节点
                daatr_str.node_type = Node_Management;
            }
            else if (items[0] == "GATEWAY")
            { // 网关节点
                daatr_str.node_type = Node_Gateway;
            }
            else if (items[0] == "STANDBY_GATEWAY")
            { // 备份网关节点
                daatr_str.node_type = Node_Standby_Gateway;
            }
        }
        else if (type == "MANA_CHANEL_BUILD_STATE_SLOT_NODE_ID")
        { // 低频信道建链阶段初始化
            for (int i = 0; i < items.size(); i++)
            {
                daatr_str.low_freq_link_build_slot_table[i].nodeId = stoi(items[i]);
            }
        }
        else if (type == "MANA_CHANEL_BUILD_STATE_SLOT_STATE")
        { // 低频信道非建链阶段初始化
            for (int i = 0; i < items.size(); i++)
            {
                daatr_str.low_freq_link_build_slot_table[i].state = stoi(items[i]);
            }
        }
        else if (type == "MANA_CHANEL_OTHER_STATE_SLOT_NODE_ID")
        { // 低频信道非建链阶段初始化
            for (int i = 0; i < items.size(); i++)
            {
                daatr_str.low_freq_other_stage_slot_table[i].nodeId = stoi(items[i]);
            }
        }
        else if (type == "MANA_CHANEL_OTHER_STATE_SLOT_STATE")
        { // 低频信道非建链阶段初始化
            for (int i = 0; i < items.size(); i++)
            {
                daatr_str.low_freq_other_stage_slot_table[i].state = stoi(items[i]);
            }
        }
    }

    // 关闭文件
    file.close();
    // 配置ip地址，ip地址配置策略为'192.168.'+ 子网号 + 节点ID
    for (int i = 2; i <= daatr_str.subnet_node_num + 1; i++)
    {
        daatr_str.in_subnet_id_to_ip[i - 1] = "192.168." + to_string(daatr_str.subnetId) + "." + to_string(i);
    }
    cout << endl;
}
