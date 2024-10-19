#include "macdaatr.h"
#include "low_freq_channel.h"
#include "common_struct.h"
#include "socket_control.h"
#include "struct_converter.h"
#include "beam_maintenance.h"
#include "highFreqToolFunc.h"
#include "timer.h"

#include <string.h>

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
using namespace std;

/*****************************此文件主要包含对低频信道通信相关函数实现******************************/

/**
 * 此函数将业务添加到本节点待发送网管信道业务队列中
 * @param busin 指向控制消息的指针，包含待发送的业务信息
 * @param macdata_daatr 指向MAC数据模块数据结构的指针，包含业务队列信息
 * @return 返回true表示成功找到空闲队列位置并添加了业务，返回false表示业务队列已满无法添加新业务
 */
bool MacDaatr::MacDaatNetworkHasLowFreqChannelPacketToSend(msgFromControl *busin)
{
    int i = 0;
    int j = 0;
    int flag = 0; // 表征是否成功将busin插入业务队列

    lock_mana_channel.lock();
    for (i = 0; i < MANA_MAX_BUSINESS_NUMBER; i++)
    {
        if (low_freq_channel_business_queue[i].state == 0)
        {                                                           // 此队列对应位置没有被占用
            low_freq_channel_business_queue[i].busin_data = *busin; // 将此业务添加进队列
            low_freq_channel_business_queue[i].state = 1;           // 将此队列位置占用
            flag = 1;                                               // 证明已添加队列
            // cout << "业务目的地址为" << macdata_daatr->low_freq_channel_business_queue[i].busin_data.destAddr << endl;
            break;
        }
    }
    if (flag == 0)
    {
        cout << "网管信道队列已满" << endl;
    }
    // delete busin;//删除发送的业务内存空间（？是否需要删除）
    lock_mana_channel.unlock();
    return flag;
}

// 搜寻对应节点时隙位置
static vector<uint8_t> *MacDaatrSearchSlotLocation(int nodeId, MacDaatr *macdata_daatr)
{
    int i = 0;
    vector<uint8_t> *slot_location = new vector<uint8_t>;
    for (i = 0; i < MANAGEMENT_SLOT_NUMBER_OTHER_STAGE; i++)
    {
        if (macdata_daatr->low_freq_other_stage_slot_table[i].nodeId == nodeId)
        {
            slot_location->push_back(i);
        }
    }
    return slot_location;
}

/**
 * 从低频通道业务队列中获取业务数据
 * @param busin 指向要获取的业务数据的指针
 * @param macdata_daatr 指向MAC数据结构的指针，其中包含业务队列
 * @return 如果成功获取业务数据，则返回true；否则返回false
 *
 * 该函数用于从低频通道业务队列中取出第一个处于活动状态的业务数据，
 * 并将该业务数据赋值给传入的busin参数。同时，队列中的其他业务数据
 * 向前移动一位，以腾出空间并保持队列的有序性。如果队列为空，则不进行
 * 任何操作并返回false。
 */
bool MacDaatr::getLowChannelBusinessFromQueue(msgFromControl *busin)
{
    int i = 0;
    bool flag = false;
    lock_mana_channel.lock();
    if (low_freq_channel_business_queue[i].state == 1)
    {                                                           // 证明当前队列有业务
        *busin = low_freq_channel_business_queue[i].busin_data; // 将当前队列中的业务赋值给输入的空白业务
        for (i = 0; i < MANA_MAX_BUSINESS_NUMBER - 1; i++)
        { // 将当前业务队列中的业务前移一位
            low_freq_channel_business_queue[i] = low_freq_channel_business_queue[i + 1];
        }
        low_freq_channel_business_queue[MANA_MAX_BUSINESS_NUMBER - 1].state = 0; // 队列尾清空
        // cout << "已成功从网管信道队列中取出业务,"
        //      << "目的地址为：" << busin->destAddr << endl;
        flag = true;
    }
    else
    {
        // cout << "网管信道队列业务为空" << endl;
        writeInfo("网管信道队列业务为空");
    }
    lock_mana_channel.unlock();
    return flag;
}

static void judgeIfBackToAccess()
{
    extern MacDaatr daatr_str;
    if (daatr_str.access_state == DAATR_HAS_SENT_ACCESS_REQUEST) // 断开节点已发送接入请求
    {                                                            // 如果节点尚未收到网管节点请求接入回复
        cout << "Node " << daatr_str.nodeId << " ";
        cout << "等待接入请求回复时间超过阈值, 结束等待阶段, 重新进入调整状态, 执行随机退避策略" << endl;
        int num = rand() % (MAX_RANDOM_BACKOFF_NUM + 1);
        daatr_str.access_state = DAATR_NEED_ACCESS;
        daatr_str.access_backoff_number = num;
    }
    else if (daatr_str.access_state == DAATR_WAITING_TO_ACCESS)
    { // 如果节点已收到同意接入, 但是未收到全网跳频图案, 此时可能出现丢包, 重新进入接入状态
        cout << "Node " << daatr_str.nodeId << " ";
        cout << "等待全网跳频图案接收时间时间超过阈值, 重新进入接入状态" << endl;
        daatr_str.access_state = DAATR_NEED_ACCESS;
    }
}

// 低频信道 发送线程 每20ms被调用一次
// 在调用的时候查看当前的状态信息
void lowFreqSendThread()
{
    extern MacDaatr daatr_str;
    extern bool end_simulation;
    MacState state_now = daatr_str.state_now;
    int Spectrum_sensing_file = 0;
    int file_open_time = 0;
    int timeadd = 0;
    bool file_open_flag = 1;
    unique_lock<mutex> lowthreadlock(daatr_str.lowthreadmutex);
    // 修改线程优先级
    pthread_t tid = pthread_self();
    int policy;
    struct sched_param param;
    policy = SCHED_RR;         // 使用实时调度策略
    param.sched_priority = 10; // 设置优先级，值越高优先级越高
    if (pthread_setschedparam(tid, policy, &param) != 0)
    {
        perror("pthread_setschedparam");
        exit(EXIT_FAILURE);
    }
    //...........................//
    while (1)
    {
        daatr_str.lowthreadcondition_variable.wait(lowthreadlock);
        writeInfo("时隙:%2d---------------------------------------------------------------------", daatr_str.low_slottable_should_read);
        state_now = daatr_str.state_now;
        int slot_num = daatr_str.low_slottable_should_read;
        daatr_str.low_slottable_should_read++;
        if (state_now == Mac_Initialization)
        { // 建链阶段
            if (daatr_str.low_slottable_should_read == 25)
            { // 经过一帧
                daatr_str.low_slottable_should_read = 0;
            }
        }
        else
        {
            if (daatr_str.low_slottable_should_read == 50)
            { // 经过一帧
                daatr_str.low_slottable_should_read = 0;
            }
        }

        // 读取频谱感知结果
        if (file_open_flag && timeadd == 300)
        {
            char filePath[100];
            char spec_temp_buffer[TOTAL_FREQ_POINT + 1] = {0};
            sprintf(filePath, "/nfs/Spectrum_sensing/NODE%d/time%d.txt", daatr_str.nodeId, file_open_time);
            Spectrum_sensing_file = open(filePath, O_RDONLY);

            if (!Spectrum_sensing_file)
                file_open_flag = 0;

            if (file_open_flag)
            {
                int resnum = read(Spectrum_sensing_file, spec_temp_buffer, TOTAL_FREQ_POINT + 1);
                if (resnum == -1)
                {
                    perror("read()");
                }
                for (int i = 0; i < TOTAL_FREQ_POINT; i++)
                {
                    if (spec_temp_buffer[i] == '0')
                        daatr_str.spectrum_sensing_node[i] = 0;
                    else
                        daatr_str.spectrum_sensing_node[i] = 1;
                }
                close(Spectrum_sensing_file);
                file_open_time++;
                timeadd = 0;

                if (daatr_str.nodeId != daatr_str.mana_node)
                {
                    MacDaatr_struct_converter mac_converter_Spec(5);
                    mac_converter_Spec.set_struct((uint8_t *)daatr_str.spectrum_sensing_node, 5);
                    mac_converter_Spec.daatr_freq_sense_to_0_1();
                    uint8_t *bit_seq = mac_converter_Spec.get_sequence(); // 63 byte

                    vector<uint8_t> *SpecData = new vector<uint8_t>;
                    for (int i = 0; i < 63; i++)
                    {
                        SpecData->push_back(bit_seq[i]);
                    }

                    msgFromControl *MFC_temp = new msgFromControl;
                    MFC_temp->srcAddr = daatr_str.nodeId;
                    MFC_temp->destAddr = daatr_str.mana_node; // 给网管节点发送
                    MFC_temp->backup = 1;                     // 标识本层MFC数据包
                    MFC_temp->data = (char *)SpecData;
                    MFC_temp->priority = 0; // 最高优先级
                    MFC_temp->packetLength = 73;
                    MFC_temp->msgType = 15;
                    MFC_temp->fragmentNum = 15; // 以上三项都是15标识频谱感知结果

                    cout << "节点 " << daatr_str.nodeId << " 将结果存入业务队列中" << endl;
                    daatr_str.MAC_NetworkLayerHasPacketToSend(MFC_temp); // 将此业务添加进业务信道队列
                }
                else
                {
                    for (int i = 0; i < TOTAL_FREQ_POINT; i++)
                        daatr_str.spectrum_sensing_sum[daatr_str.mana_node - 1][i] = daatr_str.spectrum_sensing_node[i];
                }

                // 只有网管节点会对频域调整阶段进行判定
                if (daatr_str.nodeId == daatr_str.mana_node)
                {
                    judgeIfEnterFreqAdjustment(&daatr_str);
                }
            }

            _writeInfo(0, "当前频谱感知结果为:\n");
            char temp[TOTAL_FREQ_POINT];
            int count = 0;
            for (int i = 1; i < TOTAL_FREQ_POINT + 1; i++)
            {
                count += sprintf(temp + count, "%d", daatr_str.spectrum_sensing_node[i - 1]);
                if (i % 100 == 0 || i == TOTAL_FREQ_POINT)
                {
                    sprintf(temp + count, "\n");
                    _writeInfo(0, temp);
                    memset(temp, 0, TOTAL_FREQ_POINT);
                    count = 0;
                }
            }
        }
        timeadd++;

        if (daatr_str.isValid)
            continue;

        if (state_now == Mac_Initialization)
        { // 建链阶段
            if (daatr_str.low_slottable_should_read == 25)
            { // 经过一帧
                daatr_str.low_slottable_should_read = 0;
            }
            // 向物理层发送信道参数

            if (daatr_str.low_freq_link_build_slot_table[slot_num].nodeId == daatr_str.nodeId &&
                daatr_str.low_freq_link_build_slot_table[slot_num].state == DAATR_STATUS_FLIGHTSTATUS_SEND)
            { // 飞行状态信息广播时隙
                MacHeader2 *mac_header2_ptr = new MacHeader2;
                // macpacket_daatr1.mac_header = new MacHeader;
                mac_header2_ptr->PDUtype = 1;
                mac_header2_ptr->srcAddr = daatr_str.nodeId;       // 源地址
                mac_header2_ptr->destAddr = 0x3ff;                 // 目的地址全1
                mac_header2_ptr->packetLength = 42;                // 帧长度
                mac_header2_ptr->backup = 1;                       // 备用字段为1
                mac_header2_ptr->fragment_tail_identification = 1; // 分片尾标识
                FlightStatus flight_sta = daatr_str.local_node_position_info;
                Low_Freq_Packet_Type packet_type;
                if (daatr_str.node_type != Node_Management)
                {
                    packet_type.type = 4; // 普通节点飞行状态信息通告
                }
                else
                {
                    packet_type.type = 5; // 网管节点飞行状态信息通告
                }
                MacDaatr_struct_converter mac_converter1(2); // PDU2
                mac_converter1.set_struct((uint8_t *)mac_header2_ptr, 2);
                mac_converter1.daatr_struct_to_0_1();
                MacDaatr_struct_converter mac_converter2(3); // 数据
                mac_converter2.set_struct((uint8_t *)&flight_sta, 3);
                mac_converter2.daatr_struct_to_0_1();
                MacDaatr_struct_converter mac_converter3(8); // 数据类型字段
                mac_converter3.set_struct((uint8_t *)&packet_type, 8);
                mac_converter3.daatr_struct_to_0_1();
                mac_converter1 + mac_converter3 + mac_converter2; // 合包
                uint8_t *frame_ptr = mac_converter1.get_sequence();
                uint32_t len = mac_converter1.get_length();
                uint8_t *temp_buf = new uint8_t[len]; // 此处只是为了防止转换类中的bit指针被释放，所以保险起见复制一份，也可以尝试直接使用
                memcpy(temp_buf, frame_ptr, len);
                daatr_str.macDaatrSocketLowFreq_Send(temp_buf, len); // 发送

                writeInfo("NODE %2d 发送飞行状态信息", daatr_str.nodeId);
                cout << "[建链阶段] Node " << daatr_str.nodeId << " 广播飞行状态信息  ";
                printTime_ms();
                delete temp_buf;
                delete mac_header2_ptr;
            }
            else
            { // 接收，向物理层发送信道参数
            }
        }
        else
        { // 非建链阶段

            if (daatr_str.low_slottable_should_read == 50)
            { // 经过一帧
                daatr_str.low_slottable_should_read = 0;
            }
            // 向物理层发送信道参数

            if (daatr_str.low_freq_other_stage_slot_table[slot_num].nodeId == daatr_str.nodeId ||
                daatr_str.low_freq_other_stage_slot_table[slot_num].state == DAATR_STATUS_ACCESS_REQUEST)
            { // 如果此时隙是本节点发送时隙，或者是接入请求时隙
                uint16_t slot_state = daatr_str.low_freq_other_stage_slot_table[slot_num].state;
                switch (slot_state)
                {
                case DAATR_STATUS_ACCESS_REQUEST:
                { // 接入请求
                    if (daatr_str.state_now != Mac_Access)
                    { // 节点未处于接入状态, 不需要广播接入请求
                      // printTime_ms(); // 打印时间
                      // cout << "[接入广播-MANA] Node ID " << node->nodeId << " 未处于接入状态, 非建链阶段不需要广播 网管信道 接入请求 ";
                      // cout << "Time : " << time << "ms" << endl;
                      // temp_Management_Channel_Information.SendOrReceive = 2;
                    }
                    else if (daatr_str.access_state == DAATR_NEED_ACCESS && daatr_str.state_now == Mac_Access)
                    { // 节点处于接入状态,且未发送接入请求, 需要广播接入请求
                        if (daatr_str.access_backoff_number == 0)
                        { // 如果等待退避数目时隙数为0
                            // cout << endl;
                            // printTime_ms(); // 打印时间
                            MacHeader2 *mac_header2_ptr = new MacHeader2;
                            mac_header2_ptr->PDUtype = 1;
                            mac_header2_ptr->srcAddr = daatr_str.nodeId;
                            mac_header2_ptr->destAddr = daatr_str.mana_node;   // 目的地址为网管节点
                            mac_header2_ptr->packetLength = 18;                // 帧长度17 + 1
                            mac_header2_ptr->backup = 1;                       // 备用字段为1
                            mac_header2_ptr->fragment_tail_identification = 1; // 分片尾标识
                            Low_Freq_Packet_Type packet_type;
                            packet_type.type = 1; // 随遇接入请求
                            MacDaatr_struct_converter mac_converter1(2);
                            mac_converter1.set_struct((uint8_t *)mac_header2_ptr, 2);
                            mac_converter1.daatr_struct_to_0_1();
                            MacDaatr_struct_converter mac_converter2(8);
                            mac_converter2.set_struct((uint8_t *)&packet_type, 8);
                            mac_converter2.daatr_struct_to_0_1();
                            mac_converter1 + mac_converter2; // 合包
                            mac_converter1.daatr_mana_channel_add_0_to_full();
                            uint8_t *frame_ptr = mac_converter1.get_sequence();
                            uint32_t len = mac_converter1.get_length();
                            uint8_t *temp_buf = new uint8_t[len]; // 此处只是为了防止转换类中的bit指针被释放，所以保险起见复制一份，也可以尝试直接使用
                            memcpy(temp_buf, frame_ptr, len);

                            daatr_str.macDaatrSocketLowFreq_Send(temp_buf, len); // 发送
                            delete temp_buf;
                            delete mac_header2_ptr;
                            daatr_str.access_state = DAATR_HAS_SENT_ACCESS_REQUEST; // 断开节点已发送接入请求
                            writeInfo("NODE %2d 已发送接入请求", daatr_str.nodeId);

                            printf("[接入广播-MANA] Node %2d 已发送接入请求  ", daatr_str.nodeId);
                            printTime_ms();

                            if (!insertEventTimer_ms(ACCESS_REQUREST_WAITING_MAX_TIME, judgeIfBackToAccess))
                            {
                                cout << "事件队列已满，无法进行事件插入!!!!!!!!!" << endl;
                            }
                        }
                        else
                        { // 等待退避数减1
                            daatr_str.access_backoff_number--;
                            cout << endl;
                            cout << "[接入广播-MANA] Node " << daatr_str.nodeId << " 处于退避状态, 还需要退避 "
                                 << (int)daatr_str.access_backoff_number << " 接入时隙";
                            printTime_ms(); // 打印时间
                        }
                    }
                    else if (daatr_str.access_state == DAATR_HAS_SENT_ACCESS_REQUEST)
                    {
                        cout << "[接入广播-MANA] Node " << daatr_str.nodeId << " 处于等待接收网管节点回复接入请求状态 ";
                        printTime_ms();

                        writeInfo("[接入广播-MANA] Node %2d 处于等待接收网管节点回复接入请求状态", daatr_str.nodeId);
                        // printTime_ms(); // 打印时间
                    }
                    else if (daatr_str.access_state == DAATR_WAITING_TO_ACCESS)
                    {
                        writeInfo("|接入广播-MANA| Node %2d 已收到同意接入回复, 处于等待接收网管节点发送全网跳频图案状态", daatr_str.nodeId);
                        // cout << endl;
                        // cout << "[接入广播-MANA] Node ID " << daatr_str.nodeId << " 已收到同意接入回复, 处于等待接收网管节点发送全网跳频图案状态 ";
                        // printTime_ms(); // 打印时间
                    }
                    break;
                }
                case DAATR_STATUS_ACCESS_REPLY:
                { // 接入请求回复
                    if (daatr_str.state_now == Mac_Access)
                    {
                        // cout << "处于接入阶段，停止收发" << endl;
                        break;
                    }
                    if (daatr_str.access_state == DAATR_WAITING_TO_REPLY && daatr_str.state_now == Mac_Execution)
                    { // 如果网管节点有需要回复的节点
                        writeInfo("|接入广播-MANA| Node %2d 非建链阶段广播 网管信道 接入请求回复", daatr_str.nodeId);
                        // cout << endl;
                        cout << "[接入广播-MANA] Node " << daatr_str.nodeId << " 非建链阶段广播 网管信道 接入请求回复 ";
                        printTime_ms(); // 打印时间
                        vector<uint8_t> *slot_location;
                        slot_location = MacDaatrSearchSlotLocation(daatr_str.waiting_to_access_node, &daatr_str);
                        mana_access_reply access_reply;
                        int i = 0;
                        for (i = 0; i < FREQUENCY_COUNT; i++)
                        {
                            access_reply.mana_node_hopping[i] = daatr_str.freqSeq[daatr_str.nodeId][i]; // 将网管节点的跳频序列赋值进去
                            // access_reply.mana_node_hopping[i] = i;//测试用
                        }
                        for (i = 0; i < 5; i++)
                        {
                            access_reply.slot_location[i] = 60;
                        }
                        access_reply.slot_num = slot_location->size();
                        for (i = 0; i < access_reply.slot_num; i++)
                        {
                            access_reply.slot_location[i] = slot_location->at(i);
                        }
                        MacHeader2 *mac_header2_ptr = new MacHeader2;
                        mac_header2_ptr->PDUtype = 1;
                        mac_header2_ptr->srcAddr = daatr_str.nodeId;
                        mac_header2_ptr->destAddr = daatr_str.waiting_to_access_node; // 目的地址为接入节点
                        mac_header2_ptr->packetLength = 50;                           // 帧长度 17 + 1 + 32
                        mac_header2_ptr->backup = 1;                                  // 备用字段为1
                        mac_header2_ptr->fragment_tail_identification = 1;            // 分片尾标识
                        Low_Freq_Packet_Type packet_type;
                        packet_type.type = 2; // 随遇接入同意请求
                        MacDaatr_struct_converter mac_converter1(2);
                        mac_converter1.set_struct((uint8_t *)mac_header2_ptr, 2);
                        mac_converter1.daatr_struct_to_0_1();
                        MacDaatr_struct_converter mac_converter2(8);
                        mac_converter2.set_struct((uint8_t *)&packet_type, 8);
                        mac_converter2.daatr_struct_to_0_1();
                        MacDaatr_struct_converter mac_converter3(9);
                        mac_converter3.set_struct((uint8_t *)&access_reply, 9);
                        mac_converter3.daatr_struct_to_0_1();
                        mac_converter1 + mac_converter2 + mac_converter3; // 合包
                        // mac_converter1.daatr_mana_channel_add_0_to_full();
                        uint8_t *frame_ptr = mac_converter1.get_sequence();
                        uint32_t len = mac_converter1.get_length();
                        uint8_t *temp_buf = new uint8_t[len]; // 此处只是为了防止转换类中的bit指针被释放，所以保险起见复制一份，也可以尝试直接使用
                        memcpy(temp_buf, frame_ptr, len);
                        daatr_str.macDaatrSocketLowFreq_Send(temp_buf, len); // 发送
                        delete temp_buf;
                        delete mac_header2_ptr;
                        daatr_str.access_state = DAATR_WAITING_TO_SEND_HOPPING_PARTTERN;
                    }
                    else if (daatr_str.access_state == DAATR_WAITING_TO_REPLY && daatr_str.state_now != Mac_Execution)
                    { // 不处于执行阶段, 拒绝接入
                        // cout << endl;
                        cout << "[接入广播-MANA] Node " << daatr_str.nodeId << " 非建链阶段广播 网管信道 拒绝接入请求回复 ";
                        printTime_ms(); // 打印时间
                        writeInfo("|接入广播-MANA| Node %2d 非建链阶段广播 网管信道 拒绝接入请求回复", daatr_str.nodeId);
                        MacHeader2 *mac_header2_ptr = new MacHeader2;
                        mac_header2_ptr->PDUtype = 1;
                        mac_header2_ptr->srcAddr = daatr_str.nodeId;
                        mac_header2_ptr->destAddr = daatr_str.waiting_to_access_node; // 目的地址为接入节点
                        mac_header2_ptr->packetLength = 18;                           // 帧长度17+1
                        mac_header2_ptr->backup = 1;                                  // 备用字段为1
                        mac_header2_ptr->fragment_tail_identification = 1;            // 分片尾标识
                        Low_Freq_Packet_Type packet_type;
                        packet_type.type = 3; // 拒绝接入请求
                        MacDaatr_struct_converter mac_converter1(2);
                        mac_converter1.set_struct((uint8_t *)mac_header2_ptr, 2);
                        mac_converter1.daatr_struct_to_0_1();
                        MacDaatr_struct_converter mac_converter2(7);
                        mac_converter2.set_struct((uint8_t *)&packet_type, 7);
                        mac_converter2.daatr_struct_to_0_1();
                        mac_converter1 + mac_converter2; // 合包
                        mac_converter1.daatr_mana_channel_add_0_to_full();
                        uint8_t *frame_ptr = mac_converter1.get_sequence();
                        uint32_t len = mac_converter1.get_length();
                        uint8_t *temp_buf = new uint8_t[len]; // 此处只是为了防止转换类中的bit指针被释放，所以保险起见复制一份，也可以尝试直接使用
                        memcpy(temp_buf, frame_ptr, len);
                        daatr_str.macDaatrSocketLowFreq_Send(temp_buf, len); // 发送
                        delete temp_buf;
                        delete mac_header2_ptr;
                        daatr_str.waiting_to_access_node = 0;
                    }
                    else
                    {
                        // 网管节点无需回复
                        // cout << endl;
                        // cout << "无接入节点, 网管节点无需回复 ";
                        // printTime_ms(); // 打印时间
                        // writeInfo("无接入节点, 网管节点无需回复", daatr_str.nodeId);
                    }
                    break;
                }
                case DAATR_STATUS_FLIGHTSTATUS_SEND:
                { // 飞行状态信息广播
                    MacHeader2 *mac_header2_ptr = new MacHeader2;
                    // macpacket_daatr1.mac_header = new MacHeader;
                    mac_header2_ptr->PDUtype = 1;
                    mac_header2_ptr->srcAddr = daatr_str.nodeId;       // 源地址
                    mac_header2_ptr->destAddr = 0x3ff;                 // 目的地址全1
                    mac_header2_ptr->packetLength = 42;                // 帧长度
                    mac_header2_ptr->backup = 1;                       // 备用字段为1
                    mac_header2_ptr->fragment_tail_identification = 1; // 分片尾标识
                    FlightStatus flight_sta = daatr_str.local_node_position_info;
                    // printf("本地飞行状态信息广播\n");
                    // cout << daatr_str.local_node_position_info.nodeId << " " << daatr_str.local_node_position_info.positionX << " " << daatr_str.local_node_position_info.positionY << endl;
                    Low_Freq_Packet_Type packet_type;
                    if (daatr_str.node_type != Node_Management)
                    {
                        packet_type.type = 4; // 普通节点飞行状态信息通告
                    }
                    else
                    {
                        packet_type.type = 5; // 网管节点飞行状态信息通告
                    }
                    MacDaatr_struct_converter mac_converter1(2); // PDU2
                    mac_converter1.set_struct((uint8_t *)mac_header2_ptr, 2);
                    mac_converter1.daatr_struct_to_0_1();
                    MacDaatr_struct_converter mac_converter2(3); // 数据
                    mac_converter2.set_struct((uint8_t *)&flight_sta, 3);
                    mac_converter2.daatr_struct_to_0_1();
                    MacDaatr_struct_converter mac_converter3(8); // 数据类型字段
                    mac_converter3.set_struct((uint8_t *)&packet_type, 8);
                    mac_converter3.daatr_struct_to_0_1();
                    mac_converter1 + mac_converter3 + mac_converter2; // 合包
                    uint8_t *frame_ptr = mac_converter1.get_sequence();
                    uint32_t len = mac_converter1.get_length();
                    uint8_t *temp_buf = new uint8_t[len]; // 此处只是为了防止转换类中的bit指针被释放，所以保险起见复制一份，也可以尝试直接使用
                    memcpy(temp_buf, frame_ptr, len);
                    daatr_str.macDaatrSocketLowFreq_Send(temp_buf, len); // 发送
                    writeInfo("NODE %2d 发送飞行状态信息", daatr_str.nodeId);
                    cout << "[MANA] Node " << daatr_str.nodeId << " 广播飞行状态信息  ";
                    printTime_ms();
                    delete temp_buf;
                    delete mac_header2_ptr;
                    break;
                }
                case DAATR_STATUS_MANA_ANNOUNCEMENT:
                { // 网管节点信息通告
                  //  发送网管节点通告消息
                    int state = 0;
                    if (daatr_str.need_change_state == 0)
                    { // 若当前不转变状态
                        state = 0;
                    }
                    else if (daatr_str.need_change_state == 1)
                    { // 若转变为时隙调整阶段
                        if (daatr_str.state_now != Mac_Adjustment_Slot &&
                            daatr_str.state_now != Mac_Adjustment_Frequency)
                        {
                            // 若当前状态不为时隙调整阶段且不为时隙调整阶段
                            state = 1;
                            cout << "网管节点改变自己节点 state_now 为 Adjustment_Slot" << endl;
                            daatr_str.state_now = Mac_Adjustment_Slot; // 调整网管节点（本节点）为时隙调整阶段
                            daatr_str.need_change_state = 0;           // 已转变状态, 状态位复原
                            daatr_str.receivedSlottableTimes++;        // 初始未收到时隙表
                        }
                    }
                    else if (daatr_str.need_change_state == 2)
                    { // 若转变为频率调整阶段
                        if (daatr_str.state_now != Mac_Adjustment_Slot && daatr_str.state_now != Mac_Adjustment_Frequency)
                        {
                            // 若当前状态不为时隙调整阶段且不为时隙调整阶段
                            state = 2;
                            cout << "网管节点改变自己节点 state_now 为 Mac_Adjustment_Frequency" << endl;
                            daatr_str.state_now = Mac_Adjustment_Frequency; // 调整网管节点（本节点）为频率调整阶段
                            daatr_str.need_change_state = 0;                // 已转变状态, 状态位复原
                            daatr_str.has_received_sequence = false;
                        }
                    }
                    else if (daatr_str.need_change_state == 3)
                    {                                              // 若在转变时隙调整阶段过程中发生频率调整阶段
                        daatr_str.state_now = Mac_Adjustment_Slot; // 调整网管节点（本节点）为时隙调整阶段
                        state = 1;
                        daatr_str.need_change_state = 2; // 转变成需要进入频率调整阶段
                    }
                    else if (daatr_str.need_change_state == 4)
                    {                                                   // 若在转变频率调整阶段过程中发生时隙调整阶段, 需要先进入频率调整阶段
                        daatr_str.state_now = Mac_Adjustment_Frequency; // 调整网管节点（本节点）为时隙调整阶段
                        state = 2;
                        daatr_str.need_change_state = 1; // 转变成需要进入频率调整阶段
                    }

                    MacHeader2 *mac_header2_ptr = new MacHeader2;
                    mac_header2_ptr->PDUtype = 1;
                    mac_header2_ptr->srcAddr = daatr_str.nodeId;
                    mac_header2_ptr->destAddr = 0x3ff;                 // 目的地址全1
                    mac_header2_ptr->packetLength = 18 + 1;            // 帧长度
                    mac_header2_ptr->backup = 1;                       // 链路层消息
                    mac_header2_ptr->fragment_tail_identification = 1; // 分片尾标识
                    if_need_change_state change_state;
                    change_state.state = state;
                    Low_Freq_Packet_Type packet_type;
                    packet_type.type = 6; // 网管节点飞行状态信息通告
                    MacDaatr_struct_converter mac_converter1(2);
                    mac_converter1.set_struct((uint8_t *)mac_header2_ptr, 2);
                    mac_converter1.daatr_struct_to_0_1();
                    MacDaatr_struct_converter mac_converter2(4);
                    mac_converter2.set_struct((uint8_t *)&change_state, 4);
                    mac_converter2.daatr_struct_to_0_1();
                    MacDaatr_struct_converter mac_converter3(8);
                    mac_converter3.set_struct((uint8_t *)&packet_type, 8);
                    mac_converter3.daatr_struct_to_0_1();
                    mac_converter1 + mac_converter3 + mac_converter2; // 合包
                    uint8_t *frame_ptr = mac_converter1.get_sequence();
                    uint32_t len = mac_converter1.get_length();
                    uint8_t *temp_buf = new uint8_t[len]; // 此处只是为了防止转换类中的bit指针被释放，所以保险起见复制一份
                    memcpy(temp_buf, frame_ptr, len);
                    daatr_str.macDaatrSocketLowFreq_Send(temp_buf, len); // 发送
                    delete temp_buf;
                    delete mac_header2_ptr;
                    break;
                }
                case DAATR_STATUS_NETWORK_LAYER_SEND:
                { // 网络层管控信息通告
                    if (daatr_str.state_now == Mac_Access)
                    {
                        // cout << "处于接入阶段，停止收发" << endl;
                        writeInfo("处于接入阶段，停止收发");
                        break;
                    }
                    writeInfo("网络层管控信息通告");
                    msgFromControl buss_to_be_sent;
                    bool flag;
                    flag = daatr_str.getLowChannelBusinessFromQueue(&buss_to_be_sent);
                    if (flag == true)
                    { // 如果业务队列不为空, 有待发送业务
                        int mfc_length = buss_to_be_sent.packetLength;
                        unsigned short packet_length = 0;
                        packet_length = mfc_length + 17; // 总包长(单位：byte)
                        MacHeader2 *mac_header2_ptr = new MacHeader2;
                        mac_header2_ptr->PDUtype = 1;
                        mac_header2_ptr->srcAddr = daatr_str.nodeId;
                        mac_header2_ptr->destAddr = 0x3ff;                 // 目的地址全1
                        mac_header2_ptr->packetLength = packet_length;     // 帧长度
                        mac_header2_ptr->backup = 0;                       // 备用字段为0,网络层业务
                        mac_header2_ptr->fragment_tail_identification = 1; // 分片尾标识
                        vector<uint8_t> *MFC_Trans_temp;
                        MFC_Trans_temp = convert_PtrMFC_01(&buss_to_be_sent); // 将MFC转换为01序列(使用vector盛放)
                        MacDaatr_struct_converter mac_converter1(2);
                        mac_converter1.set_struct((uint8_t *)mac_header2_ptr, 2);
                        mac_converter1.daatr_struct_to_0_1();
                        MacDaatr_struct_converter mac_converter2(255); // 设定混合类型的converter, 用来容纳MFC转换的01序列的seq
                        mac_converter2.set_length(buss_to_be_sent.packetLength);
                        mac_converter2.daatr_MFCvector_to_0_1(MFC_Trans_temp, buss_to_be_sent.packetLength);
                        mac_converter1 + mac_converter2; // 合包
                        uint8_t *frame_ptr = mac_converter1.get_sequence();
                        uint32_t len = mac_converter1.get_length();
                        uint8_t *temp_buf = new uint8_t[len]; // 此处只是为了防止转换类中的bit指针被释放，所以保险起见复制一份
                        memcpy(temp_buf, frame_ptr, len);
                        daatr_str.macDaatrSocketLowFreq_Send(temp_buf, len); // 发送
                        delete temp_buf;
                        delete mac_header2_ptr;
                        break;
                    }
                }
                }
            }
        }
        if (end_simulation)
        {
            printf("Node %2d LowSendThread is Over\n", daatr_str.nodeId);
            break;
        }
    }
}

/**
 * @brief 处理低频信道接收的数据包。
 *
 * @param bit_seq 指向接收到的位序列的指针。
 * @param len 位序列的长度。
 */
void lowFreqChannelRecvHandle(uint8_t *bit_seq, uint64_t len)
{
    extern MacDaatr daatr_str;
    MacDaatr_struct_converter mac_converter(255);
    int frame_length = mac_converter.get_PDU2_sequence_length(bit_seq);
    mac_converter.set_length(frame_length);
    mac_converter.set_bit_sequence(bit_seq, 255);
    int i = 0;
    MacDaatr_struct_converter mac_converter2(2); // PDU2

    mac_converter2 - mac_converter; // PDU2
    mac_converter2.daatr_0_1_to_struct();
    MacHeader2 *mac_header2_ptr = (MacHeader2 *)mac_converter2.get_struct();
    MacHeader2 mac_header2;
    mac_header2 = *mac_header2_ptr;
    if (mac_header2.backup == 1)
    { // 链路层数据包
        mac_converter2.set_type(8);
        mac_converter2 - mac_converter; // 网管信道消息类型
        uint8_t *temp_ptr = mac_converter2.get_sequence();
        Low_Freq_Packet_Type packet_type;
        packet_type.type = temp_ptr[0];
        switch (packet_type.type)
        {
        case 1:
        { // 随遇接入请求
            if (daatr_str.nodeId == daatr_str.mana_node && daatr_str.waiting_to_access_node == 0)
            { // 如果本节点是网管节点
                // cout << endl
                //     << "网管节点 " << daatr_str.nodeId << " 已收到节点 " << mac_header2.srcAddr << " 接入请求 ";
                // printTime_ms();//打印时间
                writeInfo("网管节点 NODE %2d 已收到节点 NODE %2d 接入请求", daatr_str.nodeId, mac_header2.srcAddr);
                daatr_str.access_state = DAATR_WAITING_TO_REPLY; // 网管节点已收到接入请求, 等待下一接入请求恢复时隙回复
                daatr_str.waiting_to_access_node = mac_header2.srcAddr;
            }
            else if (daatr_str.nodeId == daatr_str.mana_node && daatr_str.waiting_to_access_node != 0)
            {
                // cout << "当前子网已有待接入节点, 待网管节点发送完全子网跳频图案后, 在进行接入" << endl;
                writeInfo("当前子网已有待接入节点, 待网管节点发送完全子网跳频图案后, 在进行接入");
            }

            break;
        }
        case 2:
        { // 同意随遇接入请求
            if (daatr_str.nodeId == mac_header2.destAddr)
            {
                // cout << endl
                //      << "Node " << daatr_str.nodeId << "已收到网管节点同意接入请求回复 ";
                // printTime_ms();//打印时间

                writeInfo("NODE %2d 已收到网管节点同意接入请求回复", daatr_str.nodeId);
                mac_converter2.set_type(9);
                mac_converter2 - mac_converter; // 网管节点接入请求
                mac_converter2.daatr_0_1_to_struct();
                mana_access_reply *access_reply = (mana_access_reply *)mac_converter2.get_struct();
                int slot_location = 0;
                for (i = 0; i < FREQUENCY_COUNT; i++)
                {
                    daatr_str.freqSeq[mac_header2.srcAddr - 1][i] = access_reply->mana_node_hopping[i]; // 将网管节点的跳频序列赋值进去
                }
                for (i = 0; i < 1; i++)
                { // 第一个时隙为飞行状态信息
                    slot_location = access_reply->slot_location[i];
                    // cout << slot_location << " "; // 测试用
                    daatr_str.low_freq_other_stage_slot_table[slot_location].nodeId = daatr_str.nodeId;
                    daatr_str.low_freq_other_stage_slot_table[slot_location].state = DAATR_STATUS_FLIGHTSTATUS_SEND;
                }
                for (i = 0; i < access_reply->slot_num; i++)
                { // 剩余时隙为网络层业务时隙
                    slot_location = access_reply->slot_location[i];
                    // cout << slot_location << " "; // 测试用
                    daatr_str.low_freq_other_stage_slot_table[slot_location].nodeId = daatr_str.nodeId;
                    daatr_str.low_freq_other_stage_slot_table[slot_location].state = DAATR_STATUS_NETWORK_LAYER_SEND;
                }
                daatr_str.access_state = DAATR_WAITING_TO_ACCESS; // 节点已收到网管节点同意请求, 等待接收全网跳频图案
                daatr_str.mana_node = mac_header2.srcAddr;        // 此时的请求回复的源地址即为网管节点, 重新更新
                if (!insertEventTimer_ms(ACCESS_SUBNET_FREQUENCY_PARTTERN_WAITTING_MAX_TIME, judgeIfBackToAccess))
                {
                    cout << "事件队列已满，无法进行事件插入!!!!!!!!!" << endl;
                }
                // 设置定时器，以给接收跳频图案接收一个最大时间！！！！！！！！！！！！！！！！！！！！！！！！！！！

                // 同步接入回复请求网管节点
                daatr_str.low_freq_other_stage_slot_table[9].nodeId = mac_header2.srcAddr;
                daatr_str.low_freq_other_stage_slot_table[21].nodeId = mac_header2.srcAddr;
                daatr_str.low_freq_other_stage_slot_table[34].nodeId = mac_header2.srcAddr;
                daatr_str.low_freq_other_stage_slot_table[46].nodeId = mac_header2.srcAddr;
            }
            break;
        }
        case 3:
        { // 拒绝随遇接入请求
            if (daatr_str.nodeId == mac_header2.destAddr)
            {
                cout << "\nNode " << daatr_str.nodeId << "收到网管节点拒绝接入回复 ";
                printTime_ms(); // 打印时间
                int num = rand() % (MAX_RANDOM_BACKOFF_NUM + 1);
                daatr_str.access_state = DAATR_NEED_ACCESS; // 节点已收到网管节点拒绝接入请求, 重新等待接入
                daatr_str.access_backoff_number = num;      // 随机回退

                // 同步接入回复请求网管节点
                daatr_str.low_freq_other_stage_slot_table[9].nodeId = mac_header2.srcAddr;
                daatr_str.low_freq_other_stage_slot_table[21].nodeId = mac_header2.srcAddr;
                daatr_str.low_freq_other_stage_slot_table[34].nodeId = mac_header2.srcAddr;
                daatr_str.low_freq_other_stage_slot_table[46].nodeId = mac_header2.srcAddr;
            }
            break;
        }
        case 4:
        { // 普通节点飞行状态信息

            writeInfo("NODE %2d 收到普通节点飞行状态信息", daatr_str.nodeId);

            // printf("Node %2d  收到普通节点飞行状态信息\n", daatr_str.nodeId);
            MacPacket_Daatr macpacket_daatr;
            mac_converter2.set_type(3);
            mac_converter2 - mac_converter; // 飞行状态信息
            mac_converter2.daatr_0_1_to_struct();
            FlightStatus *flight_sta = (FlightStatus *)mac_converter2.get_struct();
            macpacket_daatr.node_position = *flight_sta;
            macpacket_daatr.node_position.nodeId = mac_header2.srcAddr;
            UpdatePosition(&macpacket_daatr, &daatr_str);
            Get_Angle_Info(&macpacket_daatr, &daatr_str);
            break;
        }
        case 5:
        { // 网管节点飞行状态信息

            writeInfo("NODE %2d 收到网管节点飞行状态信息", daatr_str.nodeId);
            // printf("Node %2d  收到网管节点飞行状态信息\n", daatr_str.nodeId);
            MacPacket_Daatr macpacket_daatr;
            mac_converter2.set_type(3);
            mac_converter2 - mac_converter; // 飞行状态信息
            mac_converter2.daatr_0_1_to_struct();
            FlightStatus *flight_sta = (FlightStatus *)mac_converter2.get_struct();
            macpacket_daatr.node_position = (FlightStatus)*flight_sta;
            macpacket_daatr.node_position.nodeId = mac_header2.srcAddr;
            daatr_str.mana_node = mac_header2.srcAddr; // 更新网管节点
            UpdatePosition(&macpacket_daatr, &daatr_str);
            Get_Angle_Info(&macpacket_daatr, &daatr_str);
            break;
        }
        case 6:
        {
            // 网管节点通告消息
            // 网管节点的macdata_daatr->state_now 在"MAC_DAATR_ReceiveTaskInstruct"中更改
            // 非网管节点的 在此处更改, 接收网管节点的消息后根据数据包内容更改
            mac_converter2.set_type(4);
            mac_converter2 - mac_converter; // 网管节点通告消息
            mac_converter2.daatr_0_1_to_struct();
            if_need_change_state *change_state = (if_need_change_state *)mac_converter2.get_struct();
            if (change_state->state == 0)
            {
                // cout << "节点 " << daatr_str.nodeId << " 此时未收到状态调整信息" << endl;
                // printTime_ms();//打印时间
                writeInfo("NODE %2d 此时未收到状态调整信息", daatr_str.nodeId);
            }

            else if (change_state->state == 1)
            {
                daatr_str.state_now = Mac_Adjustment_Slot; // 调整本节点为时隙调整阶段
                daatr_str.receivedSlottableTimes++;        // 初始未收到时隙表
                cout << "节点 " << daatr_str.nodeId << " 收到网管节点广播消息, 并改变自己节点 state_now 为 Adjustment_Slot. " << endl;
                printTime_ms(); // 打印时间
                // 在收到网管节点发来的状态调整信息后, 立刻将自己的波束指向网管节点, 等待接收时隙表(需要调整！！！！！)
                // Node *dest_node_ptr = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, macdata_daatr->mana_node);
                // Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_RX, 0);
            }
            else if (change_state->state == 2)
            {
                daatr_str.state_now = Mac_Adjustment_Frequency; // 调整本节点为时隙调整阶段
                daatr_str.has_received_sequence = false;
                cout << "节点 " << daatr_str.nodeId << " 收到网管节点广播消息, 并改变自己节点 state_now 为 Adjustment_Freqency. " << endl;
                printTime_ms(); // 打印时间
                // 在收到网管节点发来的状态调整信息后, 立刻将自己的波束指向网管节点, 等待接收时隙表
                // Node *dest_node_ptr = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, macdata_daatr->mana_node);
                // Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_RX, 0);
            }
            else
            {
                cout << "Incorrect Data!" << endl;
            }
            break;
        }
        }
    }
    else if (mac_header2.backup == 0)
    {
        // 网络层数据包发送
        writeInfo("Node %2d 收到网管信道数据包, 并向网络层传递", daatr_str.nodeId);
        MacDaatr_struct_converter mac_converter_MFCs(100); // 用来盛放帧除去包头外的MFC的01序列
        mac_converter_MFCs - mac_converter;
        mac_converter_MFCs.set_length(frame_length - 17); // 17为PDU2的大小
        vector<uint8_t> *MFC_Trans_temp;
        MFC_Trans_temp = mac_converter_MFCs.Get_Single_MFC(); // 注意：MFC_Trans_temp需要释放内存！！！！！！
        // msgFromControl *MFC_temp;
        // MFC_temp = AnalysisLayer2MsgFromControl(MFC_Trans_temp);
        daatr_str.macToNetworkBufferHandle(MFC_Trans_temp->data(), 0x10, MFC_Trans_temp->size()); // 写入mac->net缓冲区
        // vector->data()  返回的是vector中存储的元素的首地址
        delete MFC_Trans_temp;
    }
}
