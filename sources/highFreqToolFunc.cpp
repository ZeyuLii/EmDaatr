#include "highFreqToolFunc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream> // std::stringstream ss;

using namespace std;

// 根据输入的两节点距离计算对应的传输速率 kbps
double Calculate_Transmission_Rate_by_Distance(double distance)
{
    double tran_speed = 0;
    if (distance <= 50)
        tran_speed = 11800;

    else if (distance > 50 && distance <= 100)
        tran_speed = 4096;

    else if (distance > 100 && distance <= 200)
        tran_speed = 2048;

    else if (distance > 200 && distance <= 300)
        tran_speed = 1024;

    else if (distance > 300 && distance <= 500)
        tran_speed = 512;

    else if (distance > 500 && distance <= 600)
        tran_speed = 256;

    else if (distance > 600 && distance <= 700)
        tran_speed = 128;

    else if (distance > 700 && distance <= 800)
        tran_speed = 64;

    else if (distance > 800 && distance <= 900)
        tran_speed = 32;

    else if (distance > 900 && distance <= 1200)
        tran_speed = 16;

    else if (distance > 1200 && distance <= 1500)
        tran_speed = 8;
    // return tran_speed;
    return tran_speed / 3; // 抑制发送
}

// 若待发送队列中不包含重传包, 则返回false, 反之返回true
bool judgeReTrans(const msgFromControl &MFC_temp, const vector<msgFromControl> &MFC_list_temp)
{
    if (MFC_list_temp.size() == 0 || MFC_temp.repeat == 0)
        return false;

    for (auto mfc_in_list : MFC_list_temp)
    {
        if (mfc_in_list.packetLength == MFC_temp.packetLength &&
            mfc_in_list.seq == MFC_temp.seq &&
            mfc_in_list.msgType == MFC_temp.msgType &&
            mfc_in_list.priority == MFC_temp.priority &&
            mfc_in_list.fragmentNum == MFC_temp.fragmentNum &&
            mfc_in_list.fragmentSeq == MFC_temp.fragmentSeq)
        {
            return true;
        }
    }
    return false;
}

/// @brief 将业务信道的业务队列前移一格(第一个业务发送完成, 消除其队列)
/// @param prio:      优先级对应的需要前移的队列 (0--(TRAFFIC_CHANNEL_PRIORITY-1))
/// @param recv:      某接收节点对应的优先级
/// @param dest_node: 对应的节点(1-20), 所以需要减一
/// @param location:  位置(0-(TRAFFIC_MAX_BUSINESS_NUMBER-1))
void Business_Lead_Forward(MacDaatr *macdata_daatr, int dest_node, int prio, int location)
{
    int i = location;
    while (macdata_daatr->businessQueue[dest_node - 1].business[prio][i + 1].priority != -1 &&
           i < TRAFFIC_MAX_BUSINESS_NUMBER - 1)
    {
        macdata_daatr->businessQueue[dest_node - 1].business[prio][i] =
            macdata_daatr->businessQueue[dest_node - 1].business[prio][i + 1];
        // 对应优先级队列往前移一位, 同时也完成了指定业务的移除
        i++;
    }
    macdata_daatr->businessQueue[dest_node - 1].business[prio][i].priority = -1; // 队列最后一个业务置空(优先级调为-1)
}

void MFC_Priority_Adjustment(MacDaatr *macdata_daatr)
{
    int i, j, k;
    double max_waiting_slotnum = floor(MAX_WAITING_TIME / (TRAFFIC_SLOT_NUMBER * HIGH_FREQ_SLOT_LEN));
    for (i = 1; i <= SUBNET_NODE_NUMBER_MAX; i++)
    {
        for (j = 1; j < TRAFFIC_CHANNEL_PRIORITY; j++) // 对于已经处在最高优先级的业务不做处理
        {
            for (k = 0; k < TRAFFIC_MAX_BUSINESS_NUMBER; k++)
            {
                if (macdata_daatr->businessQueue[i].business[j][k].waiting_time > max_waiting_slotnum)
                {
                    Business_Lead_Forward(macdata_daatr, i, j, k);
                }
            }
        }
    }
}

/// @brief 查询当前业务信道对应节点优先级队列在第几个后空
/// @param node: 对应的节点(1-20)
/// @param pri:  优先级对应的需要前移的队列(0--(TRAFFIC_CHANNEL_PRIORITY-1))
static int SearchEmptyLocationOfQueue(MacDaatr *macdata_daatr, int dest_node, int pri)
{
    int i;
    for (i = 0; i < TRAFFIC_MAX_BUSINESS_NUMBER; i++)
    {
        if (macdata_daatr->businessQueue[dest_node - 1].business[pri][i].priority == -1)
            // 当前i对应的位置即为空
            break;
    }
    return i;
}

/// @brief 对第 pri 个优先级队列进行检查, 找到队列中非空的第一个位置 loc 并返回
/// @param dest_node 1 - SUBNET_NODE_NUM
/// @param dest_node -1 时表示检查网管信道队列
int getAvailableLocationOfQueue(MacDaatr *macdata_daatr, int dest_node, int pri)
{
    int loc = 0;
    if (dest_node != -1)
    {
        while (loc <= TRAFFIC_MAX_BUSINESS_NUMBER - 1)
        {
            // 从前向后检查接收节点的业务信道的业务量, 找到第一个空位置 loc
            if ((macdata_daatr->businessQueue[dest_node - 1].business[pri][loc].priority == -1))
                break;

            loc++;
        }
    }
    return loc;
}

// destAddr: 目的节点
int searchNextHopAddr(MacDaatr *macdata_daatr, int destAddr) { return int(macdata_daatr->Forwarding_Table[destAddr - 1][1]); }

/// @brief 将busin的详细信息添加进业务信道表的具体位置 (优先级为pri的队列的第loc个位置)中
/// @param temp 代表是否向该队列的第一个位置处添加上述信息
/// @param temp  1: 本队列未满, 在原有优先级队列的末尾处添加即可
/// @param temp  0: 原本的优先级队列已满, 在更低优先级队列的第一个业务处添加busin
/// @param my_pkt_flag = 0 : 表示这是链路层自己的数据包, 不需要查看路由表
void Insert_MFC_to_Queue(MacDaatr *macdata_daatr, msgFromControl *busin,
                         int pri, int loc, int temp, int my_pkt_flag)
{
    int i = 0;
    if (my_pkt_flag == 0)
    {
        if (temp)
        {
            macdata_daatr->businessQueue[busin->destAddr - 1].business[pri][loc].priority = pri;
            macdata_daatr->businessQueue[busin->destAddr - 1].business[pri][loc].data_kbyte_sum = busin->packetLength;
            macdata_daatr->businessQueue[busin->destAddr - 1].business[pri][loc].waiting_time = 0;
            macdata_daatr->businessQueue[busin->destAddr - 1].business[pri][loc].busin_data = *busin;
            macdata_daatr->businessQueue[busin->destAddr - 1].recv_node = busin->destAddr;
            macdata_daatr->businessQueue[busin->destAddr - 1].state = 0;
        }
        else
        {
            // 将原本优先级队列的业务后移一位
            for (i = loc; i > 0; i--)
                macdata_daatr->businessQueue[busin->destAddr - 1].business[pri][i] = macdata_daatr->businessQueue[busin->destAddr - 1].business[pri][i - 1];

            macdata_daatr->businessQueue[busin->destAddr - 1].business[pri][0].priority = pri;
            macdata_daatr->businessQueue[busin->destAddr - 1].business[pri][0].data_kbyte_sum = busin->packetLength;
            macdata_daatr->businessQueue[busin->destAddr - 1].business[pri][0].waiting_time = 0;
            macdata_daatr->businessQueue[busin->destAddr - 1].business[pri][0].busin_data = *busin;
            macdata_daatr->businessQueue[busin->destAddr - 1].recv_node = busin->destAddr;
            macdata_daatr->businessQueue[busin->destAddr - 1].state = 0;
        }
    }
    else // 这说明这是需要查看路由表后再插入队列的数据包
    {
        int next_hop_addr = searchNextHopAddr(macdata_daatr, busin->destAddr);
        if (temp)
        {
            macdata_daatr->businessQueue[next_hop_addr - 1].business[pri][loc].priority = pri;
            macdata_daatr->businessQueue[next_hop_addr - 1].business[pri][loc].data_kbyte_sum = busin->packetLength;
            macdata_daatr->businessQueue[next_hop_addr - 1].business[pri][loc].waiting_time = 0;
            macdata_daatr->businessQueue[next_hop_addr - 1].business[pri][loc].busin_data = *busin;
            macdata_daatr->businessQueue[next_hop_addr - 1].recv_node = next_hop_addr;
            macdata_daatr->businessQueue[next_hop_addr - 1].state = 0;
        }
        else
        {
            // 将原本优先级队列的业务后移一位
            for (i = loc; i > 0; i--)
                macdata_daatr->businessQueue[next_hop_addr - 1].business[pri][i] = macdata_daatr->businessQueue[next_hop_addr - 1].business[pri][i - 1];

            macdata_daatr->businessQueue[next_hop_addr - 1].business[pri][0].priority = pri;
            macdata_daatr->businessQueue[next_hop_addr - 1].business[pri][0].data_kbyte_sum = busin->packetLength;
            macdata_daatr->businessQueue[next_hop_addr - 1].business[pri][0].waiting_time = 0;
            macdata_daatr->businessQueue[next_hop_addr - 1].business[pri][0].busin_data = *busin;
            macdata_daatr->businessQueue[next_hop_addr - 1].recv_node = next_hop_addr;
            macdata_daatr->businessQueue[next_hop_addr - 1].state = 0;
        }
    }
}

bool Compare_by_Priority(const LinkAssignment_single LA1, const LinkAssignment_single LA2) { return LA1.priority <= LA2.priority; }

static int Rand(int i) { return rand() % i; }

// 判断链路的收发节点是否包含网关节点(网关节点ID为2)
// 包含返回true
static bool Judge_If_Include_Gateway(MacDaatr *macdata_daatr,
                                     vector<LinkAssignment_single>::iterator LAtemp)
{
    return (LAtemp->begin_node == macdata_daatr->gateway_node ||
            LAtemp->end_node == macdata_daatr->gateway_node ||
            LAtemp->begin_node == macdata_daatr->standby_gateway_node ||
            LAtemp->end_node == macdata_daatr->standby_gateway_node);
}

// 判断这个时隙序列是否可以提供给这个节点(主要用于频分复用, 在频分复用中,
// 节点号都只能在收发节点中出现一次) 传入的参数为: 待编排的业务; 时隙表;
// 业务的待分配时隙的长度;
static bool Judge_If_Choose_Slot(vector<LinkAssignment_single>::iterator LinkAssignment_single_temp,
                                 Alloc_slot_traffic Alloc_slottable_traffic[],
                                 int slot_length)
{
    for (int i = 0; i < slot_length; i++)
    {
        if (Alloc_slottable_traffic[i].multiplexing_num == 0)
            return false;

        for (int j = 0; j < FREQUENCY_DIVISION_MULTIPLEXING_NUMBER - Alloc_slottable_traffic[i].multiplexing_num; j++)
        // 计算当前时隙已经进行多少个复用
        {
            if (LinkAssignment_single_temp->begin_node == Alloc_slottable_traffic[i].send_node[j] ||
                LinkAssignment_single_temp->end_node == Alloc_slottable_traffic[i].recv_node[j] ||
                LinkAssignment_single_temp->begin_node == Alloc_slottable_traffic[i].recv_node[j] ||
                LinkAssignment_single_temp->end_node == Alloc_slottable_traffic[i].send_node[j])
            {
                // 如果时隙内已有此发送或者接收节点, 则返回true, 不能分配此时隙
                return false;
            }
        }
    }
    return true;
}

// 根据LA和节点间距离计算链路构建需求所需的时隙数 data_slot_per_frame
static void Calculate_slotnum_per_frame(MacDaatr *macdata_daatr,
                                        // LinkAssignment_single *LinkAssignment_single_temp,
                                        vector<LinkAssignment_single>::iterator LinkAssignment_single_temp)
{
    int i, node1, node2;
    int temp_posiX1, temp_posiY1, temp_posiZ1;
    int temp_posiX2, temp_posiY2, temp_posiZ2;

    if (LinkAssignment_single_temp->begin_node == macdata_daatr->nodeId)
    {
        temp_posiX1 = macdata_daatr->local_node_position_info.positionX;
        temp_posiY1 = macdata_daatr->local_node_position_info.positionY;
        temp_posiZ1 = macdata_daatr->local_node_position_info.positionZ;

        for (i = 0; i < SUBNET_NODE_NUMBER_MAX - 1; i++)
        {
            if (LinkAssignment_single_temp->end_node == macdata_daatr->subnet_other_node_position[i].nodeId)
            {
                temp_posiX2 = macdata_daatr->subnet_other_node_position[i].positionX;
                temp_posiY2 = macdata_daatr->subnet_other_node_position[i].positionY;
                temp_posiZ2 = macdata_daatr->subnet_other_node_position[i].positionZ;
                break;
            }
        }
    }
    else if (LinkAssignment_single_temp->end_node == macdata_daatr->nodeId)
    {
        temp_posiX1 = macdata_daatr->local_node_position_info.positionX;
        temp_posiY1 = macdata_daatr->local_node_position_info.positionY;
        temp_posiZ1 = macdata_daatr->local_node_position_info.positionZ;

        for (i = 0; i < SUBNET_NODE_NUMBER_MAX - 1; i++)
        {
            if (LinkAssignment_single_temp->begin_node == macdata_daatr->subnet_other_node_position[i].nodeId)
            {
                temp_posiX2 = macdata_daatr->subnet_other_node_position[i].positionX;
                temp_posiY2 = macdata_daatr->subnet_other_node_position[i].positionY;
                temp_posiZ2 = macdata_daatr->subnet_other_node_position[i].positionZ;
                break;
            }
        }
    }
    else
    {
        for (i = 0; i < SUBNET_NODE_NUMBER_MAX - 1; i++)
        {
            if (LinkAssignment_single_temp->begin_node == macdata_daatr->subnet_other_node_position[i].nodeId)
            {
                temp_posiX1 = macdata_daatr->subnet_other_node_position[i].positionX;
                temp_posiY1 = macdata_daatr->subnet_other_node_position[i].positionY;
                temp_posiZ1 = macdata_daatr->subnet_other_node_position[i].positionZ;
                break;
            }
        }

        for (i = 0; i < SUBNET_NODE_NUMBER_MAX - 1; i++)
        {
            if (LinkAssignment_single_temp->end_node == macdata_daatr->subnet_other_node_position[i].nodeId)
            {
                temp_posiX2 = macdata_daatr->subnet_other_node_position[i].positionX;
                temp_posiY2 = macdata_daatr->subnet_other_node_position[i].positionY;
                temp_posiZ2 = macdata_daatr->subnet_other_node_position[i].positionZ;
                break;
            }
        }
    }

    // 正常运行时将下面的部分解注释
    // LinkAssignment_single_temp->distance =
    // Calculate_Distance_Between_Nodes(temp_posiX1, temp_posiY1, temp_posiZ1,
    //                                                                temp_posiX2,
    //                                                                temp_posiY2,
    //                                                                temp_posiZ2);
    LinkAssignment_single_temp->distance = 49;
    float data_slot_per_frame_temp = (float)((LinkAssignment_single_temp->size * 8) / 1000.0) / LinkAssignment_single_temp->QOS; // kBytes
    double trans_speed = Calculate_Transmission_Rate_by_Distance(LinkAssignment_single_temp->distance);                          // kbits/s
    float trans_data = trans_speed * 0.0025;                                                                                     // 一个时隙可以传输的数据量 kbits
    // LinkAssignment_single_temp->data_slot_per_frame = ceil(data_slot_per_frame_temp * 8 / trans_data);
    LinkAssignment_single_temp->data_slot_per_frame = 1;
}

// 将按权重升序排列的诸多单链路构建需求进行链路的分配
void ReAllocate_Traffic_slottable(MacDaatr *macdata_daatr,
                                  vector<Alloc_slot_traffic> &Alloc_slottable_traffic,
                                  vector<LinkAssignment_single> &LinkAssignment_Storage)
{
    int rest_slot_num_per_node[SUBNET_NODE_NUMBER_MAX]; // 网关节点之外的节点有100个可发送时隙
    for (int i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
        rest_slot_num_per_node[i] = TRAFFIC_SLOT_NUMBER;

    if (macdata_daatr->gateway_node != 0)
        rest_slot_num_per_node[macdata_daatr->gateway_node - 1] = TRAFFIC_SLOT_NUMBER * 0.6; // 暂定网关节点2
    if (macdata_daatr->standby_gateway_node != 0)
        rest_slot_num_per_node[macdata_daatr->standby_gateway_node - 1] = TRAFFIC_SLOT_NUMBER * 0.6;

    // 将分配表初始化
    for (int i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    {
        // Alloc_slottable_traffic[i].subnet_ID = subnet;
        Alloc_slottable_traffic[i].multiplexing_num = FREQUENCY_DIVISION_MULTIPLEXING_NUMBER;
        Alloc_slottable_traffic[i].attribute = 0; // 先全部设为下级网业务时隙
    }
    for (int i = 0; i < TRAFFIC_SLOT_NUMBER; i = i + 20)
    {
        for (int j = 8; j < 16; j++)
        {
            Alloc_slottable_traffic[i + j].attribute = 1; // 设为不可与网关节点通信的时隙
        }
    }

    // 全连接
    int i = 0, j = 0, k = 0, m;
    int slot_loc = 0; // 用来标识目前正在分配时隙的位置, 非子网间时隙才可以分

    string Full_connection_send = "./config/FullConnection/FullConnection_10_send.txt";
    string Full_connection_recv = "./config/FullConnection/FullConnection_10_recv.txt";

    ifstream fin1(Full_connection_send);
    ifstream fin2(Full_connection_recv);

    if (!fin1.is_open())
        cout << "Cannot Open FullConnection File1" << endl;
    if (!fin2.is_open())
        cout << "Cannot Open FullConnection File2" << endl;

    string str1, str2, temp;
    vector<int> send_node;
    vector<int> recv_node;

    getline(fin1, str1);
    getline(fin2, str2);
    stringstream ss1(str1);
    while (getline(ss1, temp, ','))
    {
        send_node.push_back(stoi(temp));
    }
    stringstream ss2(str2);
    while (getline(ss2, temp, ','))
    {
        recv_node.push_back(stoi(temp));
    }

    while (i < 18)
    {
        if (slot_loc % 20 < 8 || slot_loc % 20 >= 16)
        {
            for (j = 0; j < 5; j++)
            {
                Alloc_slottable_traffic[slot_loc].send_node[j] = send_node[5 * i + j];
                Alloc_slottable_traffic[slot_loc].recv_node[j] = recv_node[5 * i + j];

                rest_slot_num_per_node[send_node[5 * i + j] - 1] -= 1;
                rest_slot_num_per_node[recv_node[5 * i + j] - 1] -= 1;
            }
            Alloc_slottable_traffic[slot_loc].multiplexing_num = 0;
            slot_loc++;
            i++;
        }
        else
        {
            slot_loc += 8;
        }
    }

    int LinkAssNum = LinkAssignment_Storage.size();
    vector<LinkAssignment_single>::iterator la;
    for (la = LinkAssignment_Storage.begin(); la != LinkAssignment_Storage.end(); la++)
    {
        if (rest_slot_num_per_node[(*la).begin_node - 1] == 0 || rest_slot_num_per_node[(*la).end_node - 1] == 0)
        {
            cout << "Failed to Allocate Slot for LA - " << (*la).begin_node;
            cout << "-" << (*la).end_node << endl;
            continue;
        }

        Calculate_slotnum_per_frame(macdata_daatr, la); // 计算LA_single的时隙数
        // cout << "data_slot_per_frame: " << (*la).data_slot_per_frame << endl;

        if (Judge_If_Include_Gateway(macdata_daatr, la)) // 包含
        {
            m = 0;
            for (j = 0; j <= TRAFFIC_SLOT_NUMBER - 20; j += 20)
            {
                for (k = 0; k < 8; k++)
                {
                    if (Judge_If_Choose_Slot(la, &Alloc_slottable_traffic[j + k], 1))
                    {
                        // 如果此时隙内不包含接收或者发送节点, 则不会影响频分复用结果,
                        // 可以分配此时隙
                        int temp = FREQUENCY_DIVISION_MULTIPLEXING_NUMBER - Alloc_slottable_traffic[j + k].multiplexing_num;
                        Alloc_slottable_traffic[j + k].send_node[temp] = (*la).begin_node; // 将发送节点添加进时隙的发送节点集合中
                        Alloc_slottable_traffic[j + k].recv_node[temp] = (*la).end_node;   // 将接收节点添加进时隙的发送节点集合中
                        Alloc_slottable_traffic[j + k].multiplexing_num -= 1;              // 可复用数减1
                        rest_slot_num_per_node[(*la).end_node - 1] -= 1;                   // 此节点可分配时隙数减1
                        rest_slot_num_per_node[(*la).begin_node - 1] -= 1;                 // 此节点可分配时隙数减1
                        m++;

                        if (m == (*la).data_slot_per_frame)
                        {
                            break; // 已分配足够时隙或时隙表所有时隙已全部分配, 跳出时隙分配
                        }
                    }
                }

                if (m != (*la).data_slot_per_frame)
                {
                    for (k = 16; k < 20; k++)
                    {
                        if (Judge_If_Choose_Slot(la, &Alloc_slottable_traffic[j + k], 1))
                        {
                            // 如果此时隙内不包含接收或者发送节点, 则不会影响频分复用结果,
                            // 可以分配此时隙
                            int temp = FREQUENCY_DIVISION_MULTIPLEXING_NUMBER - Alloc_slottable_traffic[j + k].multiplexing_num;
                            Alloc_slottable_traffic[j + k].send_node[temp] = (*la).begin_node; // 将发送节点添加进时隙的发送节点集合中
                            Alloc_slottable_traffic[j + k].recv_node[temp] = (*la).end_node;   // 将接收节点添加进时隙的发送节点集合中
                            Alloc_slottable_traffic[j + k].multiplexing_num -= 1;              // 可复用数减1
                            rest_slot_num_per_node[(*la).end_node - 1] -= 1;                   // 此节点可分配时隙数减1
                            rest_slot_num_per_node[(*la).begin_node - 1] -= 1;                 // 此节点可分配时隙数减1
                            m++;

                            if (m == (*la).data_slot_per_frame || j + k == TRAFFIC_SLOT_NUMBER - 1)
                            {
                                break; // 已分配足够时隙或时隙表所有时隙已全部分配,
                                // 跳出时隙分配
                            }
                        }
                    }
                }
                if (m == (*la).data_slot_per_frame || j == TRAFFIC_SLOT_NUMBER - 20)
                {
                    break;
                }
            }
        }
        else // 不包含网关节点
        {
            m = 0; // m作为已分配时隙数
            for (j = 0; j < TRAFFIC_SLOT_NUMBER; j++)
            {
                if (Judge_If_Choose_Slot(la, &Alloc_slottable_traffic[j], 1))
                {
                    // 如果此时隙内不包含接收或者发送节点, 则不会影响频分复用结果,
                    // 可以分配此时隙
                    int temp = FREQUENCY_DIVISION_MULTIPLEXING_NUMBER - Alloc_slottable_traffic[j].multiplexing_num;
                    Alloc_slottable_traffic[j].send_node[temp] = (*la).begin_node; // 将发送节点添加进时隙的发送节点集合中
                    Alloc_slottable_traffic[j].recv_node[temp] = (*la).end_node;   // 将接收节点添加进时隙的发送节点集合中
                    Alloc_slottable_traffic[j].multiplexing_num -= 1;              // 可复用数减1
                    rest_slot_num_per_node[(*la).end_node - 1] -= 1;               // 此节点可分配时隙数减1
                    rest_slot_num_per_node[(*la).begin_node - 1] -= 1;             // 此节点可分配时隙数减1
                    m++;

                    if (m == (*la).data_slot_per_frame || j == TRAFFIC_SLOT_NUMBER - 1)
                    {
                        break; // 已分配足够时隙或时隙表所有时隙已全部分配, 跳出时隙分配
                    }
                }
            }
        }
    }

    // Full Multiplexing
    vector<int> node_pair_1; // 1 是非子网间时隙的分配结果
    vector<int> node_pair_2; // 2 是8-15时隙
    // srand((unsigned)time(NULL));

    for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    {
        vector<int>().swap(node_pair_1);
        vector<int>().swap(node_pair_2);

        if (Alloc_slottable_traffic[i].multiplexing_num == FREQUENCY_DIVISION_MULTIPLEXING_NUMBER) // 此时隙未分配任何链路
        {
            node_pair_1.push_back(macdata_daatr->mana_node);
            if (macdata_daatr->gateway_node != 0)
            {
                node_pair_1.push_back(macdata_daatr->gateway_node);
            }
            if (macdata_daatr->standby_gateway_node != 0)
            {
                node_pair_1.push_back(macdata_daatr->standby_gateway_node);
            }

            node_pair_2.push_back(macdata_daatr->mana_node);

            for (int ii = 1; ii <= macdata_daatr->subnet_node_num; ii++)
            {
                if (ii != macdata_daatr->mana_node &&
                    ii != macdata_daatr->gateway_node &&
                    ii != macdata_daatr->standby_gateway_node)
                {
                    node_pair_1.push_back(ii);
                    node_pair_2.push_back(ii);
                }
            }

            if (i % 20 >= 8 && i % 20 < 16)
            {
                random_shuffle(node_pair_2.begin(), node_pair_2.end(), Rand);
                for (j = 0; j < FREQUENCY_DIVISION_MULTIPLEXING_NUMBER - 1; j++)
                {
                    Alloc_slottable_traffic[i].send_node[j] = node_pair_2[j * 2];
                    Alloc_slottable_traffic[i].recv_node[j] = node_pair_2[j * 2 + 1];
                    Alloc_slottable_traffic[i].multiplexing_num -= 1;
                    rest_slot_num_per_node[node_pair_2[j * 2] - 1] -= 1;
                    rest_slot_num_per_node[node_pair_2[j * 2]] -= 1;
                }
            }
            else
            {
                random_shuffle(node_pair_1.begin(), node_pair_1.end(), Rand);
                for (j = 0; j < FREQUENCY_DIVISION_MULTIPLEXING_NUMBER; j++)
                {
                    Alloc_slottable_traffic[i].send_node[j] = node_pair_1[j * 2];
                    Alloc_slottable_traffic[i].recv_node[j] = node_pair_1[j * 2 + 1];
                    Alloc_slottable_traffic[i].multiplexing_num -= 1;
                    rest_slot_num_per_node[node_pair_1[j * 2] - 1] -= 1;
                    rest_slot_num_per_node[node_pair_1[j * 2]] -= 1;
                }
            }
        }
        else if (Alloc_slottable_traffic[i].multiplexing_num <= 1)
        {
            continue;
        }
        else
        {
            int temp = FREQUENCY_DIVISION_MULTIPLEXING_NUMBER - Alloc_slottable_traffic[i].multiplexing_num;
            if (i % 20 >= 8 && i % 20 < 16)
            {
                for (j = 0; j < temp; j++)
                {
                    for (k = 1; k <= macdata_daatr->subnet_node_num; k++)
                    {
                        if (k != Alloc_slottable_traffic[i].send_node[j] &&
                            k != Alloc_slottable_traffic[i].recv_node[j] &&
                            k != macdata_daatr->gateway_node &&
                            k != macdata_daatr->standby_gateway_node)
                        {
                            node_pair_2.push_back(k);
                        }
                    }
                }
                random_shuffle(node_pair_2.begin(), node_pair_2.end(), Rand);
                for (j = temp; j < FREQUENCY_DIVISION_MULTIPLEXING_NUMBER - 1; j++)
                {
                    Alloc_slottable_traffic[i].send_node[j] = node_pair_2[2 * (j - temp)];
                    Alloc_slottable_traffic[i].recv_node[j] = node_pair_2[2 * (j - temp) + 1];
                    Alloc_slottable_traffic[i].multiplexing_num -= 1;
                    rest_slot_num_per_node[node_pair_2[2 * (j - temp)] - 1] -= 1;
                    rest_slot_num_per_node[node_pair_2[2 * (j - temp)]] -= 1;
                }
            }
            else
            {
                for (j = 0; j < temp; j++)
                {
                    for (k = 1; k <= macdata_daatr->subnet_node_num; k++)
                    {
                        if (k != Alloc_slottable_traffic[i].send_node[j] &&
                            k != Alloc_slottable_traffic[i].recv_node[j])
                        {
                            node_pair_1.push_back(k);
                        }
                    }
                }
                random_shuffle(node_pair_1.begin(), node_pair_1.end(), Rand);
                for (j = temp; j < FREQUENCY_DIVISION_MULTIPLEXING_NUMBER; j++)
                {
                    Alloc_slottable_traffic[i].send_node[j] = node_pair_1[2 * (j - temp)];
                    Alloc_slottable_traffic[i].recv_node[j] = node_pair_1[2 * (j - temp) + 1];
                    Alloc_slottable_traffic[i].multiplexing_num -= 1;
                    rest_slot_num_per_node[node_pair_1[2 * (j - temp)] - 1] -= 1;
                    rest_slot_num_per_node[node_pair_1[2 * (j - temp)]] -= 1;
                }
            }
        }
    }
}

void sendLocalLinkStatus(MacDaatr *macdata_daatr)
{
    // TODO 核对if_receive_mana_flight_frame

    vector<nodeLocalNeighList> *send_neighborList = new vector<nodeLocalNeighList>;
    for (int dest_node = 1; dest_node <= macdata_daatr->subnet_node_num; dest_node++)
    {
        if (macdata_daatr->businessQueue[dest_node - 1].distance != 0 &&
            macdata_daatr->if_receive_mana_flight_frame[dest_node - 1] == true)
        {
            uint16_t Capacity = Calculate_Transmission_Rate_by_Distance(macdata_daatr->businessQueue[dest_node - 1].distance);

            nodeLocalNeighList *nodeLocalNeighdata = new nodeLocalNeighList;
            nodeLocalNeighdata->nodeAddr = dest_node;
            nodeLocalNeighdata->Capacity = Capacity;
            nodeLocalNeighdata->Delay = 7;
            nodeLocalNeighdata->Queuelen = 15;
            nodeLocalNeighdata->Load = 1;
            send_neighborList->push_back(*nodeLocalNeighdata);
            delete nodeLocalNeighdata;
        }
    }

    for (int i = 0; i < macdata_daatr->subnet_node_num; i++)
    {
        macdata_daatr->if_receive_mana_flight_frame[i] = false;
    }

    ReceiveLocalLinkState_Msg *loca_linkstate_struct = new ReceiveLocalLinkState_Msg;
    loca_linkstate_struct->neighborList = *send_neighborList;

    int len = send_neighborList->size() * sizeof(nodeLocalNeighList);

    macdata_daatr->macToNetworkBufferHandle(loca_linkstate_struct, 0x0d, len);

    cout << "[PERIODIC] Node " << macdata_daatr->nodeId << " 向网络层上报 本地链路状态信息      ";
    printTime_ms();
}

void sendOtherNodePosition(MacDaatr *macdata_daatr)
{
    int nodeNum = 1;
    for (nodeNum; nodeNum <= SUBNET_NODE_NUMBER_MAX; nodeNum++)
    {
        if (macdata_daatr->subnet_other_node_position[nodeNum].nodeId == 0)
        { // 此处截止
            break;
        }
    }

    FlightStatus *NodePosition_test = new FlightStatus[nodeNum];
    int i;
    for (i = 0; i < nodeNum - 1; i++)
    { // 将已收到的结构体信息赋值给要发送的结构体中
        NodePosition_test[i] = macdata_daatr->subnet_other_node_position[i];
    }
    NodePosition_test[i] = macdata_daatr->local_node_position_info;                       // 添加本节点飞行状态信息2023.8.17
    uint32_t fullPacketSize = sizeof(NetFlightStateMsg) + nodeNum * sizeof(FlightStatus); // packet的总大小

    uint8_t *pkt_nodeposition = (uint8_t *)malloc(fullPacketSize); // 存储该packet的空间
    NetFlightStateMsg *node_position_header = (NetFlightStateMsg *)pkt_nodeposition;
    node_position_header->nodeNum = nodeNum; // 本packet包含多少节点
    FlightStatus *node_position = (FlightStatus *)(pkt_nodeposition + sizeof(NetFlightStateMsg));
    memcpy(node_position, NodePosition_test, nodeNum * sizeof(FlightStatus));

    macdata_daatr->macToNetworkBufferHandle((uint8_t *)pkt_nodeposition, 0x0c, fullPacketSize);
    cout << "[PERIODIC] Node " << macdata_daatr->nodeId << " 向网络层上报 其他节点飞行状态信息  ";
    printTime_ms();

    delete[] NodePosition_test;
}

vector<LinkAssignment> *Generate_LinkAssignment_Initialization(MacDaatr *macdata_daatr)
{
    int i, j;
    vector<LinkAssignment> *link_assign_queue = new vector<LinkAssignment>;

    for (i = 1; i <= macdata_daatr->subnet_node_num; i++)
    {
        if (i != macdata_daatr->mana_node)
        {
            LinkAssignment link_assignment_temp;
            link_assignment_temp.begin_node = macdata_daatr->mana_node;
            link_assignment_temp.end_node = i;

            link_assignment_temp.listNumber = 2;
            link_assignment_temp.priority[0] = 0;
            link_assignment_temp.size[0] = 400 / 8;
            link_assignment_temp.QOS[0] = 4;
            link_assignment_temp.priority[1] = 0;
            link_assignment_temp.size[1] = 400 / 8;
            link_assignment_temp.QOS[1] = 4;
            link_assign_queue->push_back(link_assignment_temp);
        }
    }

    for (i = 1; i <= macdata_daatr->subnet_node_num; i++)
    {
        for (j = 1; j <= macdata_daatr->subnet_node_num; j++)
        {
            if (i != macdata_daatr->mana_node && j != macdata_daatr->mana_node && i != j)
            {
                LinkAssignment link_assignment_temp3;
                link_assignment_temp3.begin_node = i;
                link_assignment_temp3.end_node = j;

                link_assignment_temp3.listNumber = 2;
                link_assignment_temp3.priority[0] = 1;
                link_assignment_temp3.size[0] = 400 / 8;
                link_assignment_temp3.QOS[0] = 4;

                link_assignment_temp3.priority[1] = 1;
                link_assignment_temp3.size[1] = 400 / 8;
                link_assignment_temp3.QOS[1] = 4;
                link_assign_queue->push_back(link_assignment_temp3);
            }
        }
    }

    return link_assign_queue;
}

void generateSlottableExecution(MacDaatr *macdata_daatr)
{
    if (macdata_daatr->nodeId == macdata_daatr->mana_node)
    {
        vector<LinkAssignment> *link_assign_queue = Generate_LinkAssignment_Initialization(macdata_daatr);
        vector<LinkAssignment_single> LinkAssignment_Storage;
        for (auto &LA_in_list : *link_assign_queue)
        {
            for (int j = 0; j < LA_in_list.listNumber; j++)
            {
                LinkAssignment_single LinkAssignment_single_temp;
                LinkAssignment_single_temp.begin_node = LA_in_list.begin_node;
                LinkAssignment_single_temp.end_node = LA_in_list.end_node;
                LinkAssignment_single_temp.type = LA_in_list.type[j];
                LinkAssignment_single_temp.priority = LA_in_list.priority[j];
                LinkAssignment_single_temp.size = LA_in_list.size[j];

                LinkAssignment_single_temp.QOS = LA_in_list.QOS[j];
                LinkAssignment_single_temp.frequency = LA_in_list.frequency[j];

                if (LinkAssignment_single_temp.size != 0)
                {
                    LinkAssignment_Storage.push_back(LinkAssignment_single_temp);
                }
            }
        }

        sort(LinkAssignment_Storage.begin(), LinkAssignment_Storage.end(), Compare_by_Priority);
        vector<Alloc_slot_traffic> allocSlottableTraffic(TRAFFIC_SLOT_NUMBER);
        ReAllocate_Traffic_slottable(macdata_daatr, allocSlottableTraffic, LinkAssignment_Storage);

        // 下面需要将分配表转换为各节点对应的时隙表
        for (int i = 0; i < SUBNET_NODE_NUMBER_MAX; i++)
        {
            for (int j = 0; j < TRAFFIC_SLOT_NUMBER; j++)
                macdata_daatr->slot_traffic_execution_new[i][j].state = 0; // 本应全置为IDLE
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
                        macdata_daatr->slot_traffic_execution_new[allocSlottableTraffic[j].send_node[k] - 1][j].state = DAATR_STATUS_TX;
                        macdata_daatr->slot_traffic_execution_new[allocSlottableTraffic[j].send_node[k] - 1][j].send_or_recv_node =
                            allocSlottableTraffic[j].recv_node[k];

                        macdata_daatr->slot_traffic_execution_new[allocSlottableTraffic[j].recv_node[k] - 1][j].state = DAATR_STATUS_RX;
                        macdata_daatr->slot_traffic_execution_new[allocSlottableTraffic[j].recv_node[k] - 1][j].send_or_recv_node =
                            allocSlottableTraffic[j].send_node[k];
                    }
                }
            }
        }

        // 生成射前时隙表
        int dest_node;
        cout << "网管节点生成子网时隙表" << endl;
        for (dest_node = 1; dest_node <= macdata_daatr->subnet_node_num; dest_node++)
        {
            ofstream fout1, fout2;
            string stlotable_state_filename = "./config/SlottableExecution/Slottable_RXTX_State_" +
                                              to_string(static_cast<long long>(dest_node)) + ".txt";
            string stlotable_node_filename = "./config/SlottableExecution/Slottable_RXTX_Node_" +
                                             to_string(static_cast<long long>(dest_node)) + ".txt";
            fout1.open(stlotable_state_filename);
            fout2.open(stlotable_node_filename);

            if (!fout1.is_open())
                cout << "Could Not Open Execution File1" << endl;

            if (!fout2.is_open())
                cout << "Could Not Open Execution File2" << endl;

            for (int i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
            {
                fout1 << macdata_daatr->slot_traffic_execution_new[dest_node - 1][i].state << ",";
            }
            for (int i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
            {
                if (macdata_daatr->slot_traffic_execution_new[dest_node - 1][i].state == DAATR_STATUS_TX &&
                    macdata_daatr->slot_traffic_execution_new[dest_node - 1][i].send_or_recv_node != 0)
                {
                    fout2 << macdata_daatr->slot_traffic_execution_new[dest_node - 1][i].send_or_recv_node << ",";
                }
                else if (macdata_daatr->slot_traffic_execution_new[dest_node - 1][i].state == DAATR_STATUS_RX &&
                         macdata_daatr->slot_traffic_execution_new[dest_node - 1][i].send_or_recv_node != 0)
                {
                    fout2 << macdata_daatr->slot_traffic_execution_new[dest_node - 1][i].send_or_recv_node << ",";
                }
                else
                {
                    fout2 << 0 << ",";
                }
            }

            fout1.close();
            fout2.close();
        }
    }
}

// 计算子网使用频点的个数
static int calculateFreqNum(MacDaatr *macdata_daatr)
{
    int num = 0;
    for (int i = 0; i < TOTAL_FREQ_POINT; i++)
    {
        if (macdata_daatr->subnet_frequency_sequence[i] == 1)
            num++;
    }
    return num;
}

// 生成下一个m序列值
unsigned char Generate_M_SequenceValue()
{
    // 生成九位m序列
    unsigned char m_sequence[M_SEQUENCE_LENGTH] = {0, 0, 0, 0, 0, 0, 0, 0, 1};
    unsigned char m_sequence_index = M_SEQUENCE_LENGTH - 1;

    unsigned char feedback = (m_sequence[m_sequence_index] + m_sequence[0]) % 2;
    unsigned char output = m_sequence[m_sequence_index];

    for (int i = m_sequence_index; i > 0; i--)
        m_sequence[i] = m_sequence[i - 1];

    m_sequence[0] = feedback;

    return output;
}

// 调整窄点
void adjustNarrowBand(unsigned int freqSeq[SUBNET_NODE_NUMBER_MAX][FREQUENCY_COUNT])
{
    int prev_node;
    for (int time = 0; time < FREQUENCY_COUNT; time++)
    {
        for (int node = 0; node < 5; node++)
        {
            prev_node = node - 1;
            while (prev_node > 0)
            {
                if (freqSeq[node][time] - freqSeq[prev_node][time] <= MIN_FREQUENCY_DIFFERENCE ||
                    freqSeq[prev_node][time] - freqSeq[node][time] <= MIN_FREQUENCY_DIFFERENCE)
                {
                    prev_node = node - 1;
                    freqSeq[node][time] += NARROW_BAND_WIDTH;

                    if (freqSeq[node][time] >= MAX_FREQUENCY)
                        freqSeq[node][time] -= MAX_FREQUENCY;
                }
                else
                    prev_node--;
            }
        }
    }
}

// 跳频序列调整
static void adjustFreqSequence(unsigned int freqSeq[SUBNET_NODE_NUMBER_MAX][FREQUENCY_COUNT],
                               const unsigned char available_frequency[TOTAL_FREQ_POINT])
{
    int flag = 0;
    for (int time = 0; time < FREQUENCY_COUNT; time++)
    {
        for (int node = 0; node < SUBNET_NODE_NUMBER_MAX; node++)
        {
            unsigned int current_frequency = freqSeq[node][time];
            unsigned int frequency_found_count = 0;
            // 如果当前频率点不可用, 则顺延至下一个可用频点
            while (available_frequency[current_frequency] == 0)
            {
                current_frequency++;
                frequency_found_count++;

                // 检查频点是否超过最大频率, 若超过则减去最大频率
                if (current_frequency >= MAX_FREQUENCY)
                    current_frequency -= MAX_FREQUENCY;
                if (frequency_found_count == TOTAL_FREQ_POINT)
                {
                    printf("找不到可用频点!\n");
                    break;
                }
            }

            freqSeq[node][time] = current_frequency;
            frequency_found_count = 0;
            // 顺延之后的频点, 依次判断是否占用了不可用频点
            for (int i = node + 1; i < SUBNET_NODE_NUMBER_MAX; i++)
            {
                unsigned int next_frequency = freqSeq[i][time];

                // 如果下一个频率点是不可用的, 则顺延至下一个可用频点
                while (available_frequency[next_frequency] == 0)
                {
                    next_frequency++;
                    frequency_found_count++;

                    // 检查频点是否超过最大频率, 若超过则减去最大频率
                    if (next_frequency >= MAX_FREQUENCY)
                        next_frequency -= MAX_FREQUENCY;
                    if (frequency_found_count == TOTAL_FREQ_POINT)
                    {
                        // printf("找不到可用频点\n");
                        flag = 1;
                        break;
                    }
                }

                freqSeq[i][time] = next_frequency;
                if (flag == 1)
                    break;
            }
            if (flag == 1)
                break;
        }
        if (flag == 1)
            break;
    }
}

static void generateFreqSequence(MacDaatr *macdata_daatr)
{
    unsigned int freqSeq[SUBNET_NODE_NUMBER_MAX][FREQUENCY_COUNT]; // 调整前序列
    unsigned char available_frequency[TOTAL_FREQ_POINT];
    float interference_ratios_before = 0;
    float interference_ratios_after = 0;
    int interference_count_before = 0;
    int interference_count_after = 0;

    for (int time = 0; time < FREQUENCY_COUNT; time++)
    {
        // int time = FREQUENCY_COUNT - 1;
        for (int node = 0; node < SUBNET_NODE_NUMBER_MAX; node++)
        {
            unsigned int m_sequence_decimal = 0;

            for (int i = 0; i < M_SEQUENCE_LENGTH; i++)
            {
                m_sequence_decimal <<= 1;
                m_sequence_decimal |= Generate_M_SequenceValue();
            }

            freqSeq[node][time] = m_sequence_decimal % MAX_FREQUENCY;
        }
    }
    adjustNarrowBand(freqSeq);
    for (int j = 0; j < TOTAL_FREQ_POINT; j++)
    {
        available_frequency[j] = macdata_daatr->unavailable_frequency_points[j];
    }

    // 优先满足窄带调整, 直到没有窄带
    int narrow_band_found = 1;
    int unavailable_freq_found = 1;
    int total_attempts = 0;
    int max_attempts = 10; // Maximum attempts to find suitable frequencies
    while ((narrow_band_found || unavailable_freq_found) && total_attempts < max_attempts)
    {
        narrow_band_found = 0;
        unavailable_freq_found = 0;

        adjustNarrowBand(freqSeq);
        adjustFreqSequence(freqSeq, available_frequency);

        // 检查是否有窄带
        for (int time = 0; time < FREQUENCY_COUNT; time++)
        {
            for (int node = 0; node < SUBNET_NODE_NUMBER_MAX; node++)
            {
                int prev_node = node - 1;
                while (prev_node >= 0)
                {
                    if (freqSeq[node][time] - freqSeq[prev_node][time] <= MIN_FREQUENCY_DIFFERENCE ||
                        freqSeq[prev_node][time] - freqSeq[node][time] <= MIN_FREQUENCY_DIFFERENCE)
                    {
                        narrow_band_found = 1;
                        break;
                    }
                    else
                        prev_node--;
                }
                if (available_frequency[freqSeq[node][time]] == 0)
                {
                    unavailable_freq_found = 1;
                    break;
                }
            }

            if (narrow_band_found || unavailable_freq_found)
                break;
        }
        total_attempts++;
        break;
    }
    for (int i = 0; i < SUBNET_NODE_NUMBER_MAX; i++)
    { // 将生成的调频数组赋值给网管节点保存的生成跳频组
        for (int j = 0; j < FREQUENCY_COUNT; j++)
            macdata_daatr->freqSeq[i][j] = freqSeq[i][j];
    }
    // 打印调整后每个节点的频率序列
    /*printf("调整后跳频序列\n");
    for (int node = 0; node < SUBNET_NODE_NUMBER_MAX; node++)
    {
        printf("Node %d:", node + 1);

        for (int time = 0; time < FREQUENCY_COUNT; time++)
        {
            printf("%d ", macdata_daatr->freqSeq[node][time]);
        }
        printf("\n");
    }*/
}

void MacDaatrInitialize_Frequency_Seqence(MacDaatr *macdata_daatr)
{
    unsigned int pre_freqSeq[SUBNET_NODE_NUMBER_MAX][FREQUENCY_COUNT]; // 调整前序列

    unsigned char available_frequency[TOTAL_FREQ_POINT];
    for (int time = 0; time < FREQUENCY_COUNT; time++)
    {
        // int time = FREQUENCY_COUNT - 1;
        for (int node = 0; node < SUBNET_NODE_NUMBER_MAX; node++)
        {
            unsigned int m_sequence_decimal = 0;
            for (int i = 0; i < M_SEQUENCE_LENGTH; i++)
            {
                m_sequence_decimal <<= 1;
                m_sequence_decimal |= Generate_M_SequenceValue();
            }

            pre_freqSeq[node][time] = m_sequence_decimal % MAX_FREQUENCY;
        }
    }

    // 调整窄带, 直到没有窄带
    int narrow_band_found = 1;
    int total_attempts = 0;
    int max_attempts = 100; // Maximum attempts to find suitable frequencies
    while (narrow_band_found == 1 && total_attempts < max_attempts)
    {
        narrow_band_found = 0;
        adjustNarrowBand(pre_freqSeq);

        // 检查是否有窄带
        for (int time = 0; time < FREQUENCY_COUNT; time++)
        {
            for (int node = 0; node < SUBNET_NODE_NUMBER_MAX; node++)
            {
                int prev_node = node - 1;
                while (prev_node >= 0)
                {
                    if (((pre_freqSeq[node][time] - pre_freqSeq[prev_node][time] >= 0) &&
                         (pre_freqSeq[node][time] - pre_freqSeq[prev_node][time] < MIN_FREQUENCY_DIFFERENCE)) ||
                        ((pre_freqSeq[node][time] - pre_freqSeq[prev_node][time] < 0) &&
                         (pre_freqSeq[prev_node][time] - pre_freqSeq[node][time] < MIN_FREQUENCY_DIFFERENCE)))

                    {
                        narrow_band_found = 1;
                        break;
                    }
                    else
                        prev_node--;
                }
            }
        }
    }

    // 打印节点初始的频率序列
    // printf("初始跳频序列\n");
    // for (int node = 0; node < SUBNET_NODE_NUMBER_MAX; node++)
    // {
    //     printf("Node %d:", node + 1);
    //     for (int time = 0; time < FREQUENCY_COUNT; time++)
    //     {
    //         printf("%d ", pre_freqSeq[node][time]);
    //     }
    //     printf("\n");
    // }

    for (int i = 0; i < SUBNET_NODE_NUMBER_MAX; i++)
    { // 将生成的调频数组赋值给网管节点保存的生成跳频组
        for (int j = 0; j < FREQUENCY_COUNT; j++)
            macdata_daatr->freqSeq[i][j] = pre_freqSeq[i][j];
    }
}

// 根据子网分配跳频点生成子网内使用的频段
static void generateSubnetUsedFrequency(MacDaatr *macdata_daatr)
{
    int i, j;
    int temp = 0;
    for (i = 0; i < TOTAL_FREQ_POINT; i++)
    {                                                    // 首先全部置0
        macdata_daatr->subnet_frequency_sequence[i] = 0; // 设置为使用频点
    }
    for (i = 0; i < SUBNET_NODE_NUMBER_MAX; i++)
    {
        for (j = 0; j < FREQUENCY_COUNT; j++)
        {
            temp = macdata_daatr->freqSeq[i][j];
            macdata_daatr->subnet_frequency_sequence[temp] = 1; // 设置为使用频点
        }
    }
    for (i = 0; i < TOTAL_FREQ_POINT; i++)
    {                                                               // 打印当前子网所使用频点
        cout << macdata_daatr->subnet_frequency_sequence[i] << " "; // 设置为使用频点
    }
}

void judgeIfEnterFreqAdjustment(MacDaatr *macdata_daatr)
{
    // 判断是否进入频率调整阶段
    int i = 0, j = 0;
    int interfer_number = 0;     // 子网使用频段干扰频段数量
    int interfer_number_sum = 0; // 总频段被干扰数目
    for (i = 0; i < TOTAL_FREQ_POINT; i++)
    {
        macdata_daatr->unavailable_frequency_points[i] = 1;
    }
    for (i = 0; i < TOTAL_FREQ_POINT; i++)
    {
        for (j = 0; j < SUBNET_NODE_NUMBER_MAX; j++)
        {
            if (macdata_daatr->spectrum_sensing_sum[j][i] == 0 &&
                macdata_daatr->spectrum_sensing_sum[j][TOTAL_FREQ_POINT] == 1)
            { // 若当前节点对应的频谱感知结果已经接收到了,
                // 且此频段受到干扰了
                if (macdata_daatr->subnet_frequency_sequence[i] == 1)
                {
                    interfer_number++;
                } // 此值对应的是子网现使用频段被干扰的数目，不对应总的干扰频段数目
                macdata_daatr->unavailable_frequency_points[i] = 0; // 此频点受到干扰, 赋0(此处为总干扰频段)
                interfer_number_sum++;
                break;
            }
        }
    }

    // for (i = 0; i < TOTAL_FREQ_POINT; i++)
    //     cout << macdata_daatr->unavailable_frequency_points[i] << " ";

    // cout << endl;

    // cout << "总频段被干扰数目为： " << interfer_number_sum << endl;
    // cout << "子网使用频段被干扰数目为： " << interfer_number << endl;

    int subnet_using_freq_num = calculateFreqNum(macdata_daatr);
    float using_freq_interfer_ratio = (float)interfer_number / subnet_using_freq_num;

    // cout << "子网使用频段个数为： " << subnet_using_freq_num << endl;
    // cout << "子网使用频段被干扰比例为： " << using_freq_interfer_ratio * 100 << endl;

    if (using_freq_interfer_ratio >= INTERFEREENCE_FREQUENCY_THRESHOLD)
    {
        // 干扰频段数目超过阈值, 进入频率调整阶段
        cout << "干扰频段数目超过阈值, 进入频率调整阶段, Time:  ";
        printTime_ms();

        generateFreqSequence(macdata_daatr);
        generateSubnetUsedFrequency(macdata_daatr);

        if (macdata_daatr->need_change_state == 0 &&
            macdata_daatr->state_now != Mac_Adjustment_Slot &&
            macdata_daatr->state_now != Mac_Adjustment_Frequency)
        {
            // 当前状态稳定, 在这之前没有发生要调整状态的事件, 且不处于任何调整状态
            cout << "即将进入频域调整阶段!";
            printTime_ms();
            macdata_daatr->need_change_state = 2; // 即将进入频率调整阶段
        }
        else if (macdata_daatr->need_change_state == 1)
        { // 在这之前已经发生了要进入时隙调整阶段, 但还未广播此信息,
            // 也即未进入
            cout << "正处于时域调整阶段 无法进入频域调整! ";
            printTime_ms();
            macdata_daatr->need_change_state = 3; // 设置为首先进入时隙调整阶段,
            // 在时隙调整阶段结束后进入频率调整阶段
        }

        // 首先将网管节点自己的跳频图案进行存储
        for (int j = 0; j < FREQUENCY_COUNT; j++)
            macdata_daatr->spectrum_sensing_node[j] = macdata_daatr->freqSeq[1 - 1][j];
    }
    else
        cout << "干扰频段数未达到阈值, 不作调整" << endl;
}

int Generate_LinkAssignment_Stage_1(LinkAssignment link_assign[])
{
    int i, j, k, LA_index = 0;
    // while (LA_index < num)
    // {
    for (i = 1; i <= 3; i++)
    {
        if (i == 1) // 网管节点的相关业务构建需求
        {
            for (j = 2; j <= 3; j++)
            {

                link_assign[LA_index].begin_node = i;
                link_assign[LA_index].end_node = j;

                link_assign[LA_index].listNumber = 4;
                link_assign[LA_index].priority[0] = 0;
                link_assign[LA_index].size[0] = 462 / 8;
                link_assign[LA_index].QOS[0] = 4;

                link_assign[LA_index].priority[1] = 1;
                link_assign[LA_index].size[1] = 480 / 8;
                link_assign[LA_index].QOS[1] = 4;

                link_assign[LA_index].priority[2] = 2;
                link_assign[LA_index].size[2] = 950 / 8;
                link_assign[LA_index].QOS[2] = 4;

                link_assign[LA_index].priority[3] = 3;
                link_assign[LA_index].size[3] = 298 / 8;
                link_assign[LA_index].QOS[3] = 4;

                LA_index++;
            }
        }
        else
        {
            for (j = 1; j <= 3; j++)
            {
                if (i != j)
                {
                    link_assign[LA_index].begin_node = i;
                    link_assign[LA_index].end_node = j;

                    link_assign[LA_index].listNumber = 1;

                    link_assign[LA_index].priority[0] = 3;
                    link_assign[LA_index].size[0] = 298 / 8;
                    link_assign[LA_index].QOS[0] = 4;

                    LA_index++;
                }
            }
        }
    }

    return LA_index;
}

bool canSendConfigLinkRequest()
{
    extern MacDaatr daatr_str;
    return daatr_str.state_now == Mac_Execution;
}