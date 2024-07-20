#include "highFreqToolFunc.h"

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
