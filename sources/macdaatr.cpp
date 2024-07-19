#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "macdaatr.h"
#include "main.h"

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
    cout << "Time: " << time_ms << " ms" << endl;
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

/*****************************************MacDaatr类函数**********************************************/

/**
 * @brief 构造函数MacDaatr
 *
 * 该构造函数用于初始化MacDaatr类的实例。
 * 它设置了节点ID和子网ID，同时初始化了时钟触发器和两个MAC DAATR套接字的文件描述符。
 *
 */
MacDaatr::MacDaatr()
{
    clock_trigger = 0; // 初始化时钟触发器为0，表示尚未触发
    time = 0;          // 绝对时间初始化为0

    // 初始化高频率MAC DAATR套接字文件描述符为-1，表示尚未打开套接字。
    mac_daatr_high_freq_socket_fid = -1;

    // 初始化低频率MAC DAATR套接字文件描述符为-1，表示尚未打开套接字。
    mac_daatr_low_freq_socket_fid = -1;
}

/// @brief 读取建链阶段时隙表
void MacDaatr::LoadSlottable_setup()
{
    // cout << "Node " << nodeId << " 读入建链阶段时隙表" << endl;
    string stlotable_state_filename = "../config/SlottableInitialization/Slottable_RXTX_State_" +
                                      to_string(static_cast<long long>(nodeId)) + ".txt";
    string stlotable_node_filename = "../config/SlottableInitialization/Slottable_RXTX_Node_" +
                                     to_string(static_cast<long long>(nodeId)) + ".txt";
    ifstream fin1(stlotable_state_filename);
    ifstream fin2(stlotable_node_filename);

    if (!fin1.is_open())
        cout << "Could Not Open File1" << endl;

    if (!fin2.is_open())
        cout << "Could Not Open File2" << endl;

    vector<int> RXTX_state;
    vector<int> RXTX_node;
    string str1, str2, temp;

    getline(fin1, str1);
    stringstream ss1(str1);
    while (getline(ss1, temp, ','))
    {
        RXTX_state.push_back(stoi(temp));
    }
    getline(fin2, str2);
    stringstream ss2(str2);
    while (getline(ss2, temp, ','))
    {
        RXTX_node.push_back(stoi(temp));
    }
    fin1.close();
    fin2.close();

    for (int i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    {
        slottable_setup[i].state = RXTX_state[i];
        if (slottable_setup[i].state == DAATR_STATUS_TX && RXTX_node[i] != 0)
        {
            slottable_setup[i].send_or_recv_node = RXTX_node[i];
        }
        else if (slottable_setup[i].state == DAATR_STATUS_RX && RXTX_node[i] != 0)
        {
            slottable_setup[i].send_or_recv_node = RXTX_node[i];
        }
        else
        {
            slottable_setup[i].send_or_recv_node = 0;
        }
    }

    // cout << endl;
    // cout << "Node " << nodeId << " 将时隙表切换为建链时隙表 " << endl;
    // for (int i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    // {
    //     if (slottable_setup[i].state == DAATR_STATUS_TX &&
    //         slottable_setup[i].send_or_recv_node != 0)
    //     {
    //         cout << "|TX:" << slottable_setup[i].send_or_recv_node;
    //     }
    //     else if (slottable_setup[i].state == DAATR_STATUS_RX &&
    //              slottable_setup[i].send_or_recv_node != 0)
    //     {
    //         cout << "|RX:" << slottable_setup[i].send_or_recv_node;
    //     }
    //     else
    //         cout << "|IDLE";
    // }
    // cout << endl;
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
void MacDaatr::macToNetworkBufferHandle(void *data, uint8_t type, uint16_t len)
{
    // extern MacDaatr daatr_str; // mac层协议类
    extern ringBuffer macToRouting_Buffer;
    uint8_t *ret = NULL; // 返回值
    ret = (uint8_t *)malloc(len + 3);
    memset(ret, 0, sizeof(ret)); // 清零
    ret[0] = type;
    // memcpy((ret+1),&len,2); // 小端序
    // 大端序
    ret[1] |= ((len >> 8) & 0xff);
    ret[2] |= (len & 0xff);
    memcpy((ret + 3), data, len);
    // 写进Mac->Net缓冲区
    lock_Mac_to_Net.lock();
    macToRouting_Buffer.ringBuffer_put(ret, len + 3);
    lock_Mac_to_Net.unlock();
    free(ret);
}

/**
 * @brief net->MAC缓冲区数据处理函数
 *
 * @param rBuffer_mac net->MAC缓冲区取得的数据，注意，这只为一行数据，大小最大
 *
 * 该函数首先解析MAC缓冲区中的数据类型和长度字段，然后根据数据类型来处理相应的数据。
 * 目前支持的数据类型包括飞行状态信息、身份配置信息、链路构建需求、转发表和网络数据包。
 * 对于每种数据类型，函数会在switch语句中根据类型值进行分支，并在每个分支中提供一个处理逻辑的框架。
 *
 * 注意：当前函数实现中，各个分支内的处理逻辑尚未实现，需要根据具体需求进行补充。
 */
void MacDaatr::networkToMacBufferHandle(uint8_t *rBuffer_mac)
{
    // extern MacDaatr daatr_str;     // mac层协议类
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
        local_node_position_info = *temp;
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

void MacDaatr::macParameterInitialization(uint32_t idx)
{
    // extern MacDaatr daatr_str; // mac层协议类
    string filePath = "../config/daatr_config.txt";
    // 打开文件
    ifstream file(filePath);
    if (!file.is_open())
    {
        cerr << "!!!!无法打开文件: " << filePath << endl;
    }

    // 读取文件内容
    cout << "macInit " << idx << ": 从Config中读取配置信息" << endl;
    string line;
    while (getline(file, line))
    {
        istringstream iss(line); // 将读取的行放入字符串流中以便进一步分割
        vector<string> items;
        string item;
        while (getline(iss, item, ' '))
        {
            // 使用空格分割每一项
            items.push_back(item); // 将分割出的项存入vector中
        }
        string type = items[0];
        items.erase(items.begin()); // 删除数据类型
        if (type == "SUBNET")
        { // 子网号
            subnetId = stoi(items[0]);
        }
        else if (type == "NODEID")
        { // 节点号
            // nodeId = stoi(items[0]);
            nodeId = idx;
        }
        else if (type == "SUBNET_NODE_NUM")
        { // 子网节点数量
            subnet_node_num = stoi(items[0]);
        }
        else if (type == "IMPORTANT_NODE")
        {                                          // 子网重要节点
            mana_node = stoi(items[0]);            // n网管节点
            gateway_node = stoi(items[1]);         // 网关节点
            standby_gateway_node = stoi(items[2]); // 备份网关节点
        }
        else if (type == "NODE_TYPE")
        { // 节点身份
            if (items[0] == "ORDINARY")
            { // 普通节点
                node_type = Node_Ordinary;
            }
            else if (items[0] == "MANAGEMENT")
            { // 网管节点
                node_type = Node_Management;
            }
            else if (items[0] == "GATEWAY")
            { // 网关节点
                node_type = Node_Gateway;
            }
            else if (items[0] == "STANDBY_GATEWAY")
            { // 备份网关节点
                node_type = Node_Standby_Gateway;
            }
        }
        else if (type == "MANA_CHANEL_BUILD_STATE_SLOT_NODE_ID")
        { // 低频信道建链阶段初始化
            for (int i = 0; i < items.size(); i++)
            {
                low_freq_link_build_slot_table[i].nodeId = stoi(items[i]);
            }
        }
        else if (type == "MANA_CHANEL_BUILD_STATE_SLOT_STATE")
        { // 低频信道非建链阶段初始化
            for (int i = 0; i < items.size(); i++)
            {
                low_freq_link_build_slot_table[i].state = stoi(items[i]);
            }
        }
        else if (type == "MANA_CHANEL_OTHER_STATE_SLOT_NODE_ID")
        { // 低频信道非建链阶段初始化
            for (int i = 0; i < items.size(); i++)
            {
                low_freq_other_stage_slot_table[i].nodeId = stoi(items[i]);
            }
        }
        else if (type == "MANA_CHANEL_OTHER_STATE_SLOT_STATE")
        { // 低频信道非建链阶段初始化
            for (int i = 0; i < items.size(); i++)
            {
                low_freq_other_stage_slot_table[i].state = stoi(items[i]);
            }
        }
    }
    file.close();

    cout << "macInit " << nodeId << ": 配置 IP_Addr" << endl;
    // 配置ip地址，ip地址配置策略为'192.168.'+ 子网号 + 节点ID
    for (int i = 2; i <= subnet_node_num + 1; i++)
        in_subnet_id_to_ip[i - 1] = "192.168." + to_string(subnetId) + "." + to_string(i);

    cout << "macInit " << nodeId << ": 配置业务信道信息" << endl;
    need_change_state = 0;                // 不需要改变状态
    state_now = Mac_Initialization;       // 初始为建链阶段
    receivedChainBuildingRequest = false; // 初始未发送建链请求

    currentSlotId = 0;
    currentStatus = DAATR_STATUS_IDLE;
    LoadSlottable_setup();

    cout << "macInit " << nodeId << ": 配置网管信道信息" << endl;
    access_state = DAATR_NO_NEED_TO_ACCESS; // 默认不需要接入
    access_backoff_number = 0;              // 接入时隙
    low_slottable_should_read = 0;

    cout << endl;
}
