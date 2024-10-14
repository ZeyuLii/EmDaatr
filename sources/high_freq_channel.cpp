#include <chrono>

#include "macdaatr.h"
#include "common_struct.h"
#include "socket_control.h"
#include "struct_converter.h"
#include "beam_maintenance.h"
#include "highFreqToolFunc.h"
#include "timer.h"

using namespace std;
using namespace chrono;

#define DEBUG_SETUP 1
#define DEBUG_EXECUTION 1
#define DEBUG_ADJUST 1

/*********************** 此文件主要包含对高频信道通信相关函数实现 ***********************/

/*
 TODO 0720:
    Y1. 时域调整阶段的数据收发(补充发函数与收函数的事件处理函数) &
        频域调整阶段 (+ 干扰频点汇总 新调频图案等数据包的发送)
    Y2. 状态的切换 Setup->Exe->Adj 标志位的更改 在macdaatr.cpp中
    Y3. 执行阶段的读取数据包时，需要给数据 <加锁> 并把队列放进到 private 中
        (后者无法实现，因为有非成员函数操作队列，如 UpdatePosition)
    Y4. 接收上层缓冲区数据包并处理的事件处理函数
        -Y prior 数据包——放入队列 MSG_NETWORK_ReceiveNetworkPkt <加锁>
        -Y 链路构建需求——调整阶段
        -Y 身份配置信息调整与下发
        -Y 转发表与路由表
        -Y 本地飞行状态信息
    Y5. 周期性向网络层缓冲区发送数据包
        -Y 其他节点飞行状态信息 MAC_DAATR_SendOtherNodePosition
        -Y 本地链路状态信息 Send_LOcal_Link_Status()
    Y6. 核对 身份配置信息变更后，其他节点的定时器修改问题 (processNodeNotificationFromNet函数中, 频域相关)
    Y7. 射前时隙表生成函数 根据 node_num
    Y8. 接入相关
    Y9. 接收和生成频谱感知结果数据包 MAC_DAATR_SpectrumSensing (from PHY)
    0. 如需必要 将队列更改为set 重写出入队的操作
*/

/// @brief 高频信道发送线程函数
void MacDaatr::highFreqSendThread()
{
    // 首先检查此时的节点所处的阶段
    // -- 如果是 Mac_Initialiation 则发送内容为 建链请求/建链回复
    // -- 如果是 Mac_Exucation 则发送内容为 业务数据包
    // -- 如果是 Mac_Adjustment 则发送内容为 时隙表/ACK

    extern bool end_simulation;
    while (true)
    {
        // 首先更新时隙编号 slotId
        // 相当于每调用一次这个函数，业务信道的 slotId 就递增1
        currentSlotId = (currentSlotId == TRAFFIC_SLOT_NUMBER - 1) ? 0 : currentSlotId + 1;

        // 在此处放置一把保护队列中业务的锁，维持互斥，将在队列业务调度结束之后提前释放
        unique_lock<mutex> highthreadLock(highThreadSendMutex);
        highThread_condition_variable.wait(highthreadLock);

        if (isValid) // 掉链标志位
            continue;

        if (state_now == Mac_Initialization)
        { // -- 如果是 Mac_Initialiation 则发送内容为 建链请求/建链回复
            currentStatus = slottable_setup[currentSlotId].state;
            int dest_node = slottable_setup[currentSlotId].send_or_recv_node;

            if (currentStatus == DAATR_STATUS_TX && dest_node != 0) // TX
            {
                // 在建链阶段 普通节点发送建链请求 网管节点发送 回复建链请求
                if (nodeId == mana_node)
                {
                    if (dest_node <= subnet_node_num)
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
#ifdef DEBUG_SETUP
                        cout << "[" << currentSlotId << "T] 网管节点回复 Node " << dest_node << " 的建链请求--" << mac_header_ptr->seq << "  ";
                        printTime_ms();
#endif
                        delete mac_header_ptr;
                        delete temp_buf;
                    }
                }
                else
                {
                    if (dest_node <= subnet_node_num)
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
#ifdef DEBUG_SETUP
                        cout << "[" << currentSlotId << "T] Node " << nodeId << " 向网管节点发送建链请求--" << blTimes << " ";
                        printTime_ms();
#endif
                        delete mac_header_ptr;
                        delete temp_buf;
                    }
                }
            }
            else if (currentStatus == DAATR_STATUS_RX && dest_node != 0)
            {
                // 在建链阶段 普通节点收到建链请求的Approval 网管节点收到普通节点的建链请求
                // 调整波束指向 等
            }
            else // IDLE
            {
            }
        }
        else if (state_now == Mac_Execution)
        { // -- 如果是 Mac_Exucation 则发送内容为 业务数据包
            currentStatus = slottable_execution[currentSlotId].state;
            int dest_node = slottable_execution[currentSlotId].send_or_recv_node;

            // 全连接时隙
            bool FullConnection_slot = false;
            if ((currentSlotId % 20 < 8 || currentSlotId % 20 >= 16) && currentSlotId < 26)
                FullConnection_slot = true;

            if (currentStatus == DAATR_STATUS_TX && dest_node != 0 && dest_node <= subnet_node_num) // TX
            {
#ifdef DEBUG_EXECUTION
                cout << "[" << currentSlotId << "T] Node " << nodeId << "->" << dest_node << "  ";
                writeInfo("[%d T] Node %2d-> %2d  ", currentSlotId, nodeId, dest_node);
                printTime_ms();
#endif
                // 接入阶段 网管节点向接入节点发送新跳频图案
                if (access_state == DAATR_WAITING_TO_SEND_HOPPING_PARTTERN &&
                    waiting_to_access_node == dest_node && FullConnection_slot == true)
                {
                    MacHeader *mac_header = new MacHeader;
                    mac_header->is_mac_layer = 1;
                    mac_header->backup = 5;
                    mac_header->PDUtype = 0;
                    mac_header->packetLength = 563 + 25; // MFC_Spec + 包头25
                    mac_header->destAddr = waiting_to_access_node;
                    mac_header->srcAddr = nodeId;
                    mac_header->mac_pSynch = 1101; // 用来标识接入阶段发送的调频图案
                    mac_header->mac_backup = 1;

                    MacDaatr_struct_converter mac_converter1(1);
                    mac_converter1.set_struct((uint8_t *)mac_header, 1);
                    mac_converter1.daatr_struct_to_0_1();

                    MacDaatr_struct_converter subnet_parttern_ptr(10);
                    subnet_frequency_parttern *subnet_parttern_str = new subnet_frequency_parttern;
                    for (int i = 0; i < SUBNET_NODE_NUMBER_MAX; i++)
                    {
                        for (int j = 0; j < FREQUENCY_COUNT; j++)
                        {
                            subnet_parttern_str->subnet_node_hopping[i][j] = freqSeq[i][j];
                        }
                    }
                    subnet_parttern_ptr.set_struct((uint8_t *)subnet_parttern_str, 10);
                    subnet_parttern_ptr.daatr_struct_to_0_1();

                    mac_converter1 + subnet_parttern_ptr;

                    uint8_t *frame_ptr = mac_converter1.get_sequence();
                    uint32_t len = mac_converter1.get_length();
                    uint8_t *temp_buf = new uint8_t[len];
                    memcpy(temp_buf, frame_ptr, len);
                    macDaatrSocketHighFreq_Send(temp_buf, len, dest_node);

                    access_state = DAATR_NO_NEED_TO_ACCESS;
                    waiting_to_access_node = 0;

                    cout << "[" << currentSlotId << "T] 网管节点向接入节点发送新跳频图案  ";
                    printTime_ms();

                    delete mac_header;
                    delete subnet_parttern_str;
                    delete temp_buf;
                }

                double transSpeed = Calculate_Transmission_Rate_by_Distance(businessQueue[dest_node - 1].distance);
                double transData = transSpeed * HIGH_FREQ_SLOT_LEN / 8 - MAC_HEADER1_LENGTH;
                // 减去PDU1的包头数据量,才是真正一帧可传输的数据量

                int multi = 0;
                int skip_pkt = 0; // 表示这是大数据包(的个数), 无法放入此PDU, 跳过
                vector<msgFromControl> MFC_list_temp;

                for (int i = 0; i < TRAFFIC_CHANNEL_PRIORITY; i++)
                {
                    // i对应不同优先级
                    skip_pkt = 0;
                    for (int j = 0; j < TRAFFIC_MAX_BUSINESS_NUMBER; j++)
                    {
                        if (businessQueue[dest_node - 1].business[i][skip_pkt].priority == -1)
                            break;

                        if (businessQueue[dest_node - 1].business[i][0].busin_data.backup == 0)
                        // 第一个可发送业务是网络层下发的业务
                        // 向后遍历每一个业务,
                        // 查看 1.是否继续发送会大于可传输量 2.是否下一个业务是链路层业务
                        // 有任何一个条件不满足, 则停止遍历, 并将此前的所有业务打包发送
                        {
                            if ((transData >= businessQueue[dest_node - 1].business[i][skip_pkt].data_kbyte_sum) &&
                                Judge_If_Include_ReTrans(businessQueue[dest_node - 1].business[i][skip_pkt].busin_data, MFC_list_temp) == false)
                            {
                                transData -= businessQueue[dest_node - 1].business[i][skip_pkt].data_kbyte_sum;
                                multi += 1;
                                MFC_list_temp.push_back(businessQueue[dest_node - 1].business[i][skip_pkt].busin_data);

                                Business_Lead_Forward(this, dest_node, i, skip_pkt); // 此业务发送成功, 更新当前业务队列
                            }
                            else
                            {
                                skip_pkt += 1;
                                continue;
                            }
                        }
                        else // 队列中第一个业务是链路层自行下发的业务
                        {
                            msgFromControl busin_data = businessQueue[dest_node - 1].business[i][0].busin_data;
                            if (busin_data.packetLength == 73 && busin_data.msgType == 15 && busin_data.fragmentNum == 15) // 说明待发送的是频谱感知结果
                            {
                                // 设置包头
                                MacHeader *mac_header = new MacHeader;
                                mac_header->PDUtype = 0;
                                mac_header->is_mac_layer = 0;
                                mac_header->backup = 7;
                                mac_header->mac_backup = 1;
                                mac_header->packetLength = 73 + 25; // MFC_Spec + 包头25
                                mac_header->destAddr = mana_node;
                                mac_header->srcAddr = nodeId;

                                // 包头的 converter
                                MacDaatr_struct_converter mac_converter_PDU1(1);
                                mac_converter_PDU1.set_struct((uint8_t *)mac_header, 1);
                                mac_converter_PDU1.daatr_struct_to_0_1();

                                // 包头后的内容
                                vector<uint8_t> *MFC_Spec_temp = new vector<uint8_t>; // 存储盛放频谱感知结果的MFC的vector序列
                                MFC_Spec_temp = convert_PtrMFC_01(&busin_data);       // 将MFC转换为 01 seq, 使用vector存放
                                MacDaatr_struct_converter mac_converter_Spec(255);    // 设定converter, 类别为255
                                // (由于没有header + MFC的type, 所以设为255)
                                mac_converter_Spec.set_length(busin_data.packetLength);                            // 设定此converter的长度为MFC的长度73(固定长度)
                                mac_converter_Spec.daatr_MFCvector_to_0_1(MFC_Spec_temp, busin_data.packetLength); // 将MFC的vector加入converter的seq中
                                mac_converter_PDU1 + mac_converter_Spec;                                           // 将两个包拼接进 PDU

                                uint8_t *frame_ptr = mac_converter_PDU1.get_sequence(); // 将拼接好的PDU的seq提出 待发送
                                uint32_t len = mac_converter_PDU1.get_length();
                                uint8_t *temp_buf = new uint8_t[len];
                                memcpy(temp_buf, frame_ptr, len);
                                macDaatrSocketHighFreq_Send(temp_buf, len, dest_node); // 此处的的dest_node为网管节点

                                cout << "[" << currentSlotId << "T] Node " << nodeId << " 发送频谱感知结果 ";
                                printTime_ms();

                                Business_Lead_Forward(this, dest_node, i, 0);

                                delete mac_header;
                                delete MFC_Spec_temp;
                                delete temp_buf;
                            }
                        }
                    }
                }

                if (MFC_list_temp.size() != 0)
                {
                    MacHeader *mac_header = new MacHeader;
                    mac_header->PDUtype = 0;
                    mac_header->is_mac_layer = 0;
                    mac_header->backup = 0;
                    mac_header->mac_backup = multi;
#ifdef DEBUG_EXECUTION
                    cout << "节点 " << nodeId << " 向节点 " << dest_node << " 本次发送合包 " << multi << " 个 ";
                    writeInfo("本次发送合包");
                    printTime_ms();
#endif
                    mac_header->packetLength = 25; // 设定PDU包头的初始长度
                    mac_header->destAddr = dest_node;
                    mac_header->srcAddr = nodeId;

                    // 首先遍历所有待发送的MFC, 计算即将生成的PDU的长度,
                    // 更新包头位置的pktlength字段
                    for (auto mfc_t : MFC_list_temp)
                        mac_header->packetLength += mfc_t.packetLength;

                    MacDaatr_struct_converter mac_converter_PDU1(1);         // 此 converter 用来盛放包头, 设置类别为 1
                    mac_converter_PDU1.set_struct((uint8_t *)mac_header, 1); // 将上述包头与converter相关联
                    mac_converter_PDU1.daatr_struct_to_0_1();                // 将包头转换为01序列

                    vector<uint8_t> *MFC_Trans_temp = new vector<uint8_t>; // 盛放MFC转换的01序列
                    int PDU_TotalLength = mac_converter_PDU1.get_length(); // PDU_TotalLength 此时的长度只有 25 (是converter1的设定长度)

                    for (auto mfc_temp : MFC_list_temp) // 遍历待发送队列中的每个MFC
                    {
                        MFC_Trans_temp = convert_PtrMFC_01(&mfc_temp);     // 将MFC转换为01序列(使用vector盛放)
                        MacDaatr_struct_converter mac_converter_temp(255); // 设定混合类型的converter,

                        // 用来容纳MFC转换的01序列的seq
                        mac_converter_temp.set_length(mfc_temp.packetLength);
                        mac_converter_temp.daatr_MFCvector_to_0_1(MFC_Trans_temp, mfc_temp.packetLength);

                        mac_converter_PDU1 + mac_converter_temp; // 将新生成的MFC的seq添加至PDU1包头之后
                        PDU_TotalLength += mfc_temp.packetLength;
                        vector<uint8_t>().swap(*MFC_Trans_temp);
                        // MFC_Trans_temp->clear();
                    }
                    // MFC_list_temp.clear(); // 清空待发送队列
                    vector<msgFromControl>().swap(MFC_list_temp);

                    mac_converter_PDU1.set_length(PDU_TotalLength);
                    uint8_t *frame_ptr = mac_converter_PDU1.get_sequence();
                    uint32_t len = mac_converter_PDU1.get_length();
                    uint8_t *temp_buf = new uint8_t[len];
                    memcpy(temp_buf, frame_ptr, len);
                    macDaatrSocketHighFreq_Send(temp_buf, len, dest_node); // 此处的的dest_node为网管节点
#ifdef DEBUG_EXECUTION
                    cout << "[" << currentSlotId << "T] Node " << nodeId << " 发送数据包合包 ";
                    printTime_ms();
#endif
                    delete mac_header;
                    delete MFC_Trans_temp;
                    delete temp_buf;
                }

                // for (int i = 0; i < SUBNET_NODE_NUMBER_MAX; i++)
                // {
                //     for (int j = 0; j < TRAFFIC_CHANNEL_PRIORITY; j++)
                //     {
                //         for (int k = 0; k < TRAFFIC_MAX_BUSINESS_NUMBER; k++)
                //         {
                //             if (businessQueue[i].business[j][k].priority != -1)
                //                 businessQueue[i].business[j][k].waiting_time += 1;
                //         }
                //     }
                // }

                // MFC_Priority_Adjustment(this);
            }
            else if (currentStatus == DAATR_STATUS_RX && dest_node != 0)
            {
#ifdef DEBUG_EXECUTION
                // 在执行阶段 收到的内容是 数据包 和 频谱感知结果
                cout << "[" << currentSlotId << "R] Node " << nodeId << "<-" << dest_node << "  ";
                writeInfo("[%d R] Node %2d 收到本层数据包并传往上层 ", currentSlotId, nodeId);
                printTime_ms();
#endif
            }
            else // IDLE
            {
#ifdef DEBUG_EXECUTION
                // cout << "[" << currentSlotId << "IDLE] Node " << nodeId << endl;
#endif
            }

            // 在此维护层间缓冲区的写操作
            // (1000 * X + 20) ms 时向上层缓冲区发送本地链路状态 Send_Local_Link_Status()
            if (currentSlotId == 8) // 8 * 2.5 = 20
            {
                sendLocalLinkStatus(this);
                sendOtherNodePosition(this);
            }
        }
        else if (state_now == Mac_Adjustment_Slot)
        {
            currentStatus = slottable_adjustment[currentSlotId].state;
            int dest_node = slottable_adjustment[currentSlotId].send_or_recv_node;

            // 在调整阶段的最后一个时隙，将状态切换为 Mac_Execution(保险起见设为倒数第二个)
            if (currentSlotId == TRAFFIC_SLOT_NUMBER - 2)
                state_now = Mac_Execution;

            if (currentStatus == DAATR_STATUS_TX && dest_node != 0) // TX
            {
                if (nodeId == mana_node) // 网管节点发送时隙表
                {
                    if (dest_node <= subnet_node_num)
                    {
                        MacHeader *mac_header = new MacHeader;
                        mac_header->PDUtype = 0;
                        mac_header->is_mac_layer = 1 + (stACKTimes++) % 3;
                        mac_header->backup = 4;
                        mac_header->packetLength = 25 + 550;
                        mac_header->srcAddr = nodeId; // 由网管节点发送
                        mac_header->destAddr = dest_node;
                        mac_header->seq = 1 + (slottableTimes++) % 3;

                        MacDaatr_struct_converter mac_converter_PDU1(1);
                        mac_converter_PDU1.set_struct((uint8_t *)mac_header, 1);
                        mac_converter_PDU1.daatr_struct_to_0_1();

                        MacDaatr_struct_converter mac_converter_Slottable(7);
                        mac_converter_Slottable.set_length(550);
                        highFreqSlottable *New_Slottable_ptr = new highFreqSlottable;
                        for (int j = 0; j < TRAFFIC_SLOT_NUMBER; j++)
                        {
                            New_Slottable_ptr->slot_traffic_execution_new[j] =
                                slot_traffic_execution_new[dest_node - 1][j];
                        }
                        mac_converter_Slottable.set_struct((uint8_t *)New_Slottable_ptr, 7);
                        mac_converter_Slottable.daatr_struct_to_0_1();

                        mac_converter_PDU1 + mac_converter_Slottable;

                        uint8_t *frame_ptr = mac_converter_PDU1.get_sequence();
                        uint32_t len = mac_converter_PDU1.get_length();
                        uint8_t *temp_buf = new uint8_t[len];
                        memcpy(temp_buf, frame_ptr, len);
                        macDaatrSocketHighFreq_Send(temp_buf, len, dest_node); // 此处的的dest_node为待发送时隙表节点
#ifdef DEBUG_ADJUST
                        if (dest_node <= subnet_node_num)
                        {
                            cout << "[调整阶段 " << currentSlotId << "T] 网管节点向 Node " << dest_node
                                 << " 发送时隙表  ";
                            printTime_ms();
                        }
#endif
                        delete mac_header;
                        delete New_Slottable_ptr;
                        delete temp_buf;
                    }
                }
                else // 普通节点向网管节点发送ACK
                {
                    MacHeader *mac_header = new MacHeader;
                    mac_header->PDUtype = 0;
                    mac_header->is_mac_layer = 1 + (stACKTimes++) % 3;
                    mac_header->backup = 6;
                    mac_header->packetLength = 25;
                    mac_header->srcAddr = nodeId; // 由网管节点发送
                    mac_header->destAddr = dest_node;
                    mac_header->seq = 1 + (stACKTimes++) % 3;

                    MacDaatr_struct_converter mac_converter_PDU1(1);
                    mac_converter_PDU1.set_struct((uint8_t *)mac_header, 1);
                    mac_converter_PDU1.daatr_struct_to_0_1();

                    uint8_t *frame_ptr = mac_converter_PDU1.get_sequence();
                    uint32_t len = mac_converter_PDU1.get_length();
                    uint8_t *temp_buf = new uint8_t[len];
                    memcpy(temp_buf, frame_ptr, len);
                    macDaatrSocketHighFreq_Send(temp_buf, len, dest_node); // 此处的的dest_node为待发送时隙表节点
#ifdef DEBUG_ADJUST
                    cout << "[调整阶段 " << currentSlotId << "T] Node " << dest_node
                         << " 向网管节点发送时隙表ACK  ";
                    printTime_ms();
#endif
                    delete mac_header;
                    delete temp_buf;
                }
            }
            else if (currentStatus == DAATR_STATUS_RX && dest_node != 0)
            {
                // cout << "[" << currentSlotId << "R] Node " << nodeId << "<-" << dest_node << endl;
                // 在执行阶段 收到的内容是 时隙表 和 接收时隙表ACK
            }
            else
            {
                // cout << "[" << currentSlotId << "IDLE] Node " << nodeId << endl;
                // IDLE
            }
        }
        else if (state_now == Mac_Adjustment_Frequency)
        {
            currentStatus = slottable_adjustment[currentSlotId].state;
            int dest_node = slottable_adjustment[currentSlotId].send_or_recv_node;

            // 在调整阶段的最后一个时隙，将状态切换为 Mac_Execution(保险起见设为倒数第二个)
            if (currentSlotId == TRAFFIC_SLOT_NUMBER - 2)
                state_now = Mac_Execution;

            if (currentStatus == DAATR_STATUS_TX && dest_node != 0) // TX
            {
                if (nodeId == mana_node) // 网管节点发送跳频图案
                {
                    if (dest_node <= subnet_node_num)

                    {
                        MacHeader *mac_header = new MacHeader;
                        mac_header->PDUtype = 0;
                        mac_header->is_mac_layer = 1 + (seqACKTimes++) % 3;
                        mac_header->backup = 5;
                        mac_header->packetLength = 25 + 563;
                        mac_header->srcAddr = nodeId; // 由网管节点发送
                        mac_header->destAddr = dest_node;
                        mac_header->seq = 1 + (sequenceTimes++) % 3;

                        MacDaatr_struct_converter mac_converter_PDU1(1);
                        mac_converter_PDU1.set_struct((uint8_t *)mac_header, 1);
                        mac_converter_PDU1.daatr_struct_to_0_1();

                        MacDaatr_struct_converter mac_converter_Sequence(10);
                        mac_converter_Sequence.set_length(563);
                        subnet_frequency_parttern *New_freq_sequence_ptr = new subnet_frequency_parttern;
                        for (int j = 0; j < SUBNET_NODE_NUMBER_MAX; j++)
                        {
                            for (int k = 0; k < FREQUENCY_COUNT; k++)
                            {
                                New_freq_sequence_ptr->subnet_node_hopping[j][k] = freqSeq[j][k];
                            }
                        }
                        mac_converter_Sequence.set_struct((uint8_t *)New_freq_sequence_ptr, 10);
                        mac_converter_Sequence.daatr_struct_to_0_1();

                        mac_converter_PDU1 + mac_converter_Sequence;

                        uint8_t *frame_ptr = mac_converter_PDU1.get_sequence();
                        uint32_t len = mac_converter_PDU1.get_length();
                        uint8_t *temp_buf = new uint8_t[len];
                        memcpy(temp_buf, frame_ptr, len);
                        macDaatrSocketHighFreq_Send(temp_buf, len, dest_node); // 此处的的dest_node为待发送时隙表节点
#ifdef DEBUG_ADJUST
                        if (dest_node <= subnet_node_num)
                        {
                            cout << "[调整阶段 " << currentSlotId << "T] 网管节点向 Node " << dest_node
                                 << " 发送跳频图案  ";
                            printTime_ms();
                        }
#endif
                        delete mac_header;
                        delete New_freq_sequence_ptr;
                        delete temp_buf;
                    }
                }
                else // 普通节点向网管节点发送ACK
                {
                    MacHeader *mac_header = new MacHeader;
                    mac_header->PDUtype = 0;
                    mac_header->is_mac_layer = 1 + (seqACKTimes++) % 3;
                    mac_header->backup = 6;
                    mac_header->packetLength = 25;
                    mac_header->srcAddr = nodeId; // 由网管节点发送
                    mac_header->destAddr = dest_node;
                    mac_header->seq = 1 + (seqACKTimes++) % 3;

                    MacDaatr_struct_converter mac_converter_PDU1(1);
                    mac_converter_PDU1.set_struct((uint8_t *)mac_header, 1);
                    mac_converter_PDU1.daatr_struct_to_0_1();

                    uint8_t *frame_ptr = mac_converter_PDU1.get_sequence();
                    uint32_t len = mac_converter_PDU1.get_length();
                    uint8_t *temp_buf = new uint8_t[len];
                    memcpy(temp_buf, frame_ptr, len);
                    macDaatrSocketHighFreq_Send(temp_buf, len, dest_node); // 此处的的dest_node为待发送时隙表节点
#ifdef DEBUG_ADJUST
                    cout << "[调整阶段 " << currentSlotId << "T] Node " << dest_node
                         << " 向网管节点发送跳频图案ACK  ";
                    printTime_ms();
#endif
                    delete mac_header;
                    delete temp_buf;
                }
            }
            else if (currentStatus == DAATR_STATUS_RX && dest_node != 0)
            {
                // cout << "[" << currentSlotId << "R] Node " << nodeId << "<-" << dest_node << endl;
            }
            else
            {
                // cout << "[" << currentSlotId << "IDLE] Node " << nodeId << endl;
                // IDLE
            }
        }

        if (end_simulation == true)
        {
            sleep(1);
            cout << "NODE  " << nodeId << " HighSend   Thread is Over" << endl;
            break;
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

    if (mac_header.is_mac_layer == 0) // 上层数据包 + 频谱感知
    {
        int Total_MFC_num = mac_header.mac_backup; // 读取backup字段得到PDU中的MFC数量

        // cout << "Node " << nodeId << " 收到合包 " << Total_MFC_num << " 个  ";
        // printTime_ms();

        MacDaatr_struct_converter mac_converter_MFCs(99); // 用来盛放PDU中除去包头外的若干紧邻的MFC的01序列
        mac_converter_MFCs - mac_converter;
        mac_converter_MFCs.set_length(frame_length - 25);

        vector<uint8_t> *MFC_vector_temp = new vector<uint8_t>;
        msgFromControl *MFC_temp = new msgFromControl;
        int mfc_count_upper = 0, mfc_count_trans = 0;
        for (int i = 0; i < Total_MFC_num; i++)
        {
            MFC_vector_temp = mac_converter_MFCs.Get_Single_MFC();
            MFC_temp = convert_01_MFCPtr(MFC_vector_temp); // 将vector形式的01序列转为MFC
            // cout << MFC_temp->destAddr << " & " << MFC_temp->fragmentNum << endl;

            if (MFC_temp->destAddr == nodeId)
            {
                if (MFC_temp->packetLength == 73 && MFC_temp->msgType == 15 && MFC_temp->fragmentNum == 15) // 频谱感知结果
                {
                    msgFromControl *Spec_MFC = convert_01_MFCPtr(MFC_vector_temp);
                    vector<uint8_t> *sensing_vector = (vector<uint8_t> *)Spec_MFC->data; // 将data字段提取出来, 按照西电的标准是vector

                    uint8_t *sensing_seq = new uint8_t[63];
                    for (int i = 0; i < 63; i++)
                    {
                        sensing_seq[i] = (*sensing_vector)[i];
                    }
                    MacDaatr_struct_converter mac_converter_Spec_2(5);     // 存放 SpecSensing 的converter
                    mac_converter_Spec_2.set_bit_sequence(sensing_seq, 5); // 此行为了兼容 type 5 的写法,
                    // 将01序列的频谱感知结果赋给converter
                    mac_converter_Spec_2.daatr_0_1_to_struct(); // 将频谱感知结果的01序列转换为结构体
                    spectrum_sensing_struct *Spec_struct = (spectrum_sensing_struct *)mac_converter_Spec_2.get_struct();

                    int sendnode = Spec_MFC->srcAddr;
                    cout << "[" << currentSlotId << "R] 网管节点收到来自 Node " << sendnode << " 的频谱感知结果  ";
                    printTime_ms();

                    for (int i = 0; i < TOTAL_FREQ_POINT; i++)
                    {
                        spectrum_sensing_sum[sendnode - 1][i] = Spec_struct->spectrum_sensing[i];
                    }
                    spectrum_sensing_sum[sendnode - 1][TOTAL_FREQ_POINT] = 1;

                    delete sensing_seq;
                }
                else // 上层数据包
                {
                    mfc_count_upper++;
                    // cout << "[" << currentSlotId << "R] Node " << nodeId
                    //      << " 收到本层数据包并传往上层  " << mfc_count_upper << " / "
                    //      << Total_MFC_num << endl;
                    // writeInfo("[%d R] Node %2d 收到本层数据包并传往上层 ", currentSlotId, nodeId);
                    macToNetworkBufferHandle(MFC_vector_temp->data(), 0x10, MFC_vector_temp->size());
                }
            }
            else
            {
                // 查询路由表 mfc_temp->destaddr --->>> macpacket->destaddr
                // 更改MFC的destAddr, 并将其放入对应的业务信道队列中

                MAC_NetworkLayerHasPacketToSend(MFC_temp);
                mfc_count_trans++;
                // cout << nodeId << ": " << MFC_temp->srcAddr << "-" << MFC_temp->destAddr << endl;
                // cout << "[" << currentSlotId << "R] Node " << nodeId
                //      << " 收到非本节点消息 转发对应节点  " << mfc_count_trans << " / "
                //      << Total_MFC_num << endl;
            }
        }
        delete MFC_vector_temp;
        delete MFC_temp;
    }
    else // 本层数据包
    {
        switch (mac_header.backup)
        {
        case 1: // 建链请求 只有网管节点可以收到
        {
            // 检查此时的接收节点是不是网管节点
            if (nodeId != mana_node)
                cout << "Node " << nodeId << " Wrong Recv Node of BuildLink Request!" << endl;

            MacDaatr_struct_converter mac_converter_BLRequest(6);
            mac_converter_BLRequest - mac_converter;
            // mac_converter_BLRequest.set_length(2);
            mac_converter_BLRequest.daatr_0_1_to_struct();

#ifdef DEBUG_SETUP
            BuildLink_Request *blRequest_ptr = (BuildLink_Request *)mac_converter_BLRequest.get_struct();
            BuildLink_Request blRequest = *blRequest_ptr;
            cout << "[" << currentSlotId << "R] 网管节点收到 Node "
                 << blRequest.nodeID << " 的建链请求--" << mac_header.seq + 1 << "  ";
            printTime_ms();
#endif
            break;
        }
        case 2: // 建链回复 只有普通节点可以收到
        {
            // 检查此时的接收节点是不是普通节点
            if (nodeId == mana_node)
                cout << "Node " << nodeId << " Wrong Recv Node of BuildLink Response!" << endl;

            MacDaatr_struct_converter mac_converter_BLRequest(6);
            mac_converter_BLRequest - mac_converter;
            mac_converter_BLRequest.daatr_0_1_to_struct();

#ifdef DEBUG_SETUP
            BuildLink_Request *blRequest_ptr = (BuildLink_Request *)mac_converter_BLRequest.get_struct();
            BuildLink_Request blRequest = *blRequest_ptr;

            if (blRequest.if_build == 1)
            {
                cout << "[" << currentSlotId << "R] Node " << nodeId
                     << " 收到建链许可--" << mac_header.seq << "  ";
                printTime_ms();
            }
            else
            {
                cout << "[" << currentSlotId << "R] Node " << nodeId
                     << " 收到建链不许可--" << mac_header.seq << "  ";
                printTime_ms();
            }
#endif
            break;
        }
        case 4: // 普通节点收到时隙表
        {
            // 检查此时的接收节点是不是普通节点
            if (nodeId == mana_node)
                cout << "Node " << nodeId << " Wrong Recv Node of Slottable!" << endl;

            MacDaatr_struct_converter mac_converter_Slottable(7);
            mac_converter_Slottable - mac_converter;
            mac_converter_Slottable.daatr_0_1_to_struct();

            highFreqSlottable *slottable_ptr =
                (highFreqSlottable *)mac_converter_Slottable.get_struct();
            for (int i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
            {
                slottable_execution[i] = slottable_ptr->slot_traffic_execution_new[i];
            }
            receivedSlottableTimes += 1;
            cout << endl;
            cout << "[调整阶段] Node " << nodeId << " 收到新时隙表 -- "
                 << receivedSlottableTimes << "             ";
            printTime_ms();

            if (receivedSlottableTimes == 1)
            {
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
                    {
                        cout << "|IDLE";
                    }
                }
                cout << endl;
            }
            break;
        }
        case 5: // 普通节点收到调频图案
        {
            // 检查此时的接收网管节点收到节点是不是普通节点
            if (nodeId == mana_node)
                cout << "Node " << nodeId << " Wrong Recv Node of Sequence!" << endl;

            MacDaatr_struct_converter mac_converter_Sequence(10);
            mac_converter_Sequence - mac_converter;
            mac_converter_Sequence.daatr_0_1_to_struct();

            subnet_frequency_parttern *sequence_ptr =
                (subnet_frequency_parttern *)mac_converter_Sequence.get_struct();
            for (int i = 0; i < subnet_node_num; i++)
            {
                for (int j = 0; j < FREQUENCY_COUNT; j++)
                {
                    freqSeq[i][j] = sequence_ptr->subnet_node_hopping[i][j];
                    // 将接受到的网管节点发送的跳频序列存储到自己节点的调频序列中
                }
            }

            // 节点在接入阶段接收到来自网管节点的跳频图案
            if (mac_header.mac_pSynch == 1101)
            {
                state_now = Mac_Execution;
                access_state = DAATR_NO_NEED_TO_ACCESS;

                cout << "[接入阶段-HIGH] Node " << nodeId << " 收到新调频图案 ";
                printTime_ms();
            }
            else
            {
                cout << mac_header.mac_pSynch << endl;
                receivedSequenceTimes = (receivedSequenceTimes + 1) % 3;
                cout << endl;
                cout << "[调整阶段-HIGH] Node " << nodeId << " 收到新调频图案-- "
                     << receivedSequenceTimes << "  ";
                printTime_ms();
            }
            break;
        }
        case 6: // 网管节点收到时隙表ACK
        {
            // 检查此时的接收节点是不是网管节点
            if (nodeId != mana_node)
                cout << "Node " << nodeId << " Wrong Recv Node of ACK!" << endl;

#ifdef DEBUG_ADJUST
            cout << "[调整阶段 " << currentSlotId << "R] 网管节点收到 Node "
                 << mac_header.srcAddr << " 的ACK  ";
            printTime_ms();
#endif
            break;
        }
        }
    }
}