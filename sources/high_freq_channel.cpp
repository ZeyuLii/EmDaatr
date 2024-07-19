#include "macdaatr.h"
#include "common_struct.h"
#include "socket_control.h"
#include "struct_converter.h"
#include "beam_maintenance.h"
#include "timer.h"

using namespace std;
/*****************************此文件主要包含对高频信道通信相关函数实现******************************/

/// @brief 高频信道发送线程函数
void MacDaatr::highFreqSendThread()
{
    // 首先检查此时的节点所处的阶段
    // -- 如果是 Mac_Initialiation 则发送内容为 建链请求/建链回复
    // -- 如果是 Mac_Exucation 则发送内容为 业务数据包
    // -- 如果是 Mac_Adjustment 则发送内容为 时隙表/ACK
    // -- -- 如果是 发 -> 判定此时时隙表 判断此时的收发状态

    // -- -- 如果是 收 -> 则不用做什么事情 收线程一直开启

    extern bool end_simulation;

    // 首先更新时隙编号 slotId
    // 相当于每调用一次这个函数，业务信道的 slotId 就递增1
    currentSlotId = (currentSlotId == TRAFFIC_SLOT_NUMBER - 1) ? 0 : currentSlotId + 1;

    // TODO 未考虑全连接时隙
    if (state_now == Mac_Initialization)
    {
        currentStatus = slottable_setup[currentSlotId].state;
        int dest_node = slottable_setup[currentSlotId].send_or_recv_node;

        if (currentStatus == DAATR_STATUS_TX && dest_node != 0) // TX
        {
            // 在建链阶段 普通节点发送建链请求 网管节点发送 回复建链请求
            if (nodeId == mana_node)
            {
                MacHeader *mac_header_ptr = new MacHeader;
                mac_header_ptr->PDUtype = 0;
                mac_header_ptr->is_mac_layer = 1;
                mac_header_ptr->backup = 2;
                mac_header_ptr->packetLength = 2 + 25;
                mac_header_ptr->srcAddr = nodeId;
                mac_header_ptr->destAddr = dest_node;
                mac_header_ptr->seq = 1 + (blTimes++) % 3;
                MacDaatr_struct_converter mac_converter_PDU1(1);
                mac_converter_PDU1.set_struct((uint8_t *)mac_header_ptr, 1);
                mac_converter_PDU1.daatr_struct_to_0_1();

                BuildLink_Request build_link;
                build_link.nodeID = dest_node;
                build_link.if_build = 1; // 允许加入
                MacDaatr_struct_converter daatr_convert(6);
                daatr_convert.set_struct((uint8_t *)&build_link, 6);
                daatr_convert.daatr_struct_to_0_1();

                mac_converter_PDU1 + daatr_convert;

                uint8_t *frame_ptr = mac_converter_PDU1.get_sequence();
                uint32_t len = mac_converter_PDU1.get_length();
                uint8_t *temp_buf = new uint8_t[len]; // 此处只是为了防止转换类中的bit指针被释放，所以保险起见复制一份，也可以尝试直接使用
                memcpy(temp_buf, frame_ptr, len);
                macDaatrSocketHighFreq_Send(temp_buf, len, dest_node); // 此处的的dest_node为待建链节点

                cout << "[" << currentSlotId << "] 网管节点回复 Node " << dest_node << " 的建链请求--" << mac_header_ptr->seq << "  ";
                printTime_ms();

                delete mac_header_ptr;
                delete temp_buf;
            }
            else
            {
                MacHeader *mac_header_ptr = new MacHeader;
                mac_header_ptr->PDUtype = 0;
                mac_header_ptr->is_mac_layer = 1;
                mac_header_ptr->backup = 1;
                mac_header_ptr->packetLength = 2 + 25;
                mac_header_ptr->srcAddr = nodeId;
                mac_header_ptr->destAddr = dest_node;
                mac_header_ptr->seq = blTimes++;
                MacDaatr_struct_converter mac_converter_PDU1(1);
                mac_converter_PDU1.set_struct((uint8_t *)mac_header_ptr, 1);
                mac_converter_PDU1.daatr_struct_to_0_1();

                BuildLink_Request build_link;
                build_link.nodeID = nodeId;
                build_link.if_build = 0; // 未允许加入
                MacDaatr_struct_converter daatr_convert(6);
                daatr_convert.set_struct((uint8_t *)&build_link, 6);
                daatr_convert.daatr_struct_to_0_1();

                mac_converter_PDU1 + daatr_convert;

                uint8_t *frame_ptr = mac_converter_PDU1.get_sequence();
                uint32_t len = mac_converter_PDU1.get_length();
                uint8_t *temp_buf = new uint8_t[len]; // 此处只是为了防止转换类中的bit指针被释放，所以保险起见复制一份，也可以尝试直接使用
                memcpy(temp_buf, frame_ptr, len);
                macDaatrSocketHighFreq_Send(temp_buf, len, dest_node); // 此处的的dest_node为网管节点

                cout << "[" << currentSlotId << "] Node " << nodeId << " 向网管节点发送建链请求 -- " << blTimes << " ";
                printTime_ms();

                delete mac_header_ptr;
                delete temp_buf;
            }
        }
        else if (currentStatus == DAATR_STATUS_RX && dest_node != 0)
        {
            // 在建链阶段 普通节点收到建链请求的Approval 网管节点收到普通节点的建链请求
            // 调整波束指向 等
        }
        else
        {
            // IDLE
        }
    }
}
/// @brief 处理高频信道接收到的数据包 (格式均为 PDU1 + 后续内容)
/// @param bit_seq 指向接收到的位序列的指针
/// @param len 位序列的长度
void MacDaatr::highFreqChannelHandle(uint8_t *bit_seq, uint64_t len)
{
    MacDaatr_struct_converter mac_converter(255);                       // 原始bitseq(包头 + 后续内容)
    int frame_length = mac_converter.get_PDU1_sequence_length(bit_seq); // 直接从bitseq中读取长度
    mac_converter.set_length(frame_length);
    mac_converter.set_bit_sequence(bit_seq, 255);

    MacDaatr_struct_converter mac_converter_PDU1(1); // PDU1
    mac_converter_PDU1 - mac_converter;              // mac_converter_PDU1 中盛放PDU包头
    mac_converter_PDU1.daatr_0_1_to_struct();
    MacHeader *mac_header_ptr = (MacHeader *)mac_converter_PDU1.get_struct();
    MacHeader mac_header = *mac_header_ptr;
    if (mac_header.is_mac_layer == 0) // 上层数据包+频谱感知
    {
    }
    else // 本层数据包
    {
        switch (mac_header.backup)
        {
        case 1: // 建链请求 只有网管节点可以收到
        {
            // 检查此时的接收节点是不是网管节点
            if (nodeId != mana_node)
                cout << "Wrong Recv Node of BuildLink Request!" << endl;

            MacDaatr_struct_converter mac_converter_BLRequest(6);
            mac_converter_BLRequest - mac_converter;
            // mac_converter_BLRequest.set_length(2);
            mac_converter_BLRequest.daatr_0_1_to_struct();

            // uint8_t *frame = mac_converter_BLRequest.get_sequence();
            // for (int i; i < 2; i++)
            // {
            //     cout << hex << (int)frame[i] << " ";
            // }
            // cout << dec << endl;

            BuildLink_Request *blRequest_ptr = (BuildLink_Request *)mac_converter_BLRequest.get_struct();
            BuildLink_Request blRequest = *blRequest_ptr;

            cout << "网管节点收到 Node " << blRequest.nodeID << " 的建链请求--" << mac_header.seq + 1 << "  ";
            printTime_ms();

            break;
        }
        case 2: // 建链回复 只有普通节点可以收到
        {
            // 检查此时的接收节点是不是普通节点
            if (nodeId == mana_node)
                cout << "Wrong Recv Node of BuildLink Response!" << endl;

            MacDaatr_struct_converter mac_converter_BLRequest(6);
            mac_converter_BLRequest - mac_converter;
            // mac_converter_BLRequest.set_length(2);
            mac_converter_BLRequest.daatr_0_1_to_struct();

            // uint8_t *frame = mac_converter_BLRequest.get_sequence();
            // for (int i; i < 2; i++)
            // {
            //     cout << hex << (int)frame[i] << " ";
            // }
            // cout << dec << endl;

            BuildLink_Request *blRequest_ptr = (BuildLink_Request *)mac_converter_BLRequest.get_struct();
            BuildLink_Request blRequest = *blRequest_ptr;

            if (blRequest.if_build == 1)
            {
                cout << "Node " << nodeId << " 收到建链许可--" << mac_header.seq << "  ";
                printTime_ms();
            }
            else
            {
                cout << "Node " << nodeId << " 收到建链不许可--" << mac_header.seq << "  ";
                printTime_ms();
            }

            break;
        }
        }
    }
}