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

    return tran_speed;
}

// 若待发送队列中不包含重传包, 则返回false, 反之返回true
bool Judge_If_Include_ReTrans(const msgFromControl &MFC_temp, const vector<msgFromControl> &MFC_list_temp)
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

    string Full_connection_send = "../config/FullConnection/FullConnection_10_send.txt";
    string Full_connection_recv = "../config/FullConnection/FullConnection_10_recv.txt";

    ifstream fin1(Full_connection_send);
    ifstream fin2(Full_connection_recv);

    if (!fin1.is_open())
        cout << "Could Not Open File1" << endl;
    if (!fin2.is_open())
        cout << "Could Not Open File2" << endl;

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
    srand((unsigned)time(NULL));

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