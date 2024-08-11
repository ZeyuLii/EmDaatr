#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>

#include "highFreqToolFunc.h"
#include "macdaatr.h"
#include "struct_converter.h"
#include "main.h"

using namespace std;

#define DEBUG_SETUP 1
#define DEBUG_EXECUTION 1

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

///  @brief 构造函数MacDaatr
//   该构造函数用于初始化MacDaatr类的实例。
//   它设置了节点ID和子网ID，同时初始化了时钟触发器和两个MAC DAATR套接字的文件描述符。
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
    string stlotable_state_filename = "./config/SlottableInitialization/Slottable_RXTX_State_" +
                                      to_string(static_cast<long long>(nodeId)) + ".txt";
    string stlotable_node_filename = "./config/SlottableInitialization/Slottable_RXTX_Node_" +
                                     to_string(static_cast<long long>(nodeId)) + ".txt";
    ifstream fin1(stlotable_state_filename);
    ifstream fin2(stlotable_node_filename);

    if (!fin1.is_open())
        cout << "Cannot Open Setup File1" << endl;

    if (!fin2.is_open())
        cout << "Cannot Open Setup File2" << endl;

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

    cout << endl;
    cout << "Node " << nodeId << " 读入建链时隙表 " << endl;
    for (int i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    {
        if (slottable_setup[i].state == DAATR_STATUS_TX &&
            slottable_setup[i].send_or_recv_node != 0)
        {
            cout << "|TX:" << slottable_setup[i].send_or_recv_node;
        }
        else if (slottable_setup[i].state == DAATR_STATUS_RX &&
                 slottable_setup[i].send_or_recv_node != 0)
        {
            cout << "|RX:" << slottable_setup[i].send_or_recv_node;
        }
        else
            cout << "|IDLE";
    }
    cout << endl;
}

/// @brief 读取执行阶段射前时隙表
void MacDaatr::LoadSlottable_Execution()
{
    string stlotable_state_filename = "./config/SlottableExecution/Slottable_RXTX_State_" +
                                      to_string(static_cast<long long>(nodeId)) + ".txt";
    string stlotable_node_filename = "./config/SlottableExecution/Slottable_RXTX_Node_" +
                                     to_string(static_cast<long long>(nodeId)) + ".txt";
    ifstream fin1(stlotable_state_filename);
    ifstream fin2(stlotable_node_filename);

    if (!fin1.is_open())
        cout << "Cannot Open Execution File1" << endl;

    if (!fin2.is_open())
        cout << "Cannot Open Execution File2" << endl;

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
        slottable_execution[i].state = RXTX_state[i];
        if (slottable_execution[i].state == DAATR_STATUS_TX && RXTX_node[i] != 0)
        {
            slottable_execution[i].send_or_recv_node = RXTX_node[i];
        }
        else if (slottable_execution[i].state == DAATR_STATUS_RX && RXTX_node[i] != 0)
        {
            slottable_execution[i].send_or_recv_node = RXTX_node[i];
        }
        else
        {
            slottable_execution[i].send_or_recv_node = 0;
        }
    }

    cout << endl;
    cout << "Node " << nodeId << " 读取射前时隙表 " << endl;
    for (int i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    {
        if (slottable_execution[i].state == DAATR_STATUS_TX &&
            slottable_execution[i].send_or_recv_node != 0)
        {
            cout << "|TX:" << slottable_execution[i].send_or_recv_node;
        }
        else if (slottable_execution[i].state == DAATR_STATUS_RX &&
                 slottable_execution[i].send_or_recv_node != 0)
        {
            cout << "|RX:" << slottable_execution[i].send_or_recv_node;
        }
        else
            cout << "|IDLE";
    }
    cout << endl;
}

/// @brief 读取调整阶段时隙表
void MacDaatr::LoadSlottable_Adjustment()
{
    // cout << "Node " << nodeId << " 读入建链阶段时隙表" << endl;
    string stlotable_state_filename = "./config/SlottableAdjustment/Slottable_RXTX_State_" +
                                      to_string(static_cast<long long>(nodeId)) + ".txt";
    string stlotable_node_filename = "./config/SlottableAdjustment/Slottable_RXTX_Node_" +
                                     to_string(static_cast<long long>(nodeId)) + ".txt";
    ifstream fin1(stlotable_state_filename);
    ifstream fin2(stlotable_node_filename);

    if (!fin1.is_open())
        cout << "Cannot Open Adjustment File1" << endl;

    if (!fin2.is_open())
        cout << "Cannot Open Adjustment File2" << endl;

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
        slottable_adjustment[i].state = RXTX_state[i];
        if (slottable_adjustment[i].state == DAATR_STATUS_TX && RXTX_node[i] != 0)
        {
            slottable_adjustment[i].send_or_recv_node = RXTX_node[i];
        }
        else if (slottable_adjustment[i].state == DAATR_STATUS_RX && RXTX_node[i] != 0)
        {
            slottable_adjustment[i].send_or_recv_node = RXTX_node[i];
        }
        else
        {
            slottable_adjustment[i].send_or_recv_node = 0;
        }
    }

    // cout << endl;
    // cout << "Node " << nodeId << " 将时隙表切换为建链时隙表 " << endl;
    // for (int i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    // {
    //     if (slottable_adjustment[i].state == DAATR_STATUS_TX &&
    //         slottable_adjustment[i].send_or_recv_node != 0)
    //     {
    //         cout << "|TX:" << slottable_adjustment[i].send_or_recv_node;
    //     }
    //     else if (slottable_adjustment[i].state == DAATR_STATUS_RX &&
    //              slottable_adjustment[i].send_or_recv_node != 0)
    //     {
    //         cout << "|RX:" << slottable_adjustment[i].send_or_recv_node;
    //     }
    //     else
    //         cout << "|IDLE";
    // }
    // cout << endl;
}

/// @brief 该函数由网络层在有业务要发送时进行调用,
//         实现向业务信道队列和网管信道队列内添加待发送业务的目标
bool MacDaatr::MAC_NetworkLayerHasPacketToSend(msgFromControl *busin)
{
    /*
        1.检查busin的接收节点和优先级
        2.检查此时该节点的优先级队列是否已满
        3.(若满, 则将其放于下一优先级队列的top)
        4.(若未满, 则将其放于本优先级队列的last)
        5.更新此时的优先级 & waiting_time
    */
    int i, new_loc, loc;
    int flag = false; // 表征是否成功将busin插入业务队列
    int next_hop_node;

    lock_buss_channel.lock();
    if ((busin->packetLength == 73 && busin->msgType == 15 && busin->fragmentNum == 15))
    {
        // 如果是链路层自己的数据包, 那么不需要经过转发表
        loc = getAvailableLocationOfQueue(this, busin->destAddr, busin->priority);
    }
    else
    {
        // 如果是网络层数据包, 那么需要查看路由表对应节点的队列是否已满
        next_hop_node = searchNextHopAddr(this, busin->destAddr);

        if (busin->msgType == 2)
            next_hop_node = 0;

        if (next_hop_node == 0) // 即转发节点为 0 的情况
        {
            if (busin->msgType != 2) // 一般数据包
            {
                cout << "此时数据包的接收节点 " << busin->destAddr << " 不存在转发节点, 丢弃业务" << endl;
                return false;
            }
            else // type = 2 的探测包
            {
                loc = getAvailableLocationOfQueue(this, busin->destAddr, busin->priority);
            }
        }
        else
        {
            loc = getAvailableLocationOfQueue(this, next_hop_node, busin->priority);
            cout << "节点 " << nodeId << " 收到目的地址为: "
                 << busin->destAddr << " 的数据包, 下一跳转发节点为 " << next_hop_node << endl;
        }
    }

    if (loc == TRAFFIC_MAX_BUSINESS_NUMBER) // 这说明队列全满
    {
        printf("Node %d 的优先级队列 %d 已满\n", busin->destAddr, busin->priority);

        if ((busin->packetLength == 73 && busin->msgType == 15 && busin->fragmentNum == 15))
        // 这是频谱感知结果数据包
        {
            // 在此处, 说明优先级0的队列已经全满, 只能丢弃最后一个业务,
            // 腾出第一个位置来放置频谱感知结果 loc - 1 = 59, 将 1-59 的业务后移一位
            Insert_MFC_to_Queue(this, busin, busin->priority, loc - 1, 0, 0);
        }
        else if (busin->msgType == 2)
        {
            for (i = busin->priority + 1; i < TRAFFIC_CHANNEL_PRIORITY; i++)
            {
                new_loc = getAvailableLocationOfQueue(this, busin->destAddr, i);
                if (new_loc != TRAFFIC_MAX_BUSINESS_NUMBER)
                {
                    Insert_MFC_to_Queue(this, busin, i, new_loc, 0, 0); // 不查看路由表
                    flag = true;
                    break;
                }
                printf("Node %d 的优先级队列 %d 已满\n", i, busin->priority);
            }
        }
        else
        {
            // 需要将busin加到busin->priority + 1或更低优先级的队列中去的首位
            // 依次检索之后每个优先级的业务队列
            for (i = busin->priority + 1; i < TRAFFIC_CHANNEL_PRIORITY; i++)
            {
                new_loc = getAvailableLocationOfQueue(this, searchNextHopAddr(this, busin->destAddr), i);
                if (new_loc != TRAFFIC_MAX_BUSINESS_NUMBER)
                {
                    Insert_MFC_to_Queue(this, busin, i, new_loc, 0, 1);
                    flag = true;
                    break;
                }
                printf("Node %d 的优先级队列 %d 已满\n", i, busin->priority);
            }
        }
    }
    else
    {
        if (busin->packetLength == 73 && busin->msgType == 15 && busin->fragmentNum == 15)
            // 这是频谱感知结果数据包
            Insert_MFC_to_Queue(this, busin, busin->priority, loc, 0, 0);

        else if (busin->msgType == 2)
            Insert_MFC_to_Queue(this, busin, busin->priority, loc, 1, 0);

        else
            Insert_MFC_to_Queue(this, busin, busin->priority, loc, 1, 1);

        flag = true;
    }
    cout << "插入成功" << endl;
    lock_buss_channel.unlock();

    return flag;
}

/// @brief 对收到的mfcTemp进行处理
void MacDaatr::processPktFromNet(msgFromControl *MFC_temp)
{
    if (MFC_temp->msgType == 1)
        MacDaatNetworkHasLowFreqChannelPacketToSend(MFC_temp);
    else
    {
        uint8_t temp_destAddr = MFC_temp->destAddr;
        uint8_t temp_sourceAddr = MFC_temp->srcAddr;

        if (MFC_temp->backup == 0)
        {
            if (MFC_temp->msgType == 3 && Forwarding_Table[MFC_temp->destAddr - 1][1] == 0)
                cout << nodeId << "->" << MFC_temp->destAddr
                     << " 无高速链路, 丢弃数据包 (msgType = 3)" << endl;

            MAC_NetworkLayerHasPacketToSend(MFC_temp);
            // cout << "MAC 将数据包放入队列. " << endl;
        }
        else
            cout << "收到发给MAC的控制信息! " << endl;
    }
}

/// @brief 对收到的身份配置信息进行处理
void MacDaatr::processNodeNotificationFromNet(NodeNotification *temp)
{
    if (state_now == Mac_Execution)
    {
        switch (temp->nodeResponsibility)
        {
        case NODENORMAL:
        {
            node_type = Node_Ordinary;
            break;
        }
        case INTRAGROUPCONTROL:
        {
            node_type = Node_Management;
            break;
        }
        case INTERGROUPGATEWAY:
        {
            node_type = Node_Gateway;
            break;
        }
        case ISOLATION:
        { // 代表此节点脱网
            state_now = Mac_Access;
            access_state = DAATR_NEED_ACCESS;

            node_type = Node_Ordinary; // 普通节点
        }
        }

        if (mana_node != temp->intragroupcontrolNodeId)
        { // 如果网管节点发生变化,
            // 则需要修改网管信道时隙表
            int mana_node_pre = mana_node;
            int mana_node_now = temp->intragroupcontrolNodeId;
            if (nodeId == mana_node_pre)
            {
                // TODO 更改检查进入频域调整阶段的定时器
                // MESSAGE_CancelSelfMsg(node, macdata_daatr->ptr_to_period_judge_if_enter_adjustment);
            }
            else if (nodeId == mana_node_now)
            {
                // TODO 更改检查进入频域调整阶段的定时器
                // Message *send_msg = MESSAGE_Alloc(node, MAC_LAYER, MAC_PROTOCOL_DAATR, MAC_DAATR_Mana_Judge_Enter_Freq_Adjustment_Stage);
                // MESSAGE_Send(node, send_msg, SPEC_SENSING_PERIOD * MILLI_SECOND);
                // macdata_daatr->ptr_to_period_judge_if_enter_adjustment = send_msg;
            }

            low_freq_other_stage_slot_table[0].nodeId = mana_node_now;
            low_freq_other_stage_slot_table[26].nodeId = mana_node_now; // 若网管信道时隙表更改，此处更改(25)!!!!!!!!!
            low_freq_other_stage_slot_table[9].nodeId = mana_node_now;
            low_freq_other_stage_slot_table[21].nodeId = mana_node_now;
            low_freq_other_stage_slot_table[34].nodeId = mana_node_now;
            low_freq_other_stage_slot_table[46].nodeId = mana_node_now;
        }

        mana_node = temp->intragroupcontrolNodeId;
        gateway_node = temp->intergroupgatewayNodeId;
        standby_gateway_node = temp->reserveintergroupgatewayNodeId;
        cout << "Node " << nodeId << " 新身份是 " << (int)temp->nodeResponsibility << endl;
    }
    else if (state_now == Mac_Initialization)
    {
        cout << "当前子网处于建链阶段, 不接受节点身份配置信息" << endl;
    }
    else
    {
        cout << "当前子网处于调整阶段, 不接受节点身份配置信息" << endl;
    }
}

/// @brief 对收到的链路构建需求进行处理
/// @param LinkAssignment_data 链路构建需求指针
/// @param linkNum 链路构建需求数量
void MacDaatr::processLinkAssignmentFromNet(LinkAssignment *LinkAssignment_data, int linkNum)
{
    vector<LinkAssignment_single> LinkAssignment_Storage;
    // 将网络层发来的若干Linkassignment结构体(数组)分解为单个链路构建需求结构体
    // LinkAssignment_single_temp 并将其存储进 LinkAssignment_Storage 内

    for (int i = 0; i < linkNum; i++)
    {
        for (int j = 0; j < (*(LinkAssignment_data + i)).listNumber; j++)
        {
            LinkAssignment_single LinkAssignment_single_temp;
            LinkAssignment_single_temp.begin_node = (*(LinkAssignment_data + i)).begin_node;
            LinkAssignment_single_temp.end_node = (*(LinkAssignment_data + i)).end_node;
            LinkAssignment_single_temp.type = (*(LinkAssignment_data + i)).type[j];
            LinkAssignment_single_temp.priority = (*(LinkAssignment_data + i)).priority[j];
            LinkAssignment_single_temp.size = (*(LinkAssignment_data + i)).size[j];
            LinkAssignment_single_temp.QOS = (*(LinkAssignment_data + i)).QOS[j];
            LinkAssignment_single_temp.frequency = (*(LinkAssignment_data + i)).frequency[j];

            int LA_num = 1;
            // cout << "number of LA_temp is " << LA_num << endl;
            for (int k = 0; k < LA_num; k++)
            {
                if (LinkAssignment_single_temp.size != 0)
                    LinkAssignment_Storage.push_back(LinkAssignment_single_temp);
            }
        }
    }

    sort(LinkAssignment_Storage.begin(), LinkAssignment_Storage.end(), Compare_by_Priority);
    // Alloc_slot_traffic *Alloc_slottable_traffic = new Alloc_slot_traffic;
    vector<Alloc_slot_traffic> allocSlottableTraffic(TRAFFIC_SLOT_NUMBER);
    ReAllocate_Traffic_slottable(this, allocSlottableTraffic, LinkAssignment_Storage);

    // 下面需要将分配表转换为各节点对应的时隙表
    for (int i = 0; i < SUBNET_NODE_NUMBER_MAX; i++)
    {
        for (int j = 0; j < TRAFFIC_SLOT_NUMBER; j++)
            slot_traffic_execution_new[i][j].state = 0; // 本应全置为IDLE
    }

    for (int i = 0; i < SUBNET_NODE_NUMBER_MAX; i++)
    {
        for (int j = 0; j < TRAFFIC_SLOT_NUMBER; j++)
        {
            if (allocSlottableTraffic[j].multiplexing_num != FREQUENCY_DIVISION_MULTIPLEXING_NUMBER)
            {
                for (int k = 0; k < FREQUENCY_DIVISION_MULTIPLEXING_NUMBER - allocSlottableTraffic[j].multiplexing_num; k++)
                {
                    // Alloc_slottable_traffic[j].send_node 是 网管节点的分配表的第 j 个时隙的发送节点序列
                    // send[k] 是其中的第 k 组链路需求的发送节点
                    // 需要找到 20 个时隙表中的第 senk[k] 个,
                    // 然后将其第[j]个时隙的state和send进行修改 修改为 TX , 并将 send 指向 recv 节点
                    slot_traffic_execution_new[allocSlottableTraffic[j].send_node[k] - 1][j].state = DAATR_STATUS_TX;
                    slot_traffic_execution_new[allocSlottableTraffic[j].send_node[k] - 1][j].send_or_recv_node =
                        allocSlottableTraffic[j].recv_node[k];

                    slot_traffic_execution_new[allocSlottableTraffic[j].recv_node[k] - 1][j].state = DAATR_STATUS_RX;
                    slot_traffic_execution_new[allocSlottableTraffic[j].recv_node[k] - 1][j].send_or_recv_node =
                        allocSlottableTraffic[j].send_node[k];
                }
            }
        }
    }
}

/**
 * @brief buffer缓冲区写函数，将输入结构体转换为缓冲区帧并插入
 *
 * @param data 数据
 * @param type 数据类型
 * @param len 数据长度
 * @return 返回一个指向转换后缓冲区的指针
 *
 * 该函数首先对输入数据进行设定帧格式转换
 * 具体的转换流程是，对数据添加帧头，第一位为数据类型，第二、三位为数据长度（采用大端），第四位开始为数据
 * 然后将转换后的数据插入缓冲区
 *
 * 0x0C表示其它节点飞行状态信息 12 |
 * 0x0D表示本地链路状态信息 13 |
 * 0x10表示网络数据包（业务+信令）16 |
 * 0x13表示网管信道传输参数 unused |
 * 0x14表示业务信道传输参数 unused |
 * 0X17表示链路运行状态 ? |
 * 0x18表示身份配置信息发送指示 ?
 */
void MacDaatr::macToNetworkBufferHandle(void *data, uint8_t type, uint16_t len)
{
    extern ringBuffer macToRouting_Buffer;
    uint8_t *ret = NULL; // 返回值
    ret = (uint8_t *)malloc(len + 3);
    memset(ret, 0, sizeof(ret)); // 清零
    ret[0] = type;
    // memcpy((ret + 1), &len, 2); // 小端序
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
 *
 * 0x01表示本地飞行状态信息 |
 * 0x0A表示身份配置信息 |
 * 0x0B表示链路构建要求 |
 * 0x0E表示转发表信息 |
 * 0x0F表示拓扑信息 |
 * 0x10表示网络数据包（业务+信令 |
 */
void MacDaatr::networkToMacBufferHandle(uint8_t *rBuffer_mac)
{
    uint8_t type = rBuffer_mac[0]; // 数据类型
    uint16_t len = 0;              // 数据长度（单位：字节）
    len |= rBuffer_mac[1];
    len <<= 8;
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
    { // 身份配置信息 MSG_MAC_POOL_ReceiveResponsibilityConfiguration
        NodeNotification *temp = (NodeNotification *)data;
        processNodeNotificationFromNet(temp);
        break;
    }
    case 0x0b:
    { // 链路构建需求 MAC_DAATR_ReceiveTaskInstruct
        cout << "[调整阶段] Node " << nodeId << " 收到链路构建需求  ";
        printTime_ms();

        // TODO : need_change_state修改
        if (need_change_state == 0 &&
            state_now != Mac_Adjustment_Slot &&
            state_now != Mac_Adjustment_Frequency)
        {
            cout << "[调整阶段] 子网各节点即将进入时隙调整阶段!" << endl;
            need_change_state = 1; // 即将进入时隙调整阶段
        }
        else if (need_change_state == 2)
        {
            cout << "[调整阶段] 首先进入频率调整阶段, 在时隙调整阶段结束后进入时隙调整阶段! " << endl;
            need_change_state = 4;
        }

        LinkAssignmentHeader *linkAssignmentHeader = (LinkAssignmentHeader *)data;
        int linkNum = linkAssignmentHeader->linkNum;
        LinkAssignment *LinkAssignment_data = (LinkAssignment *)(linkAssignmentHeader + 1);
        processLinkAssignmentFromNet(LinkAssignment_data, linkNum);
        break;
    }
    case 0x0e:
    { // 转发表 MAC_DAATR_ReceiveForwardingTable
        cout << "Node " << nodeId << " 收到路由表";
        printTime_ms();
        Layer2_ForwardingTable_Msg *routingTable = (Layer2_ForwardingTable_Msg *)data;
        vector<vector<uint16_t>> *recvForwardingTable = routingTable->Layer2_FT_Ptr;

        if (state_now == Mac_Execution)
        {
            for (int i = 1; i <= subnet_node_num; i++)
                Forwarding_Table[i - 1][1] = 0;

            for (auto RT : *recvForwardingTable) // RT : [dest_node , next_hop]
                Forwarding_Table[RT[0] - 1][1] = RT[1];
        }
        break;
    }
    case 0x10:
    { // 网络数据包（业务+信令） MSG_NETWORK_ReceiveNetworkPkt
        vector<uint8_t> curPktPayload(data, data + len);
        msgFromControl *MFC_temp = new msgFromControl();
        MFC_temp = convert_01_MFCPtr(&curPktPayload); // 解析数组, 还原为包结构体类型

        processPktFromNet(MFC_temp);
        delete MFC_temp;
        break;
    }
    }
}

void MacDaatr::macParameterInitialization(uint32_t idx)
{
    cout << endl;
    cout << "==========================================" << endl;
    cout << "===== Node " << idx << " macInitialization Start =====" << endl;
    string filePath = "./config/daatr_config_" + to_string(static_cast<long long>(idx)) + ".txt";
    // 打开文件
    ifstream file(filePath);
    if (!file.is_open())
        cerr << "!!!!无法打开文件: " << filePath << endl;

// 读取文件内容
#ifdef DEBUG_SETUP
    cout << "macInit " << idx << ": 从Config中读取配置信息" << endl;
#endif
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

#ifdef DEBUG_SETUP
    cout << "macInit " << nodeId << ": 配置 IP_Addr" << endl;
#endif
    // 配置ip地址，ip地址配置策略为'192.168.'+ 子网号 + 节点ID
    for (int i = 2; i <= subnet_node_num + 1; i++)
        in_subnet_id_to_ip[i - 1] = "192.168." + to_string(subnetId) + "." + to_string(i);

#ifdef DEBUG_SETUP
    cout << "macInit " << nodeId << ": 配置业务信道信息" << endl;
#endif
    need_change_state = 0;                // 不需要改变状态
    state_now = Mac_Initialization;       // 初始为建链阶段
    receivedChainBuildingRequest = false; // 初始未发送建链请求

    currentSlotId = 0;
    currentStatus = DAATR_STATUS_IDLE;

    LoadSlottable_setup();
    generateSlottableExecution(this);
    LoadSlottable_Execution();
    LoadSlottable_Adjustment();

    // 初始化子网内节点路由表
    for (int i = 1; i <= subnet_node_num; i++)
    {
        Forwarding_Table[i - 1][0] = i; // 路由表全设置为自己节点
        Forwarding_Table[i - 1][1] = i;
    }
    // 对业务信道队列进行初始化
    for (int k = 0; k < SUBNET_NODE_NUMBER_MAX; k++)
    {
        for (int i = 0; i < TRAFFIC_CHANNEL_PRIORITY; i++)
        {
            for (int j = 0; j < TRAFFIC_MAX_BUSINESS_NUMBER; j++)
            {
                businessQueue[k].business[i][j].priority = -1;
            }
        }
    }

#ifdef DEBUG_SETUP
    cout << "macInit " << nodeId << ": 配置网管信道信息" << endl;
#endif
    access_state = DAATR_NO_NEED_TO_ACCESS; // 默认不需要接入
    access_backoff_number = 0;              // 接入时隙
    low_slottable_should_read = 0;

#ifdef DEBUG_SETUP
    cout << "macInit " << nodeId << ": 配置频域信息" << endl;
#endif
    // MacDaatrInitialize_Frequency_Seqence(this);

    cout << "===== Node " << nodeId << " macInitialization Completed =====" << endl;
    cout << "==============================================" << endl;
    cout << endl;
    cout << endl;

    // 测试收发包
    if (nodeId == 2)
    {
        // 测试数据
        local_node_position_info.positionX = 39.276;
        local_node_position_info.positionY = 116.627;
        local_node_position_info.positionZ = 3.37976;
        // 测试收发包
        msgFromControl *MFC_temp = new msgFromControl;
        MFC_temp->priority = 0;
        MFC_temp->backup = 0;
        MFC_temp->msgType = 3;
        MFC_temp->packetLength = 10;
        MFC_temp->srcAddr = nodeId;
        MFC_temp->destAddr = 1;
        MFC_temp->repeat = 0;
        MFC_temp->fragmentNum = 1;

        // MAC_NetworkLayerHasPacketToSend(MFC_temp);
        delete MFC_temp;

        // 测试收发包
        msgFromControl *MFC_temp2 = new msgFromControl;
        MFC_temp2->priority = 1;
        MFC_temp2->backup = 0;
        MFC_temp2->msgType = 3;
        MFC_temp2->packetLength = 10;
        MFC_temp2->srcAddr = nodeId;
        MFC_temp2->destAddr = 3;
        MFC_temp2->repeat = 0;
        MFC_temp->fragmentNum = 2;

        // MAC_NetworkLayerHasPacketToSend(MFC_temp2);
        delete MFC_temp2;
    }

    if (nodeId == 1)
    {
        // 测试数据
        local_node_position_info.positionX = 39.976;
        local_node_position_info.positionY = 116.227;
        local_node_position_info.positionZ = 3.35976;
        // 测试收发包
        msgFromControl *MFC_temp = new msgFromControl;
        MFC_temp->priority = 0;
        MFC_temp->backup = 0;
        MFC_temp->msgType = 3;
        MFC_temp->packetLength = 10;
        MFC_temp->srcAddr = nodeId;
        MFC_temp->destAddr = 2;
        MFC_temp->repeat = 0;
        MFC_temp->fragmentNum = 3;

        // MAC_NetworkLayerHasPacketToSend(MFC_temp);
        delete MFC_temp;

        // 测试收发包
        msgFromControl *MFC_temp2 = new msgFromControl;
        MFC_temp2->priority = 1;
        MFC_temp2->backup = 0;
        MFC_temp2->msgType = 3;
        MFC_temp2->packetLength = 10;
        MFC_temp2->srcAddr = nodeId;
        MFC_temp2->destAddr = 3;
        MFC_temp2->repeat = 0;
        MFC_temp->fragmentNum = 4;

        // MAC_NetworkLayerHasPacketToSend(MFC_temp2);
        delete MFC_temp2;
    }

    if (nodeId == 3)
    {
        // 测试数据
        local_node_position_info.positionX = 39.976;
        local_node_position_info.positionY = 116;
        local_node_position_info.positionZ = 3.3592;
        // 测试收发包
        msgFromControl *MFC_temp = new msgFromControl;
        MFC_temp->priority = 0;
        MFC_temp->backup = 0;
        MFC_temp->msgType = 3;
        MFC_temp->packetLength = 10;
        MFC_temp->srcAddr = nodeId;
        MFC_temp->destAddr = 1;
        MFC_temp->repeat = 0;
        MFC_temp->fragmentNum = 5;

        // MAC_NetworkLayerHasPacketToSend(MFC_temp);
        delete MFC_temp;

        // 测试收发包
        msgFromControl *MFC_temp2 = new msgFromControl;
        MFC_temp2->priority = 1;
        MFC_temp2->backup = 0;
        MFC_temp2->msgType = 3;
        MFC_temp2->packetLength = 10;
        MFC_temp2->srcAddr = nodeId;
        MFC_temp2->destAddr = 2;
        MFC_temp2->repeat = 0;
        MFC_temp->fragmentNum = 6;

        // MAC_NetworkLayerHasPacketToSend(MFC_temp2);
        delete MFC_temp2;
    }
}
