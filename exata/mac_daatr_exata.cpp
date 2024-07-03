#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <iostream>
#include <fstream>

#include "api.h"
#include "mac_tdma.h"
#include "network_ip.h"
#include "partition.h"

#include "common_struct.h"
#include "mac_daatr.h"
#include "routing_mmanet.h"

using namespace std;
#define DEBUG 0

static const clocktype DefaultTdmaSlotDuration = (2.5 * MILLI_SECOND);
unsigned short other_stage_mana_slot_table_nodeId_seq[MANAGEMENT_SLOT_NUMBER_OTHER_STAGE] =
    {1, 1, 2, 0, 3, 4, 5, 6, 7, 1, 8, 9, 10, 11, 12, 0, 13, 14, 15, 16, 17, 1, 18, 19, 20, 1, 1, 2, 0, 3, 4, 5, 6, 7, 1, 8, 9, 10, 11, 12, 0, 13, 14, 15, 16, 17, 1, 18, 19, 20};
unsigned short other_stage_mana_slot_table_state_seq[MANAGEMENT_SLOT_NUMBER_OTHER_STAGE] =
    {5, 6, 5, 3, 5, 5, 5, 5, 5, 4, 5, 5, 5, 5, 5, 3, 5, 5, 5, 5, 5, 4, 5, 5, 5, 7, 6, 7, 3, 7, 7, 7, 7, 7, 4, 7, 7, 7, 7, 7, 3, 7, 7, 7, 7, 7, 4, 7, 7, 7};
bool has_initializing_frequency_sequency = false;
int Rand(int i) { return rand() % i; }

/*
*
********************************************************************************
类型: 类函数
所属类: MacDaatr_struct_converter
备注: 1.关于类中返回的结构体/比特序列指针只为临时变量, 空间可能随时释放, 若想长久保存, 需自行开辟内存空间储存!!!
     2.在操控类之前, 需设定type_值非零,type_值默认为0!!!
     3.在给类中结构体或者比特流指针设置后, 在进行下一次设置, 会删除之前开辟的新的内存空间, 重新开辟内存空间!!!
     4.飞行状态信息因物理条件限制, 只负责转换位置信息以及三个角度信息(共24字节)!!!
     5.飞行状态信息存在移植问题(目前float型4字节, short型2字节)!!!
     6.进行等号操作时, 需注意, 此类的比特序列会新开辟内存空间, 大小与对应类相等, 且里面的值也相同,
       而结构体指针会清零, 类类型等同对应类
     7.加号操作执行合包功能, 注意, 若想将c中的比特序列添加与b比特序列后, 成为新的包,需编写如b + c,b成为新的包载体;
       同理, 若顺序相反, 则c + b, c成为新的包载体。成为新载体后, 类型均成为255, b,c皆为MacDaatr_struct_converter类
     8.减号操作为拆包操作, 形如a-b, 其中b的类型必须为255, 根据想要的包决定a的类型, a为一个空类, 即除了类型以外,
       其他皆为空, 想要什么类型, 就设置a的对应类型, 比如想要飞行状态信息, 就设置类型为3,
       若想拆掉包头, 拆掉PDU1,类型为99,拆掉PDU2,类型为100, 对应拆得的结果放置于b中,同时, 要想拆包, 必须在b内设置序列总长度!!!!
     9.返回的比特序列、结构体指针, 均为uint8_t*类型, 需自行进行强制类型转换
********************************************************************************
*
*/

MacDaatr_struct_converter::MacDaatr_struct_converter()
{
    bit_sequence_ptr_ = NULL;
    Daatr_struct_ptr_ = NULL;
    type_ = 0;
}

MacDaatr_struct_converter::MacDaatr_struct_converter(unsigned char type)
{ // temp对应结构体类型
    bit_sequence_ptr_ = NULL;
    Daatr_struct_ptr_ = NULL;
    type_ = type;
}

MacDaatr_struct_converter::~MacDaatr_struct_converter()
{ // 析构函数
    if (bit_sequence_ptr_ != NULL)
    { // 若比特序列开辟了数组内存, 需释放
        delete[] bit_sequence_ptr_;
    }
    if (Daatr_struct_ptr_ != NULL)
    { // 若结构体指针开辟了内存空间, 则需要释放
        delete Daatr_struct_ptr_;
    }
}

// 返回结构体类型
char MacDaatr_struct_converter::get_type()
{
    return type_;
}

// 返回长度
int MacDaatr_struct_converter::get_length()
{
    return length_;
}

// 返回此比特序列长度(从PDU包头得到)type: 0 PDU1包  1 PDU2
int MacDaatr_struct_converter::get_sequence_length(uint8_t *bit_seq)
{
    return ((bit_seq[1] << 8) | (bit_seq[2]));
}

// 返回结构体指针
uint8_t *MacDaatr_struct_converter::get_struct()
{
    return Daatr_struct_ptr_;
}

// 返回比特序列指针
uint8_t *MacDaatr_struct_converter::get_sequence()
{
    return bit_sequence_ptr_;
}

// 设置结构体类型
// 0: 不做任何转换; 1: PDU1; 2: PDU2; 3: 飞行状态信息; 4: 网管节点通告消息
bool MacDaatr_struct_converter::set_type(unsigned char temp)
{
    bool flag = true;
    type_ = temp;
    switch (type_)
    {
    case 0:
        length_ = 0;
        break;
    case 1:
        length_ = 25;
        break;
    case 2:
        length_ = 17;
        break;
    case 3:
        length_ = 24;
        break;
    case 4:
        length_ = 1;
        break;
    case 5:
        length_ = 63;
        break;
    case 6:
        length_ = 2;
        break;
    case 7:
        length_ = 550; // 时隙表 (1 + 10) * 400 = 4400 bits = 550 bytes
        break;
    case 8:
        length_ = 1;
        break;
    case 9:
        length_ = 32;
        break;
    case 10:
        length_ = 563;
    }
    return flag;
}

// 设置比特序列长度
bool MacDaatr_struct_converter::set_length(int length)
{
    bool flag = true;
    length_ = length;
    return flag;
}

// 返回PDU2包头类型数据包长度
int MacDaatr_struct_converter::get_PDU2_sequence_length(uint8_t *pkt)
{
    int length = 0;
    length |= (pkt[0] & 0x7f);
    length <<= 8;
    length |= pkt[1];
    length <<= 1;
    length |= (pkt[2] >> 7);
    return length;
}

// 设置类中结构体指针
// 若返回值为fasle, 则表示当前temp为0, 需先设置temp值
bool MacDaatr_struct_converter::set_struct(uint8_t *daatr_struc, unsigned char type = 0)
{
    bool flag = false;
    set_type(type); // 设置了长度信息
    if (type_ == 0)
    { // 若类型为0, 则不做任何处理
        return flag;
    }
    switch (type_)
    {
    case 1:
    { // PDU1
        if (Daatr_struct_ptr_ != NULL)
        {
            delete Daatr_struct_ptr_;
            Daatr_struct_ptr_ = NULL;
        }
        MacHeader *pdu1_ptr_temp = new MacHeader;
        MacHeader *pdu1_ptr_temp2 = (MacHeader *)daatr_struc;
        *pdu1_ptr_temp = *pdu1_ptr_temp2;
        Daatr_struct_ptr_ = (uint8_t *)pdu1_ptr_temp;
        break;
    }
    case 2:
    { // PDU2
        if (Daatr_struct_ptr_ != NULL)
        {
            delete Daatr_struct_ptr_;
            Daatr_struct_ptr_ = NULL;
        }
        MacHeader2 *pdu2_ptr_temp = new MacHeader2;
        MacHeader2 *pdu2_ptr_temp2 = (MacHeader2 *)daatr_struc;
        *pdu2_ptr_temp = *pdu2_ptr_temp2;
        Daatr_struct_ptr_ = (uint8_t *)pdu2_ptr_temp;
        break;
    }
    case 3:
    { // 飞行状态信息
        if (Daatr_struct_ptr_ != NULL)
        {
            delete Daatr_struct_ptr_;
            Daatr_struct_ptr_ = NULL;
        }
        FlightStatus *flight_ptr_temp = new FlightStatus;
        FlightStatus *flight_ptr_temp2 = (FlightStatus *)daatr_struc;
        *flight_ptr_temp = *flight_ptr_temp2;
        Daatr_struct_ptr_ = (uint8_t *)flight_ptr_temp;
        break;
    }
    case 4:
    { // 网管节点通告消息
        if (Daatr_struct_ptr_ != NULL)
        {
            delete Daatr_struct_ptr_;
            Daatr_struct_ptr_ = NULL;
        }
        if_need_change_state *ptr_temp = new if_need_change_state;
        if_need_change_state *ptr_temp2 = (if_need_change_state *)daatr_struc;
        *ptr_temp = *ptr_temp2;
        Daatr_struct_ptr_ = (uint8_t *)ptr_temp;
        break;
    }
    case 5:
    { // 频谱感知结果
        if (Daatr_struct_ptr_ != NULL)
        {
            delete Daatr_struct_ptr_;
            Daatr_struct_ptr_ = NULL;
        }
        spectrum_sensing_struct *ptr_temp = new spectrum_sensing_struct;
        spectrum_sensing_struct *ptr_temp2 = (spectrum_sensing_struct *)daatr_struc;
        *ptr_temp = *ptr_temp2;
        Daatr_struct_ptr_ = (uint8_t *)ptr_temp;
        break;
    }
    case 6:
    { // 建链请求结果
        if (Daatr_struct_ptr_ != NULL)
        {
            delete Daatr_struct_ptr_;
            Daatr_struct_ptr_ = NULL;
        }
        BuildLink_Request *ptr_temp = new BuildLink_Request;
        BuildLink_Request *ptr_temp2 = (BuildLink_Request *)daatr_struc;
        *ptr_temp = *ptr_temp2;
        Daatr_struct_ptr_ = (uint8_t *)ptr_temp;
        break;
    }
    case 7:
    { // 时隙表
        if (Daatr_struct_ptr_ != NULL)
        {
            delete Daatr_struct_ptr_;
            Daatr_struct_ptr_ = NULL;
        }
        Send_slottable *ptr_temp = new Send_slottable;
        Send_slottable *ptr_temp2 = (Send_slottable *)daatr_struc;
        *ptr_temp = *ptr_temp2;
        Daatr_struct_ptr_ = (uint8_t *)ptr_temp;
        break;
    }
    case 8:
    { // 网管信道数据包类型
        if (Daatr_struct_ptr_ != NULL)
        {
            delete Daatr_struct_ptr_;
            Daatr_struct_ptr_ = NULL;
        }
        Mana_Packet_Type *ptr_temp = new Mana_Packet_Type;
        Mana_Packet_Type *ptr_temp2 = (Mana_Packet_Type *)daatr_struc;
        *ptr_temp = *ptr_temp2;
        Daatr_struct_ptr_ = (uint8_t *)ptr_temp;
        break;
    }
    case 9:
    { // 网管节点接入请求回复（32byte）
        if (Daatr_struct_ptr_ != NULL)
        {
            delete Daatr_struct_ptr_;
            Daatr_struct_ptr_ = NULL;
        }
        mana_access_reply *ptr_temp = new mana_access_reply;
        mana_access_reply *ptr_temp2 = (mana_access_reply *)daatr_struc;
        *ptr_temp = *ptr_temp2;
        Daatr_struct_ptr_ = (uint8_t *)ptr_temp;
        break;
    }
    case 10:
    { // 跳频
        if (Daatr_struct_ptr_ != NULL)
        {
            delete Daatr_struct_ptr_;
            Daatr_struct_ptr_ = NULL;
        }
        subnet_frequency_parttern *ptr_temp = new subnet_frequency_parttern;
        subnet_frequency_parttern *ptr_temp2 = (subnet_frequency_parttern *)daatr_struc;
        *ptr_temp = *ptr_temp2;
        Daatr_struct_ptr_ = (uint8_t *)ptr_temp;
        break;
    }
    }
    flag = true;
    return flag;
}

// 设置类中比特序列指针
bool MacDaatr_struct_converter::set_bit_sequence(uint8_t *bit_sequence, unsigned char type = 0)
{
    bool flag = false;
    type_ = type;
    int i = 0;
    int number = 0;
    if (type_ == 0)
    { // 若类型为0, 则不做任何处理
        return flag;
    }
    switch (type_)
    {
    case 1:
    { // PDU1
        if (bit_sequence_ptr_ != NULL)
        { // 如果比特序列指针不为NULL, 则删除原数组内存空间
            delete[] bit_sequence_ptr_;
            bit_sequence_ptr_ = NULL;
        }
        number = 25; // 25为PDU1大小(不考虑内存对齐)
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = bit_sequence[i];
        }
        break;
    }
    case 2:
    { // PDU2
        if (bit_sequence_ptr_ != NULL)
        { // 如果比特序列指针不为NULL, 则删除原数组内存空间
            delete[] bit_sequence_ptr_;
            bit_sequence_ptr_ = NULL;
        }
        number = 17; // 17为PDU2大小(不考虑内存对齐)
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = bit_sequence[i];
        }
        break;
    }
    case 3:
    { // 飞行状态信息
        if (bit_sequence_ptr_ != NULL)
        { // 如果比特序列指针不为NULL, 则删除原数组内存空间
            delete[] bit_sequence_ptr_;
            bit_sequence_ptr_ = NULL;
        }
        number = 24; // 50为FlightStatus结构体大小(不考虑内存对齐),24为实际传输大小, 部分飞行状态信息因物理限制
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = bit_sequence[i];
        }
        break;
    }
    case 4:
    { // 网管节点通告消息(预留8bits)
        if (bit_sequence_ptr_ != NULL)
        { // 如果比特序列指针不为NULL, 则删除原数组内存空间
            delete[] bit_sequence_ptr_;
            bit_sequence_ptr_ = NULL;
        }
        number = 1;
        bit_sequence_ptr_ = new uint8_t[number]; // 1为网管节点通告消息大小
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = bit_sequence[i];
        }
        break;
    }
    case 5:
    { // 频谱感知结果(共501bits)
        if (bit_sequence_ptr_ != NULL)
        { // 如果比特序列指针不为NULL, 则删除原数组内存空间
            delete[] bit_sequence_ptr_;
            bit_sequence_ptr_ = NULL;
        }
        number = 63;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = bit_sequence[i];
        }
        break;
    }
    case 6:
    { // 建链请求
        if (bit_sequence_ptr_ != NULL)
        { // 如果比特序列指针不为NULL, 则删除原数组内存空间
            delete[] bit_sequence_ptr_;
            bit_sequence_ptr_ = NULL;
        }
        number = 2;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = bit_sequence[i];
        }
        break;
    }
    case 7:
    { // 时隙表
        if (bit_sequence_ptr_ != NULL)
        { // 如果比特序列指针不为NULL, 则删除原数组内存空间
            delete[] bit_sequence_ptr_;
            bit_sequence_ptr_ = NULL;
        }
        number = 550;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = bit_sequence[i];
        }
        break;
    }
    case 8:
    { // 网管数据包类型（预留8bits）
        if (bit_sequence_ptr_ != NULL)
        { // 如果比特序列指针不为NULL, 则删除原数组内存空间
            delete[] bit_sequence_ptr_;
            bit_sequence_ptr_ = NULL;
        }
        number = 1;
        bit_sequence_ptr_ = new uint8_t[number]; // 1为网管节点通告消息大小
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = bit_sequence[i];
        }
        break;
    }
    case 9:
    { // 网管节点接入请求回复（32byte）
        if (bit_sequence_ptr_ != NULL)
        { // 如果比特序列指针不为NULL, 则删除原数组内存空间
            delete[] bit_sequence_ptr_;
            bit_sequence_ptr_ = NULL;
        }
        number = 32;
        bit_sequence_ptr_ = new uint8_t[number]; // 1为网管节点通告消息大小
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = bit_sequence[i];
        }
        break;
    }
    case 10:
    { // 时隙表
        if (bit_sequence_ptr_ != NULL)
        { // 如果比特序列指针不为NULL, 则删除原数组内存空间
            delete[] bit_sequence_ptr_;
            bit_sequence_ptr_ = NULL;
        }
        number = 563;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = bit_sequence[i];
        }
        break;
    }
    case 255:
    {
        if (bit_sequence_ptr_ != NULL)
        { // 如果比特序列指针不为NULL, 则删除原数组内存空间
            delete[] bit_sequence_ptr_;
            bit_sequence_ptr_ = NULL;
        }
        number = length_;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = bit_sequence[i];
        }
        break;
    }
    }
    flag = true;
    return flag;
}

// PDU1->0/1序列
bool MacDaatr_struct_converter::daatr_PDU1_to_0_1()
{
    int i = 0;
    int number = 25;
    int flag = false;
    if (Daatr_struct_ptr_ == NULL || type_ != 1) // 若待转换结构体为空或者此时结构体类型不为PDU1, 则直接退出
    {
        return flag;
    }
    if (bit_sequence_ptr_ != NULL)
    { // 若指针不为空, 则释放原空间
        delete[] bit_sequence_ptr_;
        bit_sequence_ptr_ = NULL;
    }

    MacHeader *pdu1_ptr_temp = (MacHeader *)Daatr_struct_ptr_;
    uint8_t *packet = new uint8_t[number]; // 生成新空间

    packet[0] = ((pdu1_ptr_temp->PDUtype << 7) | (pdu1_ptr_temp->is_mac_layer << 5) | (pdu1_ptr_temp->backup)) & 0xff;
    packet[1] = (pdu1_ptr_temp->packetLength >> 8) & 0xff;
    packet[2] = (pdu1_ptr_temp->packetLength) & 0xff;
    packet[3] = (pdu1_ptr_temp->srcAddr >> 2) & 0xff;
    packet[4] = ((pdu1_ptr_temp->srcAddr << 6) | (pdu1_ptr_temp->destAddr >> 4)) & 0xff;
    packet[5] = ((pdu1_ptr_temp->destAddr << 4) | (pdu1_ptr_temp->seq >> 4)) & 0xff;
    packet[6] = (pdu1_ptr_temp->seq >> 4) & 0xff;
    packet[7] = ((pdu1_ptr_temp->seq << 4) | (pdu1_ptr_temp->mac_backup >> 40)) & 0xff;
    packet[8] = (pdu1_ptr_temp->mac_backup >> 32) & 0xff;
    packet[9] = (pdu1_ptr_temp->mac_backup >> 24) & 0xff;
    packet[10] = (pdu1_ptr_temp->mac_backup >> 16) & 0xff;
    packet[11] = (pdu1_ptr_temp->mac_backup >> 8) & 0xff;
    packet[12] = pdu1_ptr_temp->mac_backup & 0xff;
    packet[13] = pdu1_ptr_temp->mac_headerChecksum & 0xff;
    packet[14] = (pdu1_ptr_temp->mac_pSynch >> 56) & 0xff;
    packet[15] = (pdu1_ptr_temp->mac_pSynch >> 48) & 0xff;
    packet[16] = (pdu1_ptr_temp->mac_pSynch >> 40) & 0xff;
    packet[17] = (pdu1_ptr_temp->mac_pSynch >> 32) & 0xff;
    packet[18] = (pdu1_ptr_temp->mac_pSynch >> 24) & 0xff;
    packet[19] = (pdu1_ptr_temp->mac_pSynch >> 16) & 0xff;
    packet[20] = (pdu1_ptr_temp->mac_pSynch >> 8) & 0xff;
    packet[21] = pdu1_ptr_temp->mac_pSynch & 0xff;
    packet[22] = (pdu1_ptr_temp->mac >> 16) & 0xff;
    packet[23] = (pdu1_ptr_temp->mac >> 8) & 0xff;
    packet[24] = pdu1_ptr_temp->mac & 0xff;
    bit_sequence_ptr_ = packet;
    flag = true;
    /*
    for(i = 0; i < number; i++)
    {
        cout << hex << (short)bit_sequence_ptr_[i] << endl;
    }
    cout << dec;
    */
    return flag;
}

// PDU2->0/1序列
bool MacDaatr_struct_converter::daatr_PDU2_to_0_1()
{
    int i = 0;
    int number = 17;
    int flag = false;
    if (Daatr_struct_ptr_ == NULL || type_ != 2) // 若待转换结构体为空或者此时结构体类型不为PDU2, 则直接退出
    {
        return flag;
    }
    if (bit_sequence_ptr_ != NULL)
    { // 若指针不为空, 则释放原空间
        delete[] bit_sequence_ptr_;
        bit_sequence_ptr_ = NULL;
    }
    MacHeader2 *pdu2_ptr_temp = (MacHeader2 *)Daatr_struct_ptr_;
    uint8_t *packet = new uint8_t[number]; // 生成新空间
    packet[0] = ((pdu2_ptr_temp->PDUtype << 7) | (pdu2_ptr_temp->packetLength >> 9)) & 0xff;
    packet[1] = (pdu2_ptr_temp->packetLength >> 1) & 0xff;
    packet[2] = ((pdu2_ptr_temp->packetLength << 7) | (pdu2_ptr_temp->srcAddr >> 3)) & 0xff;
    packet[3] = ((pdu2_ptr_temp->srcAddr << 5) | (pdu2_ptr_temp->destAddr >> 5)) & 0xff;
    packet[4] = ((pdu2_ptr_temp->destAddr << 3) | (pdu2_ptr_temp->slice_head_identification << 2) | (pdu2_ptr_temp->fragment_tail_identification << 1) | (pdu2_ptr_temp->backup)) & 0xff;
    packet[5] = (pdu2_ptr_temp->mac_headerChecksum) & 0xff;
    packet[6] = (pdu2_ptr_temp->mac_pSynch >> 56) & 0xff;
    packet[7] = (pdu2_ptr_temp->mac_pSynch >> 48) & 0xff;
    packet[8] = (pdu2_ptr_temp->mac_pSynch >> 40) & 0xff;
    packet[9] = (pdu2_ptr_temp->mac_pSynch >> 32) & 0xff;
    packet[10] = (pdu2_ptr_temp->mac_pSynch >> 24) & 0xff;
    packet[11] = (pdu2_ptr_temp->mac_pSynch >> 16) & 0xff;
    packet[12] = (pdu2_ptr_temp->mac_pSynch >> 8) & 0xff;
    packet[13] = (pdu2_ptr_temp->mac_pSynch) & 0xff;
    packet[14] = (pdu2_ptr_temp->mac >> 16) & 0xff;
    packet[15] = (pdu2_ptr_temp->mac >> 8) & 0xff;
    packet[16] = (pdu2_ptr_temp->mac) & 0xff;
    bit_sequence_ptr_ = packet;
    flag = true;
    /*for(i = 0; i < number; i++)
    {
        cout << hex << (short)bit_sequence_ptr_[i] << endl;
    }
    cout << dec;*/
    return flag;
}

// 0/1比特序列->结构体PDU1
bool MacDaatr_struct_converter::daatr_0_1_to_PDU1()
{
    uint64_t temp_vari = 0;
    bool flag = false;
    int num = 0;
    if (bit_sequence_ptr_ == NULL || type_ != 1)
    {
        return flag;
    }
    if (Daatr_struct_ptr_ != NULL)
    { // 若结构体指针不为空, 则释放
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    MacHeader *struct_ptr_temp = new MacHeader;
    memset(struct_ptr_temp, 0, sizeof(MacHeader));
    struct_ptr_temp->PDUtype |= bit_sequence_ptr_[0] >> 7;
    struct_ptr_temp->is_mac_layer |= (0x60 & bit_sequence_ptr_[0]) >> 5;
    struct_ptr_temp->backup |= (0x1f & bit_sequence_ptr_[0]); // 已修改 check
    temp_vari = bit_sequence_ptr_[1];
    temp_vari = temp_vari << 8;
    temp_vari = temp_vari | bit_sequence_ptr_[2];
    struct_ptr_temp->packetLength |= temp_vari;
    temp_vari = 0;
    temp_vari |= bit_sequence_ptr_[3];
    temp_vari = temp_vari << 2;
    temp_vari |= (bit_sequence_ptr_[4] >> 6);
    struct_ptr_temp->srcAddr |= temp_vari;
    temp_vari = 0;
    temp_vari |= (bit_sequence_ptr_[4] << 2);
    temp_vari = temp_vari << 2;
    temp_vari |= (bit_sequence_ptr_[5] >> 4);
    struct_ptr_temp->destAddr |= temp_vari;
    temp_vari = 0;
    temp_vari |= (bit_sequence_ptr_[5] & 0x0f);
    temp_vari = temp_vari << 8;
    temp_vari |= bit_sequence_ptr_[6];
    temp_vari = temp_vari << 4;
    temp_vari |= (bit_sequence_ptr_[7] >> 4);
    struct_ptr_temp->seq |= temp_vari;
    temp_vari = 0;
    temp_vari |= (bit_sequence_ptr_[7] & 0x0f);
    temp_vari = temp_vari << 8;
    temp_vari |= bit_sequence_ptr_[8];
    temp_vari = temp_vari << 8;
    temp_vari |= bit_sequence_ptr_[9];
    temp_vari = temp_vari << 8;
    temp_vari |= bit_sequence_ptr_[10];
    temp_vari = temp_vari << 8;
    temp_vari |= bit_sequence_ptr_[11];
    temp_vari = temp_vari << 8;
    temp_vari |= bit_sequence_ptr_[12];
    struct_ptr_temp->mac_backup |= temp_vari;
    temp_vari = 0;
    temp_vari |= bit_sequence_ptr_[13];
    struct_ptr_temp->mac_headerChecksum |= temp_vari;
    temp_vari = 0;
    num = 14;
    for (int i = 0; i < 7; i++)
    {
        temp_vari |= bit_sequence_ptr_[num + i];
        temp_vari = temp_vari << 8;
    }
    temp_vari |= bit_sequence_ptr_[21];
    struct_ptr_temp->mac_pSynch |= temp_vari;
    temp_vari = 0;
    temp_vari |= bit_sequence_ptr_[22];
    temp_vari <<= 8;
    temp_vari |= bit_sequence_ptr_[23];
    temp_vari <<= 8;
    temp_vari |= bit_sequence_ptr_[24];
    struct_ptr_temp->mac |= temp_vari;
    Daatr_struct_ptr_ = (uint8_t *)struct_ptr_temp;
    // 测试用
    /*
    cout << "PDUtype: "  << struct_ptr_temp->PDUtype << endl;
    cout << "backup: "  << struct_ptr_temp->backup << endl;
    cout << "packetLength: "  << struct_ptr_temp->packetLength << endl;
    cout << "srcAddr: "  << struct_ptr_temp->srcAddr << endl;
    cout << "destAddr: "  << struct_ptr_temp->destAddr << endl;
    cout << "seq: "  << struct_ptr_temp->seq << endl;
    cout << "mac_backup: "  << struct_ptr_temp->mac_backup << endl;
    cout << "mac_headerChecksum: "  << struct_ptr_temp->mac_headerChecksum << endl;
    cout << "mac_pSynch: "  << struct_ptr_temp->mac_pSynch << endl;
    cout << "mac: "  << struct_ptr_temp->mac << endl;
    */
    ////////////////////
    flag = true;
    return flag;
}

// 0/1比特序列->结构体PDU2
bool MacDaatr_struct_converter::daatr_0_1_to_PDU2()
{
    uint64_t temp_vari = 0;
    bool flag = false;
    int num = 0;
    if (bit_sequence_ptr_ == NULL || type_ != 2)
    {
        return flag;
    }
    if (Daatr_struct_ptr_ != NULL)
    { // 若结构体指针不为空, 则释放
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    MacHeader2 *struct_ptr_temp = new MacHeader2;
    memset(struct_ptr_temp, 0, sizeof(MacHeader2));
    struct_ptr_temp->PDUtype |= bit_sequence_ptr_[0] >> 7;
    temp_vari |= (bit_sequence_ptr_[0] & 0x7f);
    temp_vari <<= 8;
    temp_vari |= bit_sequence_ptr_[1];
    temp_vari <<= 1;
    temp_vari |= (bit_sequence_ptr_[2] >> 7);
    struct_ptr_temp->packetLength |= temp_vari;
    temp_vari = 0;
    temp_vari |= (bit_sequence_ptr_[2] & 0x7f);
    temp_vari <<= 3;
    temp_vari |= (bit_sequence_ptr_[3] >> 5);
    struct_ptr_temp->srcAddr |= temp_vari;
    temp_vari = 0;
    temp_vari |= (bit_sequence_ptr_[3] & 0x1f);
    temp_vari <<= 5;
    temp_vari |= (bit_sequence_ptr_[4] >> 3);
    struct_ptr_temp->destAddr |= temp_vari;
    temp_vari = 0;
    temp_vari |= (bit_sequence_ptr_[4] & 0x07);
    struct_ptr_temp->slice_head_identification |= (temp_vari >> 2);
    struct_ptr_temp->fragment_tail_identification |= (temp_vari & 0x02);
    struct_ptr_temp->backup |= (temp_vari & 0x01);
    struct_ptr_temp->mac_headerChecksum |= bit_sequence_ptr_[5];
    num = 6;
    temp_vari = 0;
    for (int i = 0; i < 7; i++)
    {
        temp_vari |= bit_sequence_ptr_[num + i];
        temp_vari <<= 8;
    }
    temp_vari |= bit_sequence_ptr_[13];
    struct_ptr_temp->mac_pSynch |= temp_vari;
    temp_vari = 0;
    temp_vari |= bit_sequence_ptr_[14];
    temp_vari <<= 8;
    temp_vari |= bit_sequence_ptr_[15];
    temp_vari <<= 8;
    temp_vari |= bit_sequence_ptr_[16];
    struct_ptr_temp->mac |= temp_vari;
    Daatr_struct_ptr_ = (uint8_t *)struct_ptr_temp;

    // 测试用
    /*cout << "PDUtype: "  << struct_ptr_temp->PDUtype << endl;
    cout << "packetLength: "  << struct_ptr_temp->packetLength << endl;
    cout << "srcAddr: "  << struct_ptr_temp->srcAddr << endl;
    cout << "destAddr: "  << struct_ptr_temp->destAddr << endl;
    cout << "slice_head_identification: "  << struct_ptr_temp->slice_head_identification << endl;
    cout << "fragment_tail_identification: "  << struct_ptr_temp->fragment_tail_identification << endl;
    cout << "backup: "  << struct_ptr_temp->backup << endl;
    cout << "mac_headerChecksum: "  << struct_ptr_temp->mac_headerChecksum << endl;
    cout << "mac_pSynch: "  << struct_ptr_temp->mac_pSynch << endl;
    cout << "mac: "  << struct_ptr_temp->mac << endl;*/
    ////////////////////
    flag = true;
    return flag;
}

// 将使用vector盛放的01序列, 转换为converter格式, 为其bit_seq字段赋值
bool MacDaatr_struct_converter::daatr_MFCvector_to_0_1(vector<uint8_t> *MFC_vector_temp,
                                                       int packetlength)
{
    int flag = false;

    if (bit_sequence_ptr_ != NULL)
    { // 若指针不为空, 则释放原空间
        delete[] bit_sequence_ptr_;
        bit_sequence_ptr_ = NULL;
    }

    if (packetlength != (*MFC_vector_temp).size())
    {
        cout << "MFC sequence doesn't match given length";
        system("pause");
    }

    length_ = packetlength;
    uint8_t *packet = new uint8_t[length_];
    for (int i = 0; i < length_; i++)
    {
        packet[i] = (*MFC_vector_temp)[i];
    }
    bit_sequence_ptr_ = packet;
    flag = true;
    return flag;
}

vector<uint8_t> *MacDaatr_struct_converter::daatr_0_1_to_MFCvector()
{
    // uint8_t *packet = new uint8_t[packetlength];
    vector<uint8_t> *buffer = new vector<uint8_t>;
    for (int i = 0; i < length_; i++)
    {
        buffer->push_back(bit_sequence_ptr_[i]);
    }

    return buffer;
}

// 传入的是一个converter类型的01序列, 其组成格式为 若干通过重载 "+" 相连的MFC集合
// 此函数将从 多个MFC的01序列 中拆出来第一个MFC所属的01序列 并返回(通过MFC的pktlength字段)
vector<uint8_t> *MacDaatr_struct_converter::Get_Single_MFC()
{
    int i;
    int MFC_packetlength = (bit_sequence_ptr_[1] << 8) | bit_sequence_ptr_[2];
    // cout << "MFC_packetlength: " << MFC_packetlength << endl;
    vector<uint8_t> *MFC_vector = new vector<uint8_t>;
    for (i = 0; i < MFC_packetlength; i++)
    {
        MFC_vector->push_back(bit_sequence_ptr_[i]);
    }
    // 更新长度
    length_ -= MFC_packetlength;

    // 将 bitseq 前移 mfclength
    for (i = 0; i < length_; i++)
    {
        bit_sequence_ptr_[i] = bit_sequence_ptr_[i + MFC_packetlength];
    }
    // 将前移后空出来的位置赋零
    for (i = 0; i < MFC_packetlength; i++)
    {
        bit_sequence_ptr_[length_ + i] = 0;
    }
    return MFC_vector;
}

// 飞行状态信息转化为0/1序列
bool MacDaatr_struct_converter::daatr_flight_to_0_1()
{
    int i = 0;
    int number = 24;
    int flag = false;
    uint32_t temp_var = 0;
    float temp_fl;
    if (Daatr_struct_ptr_ == NULL || type_ != 3) // 若待转换结构体为空或者此时结构体类型不为PDU2, 则直接退出
    {
        return flag;
    }
    if (bit_sequence_ptr_ != NULL)
    { // 若指针不为空, 则释放原空间
        delete[] bit_sequence_ptr_;
        bit_sequence_ptr_ = NULL;
    }
    FlightStatus *flight_ptr_temp = (FlightStatus *)Daatr_struct_ptr_;
    uint8_t *packet = new uint8_t[number]; // 生成新空间
    temp_fl = flight_ptr_temp->positionX;
    temp_var = *(uint32_t *)&temp_fl;
    packet[0] = (temp_var >> 24) & 0xff;
    packet[1] = (temp_var >> 16) & 0xff;
    packet[2] = (temp_var >> 8) & 0xff;
    packet[3] = temp_var & 0xff;
    temp_fl = flight_ptr_temp->positionY;
    temp_var = *(uint32_t *)&temp_fl;
    packet[4] = (temp_var >> 24) & 0xff;
    packet[5] = (temp_var >> 16) & 0xff;
    packet[6] = (temp_var >> 8) & 0xff;
    packet[7] = temp_var;
    temp_fl = flight_ptr_temp->positionZ;
    temp_var = *(uint32_t *)&temp_fl;
    packet[8] = (temp_var >> 24) & 0xff;
    packet[9] = (temp_var >> 16) & 0xff;
    packet[10] = (temp_var >> 8) & 0xff;
    packet[11] = temp_var;
    temp_fl = flight_ptr_temp->pitchAngle;
    temp_var = *(uint32_t *)&temp_fl;
    packet[12] = (temp_var >> 24) & 0xff;
    packet[13] = (temp_var >> 16) & 0xff;
    packet[14] = (temp_var >> 8) & 0xff;
    packet[15] = temp_var;
    temp_fl = flight_ptr_temp->yawAngle;
    temp_var = *(uint32_t *)&temp_fl;
    packet[16] = (temp_var >> 24) & 0xff;
    packet[17] = (temp_var >> 16) & 0xff;
    packet[18] = (temp_var >> 8) & 0xff;
    packet[19] = temp_var;
    temp_fl = flight_ptr_temp->rollAngle;
    temp_var = *(uint32_t *)&temp_fl;
    packet[20] = (temp_var >> 24) & 0xff;
    packet[21] = (temp_var >> 16) & 0xff;
    packet[22] = (temp_var >> 8) & 0xff;
    packet[23] = temp_var;
    bit_sequence_ptr_ = packet;
    flag = true;
    /*for(i = 0; i < number; i++)
    {
        cout << hex << (short)bit_sequence_ptr_[i] << endl;
    }
    cout << dec;*/
    return flag;
}

// 0/1序列转化为飞行状态信息
bool MacDaatr_struct_converter::daatr_0_1_to_flight()
{
    uint32_t temp_vari = 0;
    bool flag = false;
    int num = 0;
    int i = 0;
    if (bit_sequence_ptr_ == NULL || type_ != 3)
    {
        return flag;
    }
    if (Daatr_struct_ptr_ != NULL)
    { // 若结构体指针不为空, 则释放
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    FlightStatus *struct_ptr_temp = new FlightStatus;
    memset(struct_ptr_temp, 0, sizeof(FlightStatus));
    for (i = 0; i < 3; i++)
    {
        temp_vari |= bit_sequence_ptr_[num];
        temp_vari <<= 8;
        num++;
    }
    temp_vari |= bit_sequence_ptr_[num];
    struct_ptr_temp->positionX = *(float *)&temp_vari;
    temp_vari = 0;
    num++;
    for (i = 0; i < 3; i++)
    {
        temp_vari |= bit_sequence_ptr_[num];
        temp_vari <<= 8;
        num++;
    }
    temp_vari |= bit_sequence_ptr_[num];
    struct_ptr_temp->positionY = *(float *)&temp_vari;
    temp_vari = 0;
    num++;
    for (i = 0; i < 3; i++)
    {
        temp_vari |= bit_sequence_ptr_[num];
        temp_vari <<= 8;
        num++;
    }
    temp_vari |= bit_sequence_ptr_[num];
    struct_ptr_temp->positionZ = *(float *)&temp_vari;
    temp_vari = 0;
    num++;
    for (i = 0; i < 3; i++)
    {
        temp_vari |= bit_sequence_ptr_[num];
        temp_vari <<= 8;
        num++;
    }
    temp_vari |= bit_sequence_ptr_[num];
    struct_ptr_temp->pitchAngle = *(float *)&temp_vari;
    temp_vari = 0;
    num++;
    for (i = 0; i < 3; i++)
    {
        temp_vari |= bit_sequence_ptr_[num];
        temp_vari <<= 8;
        num++;
    }
    temp_vari |= bit_sequence_ptr_[num];
    struct_ptr_temp->yawAngle = *(float *)&temp_vari;
    temp_vari = 0;
    num++;
    for (i = 0; i < 3; i++)
    {
        temp_vari |= bit_sequence_ptr_[num];
        temp_vari <<= 8;
        num++;
    }
    temp_vari |= bit_sequence_ptr_[num];
    struct_ptr_temp->rollAngle = *(float *)&temp_vari;
    Daatr_struct_ptr_ = (uint8_t *)struct_ptr_temp;
    // 测试用
    /*cout << "positionX: "  << struct_ptr_temp->positionX << endl;
    cout << "positionY: "  << struct_ptr_temp->positionY << endl;
    cout << "positionZ: "  << struct_ptr_temp->positionZ << endl;
    cout << "pitchAngle: "  << struct_ptr_temp->pitchAngle << endl;
    cout << "yawAngle: "  << struct_ptr_temp->yawAngle << endl;
    cout << "rollAngle: "  << struct_ptr_temp->rollAngle << endl;*/
    ////////////////////
    flag = true;
    return flag;
}

// 网管节点通告消息转化为0/1序列
bool MacDaatr_struct_converter::daatr_mana_info_to_0_1()
{
    int i = 0;
    int number = 1;
    int flag = false;
    if (Daatr_struct_ptr_ == NULL || type_ != 4) // 若待转换结构体为空或者此时结构体类型不为网管节点通告消息, 则直接退出
    {
        return flag;
    }
    if (bit_sequence_ptr_ != NULL)
    { // 若指针不为空, 则释放原空间
        delete[] bit_sequence_ptr_;
        bit_sequence_ptr_ = NULL;
    }
    if_need_change_state *mana_info_ptr_temp = (if_need_change_state *)Daatr_struct_ptr_;
    uint8_t *packet = new uint8_t[number]; // 生成新空间
    packet[0] = mana_info_ptr_temp->state;
    bit_sequence_ptr_ = packet;
    /*for(i = 0; i < number; i++)
    {
        cout << hex << (short)bit_sequence_ptr_[i] << endl;
    }
    cout << dec;*/
    flag = true;
    return flag;
}

// 0/1序列转化网管节点通告消息
bool MacDaatr_struct_converter::daatr_0_1_to_mana_info()
{
    uint32_t temp_vari = 0;
    bool flag = false;
    int num = 0;
    int i = 0;
    if (bit_sequence_ptr_ == NULL || type_ != 4)
    {
        return flag;
    }
    if (Daatr_struct_ptr_ != NULL)
    { // 若结构体指针不为空, 则释放
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    if_need_change_state *struct_ptr_temp = new if_need_change_state;
    memset(struct_ptr_temp, 0, sizeof(if_need_change_state));
    struct_ptr_temp->state = bit_sequence_ptr_[0];
    Daatr_struct_ptr_ = (uint8_t *)struct_ptr_temp;
    // 测试用
    // cout << "state: "  << struct_ptr_temp->state << endl;
    ////////////////////
    flag = true;
    return flag;
}

// 频谱感知结果转化为0/1序列
bool MacDaatr_struct_converter::daatr_freq_sense_to_0_1()
{
    int i = 0;
    int j = 0;
    int number = 63;
    int flag = false;
    int num = 0;
    if (Daatr_struct_ptr_ == NULL || type_ != 5) // 若待转换结构体为空或者此时结构体类型不为频谱感知结果, 则直接退出
    {
        return flag;
    }
    if (bit_sequence_ptr_ != NULL)
    { // 若指针不为空, 则释放原空间
        delete[] bit_sequence_ptr_;
        bit_sequence_ptr_ = NULL;
    }
    spectrum_sensing_struct *mana_info_ptr_temp = (spectrum_sensing_struct *)Daatr_struct_ptr_;
    uint8_t *packet = new uint8_t[number]; // 生成新空间
    uint8_t temp = 0;
    for (i = 0; i < number - 1; i++)
    {
        for (j = 0; j < 7; j++)
        {
            temp |= (mana_info_ptr_temp->spectrum_sensing[num] & 0xff);
            temp <<= 1;
            num++;
        }
        temp |= (mana_info_ptr_temp->spectrum_sensing[num] & 0xff);
        num++;
        packet[i] = temp;
        temp = 0;
    }
    while (num < TOTAL_FREQ_POINT)
    {
        temp |= (mana_info_ptr_temp->spectrum_sensing[num] & 0xff);
        if (num != TOTAL_FREQ_POINT - 1)
        {
            temp <<= 1;
        }
        num++;
    }
    packet[i] = temp;
    bit_sequence_ptr_ = packet;
    flag = true;
    return flag;
}

// 0/1序列转化为频谱感知结果
bool MacDaatr_struct_converter::daatr_0_1_to_freq_sense()
{
    bool flag = false;
    int num = 63;
    int number = 0;
    int i = 0;
    int j = 0;
    if (bit_sequence_ptr_ == NULL || type_ != 5)
    {
        return flag;
    }
    if (Daatr_struct_ptr_ != NULL)
    { // 若结构体指针不为空, 则释放
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    spectrum_sensing_struct *struct_ptr_temp = new spectrum_sensing_struct;
    memset(struct_ptr_temp->spectrum_sensing, 0, sizeof(unsigned int) * TOTAL_FREQ_POINT);
    for (i = 0; i < num - 1; i++)
    {
        for (j = 7; j >= 0; j--)
        {
            struct_ptr_temp->spectrum_sensing[number] |= ((bit_sequence_ptr_[i] >> j) & 0x01);
            number++;
        }
    }
    while (number < TOTAL_FREQ_POINT)
    {
        for (j = 4; j >= 0; j--)
        {
            struct_ptr_temp->spectrum_sensing[number] |= ((bit_sequence_ptr_[i] >> j) & 0x01);
        }
        number++;
    }
    Daatr_struct_ptr_ = (uint8_t *)struct_ptr_temp;
    flag = true;
    return flag;
}

// 建链请求转化为0/1序列
bool MacDaatr_struct_converter::daatr_build_req_to_0_1()
{
    int i = 0;
    int j = 0;
    int number = 2;
    int flag = false;
    int num = 0;
    if (Daatr_struct_ptr_ == NULL || type_ != 6) // 若待转换结构体为空或者此时结构体类型不为频谱感知结果, 则直接退出
    {
        return flag;
    }
    if (bit_sequence_ptr_ != NULL)
    { // 若指针不为空, 则释放原空间
        delete[] bit_sequence_ptr_;
        bit_sequence_ptr_ = NULL;
    }
    uint16_t temp = 0;
    BuildLink_Request *mana_info_ptr_temp = (BuildLink_Request *)Daatr_struct_ptr_;
    uint8_t *packet = new uint8_t[number]; // 生成新空间
    temp |= (mana_info_ptr_temp->nodeID >> 2) & 0xff;
    packet[0] = temp;
    temp = 0;
    temp |= (mana_info_ptr_temp->nodeID & 0x03);
    temp <<= 6;
    temp |= (mana_info_ptr_temp->if_build & 0x3f);
    packet[1] = temp;
    bit_sequence_ptr_ = packet;
    flag = true;
    return flag;
}

// 0/1序列转化为建链请求
bool MacDaatr_struct_converter::daatr_0_1_to_build_req()
{
    bool flag = false;
    int num = 2;
    int number = 0;
    int i = 0;
    int j = 0;
    if (bit_sequence_ptr_ == NULL || type_ != 6)
    {
        return flag;
    }
    if (Daatr_struct_ptr_ != NULL)
    { // 若结构体指针不为空, 则释放
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    BuildLink_Request *struct_ptr_temp = new BuildLink_Request;
    memset(struct_ptr_temp, 0, sizeof(BuildLink_Request));
    uint16_t temp = 0;
    temp |= bit_sequence_ptr_[0];
    temp <<= 2;
    temp |= (bit_sequence_ptr_[1] >> 6) & 0xff;
    struct_ptr_temp->nodeID = temp;
    temp = 0;
    temp |= (bit_sequence_ptr_[1] & 0x3f);
    struct_ptr_temp->if_build = temp;
    Daatr_struct_ptr_ = (uint8_t *)struct_ptr_temp;
    flag = true;
    return flag;
}

// 网管信道数据类型转换为0/1序列
bool MacDaatr_struct_converter::daatr_mana_packet_type_to_0_1()
{
    int i = 0;
    int number = 1;
    int flag = false;
    if (Daatr_struct_ptr_ == NULL || type_ != 8) // 若待转换结构体为空或者此时结构体类型不为网管节点通告消息, 则直接退出
    {
        return flag;
    }
    if (bit_sequence_ptr_ != NULL)
    { // 若指针不为空, 则释放原空间
        delete[] bit_sequence_ptr_;
        bit_sequence_ptr_ = NULL;
    }
    Mana_Packet_Type *mana_type_ptr_temp = (Mana_Packet_Type *)Daatr_struct_ptr_;
    uint8_t *packet = new uint8_t[number]; // 生成新空间
    packet[0] = mana_type_ptr_temp->type;
    bit_sequence_ptr_ = packet;
    /*for(i = 0; i < number; i++)
    {
        cout << hex << (short)bit_sequence_ptr_[i] << endl;
    }
    cout << dec;*/
    flag = true;
    return flag;
}

// 0/1序列转换为网管信道数据类型
bool MacDaatr_struct_converter::daatr_0_1_to_mana_packet_type()
{
    uint32_t temp_vari = 0;
    bool flag = false;
    int num = 0;
    int i = 0;
    if (bit_sequence_ptr_ == NULL || type_ != 8)
    {
        return flag;
    }
    if (Daatr_struct_ptr_ != NULL)
    { // 若结构体指针不为空, 则释放
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    Mana_Packet_Type *struct_ptr_temp = new Mana_Packet_Type;
    memset(struct_ptr_temp, 0, sizeof(Mana_Packet_Type));
    struct_ptr_temp->type = bit_sequence_ptr_[0];
    Daatr_struct_ptr_ = (uint8_t *)struct_ptr_temp;
    // 测试用
    // cout << "state: "  << struct_ptr_temp->state << endl;
    ////////////////////
    flag = true;
    return flag;
}

bool MacDaatr_struct_converter::daatr_slottable_to_0_1()
{
    int i = 0;
    int j = 0;
    int k = 0;
    int number = 550;
    int flag = false;
    if (Daatr_struct_ptr_ == NULL || type_ != 7)
    {
        return flag;
    }

    if (bit_sequence_ptr_ != NULL)
    { // 若指针不为空, 则释放原空间
        delete[] bit_sequence_ptr_;
        bit_sequence_ptr_ = NULL;
    }

    Send_slottable *slottable_temp = (Send_slottable *)Daatr_struct_ptr_;
    uint8_t *packet = new uint8_t[number]; // 生成新空间

    for (i = 0; i < 50; i++)
    {
        packet[11 * i + 0] = slottable_temp->slot_traffic_execution_new[8 * i + 0].state << 7 |
                             (slottable_temp->slot_traffic_execution_new[8 * i + 0].send_or_recv_node & 0x3f8) >> 3; // 高7, 右移3

        packet[11 * i + 1] = (slottable_temp->slot_traffic_execution_new[8 * i + 0].send_or_recv_node & 0x007) << 5 | // 低3, 左移5
                             slottable_temp->slot_traffic_execution_new[8 * i + 1].state << 4 |
                             (slottable_temp->slot_traffic_execution_new[8 * i + 1].send_or_recv_node & 0x3c0) >> 6; // 高4, 右移6

        packet[11 * i + 2] = (slottable_temp->slot_traffic_execution_new[8 * i + 1].send_or_recv_node & 0x03f) << 2 | // 低6, 左移2
                             slottable_temp->slot_traffic_execution_new[8 * i + 2].state << 1 |
                             (slottable_temp->slot_traffic_execution_new[8 * i + 2].send_or_recv_node & 0x200) >> 9; // 高1, 右移9

        packet[11 * i + 3] = (slottable_temp->slot_traffic_execution_new[8 * i + 2].send_or_recv_node & 0x1fe) >> 1; // 中8, 右移1

        packet[11 * i + 4] = (slottable_temp->slot_traffic_execution_new[8 * i + 2].send_or_recv_node & 0x001) << 7 | // 低1, 左移7
                             slottable_temp->slot_traffic_execution_new[8 * i + 3].state << 6 |
                             (slottable_temp->slot_traffic_execution_new[8 * i + 3].send_or_recv_node & 0x3f0) >> 4; // 高6, 右移4

        packet[11 * i + 5] = (slottable_temp->slot_traffic_execution_new[8 * i + 3].send_or_recv_node & 0x00f) << 4 | // 低4, 左移4
                             slottable_temp->slot_traffic_execution_new[8 * i + 4].state << 3 |
                             (slottable_temp->slot_traffic_execution_new[8 * i + 4].send_or_recv_node & 0x380) >> 7; // 高3, 右移7

        packet[11 * i + 6] = (slottable_temp->slot_traffic_execution_new[8 * i + 4].send_or_recv_node & 0x07f) << 1 | // 低7, 左移1
                             slottable_temp->slot_traffic_execution_new[8 * i + 5].state;

        packet[11 * i + 7] = (slottable_temp->slot_traffic_execution_new[8 * i + 5].send_or_recv_node & 0x3fc) >> 2; // 高8, 右移2

        packet[11 * i + 8] = (slottable_temp->slot_traffic_execution_new[8 * i + 5].send_or_recv_node & 0x003) << 6 | // 低2, 左移6
                             slottable_temp->slot_traffic_execution_new[8 * i + 6].state << 5 |
                             (slottable_temp->slot_traffic_execution_new[8 * i + 6].send_or_recv_node & 0x3e0) >> 7; // 高5, 右移5

        packet[11 * i + 9] = (slottable_temp->slot_traffic_execution_new[8 * i + 6].send_or_recv_node & 0x01f) << 3 | // 低5, 左移3
                             slottable_temp->slot_traffic_execution_new[8 * i + 7].state << 2 |
                             (slottable_temp->slot_traffic_execution_new[8 * i + 7].send_or_recv_node & 0x300) >> 8; // 高2, 右移8

        packet[11 * i + 10] = (slottable_temp->slot_traffic_execution_new[8 * i + 7].send_or_recv_node & 0x0ff); // 低8
    }
    bit_sequence_ptr_ = packet;
    flag = true;
    return flag;
}

bool MacDaatr_struct_converter::daatr_0_1_to_slottable()
{
    bool flag = false;
    int num = 550;
    int number = 0;
    int i = 0;
    int j = 0;
    if (bit_sequence_ptr_ == NULL || type_ != 7)
    {
        return flag;
    }
    if (Daatr_struct_ptr_ != NULL)
    { // 若结构体指针不为空, 则释放
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }

    Send_slottable *slottable_temp = new Send_slottable;
    memset(slottable_temp, 0, sizeof(Send_slottable));

    for (i = 0; i < 50; i++)
    {
        // for (j = 0; j < 50; j++)
        // {
        slottable_temp->slot_traffic_execution_new[8 * i + 0].state = (bit_sequence_ptr_[11 * i + 0] & 0x80) >> 7;                                                           // 高 1
        slottable_temp->slot_traffic_execution_new[8 * i + 0].send_or_recv_node = (bit_sequence_ptr_[11 * i + 0] & 0x7f) << 3 | (bit_sequence_ptr_[11 * i + 1] & 0xe0) >> 5; // 低7, 左移3  高3, 右移5

        slottable_temp->slot_traffic_execution_new[8 * i + 1].state = (bit_sequence_ptr_[11 * i + 1] & 0x10) >> 4;
        slottable_temp->slot_traffic_execution_new[8 * i + 1].send_or_recv_node = (bit_sequence_ptr_[11 * i + 1] & 0x0f) << 6 | (bit_sequence_ptr_[11 * i + 2] & 0xfc) >> 2; // 低4, 左移6  高6, 右移2

        slottable_temp->slot_traffic_execution_new[8 * i + 2].state = (bit_sequence_ptr_[11 * i + 2] & 0x02) >> 1;
        slottable_temp->slot_traffic_execution_new[8 * i + 2].send_or_recv_node = (bit_sequence_ptr_[11 * i + 2] & 0x01) << 9 |
                                                                                  (bit_sequence_ptr_[11 * i + 3] & 0xff) << 1 |
                                                                                  (bit_sequence_ptr_[11 * i + 4] & 0x80) >> 7; // 低1左移9, 中8左移1, 高1右移7

        slottable_temp->slot_traffic_execution_new[8 * i + 3].state = (bit_sequence_ptr_[11 * i + 4] & 0x40) >> 6;
        slottable_temp->slot_traffic_execution_new[8 * i + 3].send_or_recv_node = (bit_sequence_ptr_[11 * i + 4] & 0x3f) << 4 | (bit_sequence_ptr_[11 * i + 5] & 0xf0) >> 4; // 低6左移4, 高4右移4

        slottable_temp->slot_traffic_execution_new[8 * i + 4].state = (bit_sequence_ptr_[11 * i + 5] & 0x08) >> 3;                                                           // 低4
        slottable_temp->slot_traffic_execution_new[8 * i + 4].send_or_recv_node = (bit_sequence_ptr_[11 * i + 5] & 0x07) << 7 | (bit_sequence_ptr_[11 * i + 6] & 0xfe) >> 1; // 低3左移7, 高7右移1

        slottable_temp->slot_traffic_execution_new[8 * i + 5].state = (bit_sequence_ptr_[11 * i + 6] & 0x01);                                                         // 低1
        slottable_temp->slot_traffic_execution_new[8 * i + 5].send_or_recv_node = (bit_sequence_ptr_[11 * i + 7]) << 2 | (bit_sequence_ptr_[11 * i + 8] & 0xc0) >> 6; // 高8左移2, 高2右移6

        slottable_temp->slot_traffic_execution_new[8 * i + 6].state = (bit_sequence_ptr_[11 * i + 8] & 0x20) >> 5;                                                           // 高3
        slottable_temp->slot_traffic_execution_new[8 * i + 6].send_or_recv_node = (bit_sequence_ptr_[11 * i + 8] & 0x1f) << 5 | (bit_sequence_ptr_[11 * i + 9] & 0xf8) >> 3; // 低5左移5, 高5右移3

        slottable_temp->slot_traffic_execution_new[8 * i + 7].state = (bit_sequence_ptr_[11 * i + 9] & 0x04) >> 2;                                                       // 低3
        slottable_temp->slot_traffic_execution_new[8 * i + 7].send_or_recv_node = (bit_sequence_ptr_[11 * i + 9] & 0x03) << 8 | (bit_sequence_ptr_[11 * i + 10] & 0xff); // 低2左移8, 高8
        // }
    }

    Daatr_struct_ptr_ = (uint8_t *)slottable_temp;
    flag = true;
    return flag;
}

// 网管节点接入请求回复转换为0/1
bool MacDaatr_struct_converter::daatr_access_reply_to_0_1()
{
    int i = 0;
    int number = 32;
    int flag = false;
    if (Daatr_struct_ptr_ == NULL || type_ != 9) // 若待转换结构体为空或者此时结构体类型不为网管节点通告消息, 则直接退出
    {
        return flag;
    }
    if (bit_sequence_ptr_ != NULL)
    { // 若指针不为空, 则释放原空间
        delete[] bit_sequence_ptr_;
        bit_sequence_ptr_ = NULL;
    }
    mana_access_reply *mana_type_ptr_temp = (mana_access_reply *)Daatr_struct_ptr_;
    uint8_t *packet = new uint8_t[number]; // 生成新空间
    for (i = 0; i < number; i++)
    {
        packet[i] = 0;
    }
    packet[0] = ((mana_type_ptr_temp->mana_node_hopping[0] >> 1) & 0xff);
    packet[1] = ((mana_type_ptr_temp->mana_node_hopping[0] & 0x01) & 0xff);
    packet[1] <<= 7;
    packet[1] |= ((mana_type_ptr_temp->mana_node_hopping[1] >> 2) & 0xff);
    packet[2] = ((mana_type_ptr_temp->mana_node_hopping[1] & 0x03) & 0xff);
    packet[2] <<= 6;
    packet[2] |= ((mana_type_ptr_temp->mana_node_hopping[2] >> 3) & 0xff);
    packet[3] = ((mana_type_ptr_temp->mana_node_hopping[2] & 0x07) & 0xff);
    packet[3] <<= 5;
    packet[3] |= ((mana_type_ptr_temp->mana_node_hopping[3] >> 4) & 0xff);
    packet[4] = ((mana_type_ptr_temp->mana_node_hopping[3] & 0x0f) & 0xff);
    packet[4] <<= 4;
    packet[4] |= ((mana_type_ptr_temp->mana_node_hopping[4] >> 5) & 0xff);
    packet[5] = ((mana_type_ptr_temp->mana_node_hopping[4] & 0x1f) & 0xff);
    packet[5] <<= 3;
    packet[5] |= ((mana_type_ptr_temp->mana_node_hopping[5] >> 6) & 0xff);
    packet[6] = ((mana_type_ptr_temp->mana_node_hopping[5] & 0x3f) & 0xff);
    packet[6] <<= 2;
    packet[6] |= ((mana_type_ptr_temp->mana_node_hopping[6] >> 7) & 0xff);
    packet[7] = ((mana_type_ptr_temp->mana_node_hopping[6] & 0x7f) & 0xff);
    packet[7] <<= 1;
    packet[7] |= ((mana_type_ptr_temp->mana_node_hopping[7] >> 8) & 0xff);
    packet[8] = (mana_type_ptr_temp->mana_node_hopping[7] & 0xff);

    packet[9] = ((mana_type_ptr_temp->mana_node_hopping[8] >> 1) & 0xff);
    packet[10] = ((mana_type_ptr_temp->mana_node_hopping[8] & 0x01) & 0xff);
    packet[10] <<= 7;
    packet[10] |= ((mana_type_ptr_temp->mana_node_hopping[9] >> 2) & 0xff);
    packet[11] = ((mana_type_ptr_temp->mana_node_hopping[9] & 0x03) & 0xff);
    packet[11] <<= 6;
    packet[11] |= ((mana_type_ptr_temp->mana_node_hopping[10] >> 3) & 0xff);
    packet[12] = ((mana_type_ptr_temp->mana_node_hopping[10] & 0x07) & 0xff);
    packet[12] <<= 5;
    packet[12] |= ((mana_type_ptr_temp->mana_node_hopping[11] >> 4) & 0xff);
    packet[13] = ((mana_type_ptr_temp->mana_node_hopping[11] & 0x0f) & 0xff);
    packet[13] <<= 4;
    packet[13] |= ((mana_type_ptr_temp->mana_node_hopping[12] >> 5) & 0xff);
    packet[14] = ((mana_type_ptr_temp->mana_node_hopping[12] & 0x1f) & 0xff);
    packet[14] <<= 3;
    packet[14] |= ((mana_type_ptr_temp->mana_node_hopping[13] >> 6) & 0xff);
    packet[15] = ((mana_type_ptr_temp->mana_node_hopping[13] & 0x3f) & 0xff);
    packet[15] <<= 2;
    packet[15] |= ((mana_type_ptr_temp->mana_node_hopping[14] >> 7) & 0xff);
    packet[16] = ((mana_type_ptr_temp->mana_node_hopping[14] & 0x7f) & 0xff);
    packet[16] <<= 1;
    packet[16] |= ((mana_type_ptr_temp->mana_node_hopping[15] >> 8) & 0xff);
    packet[17] = (mana_type_ptr_temp->mana_node_hopping[15] & 0xff);

    packet[18] = ((mana_type_ptr_temp->mana_node_hopping[16] >> 1) & 0xff);
    packet[19] = ((mana_type_ptr_temp->mana_node_hopping[16] & 0x01) & 0xff);
    packet[19] <<= 7;
    packet[19] |= ((mana_type_ptr_temp->mana_node_hopping[17] >> 2) & 0xff);
    packet[20] = ((mana_type_ptr_temp->mana_node_hopping[17] & 0x03) & 0xff);
    packet[20] <<= 6;
    packet[20] |= ((mana_type_ptr_temp->mana_node_hopping[18] >> 3) & 0xff);
    packet[21] = ((mana_type_ptr_temp->mana_node_hopping[18] & 0x07) & 0xff);
    packet[21] <<= 5;
    packet[21] |= ((mana_type_ptr_temp->mana_node_hopping[19] >> 4) & 0xff);
    packet[22] = ((mana_type_ptr_temp->mana_node_hopping[19] & 0x0f) & 0xff);
    packet[22] <<= 4;
    packet[22] |= ((mana_type_ptr_temp->mana_node_hopping[20] >> 5) & 0xff);
    packet[23] = ((mana_type_ptr_temp->mana_node_hopping[20] & 0x1f) & 0xff);
    packet[23] <<= 3;
    packet[23] |= ((mana_type_ptr_temp->mana_node_hopping[21] >> 6) & 0xff);
    packet[24] = ((mana_type_ptr_temp->mana_node_hopping[21] & 0x3f) & 0xff);
    packet[24] <<= 2;
    packet[24] |= ((mana_type_ptr_temp->mana_node_hopping[22] >> 7) & 0xff);
    packet[25] = ((mana_type_ptr_temp->mana_node_hopping[22] & 0x7f) & 0xff);
    packet[25] <<= 1;
    packet[25] |= ((mana_type_ptr_temp->mana_node_hopping[23] >> 8) & 0xff);
    packet[26] = (mana_type_ptr_temp->mana_node_hopping[23] & 0xff);

    packet[27] = ((mana_type_ptr_temp->mana_node_hopping[24] >> 1) & 0xff);
    packet[28] = ((mana_type_ptr_temp->mana_node_hopping[24] & 0x01) & 0xff);
    packet[28] <<= 7;

    packet[28] |= ((mana_type_ptr_temp->slot_location[0] & 0xff) << 1);
    packet[28] |= ((mana_type_ptr_temp->slot_location[1] >> 5) & 0xff);
    packet[29] = ((mana_type_ptr_temp->slot_location[1] & 0x1f) & 0xff);
    packet[29] <<= 3;
    packet[29] |= ((mana_type_ptr_temp->slot_location[2] >> 3) & 0xff);
    packet[30] = ((mana_type_ptr_temp->slot_location[2] & 0x07) & 0xff);
    packet[30] <<= 5;
    packet[30] |= ((mana_type_ptr_temp->slot_location[3] >> 1) & 0xff);
    packet[31] = ((mana_type_ptr_temp->slot_location[3] & 0x01) & 0xff);
    packet[31] <<= 7;
    packet[31] |= ((mana_type_ptr_temp->slot_location[4] << 1) & 0xff);

    bit_sequence_ptr_ = packet;
    // for(i = 0; i < number; i++)
    //{
    //	cout << hex << (short)bit_sequence_ptr_[i] << endl;
    // }
    // cout << dec;
    flag = true;
    return flag;
}

// 0/1序列转换为网管节点接入请求
bool MacDaatr_struct_converter::daatr_0_1_to_access_reply()
{
    uint16_t temp_vari = 0;
    bool flag = false;
    int num = 0;
    int i = 0;
    if (bit_sequence_ptr_ == NULL || type_ != 9)
    {
        return flag;
    }
    if (Daatr_struct_ptr_ != NULL)
    { // 若结构体指针不为空, 则释放
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    mana_access_reply *struct_ptr_temp = new mana_access_reply;
    memset(struct_ptr_temp, 0, sizeof(mana_access_reply));
    temp_vari = bit_sequence_ptr_[0];
    temp_vari <<= 1;
    temp_vari |= bit_sequence_ptr_[1] >> 7;
    struct_ptr_temp->mana_node_hopping[0] = temp_vari;
    temp_vari = bit_sequence_ptr_[1] & 0x7f;
    temp_vari <<= 2;
    temp_vari |= bit_sequence_ptr_[2] >> 6;
    struct_ptr_temp->mana_node_hopping[1] = temp_vari;
    temp_vari = bit_sequence_ptr_[2] & 0x3f;
    temp_vari <<= 3;
    temp_vari |= bit_sequence_ptr_[3] >> 5;
    struct_ptr_temp->mana_node_hopping[2] = temp_vari;
    temp_vari = bit_sequence_ptr_[3] & 0x1f;
    temp_vari <<= 4;
    temp_vari |= bit_sequence_ptr_[4] >> 4;
    struct_ptr_temp->mana_node_hopping[3] = temp_vari;
    temp_vari = bit_sequence_ptr_[4] & 0x0f;
    temp_vari <<= 5;
    temp_vari |= bit_sequence_ptr_[5] >> 3;
    struct_ptr_temp->mana_node_hopping[4] = temp_vari;
    temp_vari = bit_sequence_ptr_[5] & 0x07;
    temp_vari <<= 6;
    temp_vari |= bit_sequence_ptr_[6] >> 2;
    struct_ptr_temp->mana_node_hopping[5] = temp_vari;
    temp_vari = bit_sequence_ptr_[6] & 0x03;
    temp_vari <<= 7;
    temp_vari |= bit_sequence_ptr_[7] >> 1;
    struct_ptr_temp->mana_node_hopping[6] = temp_vari;
    temp_vari = bit_sequence_ptr_[7] & 0x01;
    temp_vari <<= 8;
    temp_vari |= bit_sequence_ptr_[8];
    struct_ptr_temp->mana_node_hopping[7] = temp_vari;

    temp_vari = bit_sequence_ptr_[9];
    temp_vari <<= 1;
    temp_vari |= bit_sequence_ptr_[10] >> 7;
    struct_ptr_temp->mana_node_hopping[8] = temp_vari;
    temp_vari = bit_sequence_ptr_[10] & 0x7f;
    temp_vari <<= 2;
    temp_vari |= bit_sequence_ptr_[11] >> 6;
    struct_ptr_temp->mana_node_hopping[9] = temp_vari;
    temp_vari = bit_sequence_ptr_[11] & 0x3f;
    temp_vari <<= 3;
    temp_vari |= bit_sequence_ptr_[12] >> 5;
    struct_ptr_temp->mana_node_hopping[10] = temp_vari;
    temp_vari = bit_sequence_ptr_[12] & 0x1f;
    temp_vari <<= 4;
    temp_vari |= bit_sequence_ptr_[13] >> 4;
    struct_ptr_temp->mana_node_hopping[11] = temp_vari;
    temp_vari = bit_sequence_ptr_[13] & 0x0f;
    temp_vari <<= 5;
    temp_vari |= bit_sequence_ptr_[14] >> 3;
    struct_ptr_temp->mana_node_hopping[12] = temp_vari;
    temp_vari = bit_sequence_ptr_[14] & 0x07;
    temp_vari <<= 6;
    temp_vari |= bit_sequence_ptr_[15] >> 2;
    struct_ptr_temp->mana_node_hopping[13] = temp_vari;
    temp_vari = bit_sequence_ptr_[15] & 0x03;
    temp_vari <<= 7;
    temp_vari |= bit_sequence_ptr_[16] >> 1;
    struct_ptr_temp->mana_node_hopping[14] = temp_vari;
    temp_vari = bit_sequence_ptr_[16] & 0x01;
    temp_vari <<= 8;
    temp_vari |= bit_sequence_ptr_[17];
    struct_ptr_temp->mana_node_hopping[15] = temp_vari;

    temp_vari = bit_sequence_ptr_[18];
    temp_vari <<= 1;
    temp_vari |= bit_sequence_ptr_[19] >> 7;
    struct_ptr_temp->mana_node_hopping[16] = temp_vari;
    temp_vari = bit_sequence_ptr_[19] & 0x7f;
    temp_vari <<= 2;
    temp_vari |= bit_sequence_ptr_[20] >> 6;
    struct_ptr_temp->mana_node_hopping[17] = temp_vari;
    temp_vari = bit_sequence_ptr_[20] & 0x3f;
    temp_vari <<= 3;
    temp_vari |= bit_sequence_ptr_[21] >> 5;
    struct_ptr_temp->mana_node_hopping[18] = temp_vari;
    temp_vari = bit_sequence_ptr_[21] & 0x1f;
    temp_vari <<= 4;
    temp_vari |= bit_sequence_ptr_[22] >> 4;
    struct_ptr_temp->mana_node_hopping[19] = temp_vari;
    temp_vari = bit_sequence_ptr_[22] & 0x0f;
    temp_vari <<= 5;
    temp_vari |= bit_sequence_ptr_[23] >> 3;
    struct_ptr_temp->mana_node_hopping[20] = temp_vari;
    temp_vari = bit_sequence_ptr_[23] & 0x07;
    temp_vari <<= 6;
    temp_vari |= bit_sequence_ptr_[24] >> 2;
    struct_ptr_temp->mana_node_hopping[21] = temp_vari;
    temp_vari = bit_sequence_ptr_[24] & 0x03;
    temp_vari <<= 7;
    temp_vari |= bit_sequence_ptr_[25] >> 1;
    struct_ptr_temp->mana_node_hopping[22] = temp_vari;
    temp_vari = bit_sequence_ptr_[25] & 0x01;
    temp_vari <<= 8;
    temp_vari |= bit_sequence_ptr_[26];
    struct_ptr_temp->mana_node_hopping[23] = temp_vari;

    temp_vari = bit_sequence_ptr_[27];
    temp_vari <<= 1;
    temp_vari |= bit_sequence_ptr_[28] >> 7;
    struct_ptr_temp->mana_node_hopping[24] = temp_vari;

    temp_vari = bit_sequence_ptr_[28] & 0x7e;
    temp_vari >>= 1;
    struct_ptr_temp->slot_location[0] = temp_vari;
    temp_vari = bit_sequence_ptr_[28] & 0x01;
    temp_vari <<= 5;
    temp_vari |= (bit_sequence_ptr_[29] >> 3);
    struct_ptr_temp->slot_location[1] = temp_vari;
    temp_vari = bit_sequence_ptr_[29] & 0x07;
    temp_vari <<= 3;
    temp_vari |= (bit_sequence_ptr_[30] >> 5);
    struct_ptr_temp->slot_location[2] = temp_vari;
    temp_vari = bit_sequence_ptr_[30] & 0x1f;
    temp_vari <<= 1;
    temp_vari |= (bit_sequence_ptr_[31] >> 7);
    struct_ptr_temp->slot_location[3] = temp_vari;
    temp_vari = bit_sequence_ptr_[31] & 0x7e;
    temp_vari >>= 1;
    struct_ptr_temp->slot_location[4] = temp_vari;

    for (i = 0; i < 5; i++)
    {
        if (struct_ptr_temp->slot_location[i] == 60)
        {
            break;
        }
    }
    struct_ptr_temp->slot_num = i;
    Daatr_struct_ptr_ = (uint8_t *)struct_ptr_temp;
    flag = true;
    return flag;
}

// 全网跳频图案转换成0/1
bool MacDaatr_struct_converter::daatr_subnet_frequency_parttern_to_0_1()
{
    int i = 0, j = 0, k = 0;
    int number = 563;
    int flag = false;
    if (Daatr_struct_ptr_ == NULL || type_ != 10) // 若待转换结构体为空或者此时结构体类型不为网管节点通告消息, 则直接退出
    {
        return flag;
    }
    if (bit_sequence_ptr_ != NULL)
    { // 若指针不为空, 则释放原空间
        delete[] bit_sequence_ptr_;
        bit_sequence_ptr_ = NULL;
    }
    subnet_frequency_parttern *mana_type_ptr_temp = (subnet_frequency_parttern *)Daatr_struct_ptr_;
    uint8_t *packet = new uint8_t[number]; // 生成新空间
    for (i = 0; i < SUBNET_NODE_NUMBER; i++)
    {
        for (j = 0; j < FREQUENCY_COUNT; j++)
        {
            mana_type_ptr_temp->mana_node_hopping[k] = mana_type_ptr_temp->subnet_node_hopping[i][j];
            k++;
        }
    }
    for (i = 0; i < number; i++)
    {
        packet[i] = 0;
    }
    j = 0;
    k = 0;
    for (i = 0; i < 62; i++)
    {
        packet[0 + j] = ((mana_type_ptr_temp->mana_node_hopping[0 + k] >> 1) & 0xff);
        packet[1 + j] = ((mana_type_ptr_temp->mana_node_hopping[0 + k] & 0x01) & 0xff);
        packet[1 + j] <<= 7;
        packet[1 + j] |= ((mana_type_ptr_temp->mana_node_hopping[1 + k] >> 2) & 0xff);
        packet[2 + j] = ((mana_type_ptr_temp->mana_node_hopping[1 + k] & 0x03) & 0xff);
        packet[2 + j] <<= 6;
        packet[2 + j] |= ((mana_type_ptr_temp->mana_node_hopping[2 + k] >> 3) & 0xff);
        packet[3 + j] = ((mana_type_ptr_temp->mana_node_hopping[2 + k] & 0x07) & 0xff);
        packet[3 + j] <<= 5;
        packet[3 + j] |= ((mana_type_ptr_temp->mana_node_hopping[3 + k] >> 4) & 0xff);
        packet[4 + j] = ((mana_type_ptr_temp->mana_node_hopping[3 + k] & 0x0f) & 0xff);
        packet[4 + j] <<= 4;
        packet[4 + j] |= ((mana_type_ptr_temp->mana_node_hopping[4 + k] >> 5) & 0xff);
        packet[5 + j] = ((mana_type_ptr_temp->mana_node_hopping[4 + k] & 0x1f) & 0xff);
        packet[5 + j] <<= 3;
        packet[5 + j] |= ((mana_type_ptr_temp->mana_node_hopping[5 + k] >> 6) & 0xff);
        packet[6 + j] = ((mana_type_ptr_temp->mana_node_hopping[5 + k] & 0x3f) & 0xff);
        packet[6 + j] <<= 2;
        packet[6 + j] |= ((mana_type_ptr_temp->mana_node_hopping[6 + k] >> 7) & 0xff);
        packet[7 + j] = ((mana_type_ptr_temp->mana_node_hopping[6 + k] & 0x7f) & 0xff);
        packet[7 + j] <<= 1;
        packet[7 + j] |= ((mana_type_ptr_temp->mana_node_hopping[7 + k] >> 8) & 0xff);
        packet[8 + j] = (mana_type_ptr_temp->mana_node_hopping[7 + k] & 0xff);
        j += 9;
        k += 8;
    }
    packet[558] |= mana_type_ptr_temp->mana_node_hopping[496] >> 1;
    packet[559] |= mana_type_ptr_temp->mana_node_hopping[496] & 0x01;
    packet[559] <<= 7;
    packet[559] |= mana_type_ptr_temp->mana_node_hopping[497] >> 2;
    packet[560] |= mana_type_ptr_temp->mana_node_hopping[497] & 0x03;
    packet[560] <<= 6;
    packet[560] |= mana_type_ptr_temp->mana_node_hopping[498] >> 3;
    packet[561] |= mana_type_ptr_temp->mana_node_hopping[498] & 0x07;
    packet[561] <<= 5;
    packet[561] |= mana_type_ptr_temp->mana_node_hopping[499] >> 4;
    packet[562] |= mana_type_ptr_temp->mana_node_hopping[499] & 0x0f;
    packet[562] <<= 4;
    bit_sequence_ptr_ = packet;
    // for(i = 0; i < number; i++)
    //{
    //	cout << hex << (short)bit_sequence_ptr_[i] << endl;
    // }
    // cout << dec;
    flag = true;
    return flag;
}

// 0/1序列转换成全网跳频图案
bool MacDaatr_struct_converter::daatr_0_1_to_subnet_frequency_parttern()
{
    uint16_t temp_vari = 0;
    bool flag = false;
    int num = 0;
    int i = 0, j = 0, k = 0;
    if (bit_sequence_ptr_ == NULL || type_ != 10)
    {
        return flag;
    }
    if (Daatr_struct_ptr_ != NULL)
    { // 若结构体指针不为空, 则释放
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    subnet_frequency_parttern *struct_ptr_temp = new subnet_frequency_parttern;
    memset(struct_ptr_temp, 0, sizeof(subnet_frequency_parttern));
    for (i = 0; i < 62; i++)
    {
        temp_vari = bit_sequence_ptr_[0 + k];
        temp_vari <<= 1;
        temp_vari |= bit_sequence_ptr_[1 + k] >> 7;
        struct_ptr_temp->mana_node_hopping[0 + j] = temp_vari;
        temp_vari = bit_sequence_ptr_[1 + k] & 0x7f;
        temp_vari <<= 2;
        temp_vari |= bit_sequence_ptr_[2 + k] >> 6;
        struct_ptr_temp->mana_node_hopping[1 + j] = temp_vari;
        temp_vari = bit_sequence_ptr_[2 + k] & 0x3f;
        temp_vari <<= 3;
        temp_vari |= bit_sequence_ptr_[3 + k] >> 5;
        struct_ptr_temp->mana_node_hopping[2 + j] = temp_vari;
        temp_vari = bit_sequence_ptr_[3 + k] & 0x1f;
        temp_vari <<= 4;
        temp_vari |= bit_sequence_ptr_[4 + k] >> 4;
        struct_ptr_temp->mana_node_hopping[3 + j] = temp_vari;
        temp_vari = bit_sequence_ptr_[4 + k] & 0x0f;
        temp_vari <<= 5;
        temp_vari |= bit_sequence_ptr_[5 + k] >> 3;
        struct_ptr_temp->mana_node_hopping[4 + j] = temp_vari;
        temp_vari = bit_sequence_ptr_[5 + k] & 0x07;
        temp_vari <<= 6;
        temp_vari |= bit_sequence_ptr_[6 + k] >> 2;
        struct_ptr_temp->mana_node_hopping[5 + j] = temp_vari;
        temp_vari = bit_sequence_ptr_[6 + k] & 0x03;
        temp_vari <<= 7;
        temp_vari |= bit_sequence_ptr_[7 + k] >> 1;
        struct_ptr_temp->mana_node_hopping[6 + j] = temp_vari;
        temp_vari = bit_sequence_ptr_[7 + k] & 0x01;
        temp_vari <<= 8;
        temp_vari |= bit_sequence_ptr_[8 + k];
        struct_ptr_temp->mana_node_hopping[7 + j] = temp_vari;
        j += 8;
        k += 9;
    }
    temp_vari = bit_sequence_ptr_[558];
    temp_vari <<= 1;
    temp_vari |= bit_sequence_ptr_[559] >> 7;
    struct_ptr_temp->mana_node_hopping[496] = temp_vari;
    temp_vari = bit_sequence_ptr_[559] & 0x7f;
    temp_vari <<= 2;
    temp_vari |= bit_sequence_ptr_[560] >> 6;
    struct_ptr_temp->mana_node_hopping[497] = temp_vari;
    temp_vari = bit_sequence_ptr_[560] & 0x3f;
    temp_vari <<= 3;
    temp_vari |= bit_sequence_ptr_[561] >> 5;
    struct_ptr_temp->mana_node_hopping[498] = temp_vari;
    temp_vari = bit_sequence_ptr_[561] & 0x1f;
    temp_vari <<= 4;
    temp_vari |= bit_sequence_ptr_[562] >> 4;
    struct_ptr_temp->mana_node_hopping[499] = temp_vari;
    for (i = 0; i < 500; i++)
    {
        struct_ptr_temp->subnet_node_hopping[i / 25][i % 25] = struct_ptr_temp->mana_node_hopping[i];
    }
    Daatr_struct_ptr_ = (uint8_t *)struct_ptr_temp;
    flag = true;
    return flag;
}

void MacDaatr_struct_converter::daatr_print_result()
{
    int number;
    int i;
    if (bit_sequence_ptr_ == NULL && Daatr_struct_ptr_ == NULL)
    {
        cout << "无输出数据!" << endl;
        return;
    }
    switch (type_)
    {
    case 0:
    {
        cout << "类型为0, 不输出" << endl;
        return;
    }
    case 1:
    {
        number = 25;
        if (bit_sequence_ptr_ != NULL)
        {
            cout << "PDU1比特序列16进制表示: " << endl;
            for (i = 0; i < number; i++)
            {
                cout << hex << (short)bit_sequence_ptr_[i] << " ";
            }
            cout << dec << endl;
        }
        if (Daatr_struct_ptr_ != NULL)
        {
            MacHeader *struct_ptr_temp = (MacHeader *)Daatr_struct_ptr_;
            cout << "PDU1结构体展示: " << endl;
            cout << "PDUtype: " << struct_ptr_temp->PDUtype << endl;
            cout << "is_mac_layer: " << struct_ptr_temp->is_mac_layer << endl;
            cout << "backup: " << struct_ptr_temp->backup << endl;
            cout << "packetLength: " << struct_ptr_temp->packetLength << endl;
            cout << "srcAddr: " << struct_ptr_temp->srcAddr << endl;
            cout << "destAddr: " << struct_ptr_temp->destAddr << endl;
            cout << "seq: " << struct_ptr_temp->seq << endl;
            cout << "mac_backup: " << struct_ptr_temp->mac_backup << endl;
            cout << "mac_headerChecksum: " << struct_ptr_temp->mac_headerChecksum << endl;
            cout << "mac_pSynch: " << struct_ptr_temp->mac_pSynch << endl;
            cout << "mac: " << struct_ptr_temp->mac << endl;
        }
        break;
    }
    case 2:
    {
        number = 17;
        if (bit_sequence_ptr_ != NULL)
        {
            cout << "PDU2比特序列16进制表示: " << endl;
            for (i = 0; i < number; i++)
            {
                cout << hex << (short)bit_sequence_ptr_[i] << " ";
            }
            cout << dec << endl;
        }
        if (Daatr_struct_ptr_ != NULL)
        {
            MacHeader2 *struct_ptr_temp = (MacHeader2 *)Daatr_struct_ptr_;
            cout << "PDU2结构体展示: " << endl;
            cout << "PDUtype: " << struct_ptr_temp->PDUtype << endl;
            cout << "packetLength: " << struct_ptr_temp->packetLength << endl;
            cout << "srcAddr: " << struct_ptr_temp->srcAddr << endl;
            cout << "destAddr: " << struct_ptr_temp->destAddr << endl;
            cout << "slice_head_identification: " << struct_ptr_temp->slice_head_identification << endl;
            cout << "fragment_tail_identification: " << struct_ptr_temp->fragment_tail_identification << endl;
            cout << "backup: " << struct_ptr_temp->backup << endl;
            cout << "mac_headerChecksum: " << struct_ptr_temp->mac_headerChecksum << endl;
            cout << "mac_pSynch: " << struct_ptr_temp->mac_pSynch << endl;
            cout << "mac: " << struct_ptr_temp->mac << endl;
        }
        break;
    }
    case 3:
    {
        number = 24;
        if (bit_sequence_ptr_ != NULL)
        {
            cout << "飞行状态信息比特序列16进制表示: " << endl;
            for (i = 0; i < number; i++)
            {
                cout << hex << (short)bit_sequence_ptr_[i] << " ";
            }
            cout << dec << endl;
        }
        if (Daatr_struct_ptr_ != NULL)
        {
            FlightStatus *struct_ptr_temp = (FlightStatus *)Daatr_struct_ptr_;
            cout << "飞行状态信息结构体展示: " << endl;
            cout << "positionX: " << struct_ptr_temp->positionX << endl;
            cout << "positionY: " << struct_ptr_temp->positionY << endl;
            cout << "positionZ: " << struct_ptr_temp->positionZ << endl;
            cout << "pitchAngle: " << struct_ptr_temp->pitchAngle << endl;
            cout << "yawAngle: " << struct_ptr_temp->yawAngle << endl;
            cout << "rollAngle: " << struct_ptr_temp->rollAngle << endl;
        }
        break;
    }
    case 4:
    {
        number = 1;
        if (bit_sequence_ptr_ != NULL)
        {
            cout << "网管节点通告消息比特序列16进制表示: " << endl;
            for (i = 0; i < number; i++)
            {
                cout << hex << (short)bit_sequence_ptr_[i] << " ";
            }
            cout << dec << endl;
        }
        if (Daatr_struct_ptr_ != NULL)
        {
            if_need_change_state *struct_ptr_temp = (if_need_change_state *)Daatr_struct_ptr_;
            cout << "网管节点通告消息结构体展示: " << endl;
            cout << "state: " << struct_ptr_temp->state << endl;
        }
        break;
    }
    case 5:
    {
        number = 63;
        if (bit_sequence_ptr_ != NULL)
        {
            cout << "频谱感知结果比特序列16进制表示: " << endl;
            for (i = 0; i < number; i++)
            {
                cout << hex << (short)bit_sequence_ptr_[i] << " ";
            }
            cout << dec << endl;
        }
        if (Daatr_struct_ptr_ != NULL)
        {
            spectrum_sensing_struct *struct_ptr_temp = (spectrum_sensing_struct *)Daatr_struct_ptr_;
            cout << "频谱感知结果结构体展示: " << endl;
            for (i = 0; i < TOTAL_FREQ_POINT; i++)
            {
                cout << struct_ptr_temp->spectrum_sensing[i] << " ";
            }
            cout << endl;
        }
        break;
    }
    case 6:
    {
        number = 2;
        if (bit_sequence_ptr_ != NULL)
        {
            cout << "建链请求比特序列16进制表示: " << endl;
            for (i = 0; i < number; i++)
            {
                cout << hex << (short)bit_sequence_ptr_[i] << " ";
            }
            cout << dec << endl;
        }
        if (Daatr_struct_ptr_ != NULL)
        {
            BuildLink_Request *struct_ptr_temp = (BuildLink_Request *)Daatr_struct_ptr_;
            cout << "建链请求结构体展示: " << endl;
            cout << "nodeID: " << struct_ptr_temp->nodeID << endl;
            cout << "if_build: " << struct_ptr_temp->if_build << endl;
            cout << endl;
        }
        break;
    }
    case 8:
    {
        number = 1;
        if (bit_sequence_ptr_ != NULL)
        {
            cout << "网管信道数据包类型16进制表示：" << endl;
            for (i = 0; i < number; i++)
            {
                cout << hex << (short)bit_sequence_ptr_[i] << " ";
            }
            cout << dec << endl;
        }
        if (Daatr_struct_ptr_ != NULL)
        {
            Mana_Packet_Type *struct_ptr_temp = (Mana_Packet_Type *)Daatr_struct_ptr_;
            cout << "网管信道数据包类型结构体展示：" << endl;
            cout << "Type: " << struct_ptr_temp->type << endl;
            cout << endl;
        }
        break;
    }

    case 9:
    {
        number = 32;
        if (bit_sequence_ptr_ != NULL)
        {
            cout << "网管节点接入请求回复16进制表示：" << endl;
            for (i = 0; i < number; i++)
            {
                cout << hex << (short)bit_sequence_ptr_[i] << " ";
            }
            cout << dec << endl;
        }
        if (Daatr_struct_ptr_ != NULL)
        {
            mana_access_reply *struct_ptr_temp = (mana_access_reply *)Daatr_struct_ptr_;
            cout << "网管节点接入请求回复结构体展示：" << endl;
            cout << "mana_node_hopping: ";
            for (i = 0; i < FREQUENCY_COUNT; i++)
            {
                cout << struct_ptr_temp->mana_node_hopping[i] << " ";
            }
            cout << endl;
            cout << "slot_location: ";
            for (i = 0; i < 5; i++)
            {
                cout << (int)struct_ptr_temp->slot_location[i] << " ";
            }
            cout << endl;
            cout << "slot_num: " << (int)struct_ptr_temp->slot_num << endl;
            cout << endl;
        }
    }

    case 10:
    {
        number = 563;
        int j = 0;
        if (bit_sequence_ptr_ != NULL)
        {
            cout << "子网跳频图案16进制表示：" << endl;
            for (i = 0; i < number; i++)
            {
                cout << hex << (short)bit_sequence_ptr_[i] << " ";
            }
            cout << dec << endl;
        }
        if (Daatr_struct_ptr_ != NULL)
        {
            subnet_frequency_parttern *struct_ptr_temp = (subnet_frequency_parttern *)Daatr_struct_ptr_;
            cout << "子网跳频图案结构体展示：" << endl;
            for (i = 0; i < SUBNET_NODE_NUMBER; i++)
            {
                cout << "Node ID " << i << "  ";
                for (j = 0; j < FREQUENCY_COUNT; j++)
                {
                    cout << struct_ptr_temp->subnet_node_hopping[i][j] << " ";
                }
                cout << endl;
            }
        }
        break;
    }

    case 255:
    {
        if (bit_sequence_ptr_ != NULL)
        {
            cout << "帧比特序列16进制表示: " << endl;
            for (i = 0; i < length_; i++)
            {
                cout << hex << (short)bit_sequence_ptr_[i] << " ";
            }
            cout << dec << endl;
        }
    }
    }
}

// 根据发送速率填满0/1序列
// speed单位：kbps
// 传输数据量以byte为最小单位
bool MacDaatr_struct_converter::daatr_add_0_to_full(double speed)
{
    bool flag = false;
    if (bit_sequence_ptr_ == NULL)
    {
        return flag;
    }
    if (Daatr_struct_ptr_ != NULL)
    {
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    int i;
    double bits_trans = speed * 2.5;      // 传输数据量：bits
    double byte_trans_1 = bits_trans / 8; // 传输数据量：byte
    int byte_trans = ceil(byte_trans_1);  // 传输数据量：byte(向上取整)
    uint8_t *frame_full = new uint8_t[byte_trans];
    if (byte_trans < length_)
    { // 如果待补零序列的长度大于可发送数据量, 拒绝补零
        return flag;
    }
    memset(frame_full, 0, sizeof(uint8_t) * byte_trans);
    memcpy(frame_full, bit_sequence_ptr_, sizeof(uint8_t) * length_);
    delete[] bit_sequence_ptr_;
    length_ = byte_trans;
    bit_sequence_ptr_ = frame_full;
    type_ = 255;
    flag = true;
    return flag;
}

// 和包成帧
MacDaatr_struct_converter &MacDaatr_struct_converter::operator+(const MacDaatr_struct_converter &packet1)
{
    if (type_ == 0 || packet1.type_ == 0 || bit_sequence_ptr_ == NULL || packet1.bit_sequence_ptr_ == NULL)
    { // 若两个类其中一个类型为0或者两个类中有比特序列指针为空, 则返回
        return *this;
    }
    int i = 0;
    int frame_size = length_ + packet1.length_;
    uint8_t *temp_ptr = new uint8_t[frame_size];
    for (i = 0; i < length_; i++)
    { // 将加号前比特序列从帧比特序列起始放入
        temp_ptr[i] = bit_sequence_ptr_[i];
    }
    for (i; i < frame_size; i++)
    { // 紧跟着放入加号后的比特序列
        temp_ptr[i] = packet1.bit_sequence_ptr_[i - length_];
    }
    delete[] bit_sequence_ptr_;
    bit_sequence_ptr_ = temp_ptr;
    if (Daatr_struct_ptr_ != NULL)
    {
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    type_ = 255;          // 将类型改为5
    length_ = frame_size; // 修改比特序列长度
    return *this;
}

// 拆包
MacDaatr_struct_converter &MacDaatr_struct_converter::operator-(const MacDaatr_struct_converter &packet1)
{
    if (type_ == 0 || packet1.type_ != 255 || packet1.bit_sequence_ptr_ == NULL)
    { // 若两个类其中一个类型不为255或者两个类中有比特序列指针为空, 则返回
        return *this;
    }
    if (bit_sequence_ptr_ != NULL)
    {
        delete[] bit_sequence_ptr_;
        bit_sequence_ptr_ = NULL;
    }
    if (Daatr_struct_ptr_ != NULL)
    {
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    int number = 0;
    int i = 0;
    switch (type_)
    {
    case 0:
        return *this;
        break;
    case 1:
    { // 想要PDU1
        number = 25;
        length_ = number;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = packet1.bit_sequence_ptr_[i];
        }
        break;
    }
    case 2:
    { // 想要PDU2
        number = 17;
        length_ = number;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = packet1.bit_sequence_ptr_[i];
        }
        break;
    }
    case 3:
    { // 想要飞行状态信息
        number = 24;
        int number2 = 17 + 1; // PDU2大小
        length_ = number;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = packet1.bit_sequence_ptr_[number2 + i];
        }
        break;
    }
    case 4:
    { // 想要网管节点通告消息
        number = 1;
        int number2 = 17 + 1; // PDU2大小
        length_ = number;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = packet1.bit_sequence_ptr_[number2 + i];
        }
        break;
    }
    case 5:
    { // 想要频谱感知结果
        number = 63;
        int number2 = 25; // PDU2大小
        length_ = number;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = packet1.bit_sequence_ptr_[number2 + i];
        }
        break;
    }
    case 6:
    { // 想要建链请求
        number = 2;
        // int number2 = 17;//PDU2大小
        length_ = number;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = packet1.bit_sequence_ptr_[i];
        }
        break;
    }
    case 7:
    { // 想要时隙表
        number = 550;
        int number2 = 25; // PDU1大小
        length_ = number;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = packet1.bit_sequence_ptr_[number2 + i];
        }
        break;
    }
    case 8:
    { // 想要网管信道消息类型
        number = 17;
        int number2 = 1;
        length_ = number2;
        bit_sequence_ptr_ = new uint8_t[number2];
        for (i = number; i < number + 1; i++)
        {
            bit_sequence_ptr_[0] = packet1.bit_sequence_ptr_[i];
        }
        break;
    }
    case 9:
    { // 想要网管节点接入请求
        number = 32;
        int number2 = 18;
        length_ = number2;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = packet1.bit_sequence_ptr_[number2 + i];
        }
        break;
    }
    case 10:
    { // 想要全网跳频图案
        number = 563;
        int number2 = 25; // 全网跳频图案大小
        length_ = number;
        bit_sequence_ptr_ = new uint8_t[number];
        for (i = 0; i < number; i++)
        {
            bit_sequence_ptr_[i] = packet1.bit_sequence_ptr_[number2 + i];
        }
        break;
    }
    case 99:
    {
        number = 25;
        int len = packet1.length_;
        length_ = len - number;
        type_ = 255;
        bit_sequence_ptr_ = new uint8_t[len - number];
        for (i = number; i < len; i++)
        {
            bit_sequence_ptr_[i - number] = packet1.bit_sequence_ptr_[i];
        }
        break;
    }
    case 100:
    {
        number = 17;
        int len = packet1.length_;
        length_ = len - number;
        type_ = 255;
        bit_sequence_ptr_ = new uint8_t[len - number];
        for (i = number; i < len; i++)
        {
            bit_sequence_ptr_[i - number] = packet1.bit_sequence_ptr_[i];
        }
        break;
    }
    }
    return *this;
}

// 网管信道补零操作（网管信道速率：20kpbs）最高50byte
bool MacDaatr_struct_converter::daatr_mana_channel_add_0_to_full()
{
    bool flag = false;
    if (bit_sequence_ptr_ == NULL)
    {
        return flag;
    }
    if (Daatr_struct_ptr_ != NULL)
    {
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    int sequence_length = 50; // 帧总长(byte)
    if (sequence_length <= length_)
    { // 如果待补零序列的长度大于或等于可发送数据量, 拒绝补零
        return flag;
    }
    uint8_t *frame_full = new uint8_t[sequence_length];
    memset(frame_full, 0, sizeof(uint8_t) * sequence_length);
    memcpy(frame_full, bit_sequence_ptr_, sizeof(uint8_t) * length_);
    delete[] bit_sequence_ptr_;
    length_ = sequence_length;
    bit_sequence_ptr_ = frame_full;
    type_ = 255;
    flag = true;
    return flag;
}

// 等号
MacDaatr_struct_converter &MacDaatr_struct_converter::operator=(const MacDaatr_struct_converter &packet1)
{
    if (packet1.bit_sequence_ptr_ == NULL)
    {
        return *this;
    }
    if (Daatr_struct_ptr_ != NULL)
    { // 释放原结构体指针
        delete Daatr_struct_ptr_;
        Daatr_struct_ptr_ = NULL;
    }
    Daatr_struct_ptr_ = NULL;
    int paket_size = packet1.length_;
    bit_sequence_ptr_ = new uint8_t[paket_size];
    for (int i = 0; i < paket_size; i++)
    {
        bit_sequence_ptr_[i] = packet1.bit_sequence_ptr_[i];
    }
    length_ = paket_size;
    type_ = packet1.type_; // 类型相同
    return *this;
}

// 结构体转化为0/1序列
bool MacDaatr_struct_converter::daatr_struct_to_0_1()
{
    bool flag = false;
    switch (type_)
    {
    case 0:
        return flag;
        break;
    case 1:
        flag = daatr_PDU1_to_0_1();
        break;
    case 2:
        flag = daatr_PDU2_to_0_1();
        break;
    case 3:
        flag = daatr_flight_to_0_1();
        break;
    case 4:
        flag = daatr_mana_info_to_0_1();
        break;
    case 5:
        flag = daatr_freq_sense_to_0_1();
        break;
    case 6:
        flag = daatr_build_req_to_0_1();
        break;
    case 7:
        flag = daatr_slottable_to_0_1();
        break;
    case 8:
        flag = daatr_mana_packet_type_to_0_1();
        break;
    case 9:
        flag = daatr_access_reply_to_0_1();
        break;
    case 10:
        flag = daatr_subnet_frequency_parttern_to_0_1();
        break;
    }
    return flag;
}

// 0/1序列转化为结构体
bool MacDaatr_struct_converter::daatr_0_1_to_struct()
{
    bool flag = false;
    switch (type_)
    {
    case 0:
        return flag;
        break;
    case 1:
        flag = daatr_0_1_to_PDU1();
        break;
    case 2:
        flag = daatr_0_1_to_PDU2();
        break;
    case 3:
        flag = daatr_0_1_to_flight();
        break;
    case 4:
        flag = daatr_0_1_to_mana_info();
        break;
    case 5:
        flag = daatr_0_1_to_freq_sense();
        break;
    case 6:
        flag = daatr_0_1_to_build_req();
        break;
    case 7:
        flag = daatr_0_1_to_slottable();
        break;
    case 8:
        flag = daatr_0_1_to_mana_packet_type();
        break;
    case 9:
        flag = daatr_0_1_to_access_reply();
        break;
    case 10:
        flag = daatr_0_1_to_subnet_frequency_parttern();
        break;
    }
    return flag;
}
/****************************************************************************************************************************************/

// 函数名: AnalysisLayer2MsgFromControl
// 函数: 解析层2发来的业务数据包
// 功能: 将数组内对应的内容存入msgFromControl结构体
// 输入: uint8_t数组
static msgFromControl *AnalysisLayer2MsgFromControl(vector<uint8_t> *dataPacket)
{
    msgFromControl *temp_msgFromControl = new msgFromControl();
    vector<uint8_t> *cur_data = new vector<uint8_t>;

    temp_msgFromControl->priority = ((*dataPacket)[0] >> 5) & 0xFF;
    temp_msgFromControl->backup = ((*dataPacket)[0] >> 4) & 0x01;
    temp_msgFromControl->msgType = (*dataPacket)[0] & 0x0F;
    temp_msgFromControl->packetLength = ((*dataPacket)[1] << 8) | (*dataPacket)[2];
    temp_msgFromControl->srcAddr = ((*dataPacket)[3] << 2) | ((*dataPacket)[4] >> 6);
    temp_msgFromControl->destAddr = ((*dataPacket)[4] << 4) | ((*dataPacket)[5] >> 4);
    temp_msgFromControl->seq = ((*dataPacket)[5] << 6) | ((*dataPacket)[6] >> 2);
    temp_msgFromControl->repeat = (*dataPacket)[6] & 0x03;
    temp_msgFromControl->fragmentNum = (*dataPacket)[7] >> 4;
    temp_msgFromControl->fragmentSeq = (*dataPacket)[7] & 0x0F;

    unsigned short dataLen = temp_msgFromControl->packetLength - 10;
    for (size_t i = 1; i <= dataLen; i++)
    {
        cur_data->push_back((*dataPacket)[7 + i]);
    }
    temp_msgFromControl->data = (char *)cur_data;
    temp_msgFromControl->CRC = ((*dataPacket)[8 + dataLen] << 8) | ((*dataPacket)[9 + dataLen]);

    return temp_msgFromControl;
}

// 函数名: CRC16
// 函数: CRC-16/MODBUS
// 功能: 计算CRC校验码
// 输入: uint8_t数组
static uint16_t CRC16(vector<uint8_t> *buffer)
{
    uint16_t crc = 0xFFFF; // 初始化计算值
    for (int i = 0; i < buffer->size(); i++)
    {
        crc ^= (*buffer)[i];
        for (int i = 0; i < 8; ++i)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// 函数名: PackMsgFromControl
// 函数: 封装业务数据包
// 功能: 封装业务数据包以便进行CRC校验
// 输入: 业务数据包指针
static vector<uint8_t> *PackMsgFromControl(msgFromControl *packet)
{
    vector<uint8_t> *buffer = new vector<uint8_t>;

    // 封装第一个字节
    buffer->push_back(((packet->priority << 5) | (packet->backup << 4) | (packet->msgType)) & 0xFF);

    // 封装第二个字节
    buffer->push_back((packet->packetLength >> 8) & 0xFF);

    // 封装第三个字节
    buffer->push_back(packet->packetLength & 0xFF);

    // 封装第四个字节
    buffer->push_back((packet->srcAddr >> 2) & 0xFF);

    // 封装第五个字节
    buffer->push_back(((packet->srcAddr << 6) | (packet->destAddr >> 4)) & 0xFF);

    // 封装第六个字节
    buffer->push_back(((packet->destAddr << 4) | (packet->seq >> 6)) & 0xFF);

    // 封装第七个字节
    buffer->push_back(((packet->seq << 2) | packet->repeat) & 0xFF);

    // 封装第八个字节
    buffer->push_back(((packet->fragmentNum << 4) | packet->fragmentSeq) & 0xFF);

    vector<uint8_t> *curData = new vector<uint8_t>;
    curData = (vector<uint8_t> *)(packet->data);

    // 封装报文内容
    for (int i = 0; i < curData->size(); i++)
    {
        buffer->push_back((*curData)[i]);
    }

    // 根据前面的字节计算CRC校验码
    packet->CRC = CRC16(buffer);

    // 封装CRC
    buffer->push_back((packet->CRC >> 8) & 0xFF);
    buffer->push_back(packet->CRC & 0xFF);

    return buffer;
}

static uint8_t *deepcopy(uint8_t *frame_ptr, int length)
{
    uint8_t *frame_ptr_new = new uint8_t[length];
    for (int i = 0; i < length; i++)
    {
        frame_ptr_new[i] = frame_ptr[i];
    }
    return frame_ptr_new;
}

/* ------ 调频模块 ------ */
// 生成九位m序列
unsigned char m_sequence[M_SEQUENCE_LENGTH] = {0, 0, 0, 0, 0, 0, 0, 0, 1};
unsigned char m_sequence_index = M_SEQUENCE_LENGTH - 1;
unsigned int frequency_sequence[SUBNET_NODE_FREQ_NUMBER][FREQUENCY_COUNT]; // 初始跳频序列

// 生成下一个m序列值
static unsigned char Generate_M_SequenceValue()
{
    unsigned char feedback = (m_sequence[m_sequence_index] + m_sequence[0]) % 2;
    unsigned char output = m_sequence[m_sequence_index];

    for (int i = m_sequence_index; i > 0; i--)
    {
        m_sequence[i] = m_sequence[i - 1];
    }

    m_sequence[0] = feedback;

    return output;
}

// 调整窄点
static void Adjust_Narrow_Band(unsigned int frequency_sequence[SUBNET_NODE_FREQ_NUMBER][FREQUENCY_COUNT])
{
    int prev_node;

    for (int time = 0; time < FREQUENCY_COUNT; time++)
    {
        for (int node = 0; node < SUBNET_NODE_FREQ_NUMBER; node++)
        {
            prev_node = node - 1;
            while (prev_node >= 0)
            {
                if (frequency_sequence[node][time] - frequency_sequence[prev_node][time] <= MIN_FREQUENCY_DIFFERENCE ||
                    frequency_sequence[prev_node][time] - frequency_sequence[node][time] <= MIN_FREQUENCY_DIFFERENCE)
                {
                    prev_node = node - 1;
                    frequency_sequence[node][time] += NARROW_BAND_WIDTH;

                    if (frequency_sequence[node][time] >= MAX_FREQUENCY)
                    {
                        frequency_sequence[node][time] -= MAX_FREQUENCY;
                    }
                }
                else
                {
                    prev_node--;
                }
            }
        }
    }
}

// 跳频序列调整
static void Adjust_Frequency_Sequence(unsigned int frequency_sequence[SUBNET_NODE_FREQ_NUMBER][FREQUENCY_COUNT],
                                      const unsigned char available_frequency[TOTAL_FREQ_POINT])
{
    int flag = 0;
    for (int time = 0; time < FREQUENCY_COUNT; time++)
    {
        for (int node = 0; node < SUBNET_NODE_FREQ_NUMBER; node++)
        {
            unsigned int current_frequency = frequency_sequence[node][time];
            unsigned int frequency_found_count = 0;
            // 如果当前频率点不可用, 则顺延至下一个可用频点
            while (available_frequency[current_frequency] == 0)
            {
                current_frequency++;
                frequency_found_count++;

                // 检查频点是否超过最大频率, 若超过则减去最大频率
                if (current_frequency >= MAX_FREQUENCY)
                {
                    current_frequency -= MAX_FREQUENCY;
                }
                if (frequency_found_count == TOTAL_FREQ_POINT)
                {
                    printf("找不到可用频点!\n");
                    break;
                }
            }

            frequency_sequence[node][time] = current_frequency;
            frequency_found_count = 0;
            // 顺延之后的频点, 依次判断是否占用了不可用频点
            for (int i = node + 1; i < SUBNET_NODE_FREQ_NUMBER; i++)
            {
                unsigned int next_frequency = frequency_sequence[i][time];

                // 如果下一个频率点是不可用的, 则顺延至下一个可用频点
                while (available_frequency[next_frequency] == 0)
                {
                    next_frequency++;
                    frequency_found_count++;

                    // 检查频点是否超过最大频率, 若超过则减去最大频率
                    if (next_frequency >= MAX_FREQUENCY)
                    {
                        next_frequency -= MAX_FREQUENCY;
                    }
                    if (frequency_found_count == TOTAL_FREQ_POINT)
                    {
                        // printf("找不到可用频点\n");
                        flag = 1;
                        break;
                    }
                }

                frequency_sequence[i][time] = next_frequency;
                if (flag == 1)
                {
                    break;
                }
            }
            if (flag == 1)
            {
                break;
            }
        }
        if (flag == 1)
        {
            break;
        }
    }
}

static void Generate_Frequency_Sequence(MacDataDaatr *macdata_daatr)
{
    unsigned int frequency_sequence[SUBNET_NODE_FREQ_NUMBER][FREQUENCY_COUNT]; // 调整前序列
    // unsigned int node_frequency[SUBNET_NODE_NUMBER][FREQUENCY_COUNT];//调整后序列
    unsigned char available_frequency[TOTAL_FREQ_POINT];
    float interference_ratios_before = 0;
    float interference_ratios_after = 0;
    int interference_count_before = 0;
    int interference_count_after = 0;

    for (int time = 0; time < FREQUENCY_COUNT; time++)
    {
        // int time = FREQUENCY_COUNT - 1;
        for (int node = 0; node < SUBNET_NODE_FREQ_NUMBER; node++)
        {
            unsigned int m_sequence_decimal = 0;

            for (int i = 0; i < M_SEQUENCE_LENGTH; i++)
            {
                m_sequence_decimal <<= 1;
                m_sequence_decimal |= Generate_M_SequenceValue();
            }

            frequency_sequence[node][time] = m_sequence_decimal % MAX_FREQUENCY;
        }
    }
    Adjust_Narrow_Band(frequency_sequence);
    for (int j = 0; j < TOTAL_FREQ_POINT; j++)
    {
        available_frequency[j] = macdata_daatr->unavailable_frequency_points[j];
    }
    // 优先满足窄带调整, 直到没有窄带
    int narrow_band_found = 1;
    int unavailable_freq_found = 1;
    int total_attempts = 0;
    int max_attempts = 50; // Maximum attempts to find suitable frequencies
    while ((narrow_band_found || unavailable_freq_found) && total_attempts < max_attempts)
    {
        narrow_band_found = 0;
        unavailable_freq_found = 0;

        Adjust_Narrow_Band(frequency_sequence);

        Adjust_Frequency_Sequence(frequency_sequence, available_frequency);

        // 检查是否有窄带
        for (int time = 0; time < FREQUENCY_COUNT; time++)
        {
            for (int node = 0; node < SUBNET_NODE_FREQ_NUMBER; node++)
            {
                int prev_node = node - 1;
                while (prev_node >= 0)
                {
                    if (frequency_sequence[node][time] - frequency_sequence[prev_node][time] <= MIN_FREQUENCY_DIFFERENCE ||
                        frequency_sequence[prev_node][time] - frequency_sequence[node][time] <= MIN_FREQUENCY_DIFFERENCE)
                    {
                        narrow_band_found = 1;
                        break;
                    }
                    else
                    {
                        prev_node--;
                    }
                }
                if (available_frequency[frequency_sequence[node][time]] == 0)
                {
                    unavailable_freq_found = 1;
                    break;
                }
            }

            if (narrow_band_found || unavailable_freq_found)
            {
                break;
            }
        }
        total_attempts++;
        break;
    }
    for (int i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
    { // 将生成的调频数组赋值给网管节点保存的生成跳频组
        for (int j = 0; j < FREQUENCY_COUNT; j++)
        {
            macdata_daatr->frequency_sequence[i][j] = frequency_sequence[i][j];
        }
    }
    // 打印调整后每个节点的频率序列
    printf("调整后跳频序列\n");
    for (int node = 0; node < SUBNET_NODE_FREQ_NUMBER; node++)
    {
        printf("Node %d:", node + 1);

        for (int time = 0; time < FREQUENCY_COUNT; time++)
        {
            printf("%d ", macdata_daatr->frequency_sequence[node][time]);
        }
        printf("\n");
    }
}

static void MacDaatr_Node_Frequency_Initialization(Node *node, MacDataDaatr *macdata_daatr)
{
    if (has_initializing_frequency_sequency == false)
    {
        unsigned char available_frequency[TOTAL_FREQ_POINT];
        float interference_ratios_before = 0;
        float interference_ratios_after = 0;
        int interference_count_before = 0;
        int interference_count_after = 0;
        for (int time = 0; time < FREQUENCY_COUNT; time++)
        {
            // int time = FREQUENCY_COUNT - 1;
            for (int node = 0; node < SUBNET_NODE_FREQ_NUMBER; node++)
            {
                unsigned int m_sequence_decimal = 0;
                for (int i = 0; i < M_SEQUENCE_LENGTH; i++)
                {
                    m_sequence_decimal <<= 1;
                    m_sequence_decimal |= Generate_M_SequenceValue();
                }

                frequency_sequence[node][time] = m_sequence_decimal % MAX_FREQUENCY;
            }
        }
        // 调整窄带, 直到没有窄带
        int narrow_band_found = 1;
        int total_attempts = 0;
        int max_attempts = 100; // Maximum attempts to find suitable frequencies
        while (narrow_band_found == 1 && total_attempts < max_attempts)
        {
            narrow_band_found = 0;
            Adjust_Narrow_Band(frequency_sequence);
            // 检查是否有窄带
            for (int time = 0; time < FREQUENCY_COUNT; time++)
            {
                for (int node = 0; node < SUBNET_NODE_FREQ_NUMBER; node++)
                {
                    int prev_node = node - 1;
                    while (prev_node >= 0)
                    {
                        if (((frequency_sequence[node][time] - frequency_sequence[prev_node][time] >= 0) && (frequency_sequence[node][time] - frequency_sequence[prev_node][time] < MIN_FREQUENCY_DIFFERENCE)) || ((frequency_sequence[node][time] - frequency_sequence[prev_node][time] < 0) && (frequency_sequence[prev_node][time] - frequency_sequence[node][time] < MIN_FREQUENCY_DIFFERENCE)))

                        {
                            narrow_band_found = 1;
                            break;
                        }
                        else
                        {
                            prev_node--;
                        }
                    }
                }
            }
        }
        has_initializing_frequency_sequency = true;
        // 打印节点初始的频率序列
        printf("初始跳频序列\n");
        for (int node = 0; node < SUBNET_NODE_FREQ_NUMBER; node++)
        {
            printf("Node %d:", node + 1);
            for (int time = 0; time < FREQUENCY_COUNT; time++)
            {
                printf("%d ", frequency_sequence[node][time]);
            }
            printf("\n");
        }
    }
    for (int i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
    { // 将生成的跳频数组赋值给网管节点保存的生成跳频组
        for (int j = 0; j < FREQUENCY_COUNT; j++)
        {
            macdata_daatr->frequency_sequence[i][j] = frequency_sequence[i][j];
        }
    }
}

/* ------ 通感算模块 ------*/
static void TGS_copy_node_topology(MacDataDaatr *macdata_daatr, std::vector<std::vector<uint16_t>> *topo_recv)
{ // 复制接收到的网络拓扑结构矩阵
    int i, j;
    for (i = 0; i < SUBNET_NODE_NUMBER; i++)
    {
        for (j = 0; j < SUBNET_NODE_NUMBER; j++)
        {
            macdata_daatr->subnet_topology_matrix[i][j] = (*topo_recv)[i][j];
        }
    }
}

// 返回任意两节点间距离(单位: km)
static double calculateLinkDistance(float lat_1, float lng_1, float h_1, float lat_2, float lng_2, float h_2)
{
    lat_1 = PI * lat_1 / 180.0;
    lng_1 = PI * lng_1 / 180.0;
    lat_2 = PI * lat_2 / 180.0;
    lng_2 = PI * lng_2 / 180.0;
    float e2 = (pow(RADIS_A, 2) - pow(RADIS_B, 2)) / pow(RADIS_A, 2); // 椭球偏心率
    float n_1 = RADIS_A / sqrt(1 - e2 * pow(sin(lat_1), 2));
    float n_2 = RADIS_A / sqrt(1 - e2 * pow(sin(lat_2), 2));
    float Pdx = 0, Pdy = 0, Pdz = 0;
    Pdx = (n_2 + h_2) * cos(lat_2) * cos(lng_2) - (n_1 + h_1) * cos(lat_1) * cos(lng_1);
    Pdy = (n_2 + h_2) * cos(lat_2) * sin(lng_2) - (n_1 + h_1) * cos(lat_1) * sin(lng_1);
    Pdz = (n_2 * (1 - e2) + h_2) * sin(lat_2) - (n_1 * (1 - e2) + h_1) * sin(lat_1);
    float dx = 0, dy = 0, dz = 0;
    dx = -sin(lat_1) * cos(lng_1) * Pdx - sin(lat_1) * sin(lng_1) * Pdy + cos(lat_1) * Pdz;
    dy = cos(lat_1) * cos(lng_1) * Pdx + cos(lat_1) * sin(lng_1) * Pdy + sin(lat_1) * Pdz;
    dz = -sin(lng_1) * Pdx + cos(lng_1) * Pdy;
    double dr = 0;
    dr = sqrt(pow(dx, 2) + pow(dy, 2) + pow(dz, 2));
    dr /= 1000;
    return dr;
}

// 以一定概率随机生成0或1
static int Randomly_Generate_0_1(double one_probability)
{
    // 以one_probability大小的概率生成1
    int t = 0;
    // srand(time(NULL));
    double random_number = (double)rand() / RAND_MAX;
    // cout << random_number;
    if (random_number < one_probability)
    {
        t = 1;
    }
    else
    {
        t = 0;
    }
    return t;
}

// 根据输入的两节点距离计算对应的传输速率 kbps
static double calculate_slot_type_mana(double distance)
{
    double tran_speed = 0;
    if (distance <= 50)
    {
        tran_speed = 11800;
    }
    else if (distance > 50 && distance <= 100)
    {
        tran_speed = 4096;
    }
    else if (distance > 100 && distance <= 200)
    {
        tran_speed = 2048;
    }
    else if (distance > 200 && distance <= 300)
    {
        tran_speed = 1024;
    }
    else if (distance > 300 && distance <= 500)
    {
        tran_speed = 512;
    }
    else if (distance > 500 && distance <= 600)
    {
        tran_speed = 256;
    }
    else if (distance > 600 && distance <= 700)
    {
        tran_speed = 128;
    }
    else if (distance > 700 && distance <= 800)
    {
        tran_speed = 64;
    }
    else if (distance > 800 && distance <= 900)
    {
        tran_speed = 32;
    }
    else if (distance > 900 && distance <= 1200)
    {
        tran_speed = 16;
    }
    else if (distance > 1200 && distance <= 1500)
    {
        tran_speed = 8;
    }
    return tran_speed;
}

static void Send_Local_Link_Status(Node *node, MacDataDaatr *macdata_daatr)
{
    ReceiveLocalLinkState_Msg loca_linkstate_struct;
    vector<nodeLocalNeighList *> *send_neighborList = new vector<nodeLocalNeighList *>;
    // nodeNetNeighList *nodeNetNeigh_data = new nodeNetNeighList();
    int dest_node;
    unsigned int Capacity = 0;

    for (dest_node = 1; dest_node <= SUBNET_NODE_NUMBER; dest_node++)
    {
        if (macdata_daatr->traffic_channel_business[dest_node - 1].distance != 0)
        {
            Capacity = calculate_slot_type_mana(macdata_daatr->traffic_channel_business[dest_node - 1].distance);

            nodeLocalNeighList *nodeLocalNeighdata = new nodeLocalNeighList;
            nodeLocalNeighdata->nodeAddr = dest_node;
            nodeLocalNeighdata->Capacity = Capacity;

            // nodeNetNeigh_data->localNeighList.push_back(nodeLocalNeighdata);
            send_neighborList->push_back(nodeLocalNeighdata);
        }
        else
        {
            if (node->nodeId != dest_node)
            {
                cout << endl;
            }
        }
    }
    loca_linkstate_struct.neighborList = send_neighborList;

    Message *data_msg = MESSAGE_Alloc(node,
                                      NETWORK_LAYER,
                                      ROUTING_PROTOCOL_MMANET,
                                      MSG_ROUTING_ReceiveLocalLinkMsg);
    int data_msg_packetSize = sizeof(loca_linkstate_struct);
    MESSAGE_PacketAlloc(node, data_msg, data_msg_packetSize, TRACE_DAATR);
    memcpy(MESSAGE_ReturnPacket(data_msg), &loca_linkstate_struct, data_msg_packetSize);
    MESSAGE_Send(node, data_msg, 0);
}

// 查询本节点中存储其他节点飞行状态信息的数组中有多少nodeID非零元素
static int mac_search_number_of_node_position(MacDataDaatr *macdata_daatr)
{
    int i = 0;
    for (i = 0; i < SUBNET_NODE_FREQ_NUMBER - 1; i++)
    {
        if (macdata_daatr->subnet_other_node_position[i].nodeId == 0)
        { // 此处截止
            break;
        }
    }
    return i;
}

// 函数功能：网络层数据包有网管信道业务调用此函数将业务添加到本节点待发送网管信道业务队列中
bool MacDaatNetworkHasLowFreqChannelPacketToSend(Node *node, int interfaceIndex, msgFromControl *busin)
{
    int i = 0;
    int j = 0;
    int flag = 0; // 表征是否成功将busin插入业务队列
    MacDataDaatr *macdata_daatr = (MacDataDaatr *)node->macData[interfaceIndex]->macVar;
    for (i = 0; i < MANA_MAX_BUSINESS_NUMBER; i++)
    {
        if (macdata_daatr->mana_channel_business_queue[i].state == 0)
        {                                                                      // 此队列对应位置没有被占用
            macdata_daatr->mana_channel_business_queue[i].busin_data = *busin; // 将此业务添加进队列
            macdata_daatr->mana_channel_business_queue[i].state = 1;           // 将此队列位置占用
            flag = 1;                                                          // 证明已添加队列
            cout << "节点" << node->nodeId << "已接收到网络层下发的业务并添加于网管信道队列中" << endl;
            cout << "业务目的地址为" << macdata_daatr->mana_channel_business_queue[i].busin_data.destAddr << endl;
            break;
        }
    }
    if (flag == 0)
    {
        cout << "业务队列已满" << endl;
    }
    // delete busin;//删除发送的业务内存空间（？是否需要删除）
    return flag;
}

// 函数功能：从网管信道队列中获取业务去发送
bool getLowChannelBusinessFromQueue(msgFromControl *busin, Node *node, MacDataDaatr *macdata_daatr)
{
    int i = 0;
    bool flag = false;
    if (macdata_daatr->mana_channel_business_queue[i].state == 1)
    {                                                                      // 证明当前队列有业务
        *busin = macdata_daatr->mana_channel_business_queue[i].busin_data; // 将当前队列中的业务赋值给输入的空白业务
        for (i = 0; i < MANA_MAX_BUSINESS_NUMBER - 1; i++)
        { // 将当前业务队列中的业务前移一位
            macdata_daatr->mana_channel_business_queue[i] = macdata_daatr->mana_channel_business_queue[i + 1];
        }
        macdata_daatr->mana_channel_business_queue[MANA_MAX_BUSINESS_NUMBER - 1].state = 0; // 队列尾清空
        cout << "已成功从网管信道队列中取出业务,"
             << "目的地址为：" << busin->destAddr << endl;
        flag = true;
    }
    else
    {
        cout << "网管信道队列业务为空" << endl;
    }
    return flag;
}

// 大地坐标系到地心地固直角坐标系的转换
static A_ang Coordinate_System_Transaction(float lat_1, float lng_1, float h_1, float pitch_1, float roll_1, int index, float yaw_1, float lat_2, float lng_2, float h_2)
{
    lat_1 = PI * lat_1 / 180.0;
    lng_1 = PI * lng_1 / 180.0;
    pitch_1 = PI * pitch_1 / 180.0;
    roll_1 = PI * roll_1 / 180.0;
    yaw_1 = PI * yaw_1 / 180.0;
    lat_2 = PI * lat_2 / 180.0;
    lng_2 = PI * lng_2 / 180.0;
    float e2 = (pow(RADIS_A, 2) - pow(RADIS_B, 2)) / pow(RADIS_A, 2); // 椭球偏心率
    float n_1 = RADIS_A / sqrt(1 - e2 * pow(sin(lat_1), 2));
    float n_2 = RADIS_A / sqrt(1 - e2 * pow(sin(lat_2), 2));
    float Pdx = 0, Pdy = 0, Pdz = 0; // 地心地固坐标系下的相对位置
    Pdx = (n_2 + h_2) * cos(lat_2) * cos(lng_2) - (n_1 + h_1) * cos(lat_1) * cos(lng_1);
    Pdy = (n_2 + h_2) * cos(lat_2) * sin(lng_2) - (n_1 + h_1) * cos(lat_1) * sin(lng_1);
    Pdz = (n_2 * (1 - e2) + h_2) * sin(lat_2) - (n_1 * (1 - e2) + h_1) * sin(lat_1);
    float dx = 0, dy = 0, dz = 0; // 地理坐标系下的相对位置
    dx = -sin(lat_1) * cos(lng_1) * Pdx - sin(lat_1) * sin(lng_1) * Pdy + cos(lat_1) * Pdz;
    dy = cos(lat_1) * cos(lng_1) * Pdx + cos(lat_1) * sin(lng_1) * Pdy + sin(lat_1) * Pdz;
    dz = -sin(lng_1) * Pdx + cos(lng_1) * Pdy;
    float dx1 = 0, dy1 = 0, dz1 = 0; // DT坐标系下的相对位置
    dx1 = cos(yaw_1) * cos(pitch_1) * dx + sin(pitch_1) * dy - sin(yaw_1) * cos(pitch_1) * dz;
    dy1 = (sin(yaw_1) * sin(roll_1) - cos(yaw_1) * sin(pitch_1) * cos(roll_1)) * dx + cos(pitch_1) * cos(roll_1) * dy + (cos(yaw_1) * sin(roll_1) + sin(yaw_1) * sin(pitch_1) * cos(roll_1)) * dz;
    dz1 = (sin(yaw_1) * cos(roll_1) + cos(yaw_1) * sin(pitch_1) * sin(roll_1)) * dx - cos(pitch_1) * sin(roll_1) * dy + (cos(yaw_1) * cos(roll_1) - sin(yaw_1) * sin(pitch_1) * sin(roll_1)) * dz;
    float dr = 0; // 节点距离
    dr = sqrt(pow(dx, 2) + pow(dy, 2) + pow(dz, 2));
    float ang_x = 0, ang_z = 0;      // ang_x为侧面三个天线板的坐标系旋转角度, ang_z为前后两个天线板的坐标系旋转角度
    float dxs = 0, dys = 0, dzs = 0; // 天线系下指向
    // 选择天线板, 并计算该天线系下指向
    switch (index)
    {
    case 1:
        ang_x = PI / 3.0;
        dxs = dx1;
        dys = cos(ang_x) * dy1 - sin(ang_x) * dz1;
        dzs = sin(ang_x) * dy1 + cos(ang_x) * dz1;
        break;
    case 2:
        ang_x = PI;
        dxs = dx1;
        dys = cos(ang_x) * dy1 - sin(ang_x) * dz1;
        dzs = sin(ang_x) * dy1 + cos(ang_x) * dz1;
        break;
    case 3:
        ang_x = -PI / 3.0;
        dxs = dx1;
        dys = cos(ang_x) * dy1 - sin(ang_x) * dz1;
        dzs = sin(ang_x) * dy1 + cos(ang_x) * dz1;
        break;
    case 4:
        ang_z = -PI / 2.0;
        dxs = cos(ang_z) * dx1 - sin(ang_z) * dy1;
        dys = sin(ang_z) * dx1 + cos(ang_z) * dy1;
        dzs = dz1;
        break;
    case 5:
        ang_z = PI / 2.0;
        dxs = cos(ang_z) * dx1 - sin(ang_z) * dy1;
        dys = sin(ang_z) * dx1 + cos(ang_z) * dy1;
        dzs = dz1;
        break;
    default:
        printf("ERROR!\n");
        break;
    }
    A_ang a_ang;
    a_ang.el = asin(dxs / dr);
    a_ang.az = atan(dzs / dys);
    a_ang.b_ang = acos(fabs(cos(a_ang.el) * cos(a_ang.az)));
    a_ang.dys = dys;
    return a_ang;
}

// 返回俯仰角 方位角 等天线指向信息
static A_ang Get_Angle_Info(float lat_1, float lng_1, float h_1, short int pitch_1, short int roll_1, short int yaw_1,
                            float lat_2, float lng_2, float h_2)
{
    A_ang a_ang[5];
    // 计算5个天线对应的俯仰角和方位角等信息
    a_ang[0] = Coordinate_System_Transaction(lat_1, lng_1, h_1, pitch_1, roll_1, 1, yaw_1, lat_2, lng_2, h_2);
    a_ang[1] = Coordinate_System_Transaction(lat_1, lng_1, h_1, pitch_1, roll_1, 2, yaw_1, lat_2, lng_2, h_2);
    a_ang[2] = Coordinate_System_Transaction(lat_1, lng_1, h_1, pitch_1, roll_1, 3, yaw_1, lat_2, lng_2, h_2);
    a_ang[3] = Coordinate_System_Transaction(lat_1, lng_1, h_1, pitch_1, roll_1, 4, yaw_1, lat_2, lng_2, h_2);
    a_ang[4] = Coordinate_System_Transaction(lat_1, lng_1, h_1, pitch_1, roll_1, 5, yaw_1, lat_2, lng_2, h_2);
    float flag[5] = {0}; // 俯仰角和方位角绝对值之和
    // 是否在波束范围内的判断条件: (1)位于天线板上方(dys>0); (2)波束指向向量与天线坐标系的y轴正向的夹角b_ang小于等于60°
    if (a_ang[0].dys > 0 && a_ang[0].b_ang <= PI / 3.0)
    {
        flag[0] = fabs(a_ang[0].el) + fabs(a_ang[0].az);
    }
    if (a_ang[1].dys > 0 && a_ang[1].b_ang <= PI / 3.0)
    {
        flag[1] = fabs(a_ang[1].el) + fabs(a_ang[1].az);
    }
    if (a_ang[2].dys > 0 && a_ang[2].b_ang <= PI / 3.0)
    {
        flag[2] = fabs(a_ang[2].el) + fabs(a_ang[2].az);
    }
    if (a_ang[3].dys > 0 && a_ang[3].b_ang <= PI / 3.0)
    {
        flag[3] = fabs(a_ang[3].el) + fabs(a_ang[3].az);
    }
    if (a_ang[4].dys > 0 && a_ang[4].b_ang <= PI / 3.0)
    {
        flag[4] = fabs(a_ang[4].el) + fabs(a_ang[4].az);
    }
    float min = 0;
    int min_index = 0;
    for (int i = 0; i < 5; i++)
    {
        if (flag[i] != 0)
        {
            min = flag[i];
            min_index = i;
            break;
        }
    }
    for (int j = 0; j < 5; j++)
    {
        if (flag[j] != 0)
        {
            if (flag[j] < min)
            {
                min = flag[j];
                min_index = j;
            }
        }
    }

    a_ang[min_index].el = a_ang[min_index].el * 180.0 / PI;
    a_ang[min_index].az = a_ang[min_index].az * 180.0 / PI;
    a_ang[min_index].index = min_index + 1;
    // printf("最佳天线编号: %d, 俯仰角: %lf, 方位角: %lf", min_index + 1, a_ang[min_index].el, a_ang[min_index].az);
    return a_ang[min_index];
}

/* ------ 波束维护模块 ------ */
// 完成从 short int 到 -pi/2, pi/2 的转换
static float Angle_Transaction_short_to_pi(short int ang)
{
    ang = ((ang + 32768.0) / 65536.0) * 180.0 - 90.0;
    return ang;
}

// 完成从 -pi/2, pi/2 到  short int 的转换
// 量化精度可能会出现问题?
static short int Angle_Transaction_pi_to_short(float ang)
{
    ang = ((ang + 90.0) / 180.0) * 65536.0 - 32768.0;
    return (short int)ang;
}

static void Attach_Status_Information_To_Packet(Node *node, Node *dest_node, MacDataDaatr *macdata_daatr, unsigned short RX_TX_Flag, clocktype delay)
{
    Status_Information_To_Phy *Status_Information = new Status_Information_To_Phy;
    int temp_Id = 0;
    for (int i = 0; i < SUBNET_NODE_FREQ_NUMBER - 1; i++)
    {
        if (macdata_daatr->subnet_other_node_position[i].nodeId == dest_node->nodeId)
        {
            temp_Id = i;
            break;
        }
    }
    A_ang dest_node_angle = Get_Angle_Info(macdata_daatr->local_node_position_info.positionX,
                                           macdata_daatr->local_node_position_info.positionY,
                                           macdata_daatr->local_node_position_info.positionZ,
                                           macdata_daatr->local_node_position_info.pitchAngle,
                                           macdata_daatr->local_node_position_info.rollAngle,
                                           macdata_daatr->local_node_position_info.yawAngle,
                                           macdata_daatr->subnet_other_node_position[temp_Id].positionX,
                                           macdata_daatr->subnet_other_node_position[temp_Id].positionY,
                                           macdata_daatr->subnet_other_node_position[temp_Id].positionZ);
    dest_node_angle.az = Angle_Transaction_pi_to_short(dest_node_angle.az);
    dest_node_angle.el = Angle_Transaction_pi_to_short(dest_node_angle.el);
    Status_Information->antennaID = dest_node_angle.index;
    Status_Information->azimuth = dest_node_angle.az;
    Status_Information->pitchAngle = dest_node_angle.el;
    Status_Information->SendOrReceive = RX_TX_Flag;

    Status_Information->dest_node = dest_node->nodeId;

    int temp_distance = calculateLinkDistance(macdata_daatr->local_node_position_info.positionX,
                                              macdata_daatr->local_node_position_info.positionY,
                                              macdata_daatr->local_node_position_info.positionZ,
                                              macdata_daatr->subnet_other_node_position[temp_Id].positionX,
                                              macdata_daatr->subnet_other_node_position[temp_Id].positionY,
                                              macdata_daatr->subnet_other_node_position[temp_Id].positionZ);
    // 需要 distance 到 level 的转换
    Status_Information->transmitSpeedLevel = calculate_slot_type_mana(temp_distance);
    // 需要跳频图案
    for (int i = 0; i < FREQUENCY_COUNT; i++)
    {
        Status_Information->frequency_hopping[i] = macdata_daatr->frequency_sequence[dest_node->nodeId - 1][i];
    }

    // 暂时使用 MAC 层的协议
    Message *flight_status_msg = MESSAGE_Alloc(node,
                                               MAC_LAYER,
                                               MAC_PROTOCOL_DAATR,
                                               MSG_MAC_Attach_Information_To_PHY);

    int flight_status_msg_packetSize = sizeof(Status_Information_To_Phy);
    MESSAGE_PacketAlloc(node, flight_status_msg, flight_status_msg_packetSize, TRACE_DAATR);
    memcpy(MESSAGE_ReturnPacket(flight_status_msg), Status_Information, flight_status_msg_packetSize); // 将首地址与msg相关联
    MESSAGE_Send(node, flight_status_msg, delay);
}

bool MacDaatr_judge_if_could_send_config_or_LinkRequist(Node *node, int interfaceIndex)
{
    bool flag = false;
    MacDataDaatr *macdata_daatr = (MacDataDaatr *)node->macData[interfaceIndex]->macVar;
    if (macdata_daatr->state_now == Mac_Execution)
    {
        flag = true;
    }
    return flag;
}

// 根据子网分配跳频点生成子网内使用的频段
static void MacDaatr_generate_subnet_using_freq(MacDataDaatr *macdata_daatr)
{
    int i, j;
    int temp = 0;
    for (i = 0; i < TOTAL_FREQ_POINT; i++)
    {                                                    // 首先全部置0
        macdata_daatr->subnet_frequency_sequence[i] = 0; // 设置为使用频点
    }
    for (i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
    {
        for (j = 0; j < FREQUENCY_COUNT; j++)
        {
            temp = macdata_daatr->frequency_sequence[i][j];
            macdata_daatr->subnet_frequency_sequence[temp] = 1; // 设置为使用频点
        }
    }
    for (i = 0; i < TOTAL_FREQ_POINT; i++)
    {                                                               // 打印当前子网所使用频点
        cout << macdata_daatr->subnet_frequency_sequence[i] << " "; // 设置为使用频点
    }
    cout << endl;
}

// 计算子网使用频点的个数
static int MacDaatr_Cacu_Freq_Number(MacDataDaatr *macdata_daatr)
{
    int num = 0;
    for (int i = 0; i < TOTAL_FREQ_POINT; i++)
    {
        if (macdata_daatr->subnet_frequency_sequence[i] == 1)
        {
            num++;
        }
    }
    return num;
}

// 计算子网各节点窄带干扰数目
// type: 0:更新前  1：更新后
static void MacDaatr_Cacu_node_narrow_band_interfer_ratio(MacDataDaatr *macdata_daatr, int type)
{
    int i = 0, j = 0, k;
    int node_interfer_num = 0;
    int subnet_interfer_num = 0;
    int temp;
    macdata_daatr->stats.narrow_band_interfer_nodeID.clear();
    vector<int> node_list;

    vector<ParamData> patam_narrow_list; // 节点窄带干扰比例
    ParamData pa_narrow_data;
    vector<ParamDataStr> ParamDataStr_narrow_list; // 窄带干扰频点集合
    ParamDataStr pa_narrow_data_str;

    if (type == 1)
    {
        for (i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
        {
            pa_narrow_data.nodeId = i + 1;     /////统计参数上传
            pa_narrow_data_str.nodeId = i + 1; /////统计参数上传
            for (j = i + 1; j < SUBNET_NODE_FREQ_NUMBER; j++)
            {
                for (k = 0; k < FREQUENCY_COUNT; k++)
                {
                    temp = macdata_daatr->frequency_sequence[i][k] - macdata_daatr->frequency_sequence[j][k];
                    if (abs(temp) <= 15)
                    { // 如果出现窄带干扰
                        node_interfer_num++;
                        node_list.push_back(i + 1);
                        node_list.push_back(j + 1);
                        macdata_daatr->stats.narrow_band_interfer_nodeID.push_back(node_list);
                        node_list.clear();
                        pa_narrow_data_str.value += std::to_string((long long)j + 1); /////统计参数上传
                        pa_narrow_data_str.value += ',';                              /////统计参数上传//在字符串末尾会有“,”,也即1,2,3,
                        break;
                    }
                }
            }
            macdata_daatr->stats.node_narrow_band_interfer_ratio[i] = (float)node_interfer_num / FREQUENCY_COUNT;
            macdata_daatr->stats.node_narrow_band_interfer_ratio[i] *= 100;
            //////////////////////////////统计参数上传
            pa_narrow_data.value = macdata_daatr->stats.node_narrow_band_interfer_ratio[i];
            patam_narrow_list.push_back(pa_narrow_data); /////统计参数上传
            ParamDataStr_narrow_list.push_back(pa_narrow_data_str);
            pa_narrow_data_str.value.erase(0);
            /////////////////////////////////////////
            subnet_interfer_num += node_interfer_num;
            node_interfer_num = 0;
        }
        // Stats_ProSendData(node, MAC_LAYER, 1, 11, patam_narrow_list);//窄带干扰比例
        // Stats_ProSendData(node, MAC_LAYER, 1, 12, ParamDataStr_narrow_list);//窄带干扰节点对
    }
    if (type == 0)
    {
        for (i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
        {
            for (j = i + 1; j < SUBNET_NODE_FREQ_NUMBER; j++)
            {
                for (k = 0; k < FREQUENCY_COUNT; k++)
                {
                    temp = macdata_daatr->frequency_sequence[i][k] - macdata_daatr->frequency_sequence[j][k];
                    if (abs(temp) <= 15)
                    { // 如果出现窄带干扰
                        node_interfer_num++;
                        break;
                    }
                }
            }
            macdata_daatr->stats.node_narrow_band_interfer_ratio_before[i] = (float)node_interfer_num / FREQUENCY_COUNT;
            macdata_daatr->stats.node_narrow_band_interfer_ratio_before[i] *= 100;
            subnet_interfer_num += node_interfer_num;
            node_interfer_num = 0;
        }
    }
}

// 计算子网跳频图案干扰比例
static void MacDaatr_Cacu_Subnet_Node_Interfer_ratio(Node *node, MacDataDaatr *macdata_daatr)
{
    int inerefer_number = 0;
    int i, j, k;
    for (i = 0; i < TOTAL_FREQ_POINT; i++)
    {
        if (macdata_daatr->subnet_frequency_sequence[i] == 1)
        { // 此频段使用
            if (macdata_daatr->unavailable_frequency_points[i] == 0)
            {
                inerefer_number++;
            }
        }
    }
    int subnet_using_freq_num = 0;
    subnet_using_freq_num = MacDaatr_Cacu_Freq_Number(macdata_daatr);
    macdata_daatr->stats.subnet_freq_interfer_ratio = (float)inerefer_number / subnet_using_freq_num; // 子网频谱干扰比例
    macdata_daatr->stats.subnet_freq_interfer_ratio *= 100;
    int node_interfer_number = 0;
    int temp_freq;
    for (i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
    {
        for (j = 0; j < FREQUENCY_COUNT; j++)
        {
            temp_freq = macdata_daatr->frequency_sequence[i][j];
            if (macdata_daatr->unavailable_frequency_points[temp_freq] == 0)
            {
                node_interfer_number++;
            }
        }
        macdata_daatr->stats.node_freq_interfer_ratio[i] = (float)node_interfer_number / FREQUENCY_COUNT;
        macdata_daatr->stats.node_freq_interfer_ratio[i] *= 100;
        node_interfer_number = 0;
    }
}

static void Judge_If_Enter_Freq_Adjust(Node *node, MacDataDaatr *macdata_daatr)
{
    // 判断是否进入频率调整阶段
    int i = 0, j = 0;
    int interfer_number = 0;                                         // 子网使用频段干扰频段数量
    int interfer_number_sum = 0;                                     // 总频段被干扰数目
    MacDaatr_Cacu_node_narrow_band_interfer_ratio(macdata_daatr, 0); // 变化前窄带干扰比例
    for (i = 0; i < TOTAL_FREQ_POINT; i++)
    {
        macdata_daatr->unavailable_frequency_points[i] = 1;
    }
    for (i = 0; i < TOTAL_FREQ_POINT; i++)
    {
        for (j = 0; j < SUBNET_NODE_FREQ_NUMBER; j++)
        {
            if (macdata_daatr->spectrum_sensing_sum[j][i] == 0 && macdata_daatr->spectrum_sensing_sum[j][TOTAL_FREQ_POINT] == 1)
            { // 若当前节点对应的频谱感知结果已经接收到了, 且此频段受到干扰了
                if (macdata_daatr->subnet_frequency_sequence[i] == 1)
                {
                    interfer_number++;
                } // 此值对应的是子网现使用频段被干扰的数目，不对应总的干扰频段数目
                macdata_daatr->unavailable_frequency_points[i] = 0; // 此频点受到干扰, 赋0（此处为总干扰频段）
                interfer_number_sum++;
                break;
            }
        }
    }
    for (i = 0; i < TOTAL_FREQ_POINT; i++)
    {
        cout << macdata_daatr->unavailable_frequency_points[i] << " ";
    }
    cout << endl;
    cout << "总频段被干扰数目为： " << interfer_number_sum << endl;
    cout << "子网使用频段被干扰数目为： " << interfer_number << endl;
    int subnet_using_freq_num = 0;
    subnet_using_freq_num = MacDaatr_Cacu_Freq_Number(macdata_daatr);
    float using_freq_interfer_ratio = (float)interfer_number / subnet_using_freq_num;
    cout << "子网使用频段被干扰比例为： " << using_freq_interfer_ratio << endl;
    macdata_daatr->stats.subnet_freq_interfer_ratio_before = (float)interfer_number / subnet_using_freq_num;
    macdata_daatr->stats.subnet_freq_interfer_ratio_before *= 100;
    int node_interfer_number = 0;
    int temp_freq;
    for (i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
    {
        for (j = 0; j < FREQUENCY_COUNT; j++)
        {
            temp_freq = macdata_daatr->frequency_sequence[i][j];
            if (macdata_daatr->unavailable_frequency_points[temp_freq] == 0)
            {
                node_interfer_number++;
            }
        }
        macdata_daatr->stats.node_freq_interfer_ratio_before[i] = (float)node_interfer_number / FREQUENCY_COUNT;
        macdata_daatr->stats.node_freq_interfer_ratio_before[i] *= 100;
        node_interfer_number = 0;
    }
    ///////////////////////////////////////向上传递干扰信息参数//////////////////////////////////
    vector<ParamData> patam_list; // 节点干扰比例
    ParamData pa_data;
    pa_data.nodeId = 0; // 子网干扰比例
    pa_data.value = macdata_daatr->stats.subnet_freq_interfer_ratio_before;
    patam_list.push_back(pa_data);
    for (i = 1; i <= SUBNET_NODE_FREQ_NUMBER; i++)
    {
        pa_data.nodeId = i; // 节点干扰比例
        pa_data.value = macdata_daatr->stats.node_freq_interfer_ratio_before[i];
        patam_list.push_back(pa_data);
    }
    // Stats_ProSendData(node, MAC_LAYER, 1, 13, patam_list);//发送参数
    // string test_string = "22";
    vector<ParamDataStr> ParamDataStr_list; // 干扰频点集合
    ParamDataStr pa_data_str;
    pa_data_str.nodeId = 0;
    for (i = 0; i < TOTAL_FREQ_POINT; i++)
    {
        if (macdata_daatr->unavailable_frequency_points[i] == 0)
        {                                                          // 如果此频点被干扰
            pa_data_str.value += std::to_string((long long)i + 1); // 频段范围（1-501）
            pa_data_str.value += ',';                              // 在字符串末尾会有“,”,也即1,2,3,
        }
    }
    ParamDataStr_list.push_back(pa_data_str);
    pa_data_str.value.erase(0);
    for (i = 1; i <= SUBNET_NODE_FREQ_NUMBER; i++)
    {                           // 其他节点干扰频点
        pa_data_str.nodeId = i; // 节点干扰比例
        for (j = 0; j < TOTAL_FREQ_POINT; j++)
        {
            if (macdata_daatr->spectrum_sensing_sum[i][j] == 0)
            {                                                     // 如果此频点被干扰
                pa_data_str.value += to_string((long long)j + 1); // 频段范围（1-501）
                pa_data_str.value += ',';                         // 在字符串末尾会有“,”,也即1,2,3,
            }
        }
        ParamDataStr_list.push_back(pa_data_str);
        pa_data_str.value.erase(0);
    }
    // Stats_ProSendData(node, MAC_LAYER, 1, 14, ParamDataStr_list);//发送参数
    //////////////////////////////////////////////////////////////////////////////////////////

    if (using_freq_interfer_ratio >= INTERFEREENCE_FREQUENCY_THRESHOLD)
    { // 干扰频段数目超过阈值, 进入频率调整阶段
        cout << "干扰频段数目超过阈值, 进入频率调整阶段, Time: " << (float)(node->getNodeTime() / 1000000) << endl;
        Generate_Frequency_Sequence(macdata_daatr);
        MacDaatr_generate_subnet_using_freq(macdata_daatr);
        MacDaatr_Cacu_Subnet_Node_Interfer_ratio(node, macdata_daatr);
        MacDaatr_Cacu_node_narrow_band_interfer_ratio(macdata_daatr, 1); // 变化后窄带干扰比例
        if (macdata_daatr->need_change_stage == 0 &&
            macdata_daatr->state_now != Mac_Adjustment_Slot &&
            macdata_daatr->state_now != Mac_Adjustment_Freqency)
        {
            // 当前状态稳定, 在这之前没有发生要调整状态的事件, 且不处于任何调整状态
            cout << "Subnet Is About To Enter Freq Adjustment Stage!" << endl;
            macdata_daatr->need_change_stage = 2; // 即将进入频率调整阶段
        }
        else if (macdata_daatr->need_change_stage == 1)
        { // 在这之前已经发生了要进入时隙调整阶段, 但还未广播此信息, 也即未进入
            cout << "Now Is About To Enter Slot Adjustment Stage, Can't Enter Freq Adjustment Stage! " << endl;
            macdata_daatr->need_change_stage = 3; // 设置为首先进入时隙调整阶段, 在时隙调整阶段结束后进入频率调整阶段
        }

        // 首先将网管节点自己的跳频图案进行存储
        for (int j = 0; j < FREQUENCY_COUNT; j++)
        {
            macdata_daatr->spectrum_sensing_node[j] = macdata_daatr->frequency_sequence[1 - 1][j];
        }

        // 网管节点保存子网内所有节点时隙表macdata_daatr->slot_traffic_execution_new
        // 在收到链路构建需求事件MAC_DAATR_ReceiveTaskInstruct之后 500ms 后发送时隙表
        Message *prepare_msg = MESSAGE_Alloc(node,
                                             MAC_LAYER,
                                             MAC_PROTOCOL_DAATR,
                                             MSG_MAC_ManaPrepareToSendSequence);
        clocktype current_frametime = node->getNodeTime();
        clocktype delay;
        if (((current_frametime / 1000000) % 1000) < 20)
        {
            delay = (20 * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND) + 100 * MILLI_SECOND);
        }
        else if (((current_frametime / 1000000) % 1000) >= 20 && ((current_frametime / 1000000) % 1000) < 520)
        {
            delay = (520 * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND) + 100 * MILLI_SECOND);
        }
        else
        {
            delay = (1020 * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND) + 100 * MILLI_SECOND);
        }

        MESSAGE_Send(node, prepare_msg, delay);

        // MESSAGE_Send(node, prepare_msg, SENDING_SLOTTABLE_PREPARE_TIME * MILLI_SECOND);
    }
    else
    {
        cout << "干扰频段数未达到阈值, 不作调整" << endl;
    }
}

// 输出子网内节点邻接表信息
static void Print_Wave_Info(FlightStatus node_flight, FlightStatus nei_flight[SUBNET_NODE_NUMBER - 1], int nodeId)
{
    A_ang a_ang[SUBNET_NODE_FREQ_NUMBER - 1];
    printf("节点ID为 %d 的邻接表: \n", nodeId);
    printf("邻居节点ID  天线编号  俯仰角(单位: 度)  方位角(单位: 度)\n");
    for (int i = 0; i < SUBNET_NODE_FREQ_NUMBER - 1; i++)
    {
        a_ang[i] = Get_Angle_Info(node_flight.positionX, node_flight.positionY, node_flight.positionZ,
                                  node_flight.pitchAngle, node_flight.rollAngle, node_flight.yawAngle,
                                  nei_flight[i].positionX, nei_flight[i].positionY, nei_flight[i].positionZ);
        printf("\t%d\t%d\t%f\t%f\t\n", nei_flight[i].nodeId, a_ang[i].index, a_ang[i].el, a_ang[i].az);
    }
}

// 根据网管信道数据包更新子网内各节点的位置信息
// 并将其更新业务信道队列的 distance
static void UpdatePosition(MacPacket_Daatr *macpacket_daatr, MacDataDaatr *macdata_daatr)
{
    for (int i = 0; i < SUBNET_NODE_FREQ_NUMBER - 1; i++)
    { // 存储子网其他节点位置信息
        if (macdata_daatr->subnet_other_node_position[i].nodeId == 0)
        {

            // 添加新的节点信息
            macdata_daatr->subnet_other_node_position[i] = macpacket_daatr->node_position;
            break;
        }
        else
        {
            if (macdata_daatr->subnet_other_node_position[i].nodeId == macpacket_daatr->node_position.nodeId)
            {
                // 更新子网节点位置信息
                macdata_daatr->subnet_other_node_position[i] = macpacket_daatr->node_position;
                break;
            }
        }
    }
    int node_temp = macpacket_daatr->node_position.nodeId;
    macdata_daatr->traffic_channel_business[node_temp - 1].distance =
        calculateLinkDistance(macdata_daatr->local_node_position_info.positionX,
                              macdata_daatr->local_node_position_info.positionY,
                              macdata_daatr->local_node_position_info.positionZ,
                              macpacket_daatr->node_position.positionX,
                              macpacket_daatr->node_position.positionY,
                              macpacket_daatr->node_position.positionZ);
    // cout << "Node " << node_temp << " distance: " << macdata_daatr->traffic_channel_business[node_temp - 1].distance << " km " << endl;
    // system("pause");
}

// mean:均值  sigma2:方差
static double gaussrand(double mean, double sigma2)
{ // 自定义正态随机数生成的函数(中心极限定理法)
    double x = 0, y = 0, Y = 0;
    int i;
    for (i = 0; i < NSUM; i++) // NUSM表示均匀分布随机数个数再次设置为25个
    {
        x += (double)rand() / RAND_MAX; // 生成25个均匀分布随机数并将所有随机数求和
    }
    x = x - (NSUM / 2.0);        // 求和后的随机数减
    Y = x / sqrt(NSUM / 12.0);   // 求得x
    y = sqrt(sigma2) * Y + mean; // 映射到y服从均值为1.2, 方差为2.5的正态随机数
    return y;
}

static double create_data_volume_base_gauss(double distance)
{ // 基于距离给出一个合适的数据量(单位: KByte)
    double data = 0;
    double t = rand() / (RAND_MAX + 1.0);
    if (distance <= 30)
    { // 2.5ms可传25Kbps---3.125Kbyte
        if (t <= 0.5)
        {
            data = 3.125 * 2;
        }
        else
        {
            data = 3.125 * 3;
        }
    }
    else if (distance > 30 && distance <= 80)
    { // 2.5ms可传10Kbps---1.25Kbyte
        if (t <= 0.5)
        {
            data = 1.25 * 2;
        }
        else
        {
            data = 1.25 * 3;
        }
    }
    else if (distance > 80 && distance <= 150)
    { // 2.5ms可传5Kbps---0.625Kbyte
        if (t <= 0.5)
        {
            data = 0.625 * 2;
        }
        else
        {
            data = 0.625 * 3;
        }
    }
    else if (distance > 150 && distance <= 200)
    { // 2.5ms可传1.25Kbps---0.15625Kbyte
        if (t <= 0.5)
        {
            data = 0.15625 * 2;
        }
        else
        {
            data = 0.157 * 3;
        }
    }
    else if (distance > 200 && distance <= 300)
    { // 单位: Kbps//2.5ms可传0.5Kbps---0.0635Kbyte
        if (t <= 0.5)
        {
            data = 0.06 * 2;
        }
        else
        {
            data = 0.07 * 3;
        }
    }
    else if (distance > 300 && distance <= 500)
    { // 单位: Kbps//2.5ms可传0.125Kbps---0.015625Kbyte
        if (t <= 0.5)
        {
            data = 0.015 * 2;
        }
        else
        {
            data = 0.017 * 3;
        }
    }
    else if (distance > 500 && distance <= 1500)
    { // 2.5ms可传0.005Kbps---0.000625Kbyte
        if (t <= 0.5)
        {
            data = 0.0006 * 2;
        }
        else
        {
            data = 0.0007 * 3;
        }
    }
    return data;
}

// link_assign[] : 输入链路队列 num : 链路数 mean : 均值 sigma2 : 方差
// 此函数生成的链路距离服从高斯分布, 收发节点随机
// one_probablity:链路中含网管节点(节点ID1)占所有链路的比例
static void Generate_Random_LinkAssignment(LinkAssignment link_assign[],
                                           int num, double mean, double sigma2, double one_probablity)
{ // 根据特定高斯分布生成相应的链路
    int i, j = 1, k = -1;
    int temp_i;
    int time = 0;
    int node_temp;
    int flag = 1;
    double dis = 0;
    double link_assignment_data_Kbyte; // 链路构建需求业务量
    printf("开始产生高斯分布链路\n");
    int link_include_one = ceil(one_probablity * (double)num); // 链路中含网管节点(节点ID1)的链路数, 向上取整
    for (i = 0; i < link_include_one / 2; i++)
    {                                                              // 先产生包含网管节点(ID1)的链路, 网管节点为发送节点
        link_assign[i].listNumber = rand() % BUSSINE_TYPE_NUM + 1; //(0-BUSSINE_TYPE_NUM)
        link_assign[i].begin_node = 1;
        flag = 1;
        while (flag == 1)
        {
            node_temp = rand() % 19 + 2; // 生成ID2-20的节点
            for (k = 0; k < i; k++)
            {
                if (node_temp == link_assign[k].end_node)
                {
                    break;
                }
            }
            if (k == i)
            {
                flag = 0;
            }
        }
        link_assign[i].end_node = node_temp;
        link_assign[i].listNumber = rand() % BUSSINE_TYPE_NUM + 1; //(1-BUSSINE_TYPE_NUM)
        for (j = 0; j < link_assign[i].listNumber; j++)
        {
            link_assign[i].priority[j] = rand() % 4; // 优先级(0-3)
            while (dis <= 0 || dis > 1500)
            { // 生成满足要求的距离
                dis = gaussrand(mean, sigma2);
            }
            link_assign[i].size[j] = create_data_volume_base_gauss(dis);
            link_assign[i].QOS[j] = rand() % 4 + 1;
        }
    }
    temp_i = i;
    for (i; i < link_include_one; i++)
    {                                                              // 先产生包含网管节点(ID1)的链路, 网管节点为接收节点
        link_assign[i].listNumber = rand() % BUSSINE_TYPE_NUM + 1; //(0-BUSSINE_TYPE_NUM)
        link_assign[i].end_node = 1;
        flag = 1;
        while (flag == 1)
        {
            node_temp = rand() % 19 + 2; // 生成ID2-20的节点
            for (k = temp_i; k < i; k++)
            {
                if (node_temp == link_assign[k].begin_node)
                {
                    break;
                }
            }
            if (k == i)
            {
                flag = 0;
            }
        }
        link_assign[i].begin_node = node_temp;
        link_assign[i].listNumber = rand() % BUSSINE_TYPE_NUM + 1; //(1-BUSSINE_TYPE_NUM)
        for (j = 0; j < link_assign[i].listNumber; j++)
        {
            link_assign[i].priority[j] = rand() % 4; // 优先级(0-3)
            while (dis <= 0 || dis > 1500)
            { // 生成满足要求的距离
                dis = gaussrand(mean, sigma2);
            }
            link_assign[i].size[j] = create_data_volume_base_gauss(dis);
            link_assign[i].QOS[j] = rand() % 4 + 1;
        }
    }
    temp_i = i;
    for (i; i < num; i++)
    { // 产生不包含网管节点的链路

        flag = 1;
        while (flag == 1)
        {
            j = rand() % 19 + 2;
            node_temp = j;
            while (node_temp == j)
            {
                node_temp = rand() % 19 + 2;
            } // 生成ID2-20的节点
            for (k = temp_i; k < i; k++)
            {
                if (j == link_assign[i].begin_node && node_temp == link_assign[i].end_node)
                {
                    break;
                }
            }
            if (k == i)
            {
                flag = 0;
            }
        }
        link_assign[i].begin_node = j;
        link_assign[i].end_node = node_temp;
        link_assign[i].listNumber = rand() % BUSSINE_TYPE_NUM + 1; //(1-BUSSINE_TYPE_NUM)
        for (j = 0; j < link_assign[i].listNumber; j++)
        {
            link_assign[i].priority[j] = rand() % 4; // 优先级(0-3)
            while (dis <= 0 || dis > 1500)
            { // 生成满足要求的距离
                dis = gaussrand(mean, sigma2);
            }
            link_assign[i].size[j] = create_data_volume_base_gauss(dis);
            link_assign[i].QOS[j] = rand() % 4 + 1;
        }
    }
    for (i = 0; i < num; i++)
    {
        printf("ID:%d | ", i);
        printf("%d->%d: ", link_assign[i].begin_node, link_assign[i].end_node);
        printf("ListNumber:%d  ", link_assign[i].listNumber);
        for (j = 0; j < link_assign[i].listNumber; j++)
        {
            printf(" QOS: %d ", link_assign[i].QOS[j]);
            printf("priority: %d ", link_assign[i].priority[j]);
            printf("data_sum: %f Kbyte ", link_assign[i].size[j]);
        }
        printf("\n\n");
    }
}

// 437
static int Generate_LinkAssignment_Stage_1(LinkAssignment link_assign[])
{
    int i, j, k, LA_index = 0;
    // while (LA_index < num)
    // {
    for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
    {
        if (i == 1) // 网管节点的相关业务构建需求
        {
            for (j = 2; j <= SUBNET_NODE_NUMBER; j++)
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
            for (j = 1; j <= SUBNET_NODE_NUMBER; j++)
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

    // 满复用
    // for (i = 2; i <= SUBNET_NODE_NUMBER; i++)
    // {
    //     for (j = 2; j <= SUBNET_NODE_NUMBER; j++)
    //     {
    //         if (i != j)
    //         {
    //             link_assign[LA_index].begin_node = i;
    //             link_assign[LA_index].end_node = j;

    //             link_assign[LA_index].listNumber = 2;

    //             link_assign[LA_index].priority[0] = 3;
    //             link_assign[LA_index].size[0] = 462 / 8;
    //             link_assign[LA_index].QOS[0] = 4;

    //             link_assign[LA_index].priority[1] = 3;
    //             link_assign[LA_index].size[1] = 462 / 8;
    //             link_assign[LA_index].QOS[1] = 4;

    //             LA_index++;
    //         }
    //     }
    // }

    // for (i = 0; i < LA_index; i++)
    // {
    //     printf("ID:%d | ", i);
    //     printf("%d->%d: ", link_assign[i].begin_node, link_assign[i].end_node);
    //     printf("ListNumber:%d  ", link_assign[i].listNumber);
    //     for (j = 0; j < link_assign[i].listNumber; j++)
    //     {
    //         printf(" QOS: %d ", link_assign[i].QOS[j]);
    //         printf("priority: %d ", link_assign[i].priority[j]);
    //         printf("data_sum: %f Kbyte ", link_assign[i].size[j]);
    //     }
    //     printf("\n\n");
    // }

    return LA_index;
}

// 437
static int Generate_LinkAssignment_Stage_2(LinkAssignment link_assign[])
{
    int i, j, k, LA_index = 0;
    for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
    {
        if (i == 1)
        {
            for (j = 2; j <= SUBNET_NODE_NUMBER; j++)
            {
                link_assign[LA_index].begin_node = i;
                link_assign[LA_index].end_node = j;

                link_assign[LA_index].listNumber = 2;

                // 2
                // link_assign[LA_index].priority[0] = 0;
                // link_assign[LA_index].size[0] = 0.582 / 8;
                // link_assign[LA_index].QOS[0] = 4;
                // 6
                link_assign[LA_index].priority[0] = 0;
                link_assign[LA_index].size[0] = (0.462 + 0.582) / 8;
                link_assign[LA_index].QOS[0] = 4;
                // 2 & 3
                link_assign[LA_index].priority[1] = 2;
                link_assign[LA_index].size[1] = (0.480 + 0.950) / 8;
                link_assign[LA_index].QOS[1] = 4;

                LA_index++;
            }

            for (j = 2; j <= SUBNET_NODE_NUMBER; j++)
            {
                link_assign[LA_index].begin_node = j;
                link_assign[LA_index].end_node = i;

                link_assign[LA_index].listNumber = 1;

                // 5
                link_assign[LA_index].priority[0] = 0;
                link_assign[LA_index].size[0] = 2.343 / 8;
                link_assign[LA_index].QOS[0] = 4;

                LA_index++;
            }
        }

        for (j = 1; j <= SUBNET_NODE_NUMBER; j++)
        {
            if (i != j)
            {
                link_assign[LA_index].begin_node = i;
                link_assign[LA_index].end_node = j;

                link_assign[LA_index].listNumber = 1;

                link_assign[LA_index].priority[0] = 3;
                link_assign[LA_index].size[0] = 0.298 / 8;
                link_assign[LA_index].QOS[0] = 4;

                LA_index++;
            }
        }
        // }
    }

    for (i = 0; i < LA_index; i++)
    {
        printf("ID:%d | ", i);
        printf("%d->%d: ", link_assign[i].begin_node, link_assign[i].end_node);
        printf("ListNumber:%d  ", link_assign[i].listNumber);
        for (j = 0; j < link_assign[i].listNumber; j++)
        {
            printf(" QOS: %d ", link_assign[i].QOS[j]);
            printf("priority: %d ", link_assign[i].priority[j]);
            printf("data_sum: %f Kbyte ", link_assign[i].size[j]);
        }
        printf("\n\n");
    }

    return LA_index;
}

// 450
static int Generate_LinkAssignment_Stage_3(LinkAssignment link_assign[])
{
    int i, j, k, LA_index = 0;
    for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
    {
        if (i == 1)
        {
            for (j = 2; j <= SUBNET_NODE_NUMBER; j++)
            {
                link_assign[LA_index].begin_node = i;
                link_assign[LA_index].end_node = j;

                link_assign[LA_index].listNumber = 1;

                // 5 & 6
                link_assign[LA_index].priority[0] = 0;
                link_assign[LA_index].size[0] = 0.582 * 2 / 8;
                link_assign[LA_index].QOS[0] = 4;

                LA_index++;
            }

            for (j = 2; j <= SUBNET_NODE_NUMBER; j++)
            {
                link_assign[LA_index].begin_node = j;
                link_assign[LA_index].end_node = i;

                link_assign[LA_index].listNumber = 1;

                // 5 & 6
                link_assign[LA_index].priority[0] = 0;
                link_assign[LA_index].size[0] = 4.686 / 8;
                link_assign[LA_index].QOS[0] = 4;

                LA_index++;
            }
        }

        // 1
        for (j = 1; j <= SUBNET_NODE_NUMBER; j++)
        {
            if (i != j)
            {
                link_assign[LA_index].begin_node = i;
                link_assign[LA_index].end_node = j;

                link_assign[LA_index].listNumber = 1;

                link_assign[LA_index].priority[0] = 3;
                link_assign[LA_index].size[0] = 0.298 / 8;
                link_assign[LA_index].QOS[0] = 4;

                LA_index++;
            }
        }
    }

    i = 1;
    for (j = 2; j <= 5; j++)
    {
        link_assign[LA_index].begin_node = i;
        link_assign[LA_index].end_node = j;

        link_assign[LA_index].listNumber = 2;

        link_assign[LA_index].priority[0] = 1;
        link_assign[LA_index].size[0] = 1.430 / 8;
        link_assign[LA_index].QOS[0] = 4;

        link_assign[LA_index].priority[1] = 0;
        link_assign[LA_index].size[1] = 0.462 / 8;
        link_assign[LA_index].QOS[1] = 4;

        LA_index++;
    }
    i = 6;
    for (j = 7; j <= 10; j++)
    {
        link_assign[LA_index].begin_node = i;
        link_assign[LA_index].end_node = j;

        link_assign[LA_index].listNumber = 2;

        link_assign[LA_index].priority[0] = 1;
        link_assign[LA_index].size[0] = 1.430 / 8;
        link_assign[LA_index].QOS[0] = 4;

        link_assign[LA_index].priority[1] = 0;
        link_assign[LA_index].size[1] = 0.462 / 8;
        link_assign[LA_index].QOS[1] = 4;

        LA_index++;
    }
    i = 11;
    for (j = 12; j <= 15; j++)
    {
        link_assign[LA_index].begin_node = i;
        link_assign[LA_index].end_node = j;

        link_assign[LA_index].listNumber = 2;

        link_assign[LA_index].priority[0] = 1;
        link_assign[LA_index].size[0] = 1.430 / 8;
        link_assign[LA_index].QOS[0] = 4;

        link_assign[LA_index].priority[1] = 0;
        link_assign[LA_index].size[1] = 0.462 / 8;
        link_assign[LA_index].QOS[1] = 4;

        LA_index++;
    }
    i = 16;
    for (j = 17; j <= 20; j++)
    {
        link_assign[LA_index].begin_node = i;
        link_assign[LA_index].end_node = j;

        link_assign[LA_index].listNumber = 2;

        link_assign[LA_index].priority[0] = 1;
        link_assign[LA_index].size[0] = 1.430 / 8;
        link_assign[LA_index].QOS[0] = 4;

        link_assign[LA_index].priority[1] = 0;
        link_assign[LA_index].size[1] = 0.462 / 8;
        link_assign[LA_index].QOS[1] = 4;

        LA_index++;
    }

    for (i = 0; i < LA_index; i++)
    {
        printf("ID:%d | ", i);
        printf("%d->%d: ", link_assign[i].begin_node, link_assign[i].end_node);
        printf("ListNumber:%d  ", link_assign[i].listNumber);
        for (j = 0; j < link_assign[i].listNumber; j++)
        {
            printf(" QOS: %d ", link_assign[i].QOS[j]);
            printf("priority: %d ", link_assign[i].priority[j]);
            printf("data_sum: %f Kbyte ", link_assign[i].size[j]);
        }
        printf("\n\n");
    }

    return LA_index;
}

// 495
static int Generate_LinkAssignment_Stage_4(LinkAssignment link_assign[])
{
    int i, j, k, LA_index = 0;
    for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
    {
        if (i == 1)
        {
            for (j = 2; j <= SUBNET_NODE_NUMBER; j++)
            {
                link_assign[LA_index].begin_node = i;
                link_assign[LA_index].end_node = j;

                link_assign[LA_index].listNumber = 1;

                // 8
                link_assign[LA_index].priority[0] = 0;
                link_assign[LA_index].size[0] = 0.480 / 8;
                link_assign[LA_index].QOS[0] = 4;

                LA_index++;
            }
        }

        // 1
        for (j = 1; j <= SUBNET_NODE_NUMBER; j++)
        {
            if (i != j)
            {
                link_assign[LA_index].begin_node = i;
                link_assign[LA_index].end_node = j;

                link_assign[LA_index].listNumber = 1;

                link_assign[LA_index].priority[0] = 3;
                link_assign[LA_index].size[0] = 0.298 / 8;
                link_assign[LA_index].QOS[0] = 4;

                LA_index++;
            }
        }
    }

    for (i = 2; i <= 5; i++)
    {
        for (j = 2; j <= 5; j++)
        {
            if (i != j)
            {
                link_assign[LA_index].begin_node = i;
                link_assign[LA_index].end_node = j;

                link_assign[LA_index].listNumber = 2;

                link_assign[LA_index].priority[0] = 1;
                link_assign[LA_index].size[0] = 2.343 / 8;
                link_assign[LA_index].QOS[0] = 4;

                link_assign[LA_index].priority[1] = 0;
                link_assign[LA_index].size[1] = 0.582 / 8;
                link_assign[LA_index].QOS[1] = 4;

                LA_index++;
            }
        }
    }

    for (i = 7; i <= 10; i++)
    {
        for (j = 7; j <= 10; j++)
        {
            if (i != j)
            {
                link_assign[LA_index].begin_node = i;
                link_assign[LA_index].end_node = j;

                link_assign[LA_index].listNumber = 2;

                link_assign[LA_index].priority[0] = 1;
                link_assign[LA_index].size[0] = 2.343 / 8;
                link_assign[LA_index].QOS[0] = 4;

                link_assign[LA_index].priority[1] = 0;
                link_assign[LA_index].size[1] = 0.582 / 8;
                link_assign[LA_index].QOS[1] = 4;

                LA_index++;
            }
        }
    }

    for (i = 12; i <= 15; i++)
    {
        for (j = 12; j <= 15; j++)
        {
            if (i != j)
            {
                link_assign[LA_index].begin_node = i;
                link_assign[LA_index].end_node = j;

                link_assign[LA_index].listNumber = 2;

                link_assign[LA_index].priority[0] = 1;
                link_assign[LA_index].size[0] = 2.343 / 8;
                link_assign[LA_index].QOS[0] = 4;

                link_assign[LA_index].priority[1] = 0;
                link_assign[LA_index].size[1] = 0.582 / 8;
                link_assign[LA_index].QOS[1] = 4;

                LA_index++;
            }
        }
    }

    for (i = 17; i <= 20; i++)
    {
        for (j = 17; j <= 20; j++)
        {
            if (i != j)
            {
                link_assign[LA_index].begin_node = i;
                link_assign[LA_index].end_node = j;

                link_assign[LA_index].listNumber = 2;

                link_assign[LA_index].priority[0] = 1;
                link_assign[LA_index].size[0] = 2.343 / 8;
                link_assign[LA_index].QOS[0] = 4;

                link_assign[LA_index].priority[1] = 0;
                link_assign[LA_index].size[1] = 0.582 / 8;
                link_assign[LA_index].QOS[1] = 4;

                LA_index++;
            }
        }
    }

    for (i = 0; i < LA_index; i++)
    {
        printf("ID:%d | ", i);
        printf("%d->%d: ", link_assign[i].begin_node, link_assign[i].end_node);
        printf("ListNumber:%d  ", link_assign[i].listNumber);
        for (j = 0; j < link_assign[i].listNumber; j++)
        {
            printf(" QOS: %d ", link_assign[i].QOS[j]);
            printf("priority: %d ", link_assign[i].priority[j]);
            printf("data_sum: %f Kbyte ", link_assign[i].size[j]);
        }
        printf("\n\n");
    }

    return LA_index;
}

// 两节点链路构建需求
static int Generate_LinkAssignment_Stage_2nodes(LinkAssignment link_assign[])
{
    int i, j, k, LA_index = 0;
    // while (LA_index < num)
    // {
    for (i = 1; i <= 50; i++)
    {
        link_assign[LA_index].begin_node = 1;
        link_assign[LA_index].end_node = 2;

        link_assign[LA_index].listNumber = 1;
        link_assign[LA_index].priority[0] = 0;
        link_assign[LA_index].size[0] = 0.462 / 8;
        link_assign[LA_index].QOS[0] = 4;

        LA_index++;

        link_assign[LA_index].begin_node = 2;
        link_assign[LA_index].end_node = 1;

        link_assign[LA_index].listNumber = 1;
        link_assign[LA_index].priority[0] = 0;
        link_assign[LA_index].size[0] = 0.462 / 8;
        link_assign[LA_index].QOS[0] = 4;

        LA_index++;
    }

    for (i = 0; i < LA_index; i++)
    {
        printf("ID:%d | ", i);
        printf("%d->%d: ", link_assign[i].begin_node, link_assign[i].end_node);
        printf("ListNumber:%d  ", link_assign[i].listNumber);
        for (j = 0; j < link_assign[i].listNumber; j++)
        {
            printf(" QOS: %d ", link_assign[i].QOS[j]);
            printf("priority: %d ", link_assign[i].priority[j]);
            printf("data_sum: %f Kbyte ", link_assign[i].size[j]);
        }
        printf("\n\n");
    }

    return LA_index;
}

// 两节点链路构建需求
// 96
static int Generate_LinkAssignment_Stage_3nodes(LinkAssignment link_assign[])
{
    int i, j, k, LA_index = 0;
    // while (LA_index < num)
    // {
    for (i = 1; i <= 16; i++)
    {
        link_assign[LA_index].begin_node = 1;
        link_assign[LA_index].end_node = 2;

        link_assign[LA_index].listNumber = 1;
        link_assign[LA_index].priority[0] = 0;
        link_assign[LA_index].size[0] = 0.462 / 8;
        link_assign[LA_index].QOS[0] = 4;

        LA_index++;

        link_assign[LA_index].begin_node = 2;
        link_assign[LA_index].end_node = 1;

        link_assign[LA_index].listNumber = 1;
        link_assign[LA_index].priority[0] = 0;
        link_assign[LA_index].size[0] = 0.462 / 8;
        link_assign[LA_index].QOS[0] = 4;

        LA_index++;

        link_assign[LA_index].begin_node = 1;
        link_assign[LA_index].end_node = 3;

        link_assign[LA_index].listNumber = 1;
        link_assign[LA_index].priority[0] = 0;
        link_assign[LA_index].size[0] = 0.462 / 8;
        link_assign[LA_index].QOS[0] = 4;

        LA_index++;

        link_assign[LA_index].begin_node = 3;
        link_assign[LA_index].end_node = 1;

        link_assign[LA_index].listNumber = 1;
        link_assign[LA_index].priority[0] = 0;
        link_assign[LA_index].size[0] = 0.462 / 8;
        link_assign[LA_index].QOS[0] = 4;

        LA_index++;

        link_assign[LA_index].begin_node = 2;
        link_assign[LA_index].end_node = 3;

        link_assign[LA_index].listNumber = 1;
        link_assign[LA_index].priority[0] = 0;
        link_assign[LA_index].size[0] = 0.462 / 8;
        link_assign[LA_index].QOS[0] = 4;

        LA_index++;

        link_assign[LA_index].begin_node = 3;
        link_assign[LA_index].end_node = 2;

        link_assign[LA_index].listNumber = 1;
        link_assign[LA_index].priority[0] = 0;
        link_assign[LA_index].size[0] = 0.462 / 8;
        link_assign[LA_index].QOS[0] = 4;

        LA_index++;
    }

    for (i = 0; i < LA_index; i++)
    {
        printf("ID:%d | ", i);
        printf("%d->%d: ", link_assign[i].begin_node, link_assign[i].end_node);
        printf("ListNumber:%d  ", link_assign[i].listNumber);
        for (j = 0; j < link_assign[i].listNumber; j++)
        {
            printf(" QOS: %d ", link_assign[i].QOS[j]);
            printf("priority: %d ", link_assign[i].priority[j]);
            printf("data_sum: %f Kbyte ", link_assign[i].size[j]);
        }
        printf("\n\n");
    }

    return LA_index;
}

// 搜寻对应节点时隙位置
static vector<uint8_t> *MacDaatrSrearchSlotLocation(int nodeId, MacDataDaatr *macdata_daatr)
{
    int i = 0;
    vector<uint8_t> *slot_location = new vector<uint8_t>;
    for (i = 0; i < MANAGEMENT_SLOT_NUMBER_OTHER_STAGE; i++)
    {
        if (macdata_daatr->slot_management_other_stage[i].nodeId == nodeId)
        {
            slot_location->push_back(i);
        }
    }
    return slot_location;
}

// 测试计数模块初始化
static void MacDaatrInitialize_pkt_Counter(Node *node, MacDataDaatr *macdata_daatr)
{
    macdata_daatr->stats.PDU_GotUnicast = 0;
    macdata_daatr->stats.PDU_SentUnicast = 0;
    macdata_daatr->stats.UDU_GotUnicast = 0;
    macdata_daatr->stats.UDU_SentUnicast = 0;

    macdata_daatr->stats.Missed_pkts = 0;

    Message *pkt_counter_timerMsg = MESSAGE_Alloc(node, MAC_LAYER, MAC_PROTOCOL_DAATR, MSG_MAC_PktCounter);
    MESSAGE_Send(node, pkt_counter_timerMsg, SIMULATION_RESULT_SHOW_PERIOD * MILLI_SECOND);
}

// 结束建链阶段定时器
// 时间定位T（T=1s）
static void MacDaatrInitialize_Other_End_Build_Link_Stage(Node *node, MacDataDaatr *macdata_daatr)
{
    Message *send_msg = MESSAGE_Alloc(node,
                                      MAC_LAYER,
                                      MAC_PROTOCOL_DAATR,
                                      MSG_MAC_End_Link_Build);
    MESSAGE_Send(node, send_msg, END_LINK_BUILD_TIME * MILLI_SECOND - 1 * NANO_SECOND);
}

// 定期向上层发送子网其他节点位置信息定时器初始化
static void MacDaatrInitialize_Other_subnet_NodePosition_Timer(Node *node, MacDataDaatr *macdata_daatr)
{
    Message *send_msg0 = MESSAGE_Alloc(node,
                                       MAC_LAYER,
                                       MAC_PROTOCOL_DAATR,
                                       MAC_DAATR_SendOtherNodePosition);
    MESSAGE_Send(node, send_msg0, SEND_NODE_POSITION_PERIOD * MILLI_SECOND);
}

// 一些节点必要参数初始化
static void MacDaatrInitialize_parameter(Node *node, MacDataDaatr *macdata_daatr)
{
    macdata_daatr->need_change_stage = 0;
    macdata_daatr->state_now = Mac_Initialization;              // 初始为建链阶段
    macdata_daatr->has_received_chain_building_request = false; // 初始未发送建链请求

    ifstream fin;
    fin.open("..\\..\\..\\..\\libraries\\user_models\\src\\ManaNodeConfigue\\mac_daatr.txt", ios::in);
    int mana_node;
    int gateway_node;
    int standby_gateway_node;
    char buf[50] = {0};
    char state = 0;
    int nodeid[50];
    int i = 0;
    int temp = 0;
    if (fin.is_open())
    {
        while (fin >> buf)
        {
            if (state == 0 && strcmp(buf, "SUBNET_MANA_NODE_ID") == 0)
            { // 子网网管节点ID
                // cout << "输入时隙表时隙状态" << endl;
                state = 1;
                continue;
            }
            else if (state == 0 && strcmp(buf, "SUBNET_GATEWAY_NODE_ID") == 0)
            { // 子网网关节点ID
                // cout << "输入时隙表时隙状态" << endl;
                state = 2;
                continue;
            }
            else if (state == 0 && strcmp(buf, "SUBNET_STANDBY_GATEWAY_NODE_ID") == 0)
            { // 子网备用网关节点ID
                // cout << "输入时隙表时隙状态" << endl;
                state = 6;
                continue;
            }
            else if (state == 0 && strcmp(buf, "MANA_CHANEL_BUILD_STATE_SLOT_NODE_ID") == 0)
            { // 子网建链阶段时隙表ID
                // cout << "输入时隙表时隙状态" << endl;
                state = 3;
                continue;
            }
            else if (state == 0 && strcmp(buf, "MANA_CHANEL_OTHER_STATE_SLOT_NODE_ID") == 0)
            { // 子网其他阶段时隙表ID
                // cout << "输入时隙表时隙状态" << endl;
                state = 4;
                continue;
            }
            else if (state == 0 && strcmp(buf, "MANA_CHANEL_OTHER_STATE_SLOT_STATE") == 0)
            { // 子网其他阶段时隙表时隙状态
                // cout << "输入时隙表时隙状态" << endl;
                state = 5;
                continue;
            }
            else if (strcmp(buf, "!") == 0)
            { // 读取一行的末尾
                state = 0;
                i = 0;
                continue;
            }

            switch (state)
            {
            case 1:
            { // 子网网管节点ID
                sscanf_s(buf, "%d", &mana_node, _countof(buf));
                break;
            }
            case 2:
            { // 子网网关节点ID
                sscanf_s(buf, "%d", &gateway_node, _countof(buf));
                break;
            }
            case 3:
            { // 子网建链阶段时隙表ID
                sscanf_s(buf, "%d", &temp, _countof(buf));
                macdata_daatr->slot_management[i].nodeId = temp;
                i++;
                break;
            }
            case 4:
            { // 子网其他阶段时隙表ID
                sscanf_s(buf, "%d", &temp, _countof(buf));
                macdata_daatr->slot_management_other_stage[i].nodeId = temp;
                i++;
                break;
            }
            case 5:
            { // 子网其他阶段时隙表时隙状态
                sscanf_s(buf, "%d", &temp, _countof(buf));
                macdata_daatr->slot_management_other_stage[i].state = temp;
                i++;
                break;
            }
            case 6:
            { // 子网网关节点ID
                sscanf_s(buf, "%d", &standby_gateway_node, _countof(buf));
                break;
            }
            }
            // cout << buf << endl;//每一次的buf是空格或回车键（即白色字符）分开的元素
            // cout << endl;
        }
    }

    if (node->nodeId == mana_node)
    { // 初始化节点1为网管节点
        macdata_daatr->node_type = Node_Management;
    }
    else if (node->nodeId == gateway_node)
    { // 初始化节点2为网关节点
        macdata_daatr->node_type = Node_Gateway;
    }
    else if (node->nodeId == standby_gateway_node)
    { // 初始化节点2为网关节点
        macdata_daatr->node_type = Node_Standby_Gateway;
    }
    else
    { // 初始化其他节点(3-20)为普通节点
        macdata_daatr->node_type = Node_Ordinary;
    }

    for (int i = 0; i < TOTAL_FREQ_POINT; i++)
    {
        macdata_daatr->unavailable_frequency_points[i] = 1;
        // macdata_daatr->subnet_frequency_sequence[i] = 1; // !!!!目前初始化默认子网使用所有频点, 后面根据需求改动
    }

    macdata_daatr->mana_node = mana_node; // 网管节点
    if (macdata_daatr->gateway_node != 0)
    {
        macdata_daatr->gateway_node = gateway_node; // 网关节点
    }
    if (macdata_daatr->standby_gateway_node != 0)
    {
        macdata_daatr->standby_gateway_node = standby_gateway_node;
    }
}

static void MacDaatrInitialize_Wave_Timer(Node *node)
{
    Message *wave_msg = MESSAGE_Alloc(node,
                                      MAC_LAYER,
                                      MAC_PROTOCOL_DAATR,
                                      MSG_MAC_Beam_maintenance);
    MESSAGE_Send(node, wave_msg, BEAM_MATAINMENCE_PERIOD * MILLI_SECOND);
}

// 网管节点定期判断是否进入频率调整阶段
static void MacDaatrInitialize_Judge_If_Enter_Freq_Adjust_Timer(Node *node, MacDataDaatr *macdata_daatr)
{
    Message *send_msg = MESSAGE_Alloc(node,
                                      MAC_LAYER,
                                      MAC_PROTOCOL_DAATR,
                                      MAC_DAATR_Mana_Judge_Enter_Freq_Adjustment_Stage);
    MESSAGE_Send(node, send_msg, 2000 * MILLI_SECOND);
    macdata_daatr->ptr_to_period_judge_if_enter_adjustment = send_msg;
}

// 链路状态定时器初始化
static void MacDaatrInitialize_LocalLinkState_timer(Node *node, MacDataDaatr *macdata_daatr)
{
    // Send_Local_Link_Status(node, macdata_daatr);
    Message *send_msg = MESSAGE_Alloc(node,
                                      MAC_LAYER,
                                      MAC_PROTOCOL_DAATR,
                                      MSG_MAC_SendLocalLinkStateTimer);
    MESSAGE_Send(node, send_msg, LINK_STATE_PERIOD * MILLI_SECOND);
}

static void MacDaatrInitialize_mana_Timer(Node *node, MacDataDaatr *macdata_daatr)
{
    Message *send_msg0 = MESSAGE_Alloc(node,
                                       MAC_LAYER,
                                       MAC_PROTOCOL_DAATR,
                                       Mac_Management_Channel_Period_Inquiry);
    MESSAGE_Send(node, send_msg0, 0);

    macdata_daatr->ptr_to_mana_channel_period_inquiry = send_msg0;
}

static void MacDaatrInitialize_traffic_Timer(Node *node, MacDataDaatr *macdata_daatr)
{
    int i;

    int initialStatus = (int)macdata_daatr->slot_traffic_execution[0].state;

    if (initialStatus == DAATR_STATUS_TX || initialStatus == DAATR_STATUS_RX)
    {
        i = 0;
    }
    else
    {
        for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
        {
            if ((int)macdata_daatr->slot_traffic_execution[i].state != initialStatus)
            {
                break;
            }
        }
    }

    if (i == TRAFFIC_SLOT_NUMBER)
    {
        macdata_daatr->currentStatus = initialStatus;

        if (DEBUG)
        {
            printf("No initial timer sent for node %d. Initial Status matches all slots in frame\n", node->nodeId);
        }
    }
    else
    {
        Message *timerMsg;
        clocktype delay;

        delay = macdata_daatr->slotDuration * i;

        timerMsg = MESSAGE_Alloc(node,
                                 MAC_LAYER,
                                 MAC_PROTOCOL_DAATR,
                                 MSG_MAC_Traffic_Channel_Period_Inquiry);

        macdata_daatr->currentStatus = initialStatus;
        macdata_daatr->currentSlotId = 0;
        macdata_daatr->nextStatus = macdata_daatr->slot_traffic_execution[i].state;
        macdata_daatr->nextSlotId = i;

        macdata_daatr->timerMsg = timerMsg;
        // macdata_daatr->timerExpirationTime = delay + node->getNodeTime();

        MESSAGE_SetInstanceId(timerMsg, (short)macdata_daatr->myMacData->interfaceIndex);
        MESSAGE_Send(node, timerMsg, delay);

        if (DEBUG)
        {
            cout << "Initial timer sent for node [" << node->nodeId << "] with a delay of " << (float)(delay) / 1000000 << " at " << (float)(node->getNodeTime()) / 1000000 << endl;
        }
    }
}

// 在初始化时设定各节点的业务信道开始建链的时间
static void MacDaatrInitialize_Link_Setup_Timer(Node *node, MacDataDaatr *macdata_daatr)
{
    macdata_daatr->link_build_state = 0;
    macdata_daatr->link_build_times = 0;

    if (macdata_daatr->mana_node == 1)
    {
        if (node->nodeId >= 2 && node->nodeId <= SUBNET_NODE_NUMBER) // 非网管节点
        {
            Message *Link_Setup_msg = MESSAGE_Alloc(node,
                                                    MAC_LAYER,
                                                    MAC_PROTOCOL_DAATR,
                                                    MSG_MAC_Start_Link_Build);
            clocktype sending_delay = (LINK_SETUP_GUARD_TIME + (node->nodeId - 2) * MANA_SLOT_DURATION) * MILLI_SECOND;
            MESSAGE_Send(node, Link_Setup_msg, sending_delay);
        }
        else // 网管节点
        {
            Message *Link_Setup_msg = MESSAGE_Alloc(node,
                                                    MAC_LAYER,
                                                    MAC_PROTOCOL_DAATR,
                                                    MSG_MAC_Start_Link_Build);
            clocktype sending_delay = LINK_SETUP_GUARD_TIME * MILLI_SECOND;
            MESSAGE_Send(node, Link_Setup_msg, sending_delay);
        }
    }
    else
    {
        if (node->nodeId >= 2 && node->nodeId <= SUBNET_NODE_NUMBER && node->nodeId != macdata_daatr->mana_node) // 非网管节点的其他节点保持不变
        {
            Message *Link_Setup_msg = MESSAGE_Alloc(node,
                                                    MAC_LAYER,
                                                    MAC_PROTOCOL_DAATR,
                                                    MSG_MAC_Start_Link_Build);
            clocktype sending_delay = (LINK_SETUP_GUARD_TIME + (node->nodeId - 2) * MANA_SLOT_DURATION) * MILLI_SECOND;
            MESSAGE_Send(node, Link_Setup_msg, sending_delay);
        }
        else if (node->nodeId == 1) // 节点1换在中间网管节点的位置发送建链
        {
            Message *Link_Setup_msg = MESSAGE_Alloc(node,
                                                    MAC_LAYER,
                                                    MAC_PROTOCOL_DAATR,
                                                    MSG_MAC_Start_Link_Build);
            clocktype sending_delay = (LINK_SETUP_GUARD_TIME + (macdata_daatr->mana_node - 2) * MANA_SLOT_DURATION) * MILLI_SECOND;
            MESSAGE_Send(node, Link_Setup_msg, sending_delay);
        }
        else // 网管节点
        {
            Message *Link_Setup_msg = MESSAGE_Alloc(node,
                                                    MAC_LAYER,
                                                    MAC_PROTOCOL_DAATR,
                                                    MSG_MAC_Start_Link_Build);
            clocktype sending_delay = LINK_SETUP_GUARD_TIME * MILLI_SECOND;
            MESSAGE_Send(node, Link_Setup_msg, sending_delay);
        }
    }

    // string stlotable_state_filename = "..\\..\\..\\..\\libraries\\user_models\\src\\Slottable_Initialization\\Slottable_RXTX_State_" + to_string(static_cast<long long>(node->nodeId)) + ".txt";
    // string stlotable_node_filename = "..\\..\\..\\..\\libraries\\user_models\\src\\Slottable_Initialization\\Slottable_RXTX_Node_" + to_string(static_cast<long long>(node->nodeId)) + ".txt";
    // ifstream fout1(stlotable_state_filename);
    // ifstream fout2(stlotable_node_filename);

    // if (!fout1.is_open())
    // {
    //     cout << "Could Not Open File1" << endl;
    //     system("pause");
    // }
    // if (!fout2.is_open())
    // {
    //     cout << "Could Not Open File2" << endl;
    //     system("pause");
    // }

    // vector<int> RXTX_state;
    // vector<int> RXTX_node;
    // string str1, str2, temp;

    // getline(fout1, str1);
    // stringstream ss1(str1);
    // while (getline(ss1, temp, ','))
    // {
    //     RXTX_state.push_back(stoi(temp));
    // }
    // getline(fout2, str2);
    // stringstream ss2(str2);
    // while (getline(ss2, temp, ','))
    // {
    //     RXTX_node.push_back(stoi(temp));
    // }

    // for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    // {
    //     macdata_daatr->slot_traffic_initialization[i].state = RXTX_state[i];
    //     if (macdata_daatr->slot_traffic_initialization[i].state == DAATR_STATUS_TX && RXTX_node[i] != 0)
    //     {
    //         macdata_daatr->slot_traffic_initialization[i].send_or_recv_node = RXTX_node[i];
    //         macdata_daatr->TX_Slotnum++;
    //     }
    //     else if (macdata_daatr->slot_traffic_initialization[i].state == DAATR_STATUS_RX && RXTX_node[i] != 0)
    //     {
    //         macdata_daatr->slot_traffic_initialization[i].send_or_recv_node = RXTX_node[i];
    //     }
    //     else
    //     {
    //         macdata_daatr->slot_traffic_initialization[i].send_or_recv_node = 0;
    //         // macdata_daatr->slot_traffic_execution[i].recv = 0;
    //     }
    // }

    // int i;
    // int initialStatus = (int)macdata_daatr->slot_traffic_initialization[0].state;

    // if (initialStatus == DAATR_STATUS_TX || initialStatus == DAATR_STATUS_RX)
    // {
    //     i = 0;
    // }
    // else
    // {
    //     for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    //     {
    //         if ((int)macdata_daatr->slot_traffic_initialization[i].state != initialStatus)
    //         {
    //             break;
    //         }
    //     }
    // }

    // if (i == TRAFFIC_SLOT_NUMBER)
    // {
    //     macdata_daatr->currentStatus = initialStatus;

    //     if (DEBUG)
    //     {
    //         printf("No initial timer sent for node %d. Initial Status matches all slots in frame\n", node->nodeId);
    //     }
    // }
    // else
    // {
    //     Message *timerMsg;
    //     clocktype delay;

    //     delay = macdata_daatr->slotDuration * i;

    //     timerMsg = MESSAGE_Alloc(node,
    //                              MAC_LAYER,
    //                              MAC_PROTOCOL_DAATR,
    //                              MSG_MAC_Traffic_Channel_Period_Inquiry);

    //     macdata_daatr->currentStatus = initialStatus;
    //     macdata_daatr->currentSlotId = 0;
    //     macdata_daatr->nextStatus = macdata_daatr->slot_traffic_initialization[i].state;
    //     macdata_daatr->nextSlotId = i;

    //     macdata_daatr->timerMsg = timerMsg;
    //     macdata_daatr->timerExpirationTime = delay + node->getNodeTime();

    //     MESSAGE_SetInstanceId(timerMsg, (short)macdata_daatr->myMacData->interfaceIndex);
    //     MESSAGE_Send(node, timerMsg, delay);

    //     if (DEBUG)
    //     {
    //         cout << "Initial timer sent for node [" << node->nodeId << "] with a delay of " << (float)(delay) / 1000000 << " at " << (float)(node->getNodeTime()) / 1000000 << endl;
    //     }
    // }
}

void MacDaatrInit(Node *node,
                  int interfaceIndex,
                  const NodeInput *nodeInput,
                  const int subnetListIndex,
                  const int numNodesInSubnet)
{
    int i;

    MacDataDaatr *macdata_daatr = (MacDataDaatr *)MEM_malloc(sizeof(MacDataDaatr));
    clocktype duration;

    memset(macdata_daatr, 0, sizeof(MacDataDaatr));
    macdata_daatr->myMacData = node->macData[interfaceIndex];
    macdata_daatr->myMacData->macVar = (void *)macdata_daatr;

    macdata_daatr->slotDuration = DefaultTdmaSlotDuration;

    // 建链阶段网管信道时隙表初始化
    macdata_daatr->slot_management[0].state = DAATR_STATUS_IDLE; // 此时隙对应空时隙, 作为建链准备阶段
    for (i = 0; i < MANAGEMENT_SLOT_NUMBER_LINK_BUILD; i++)
    {
        macdata_daatr->slot_management[i].state = DAATR_STATUS_IDLE;
    }
    for (i = 1; i < 21; i++)
    {
        macdata_daatr->slot_management[i].state = DAATR_STATUS_FLIGHTSTATUS_SEND;
        macdata_daatr->slot_management[i].nodeId = i;
    }
    int j;
    // 其他阶段网管信道时隙表初始化
    for (i = 0; i < MANAGEMENT_SLOT_NUMBER_OTHER_STAGE; i++)
    {
        macdata_daatr->slot_management_other_stage[i].state = other_stage_mana_slot_table_state_seq[i];
        macdata_daatr->slot_management_other_stage[i].nodeId = other_stage_mana_slot_table_nodeId_seq[i];
    }

    // 对业务信道时隙表进行初始化赋值
    macdata_daatr->TX_Slotnum = 0;
    for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    {
        macdata_daatr->slot_traffic_execution[i].state = DAATR_STATUS_RX;
        if (i % SUBNET_NODE_NUMBER == node->nodeIndex)
        {
            macdata_daatr->slot_traffic_execution[i].state = DAATR_STATUS_TX;
        }
    }

    // 对业务信道队列进行初始化
    int k;
    memset(macdata_daatr->traffic_channel_business, 0, sizeof(macdata_daatr->traffic_channel_business));
    for (k = 0; k < SUBNET_NODE_FREQ_NUMBER; k++)
    {
        for (i = 0; i < TRAFFIC_CHANNEL_PRIORITY; i++)
        {
            for (j = 0; j < TRAFFIC_MAX_BUSINESS_NUMBER; j++)
            {
                macdata_daatr->traffic_channel_business[k].business[i][j].priority = -1;
            }
        }
    }

    // 初始化子网内节点路由表
    for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
    {
        macdata_daatr->Forwarding_Table[i - 1][0] = i; // 路由表全设置为自己节点
        macdata_daatr->Forwarding_Table[i - 1][1] = i;
    }

    if (DEBUG)
    {
        printf("partition %d, node %d: ", node->partitionData->partitionId, node->nodeId);
        for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
        {
            printf("%d ", (int)macdata_daatr->slot_traffic_execution[i].state);
        }
        printf("\n");
    }

    //////////////////////////////////// 定时器初始化 ////////////////////////////////////
    MacDaatrInitialize_parameter(node, macdata_daatr);  // 节点必要参数初始化, 目前里面内容可省略, 若添加非零初始化, 不可省略
    MacDaatrInitialize_mana_Timer(node, macdata_daatr); // 网管信道定时器初始化
    // MacDaatrInitialize_Wave_Timer(node); // 波束维护模块定时器初始化

    MacDaatrInitialize_pkt_Counter(node, macdata_daatr); // 测试计数模块初始化
    // MacDaatrInitialize_LocalLinkState_timer(node, macdata_daatr); // 链路状态需求初始化
    MacDaatrInitialize_Other_subnet_NodePosition_Timer(node, macdata_daatr); // 定期向网络层发送子网其他节点位置信息初始化

    MacDaatrInitialize_Link_Setup_Timer(node, macdata_daatr);           // 业务信道开始建链初始化
    MacDaatrInitialize_Other_End_Build_Link_Stage(node, macdata_daatr); // 结束建链阶段定时器初始化
    MacDaatr_Node_Frequency_Initialization(node, macdata_daatr);        // 初始化跳频序列
    MacDaatr_generate_subnet_using_freq(macdata_daatr);
    MacDaatr_Cacu_node_narrow_band_interfer_ratio(macdata_daatr, 0);
    if (node->nodeId == macdata_daatr->mana_node)
    {
        // 如果是网管节点, 则初始化定期判断是否进入频率调整阶段
        MacDaatrInitialize_Judge_If_Enter_Freq_Adjust_Timer(node, macdata_daatr);
    }

    //////////////////////////////// 通知装载射前时隙表 ////////////////////////////////
    Message *slottable_loading_msg = MESSAGE_Alloc(node,
                                                   MAC_LAYER,
                                                   MAC_PROTOCOL_DAATR,
                                                   MSG_MAC_Read_Slottable_From_Txt);
    MESSAGE_Send(node, slottable_loading_msg, 500 * MILLI_SECOND);
    //////////////////////////////// 通知装载射前时隙表 ////////////////////////////////

    // 身份配置信息测试
    Message *msg_indentity = MESSAGE_Alloc(node,
                                           MAC_LAYER,
                                           MAC_PROTOCOL_DAATR,
                                           MSG_MAC_POOL_ReceiveResponsibilityConfiguration);
    NodeNotification node_notify;
    node_notify.intergroupgatewayNodeId = 7;
    node_notify.intragroupcontrolNodeId = 4;
    if (node->nodeId == 4)
    { // 网管
        node_notify.nodeResponsibility = 1;
    }
    else if (node->nodeId == 7)
    { // 网关
        node_notify.nodeResponsibility = 2;
    }
    else
    { // 普通
        node_notify.nodeResponsibility = 0;
    }
    MESSAGE_PacketAlloc(node, msg_indentity, sizeof(NodeNotification), TRACE_DAATR);
    memcpy((NodeNotification *)MESSAGE_ReturnPacket(msg_indentity), &node_notify, sizeof(NodeNotification));
    // MESSAGE_Send(node, msg_indentity, 1100 * MILLI_SECOND);

    /////////////////////////////// 生成频谱感知结果测试 ///////////////////////////////
    spectrum_sensing_struct spec_struct;
    for (i = 0; i < TOTAL_FREQ_POINT; i++)
    {
        spec_struct.spectrum_sensing[i] = Randomly_Generate_0_1(0.97);
    }
    Message *spec_sense_msg = MESSAGE_Alloc(node,
                                            MAC_LAYER,
                                            MAC_PROTOCOL_DAATR,
                                            MAC_DAATR_SpectrumSensing);
    MESSAGE_PacketAlloc(node, spec_sense_msg, sizeof(spectrum_sensing_struct), TRACE_DAATR);
    memcpy((spectrum_sensing_struct *)MESSAGE_ReturnPacket(spec_sense_msg),
           &spec_struct, sizeof(spectrum_sensing_struct));
    MESSAGE_Send(node, spec_sense_msg, 1200 * MILLI_SECOND);
    /////////////////////////////// 生成频谱感知结果测试 ///////////////////////////////

    //////////////////////////////// 链路构建需求测试3 ////////////////////////////////
    if (node->nodeId == macdata_daatr->mana_node)
    {
        // 437 437 450 495
        // 2 node:100
        // 3 node:96
        int linkNumTest = 437;
        LinkAssignment *linkAssList = (LinkAssignment *)MEM_malloc(linkNumTest * sizeof(LinkAssignment));
        // Generate_Random_LinkAssignment(linkAssList, linkNumTest, 75, 1000, 0.3);
        linkNumTest = Generate_LinkAssignment_Stage_1(linkAssList);
        // linkNumTest = Generate_LinkAssignment_Stage_3nodes(linkAssList);
        unsigned int fullPacketSize = sizeof(LinkAssignmentHeader) + linkNumTest * sizeof(LinkAssignment);
        char *pktStart = (char *)MEM_malloc(fullPacketSize);

        LinkAssignmentHeader *linkheader = (LinkAssignmentHeader *)pktStart;
        linkheader->linkNum = linkNumTest;

        LinkAssignment *linkAssPtr = (LinkAssignment *)(pktStart + sizeof(LinkAssignmentHeader));
        memcpy(linkAssPtr, linkAssList, linkNumTest * sizeof(LinkAssignment));

        Message *newMsg = MESSAGE_Alloc(node,
                                        MAC_LAYER,
                                        MAC_PROTOCOL_DAATR,
                                        MAC_DAATR_ReceiveTaskInstruct);
        MESSAGE_PacketAlloc(node, newMsg, fullPacketSize, TRACE_DAATR);
        memcpy(MESSAGE_ReturnPacket(newMsg), pktStart, fullPacketSize);

        MEM_free(pktStart);
        // MESSAGE_Send(node, newMsg, 1300 * MILLI_SECOND);
    }
    //////////////////////////////// 链路构建需求测试3 ////////////////////////////////
}

static void MacDaatrUpdateTimer(Node *node, MacDataDaatr *macdata_daatr)
{
    int i;
    int slotId = macdata_daatr->currentSlotId;
    clocktype delay = 0;

    for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    {
        if (slotId == TRAFFIC_SLOT_NUMBER - 1)
        {
            macdata_daatr->nextSlotId = 0;
        }
        else
        {
            macdata_daatr->nextSlotId = slotId + 1;
        }
        macdata_daatr->nextStatus = macdata_daatr->slot_traffic_execution[macdata_daatr->nextSlotId].state;

        delay += macdata_daatr->slotDuration;

        if ((macdata_daatr->nextStatus == DAATR_STATUS_TX) || (macdata_daatr->nextStatus == DAATR_STATUS_RX))
        {
            break;
        }

        slotId++;

        if (slotId == TRAFFIC_SLOT_NUMBER)
        {
            slotId = 0;
        }
    }

    // macdata_daatr->timerExpirationTime = delay + node->getNodeTime();

    MESSAGE_Send(node, macdata_daatr->timerMsg, delay);

    if (DEBUG)
    {
        cout << "Update timer sent for node [" << node->nodeId << "] with a delay of " << (float)(delay) / 1000000 << " at " << (float)(node->getNodeTime()) / 1000000 << endl;
        cout << endl;
    }
}

// prio:      优先级对应的需要前移的队列(0--(TRAFFIC_CHANNEL_PRIORITY-1))
// recv:      某接收节点对应的优先级
// dest_node: 对应的节点(1-20), 所以需要减一
// location:  位置(0-(TRAFFIC_MAX_BUSINESS_NUMBER-1))
static void traffic_business_forward_lead(MacDataDaatr *macdata_daatr, int dest_node, int prio, int location)
{
    // 对业务信道业务队列前移一格(第一个业务发送完成, 消除其队列)
    int i = location;
    // for (i = location; i < TRAFFIC_MAX_BUSINESS_NUMBER - 1; i++)
    while (macdata_daatr->traffic_channel_business[dest_node - 1].business[prio][i + 1].priority != -1)
    {
        macdata_daatr->traffic_channel_business[dest_node - 1].business[prio][i] =
            macdata_daatr->traffic_channel_business[dest_node - 1].business[prio][i + 1]; // 对应优先级队列往前移一位, 同时也完成了指定业务的移除
        i++;
    }
    macdata_daatr->traffic_channel_business[dest_node - 1].business[prio][i].priority = -1; // 队列最后一个业务置空(优先级调为-1)
}

// node: 对应的节点(1-20)
// pri:  优先级对应的需要前移的队列(0--(TRAFFIC_CHANNEL_PRIORITY-1))
static int search_traffic_location(MacDataDaatr *macdata_daatr, int dest_node, int pri)
{ // 查询当前业务信道对应节点优先级队列在第几个后空
    int i;
    // dest_node -= 1; // 节点ID从1-20, 数组从0-19
    for (i = 0; i < TRAFFIC_MAX_BUSINESS_NUMBER; i++)
    {
        if (macdata_daatr->traffic_channel_business[dest_node - 1].business[pri][i].priority == -1)
        { // 当前i对应的位置即为空
            break;
        }
    }
    return i;
}

// pri:      业务信道优先级对应的需要前移的队列(0--(TRAFFIC_CHANNEL_PRIORITY-1))
// location: 位置(0-(MANA_MAX_BUSINESS_NUMBER-1))
// temp: 0:  将pri优先级的网管信道业务队列中的location所指的业务提升一个优先级并放入高一级优先级的队列中, 且优先级减1, 若高一级优先级队列中的业务已满, 则不做任何操作
// temp: 1:  将此业务移动到最高优先级队列的第一个, 并将原队列前移一位
// (此时根据实际情景不可能出现最高优先级队列已满的情况, 对于已在最高优先级的业务不做任何处理, 因为实际上其已是第一个)
static int sequence_add_business_traffic(MacDataDaatr *macdata_daatr,
                                         int dest_node, int location, int pri, int temp)
{
    int num = 0;
    int type = 1;
    if (temp == 0)
    {
        num = search_traffic_location(macdata_daatr, dest_node, pri - 1); // 搜寻高一级优先级队列中是否还有空余位置
        if (num != TRAFFIC_MAX_BUSINESS_NUMBER)
        {                                                                                                      // 如果还有空闲位置
            macdata_daatr->traffic_channel_business[dest_node - 1].business[pri][location].priority = pri - 1; // 优先级减一
            macdata_daatr->traffic_channel_business[dest_node - 1].business[pri - 1][num] =
                macdata_daatr->traffic_channel_business[dest_node - 1].business[pri][location];
            traffic_business_forward_lead(macdata_daatr, dest_node, pri, location); // 消除此位置的业务, 并前移
            type = 0;                                                               // 代表前移成功
        }
    }
    else
    {
        macdata_daatr->traffic_channel_business[dest_node - 1].business[0][0] =
            macdata_daatr->traffic_channel_business[dest_node - 1].business[pri][location];
        macdata_daatr->traffic_channel_business[dest_node - 1].business[0][0].priority = 0;
        traffic_business_forward_lead(macdata_daatr, dest_node, pri, location); // 消除当前业务所在队列中的该业务, 并前移一位
        type = 0;                                                               // 代表成功
    }
    return type;
}

// 对节点 node (0-19) 的第 pri 个优先级队列进行检查, 找到队列中非空的第一个位置 loc 并返回
// dest_node: 1 - SUBNET_NODE_NUM
// dest_node = -1 时表示检查网管信道队列
static int get_sequence_available_location(MacDataDaatr *macdata_daatr, int dest_node, char pri)
{
    int loc = 0;
    if (dest_node != -1)
    {
        while (loc <= TRAFFIC_MAX_BUSINESS_NUMBER - 1)
        {
            // 从前向后检查接收节点的业务信道的业务量, 找到第一个空位置 loc
            if ((macdata_daatr->traffic_channel_business[dest_node - 1].business[pri][loc].priority == -1))
            {
                break;
            }
            loc++;
        }
    }
    return loc;
}

// 将busin的详细信息添加进业务信道表的具体位置(优先级为pri的队列的第loc个位置)中
// temp 代表是否向该队列的第一个位置处添加上述信息
// temp = 1: 本队列未满, 在原有优先级队列的末尾处添加即可
// temp = 0: 原本的优先级队列已满, 在更低优先级队列的第一个业务处添加busin
// my_pkt_flag = 0 : 表示这是链路层自己的数据包, 不需要查看路由表
static void Insert_mfc_to_queue(MacDataDaatr *macdata_daatr, msgFromControl *busin, int pri, int loc, int temp, int my_pkt_flag)
{
    int i = 0;
    if (my_pkt_flag == 0)
    {
        if (temp)
        {
            macdata_daatr->traffic_channel_business[busin->destAddr - 1].business[pri][loc].priority = pri;
            macdata_daatr->traffic_channel_business[busin->destAddr - 1].business[pri][loc].data_kbyte_sum = busin->packetLength;
            macdata_daatr->traffic_channel_business[busin->destAddr - 1].business[pri][loc].waiting_time = 0;
            macdata_daatr->traffic_channel_business[busin->destAddr - 1].business[pri][loc].busin_data = *busin;
            macdata_daatr->traffic_channel_business[busin->destAddr - 1].recv_node = busin->destAddr;
            macdata_daatr->traffic_channel_business[busin->destAddr - 1].state = 0;
        }
        else
        {
            // 将原本优先级队列的业务后移一位
            for (i = loc; i > 0; i--)
            {
                macdata_daatr->traffic_channel_business[busin->destAddr - 1].business[pri][i] =
                    macdata_daatr->traffic_channel_business[busin->destAddr - 1].business[pri][i - 1];
            }
            macdata_daatr->traffic_channel_business[busin->destAddr - 1].business[pri][0].priority = pri;
            macdata_daatr->traffic_channel_business[busin->destAddr - 1].business[pri][0].data_kbyte_sum = busin->packetLength;
            macdata_daatr->traffic_channel_business[busin->destAddr - 1].business[pri][0].waiting_time = 0;
            macdata_daatr->traffic_channel_business[busin->destAddr - 1].business[pri][0].busin_data = *busin;
            macdata_daatr->traffic_channel_business[busin->destAddr - 1].recv_node = busin->destAddr;
            macdata_daatr->traffic_channel_business[busin->destAddr - 1].state = 0;
        }
    }
    else // 这说明这是需要查看路由表后再插入队列的数据包
    {
        int next_hop_addr = Search_Next_Hop_Addr(macdata_daatr, busin->destAddr);
        if (temp)
        {
            macdata_daatr->traffic_channel_business[next_hop_addr - 1].business[pri][loc].priority = pri;
            macdata_daatr->traffic_channel_business[next_hop_addr - 1].business[pri][loc].data_kbyte_sum = busin->packetLength;
            macdata_daatr->traffic_channel_business[next_hop_addr - 1].business[pri][loc].waiting_time = 0;
            macdata_daatr->traffic_channel_business[next_hop_addr - 1].business[pri][loc].busin_data = *busin;
            macdata_daatr->traffic_channel_business[next_hop_addr - 1].recv_node = next_hop_addr;
            macdata_daatr->traffic_channel_business[next_hop_addr - 1].state = 0;
        }
        else
        {
            // 将原本优先级队列的业务后移一位
            for (i = loc; i > 0; i--)
            {
                macdata_daatr->traffic_channel_business[next_hop_addr - 1].business[pri][i] =
                    macdata_daatr->traffic_channel_business[next_hop_addr - 1].business[pri][i - 1];
            }
            macdata_daatr->traffic_channel_business[next_hop_addr - 1].business[pri][0].priority = pri;
            macdata_daatr->traffic_channel_business[next_hop_addr - 1].business[pri][0].data_kbyte_sum = busin->packetLength;
            macdata_daatr->traffic_channel_business[next_hop_addr - 1].business[pri][0].waiting_time = 0;
            macdata_daatr->traffic_channel_business[next_hop_addr - 1].business[pri][0].busin_data = *busin;
            macdata_daatr->traffic_channel_business[next_hop_addr - 1].recv_node = next_hop_addr;
            macdata_daatr->traffic_channel_business[next_hop_addr - 1].state = 0;
        }
    }
}

static void MFC_Priority_Adjustment(MacDataDaatr *macdata_daatr)
{
    int i, j, k;
    // for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
    // {
    //     for (j = 1; j < TRAFFIC_CHANNEL_PRIORITY; j++) // 对于已经处在最高优先级的业务不做处理
    //     {
    //         int empty_loc = search_traffic_location(macdata_daatr, i, j);
    //         for (k = 0; k < empty_loc; k++)
    //         {
    //             if (macdata_daatr->traffic_channel_business[i - 1].business[j][k].waiting_time > 1)
    //             {
    //                 if (sequence_add_business_traffic(macdata_daatr, i, 0, j, 0))
    //                 {
    //                     cout << "前一优先级队列已满, 剩余业务无法调整" << endl;
    //                     break;
    //                 }
    //             }
    //         }
    //     }
    // }
    double max_waiting_slotnum = floor(MAX_WAITING_TIME / (TRAFFIC_SLOT_NUMBER * TRAFFIC_SLOT_DURATION)) * macdata_daatr->TX_Slotnum;
    for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
    {
        for (j = 1; j < TRAFFIC_CHANNEL_PRIORITY; j++) // 对于已经处在最高优先级的业务不做处理
        {
            for (k = 0; k < TRAFFIC_MAX_BUSINESS_NUMBER; k++)
            {
                if (macdata_daatr->traffic_channel_business[i].business[j][k].waiting_time > max_waiting_slotnum)
                {
                    traffic_business_forward_lead(macdata_daatr, i, j, k);
                }
            }
        }
    }
}

// destAddr: 目的节点
static int Search_Next_Hop_Addr(MacDataDaatr *macdata_daatr, int destAddr)
{
    return int(macdata_daatr->Forwarding_Table[destAddr - 1][1]);
}

static bool Compare_by_priority(LinkAssignment_single LinkAssignment_single_1, LinkAssignment_single LinkAssignment_single_2)
{
    return LinkAssignment_single_1.priority <= LinkAssignment_single_2.priority;
}

// 判断链路的收发节点是否包含网关节点(网关节点ID为2)
// 包含返回true
static bool judge_if_include_gateway(MacDataDaatr *macdata_daatr, LinkAssignment_single *LinkAssignment_single_temp)
{
    return (LinkAssignment_single_temp->begin_node == macdata_daatr->gateway_node ||
            LinkAssignment_single_temp->end_node == macdata_daatr->gateway_node ||

            LinkAssignment_single_temp->begin_node == macdata_daatr->standby_gateway_node ||
            LinkAssignment_single_temp->end_node == macdata_daatr->standby_gateway_node);
    // 暂定网关节点为2
}

// 判断这个时隙序列是否可以提供给这个节点(主要用于频分复用, 在频分复用中, 节点号都只能在收发节点中出现一次)
// 传入的参数为: 待编排的业务; 时隙表; 业务的待分配时隙的长度;
static bool judge_if_choose_slot(LinkAssignment_single *LinkAssignment_single_temp, Alloc_slot_traffic Alloc_slottable_traffic[], int slot_length)
{
    for (int i = 0; i < slot_length; i++)
    {
        if (Alloc_slottable_traffic[i].multiplexing_num == 0)
        {
            return false;
        }
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

// 若待发送队列中不包含重传包, 则返回false, 反之返回true
static bool judge_if_include_ReTrans(msgFromControl MFC_temp, vector<msgFromControl> MFC_list_temp)
{
    if (MFC_list_temp.size() == 0 || MFC_temp.repeat == 0)
    {
        return false;
    }

    vector<msgFromControl>::iterator mfc_in_list;
    for (mfc_in_list = MFC_list_temp.begin(); mfc_in_list != MFC_list_temp.end(); mfc_in_list++)
    {
        if ((*mfc_in_list).packetLength == MFC_temp.packetLength &&
            (*mfc_in_list).seq == MFC_temp.seq &&
            (*mfc_in_list).msgType == MFC_temp.msgType &&
            (*mfc_in_list).priority == MFC_temp.priority &&
            (*mfc_in_list).fragmentNum == MFC_temp.fragmentNum &&
            (*mfc_in_list).fragmentSeq == MFC_temp.fragmentSeq)
        {
            return true;
        }
    }
    return false;
}

// 根据LA和节点间距离计算链路构建需求所需的时隙数 data_slot_per_frame
static void Caclulate_slotnum_per_frame(Node *node, MacDataDaatr *macdata_daatr, LinkAssignment_single *LinkAssignment_single_temp)
{
    int i, node1, node2;
    int temp_posiX1, temp_posiY1, temp_posiZ1;
    int temp_posiX2, temp_posiY2, temp_posiZ2;

    // if (LinkAssignment_single_temp->begin_node == node->nodeId || LinkAssignment_single_temp->end_node == node->nodeId)
    // {
    if (LinkAssignment_single_temp->begin_node == node->nodeId)
    {
        temp_posiX1 = macdata_daatr->local_node_position_info.positionX;
        temp_posiY1 = macdata_daatr->local_node_position_info.positionY;
        temp_posiZ1 = macdata_daatr->local_node_position_info.positionZ;

        for (i = 0; i < SUBNET_NODE_FREQ_NUMBER - 1; i++)
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

    else if (LinkAssignment_single_temp->end_node == node->nodeId)
    {
        temp_posiX1 = macdata_daatr->local_node_position_info.positionX;
        temp_posiY1 = macdata_daatr->local_node_position_info.positionY;
        temp_posiZ1 = macdata_daatr->local_node_position_info.positionZ;

        for (i = 0; i < SUBNET_NODE_FREQ_NUMBER - 1; i++)
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
    // }
    else
    {
        for (i = 0; i < SUBNET_NODE_FREQ_NUMBER - 1; i++)
        {
            if (LinkAssignment_single_temp->begin_node == macdata_daatr->subnet_other_node_position[i].nodeId)
            {
                temp_posiX1 = macdata_daatr->subnet_other_node_position[i].positionX;
                temp_posiY1 = macdata_daatr->subnet_other_node_position[i].positionY;
                temp_posiZ1 = macdata_daatr->subnet_other_node_position[i].positionZ;
                break;
            }
        }

        for (i = 0; i < SUBNET_NODE_FREQ_NUMBER - 1; i++)
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
    // LinkAssignment_single_temp->distance = calculateLinkDistance(temp_posiX1, temp_posiY1, temp_posiZ1,
    //                                                                temp_posiX2, temp_posiY2, temp_posiZ2);
    LinkAssignment_single_temp->distance = 49;
    // LinkAssignment_single_temp->distance = std::max({LinkAssignment_single_temp->distance, 500});
    float data_slot_per_frame_temp = (float)((LinkAssignment_single_temp->size * 8) / 1000.0) / LinkAssignment_single_temp->QOS; // kBytes
    // cout << "LinkAssignment_single_temp->size: " << LinkAssignment_single_temp->size << endl;
    // cout << "size(kbit) " << (LinkAssignment_single_temp->size * 8) / 1000.0 << endl;
    // cout << "data_slot_per_frame_temp " << data_slot_per_frame_temp << endl;
    // system("pause");
    double trans_speed = calculate_slot_type_mana(LinkAssignment_single_temp->distance); // kbits/s
    float trans_data = trans_speed * 0.0025;                                             // 一个时隙可以传输的数据量 kbits
    LinkAssignment_single_temp->data_slot_per_frame = ceil(data_slot_per_frame_temp * 8 / trans_data);
    // LinkAssignment_single_temp->data_slot_per_frame = 1;
    // if (LinkAssignment_single_temp->data_slot_per_frame != 1)
    //{
    //    cout << LinkAssignment_single_temp->begin_node << "->" << LinkAssignment_single_temp->end_node << endl;
    //}
}

// 将按权重升序排列的诸多单链路构建需求进行链路的分配
static Alloc_slot_traffic *ReAllocate_Traffic_slottable(Node *node,
                                                        MacDataDaatr *macdata_daatr,
                                                        vector<LinkAssignment_single> LinkAssignment_Storage)
{
    int i, j, k, m;
    // int break_flag = 0;
    int rest_slot_num_per_node[SUBNET_NODE_FREQ_NUMBER]; // 网关节点之外的节点有100个可发送时隙
    for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    {
        rest_slot_num_per_node[i] = TRAFFIC_SLOT_NUMBER;
    }

    if (macdata_daatr->gateway_node != 0)
    {
        rest_slot_num_per_node[macdata_daatr->gateway_node - 1] = TRAFFIC_SLOT_NUMBER * 0.6; // 暂定网关节点2
    }
    if (macdata_daatr->standby_gateway_node != 0)
    {
        rest_slot_num_per_node[macdata_daatr->standby_gateway_node - 1] = TRAFFIC_SLOT_NUMBER * 0.6;
    }

    Alloc_slot_traffic Alloc_slottable_traffic[TRAFFIC_SLOT_NUMBER];
    // Alloc_slot_traffic *Alloc_slottable_traffic = new Alloc_slot_traffic[TRAFFIC_SLOT_NUMBER];
    memset(Alloc_slottable_traffic, 0, sizeof(Alloc_slot_traffic) * TRAFFIC_SLOT_NUMBER);

    // 将分配表初始化
    for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
    {
        // Alloc_slottable_traffic[i].subnet_ID = subnet;
        Alloc_slottable_traffic[i].multiplexing_num = FREQUENCY_DIVISION_MULTIPLEXING_NUMBER;
        Alloc_slottable_traffic[i].attribute = 0; // 先全部设为下级网业务时隙
    }
    for (i = 0; i < TRAFFIC_SLOT_NUMBER; i = i + 20)
    {
        for (j = 8; j < 16; j++)
        {
            Alloc_slottable_traffic[i + j].attribute = 1; // 设为不可与网关节点通信的时隙
        }
    }

    // 全连接
    // if (SUBNET_NODE_NUMBER == 20)
    {
        int i = 0, j = 0;
        int slot_loc = 0; // 用来标识目前正在分配时隙的位置, 非子网间时隙才可以分

        string Full_connection_send = "..\\..\\..\\..\\libraries\\user_models\\src\\FullConnection\\FullConnection_20_send.txt";
        string Full_connection_recv = "..\\..\\..\\..\\libraries\\user_models\\src\\FullConnection\\FullConnection_20_recv.txt";

        ifstream fout1(Full_connection_send);
        ifstream fout2(Full_connection_recv);

        if (!fout1.is_open())
        {
            cout << "Could Not Open File1" << endl;
            system("pause");
        }
        if (!fout2.is_open())
        {
            cout << "Could Not Open File2" << endl;
            system("pause");
        }

        string str1, str2, temp;
        vector<int> send_node;
        vector<int> recv_node;

        getline(fout1, str1);
        getline(fout2, str2);
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

        while (i < 38) // 38个全连接时隙
        {
            if (slot_loc % 20 < 8 || slot_loc % 20 >= 16)
            {
                for (j = 0; j < FREQUENCY_DIVISION_MULTIPLEXING_NUMBER; j++)
                {
                    Alloc_slottable_traffic[slot_loc].send_node[j] = send_node[10 * i + j];
                    Alloc_slottable_traffic[slot_loc].recv_node[j] = recv_node[10 * i + j];

                    rest_slot_num_per_node[send_node[10 * i + j] - 1] -= 1;
                    rest_slot_num_per_node[recv_node[10 * i + j] - 1] -= 1;
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
    }

    int LinkAssNum = LinkAssignment_Storage.size();
    // cout << "The number of given single linkassignment is " << LinkAssNum << endl;
    vector<LinkAssignment_single>::iterator la;
    for (la = LinkAssignment_Storage.begin(); la != LinkAssignment_Storage.end(); la++)
    {
        if (rest_slot_num_per_node[(*la).begin_node - 1] == 0 || rest_slot_num_per_node[(*la).end_node - 1] == 0)
        {
            cout << "Failed to Allocate Slot for LA - " << (*la).begin_node;
            cout << "-" << (*la).end_node << endl;
            continue;
        }

        Caclulate_slotnum_per_frame(node, macdata_daatr, &(*la)); // 计算LA_single的时隙数
        // cout << "data_slot_per_frame: " << (*la).data_slot_per_frame << endl;

        if (judge_if_include_gateway(macdata_daatr, &(*la))) // 包含
        {
            m = 0;
            for (j = 0; j <= TRAFFIC_SLOT_NUMBER - 20; j += 20)
            {
                for (k = 0; k < 8; k++)
                {
                    if (judge_if_choose_slot(&(*la), &Alloc_slottable_traffic[j + k], 1))
                    {
                        // 如果此时隙内不包含接收或者发送节点, 则不会影响频分复用结果, 可以分配此时隙
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
                        if (judge_if_choose_slot(&(*la), &Alloc_slottable_traffic[j + k], 1))
                        {
                            // 如果此时隙内不包含接收或者发送节点, 则不会影响频分复用结果, 可以分配此时隙
                            int temp = FREQUENCY_DIVISION_MULTIPLEXING_NUMBER - Alloc_slottable_traffic[j + k].multiplexing_num;
                            Alloc_slottable_traffic[j + k].send_node[temp] = (*la).begin_node; // 将发送节点添加进时隙的发送节点集合中
                            Alloc_slottable_traffic[j + k].recv_node[temp] = (*la).end_node;   // 将接收节点添加进时隙的发送节点集合中
                            Alloc_slottable_traffic[j + k].multiplexing_num -= 1;              // 可复用数减1
                            rest_slot_num_per_node[(*la).end_node - 1] -= 1;                   // 此节点可分配时隙数减1
                            rest_slot_num_per_node[(*la).begin_node - 1] -= 1;                 // 此节点可分配时隙数减1
                            m++;

                            if (m == (*la).data_slot_per_frame || j + k == TRAFFIC_SLOT_NUMBER - 1)
                            {
                                break; // 已分配足够时隙或时隙表所有时隙已全部分配, 跳出时隙分配
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
                if (judge_if_choose_slot(&(*la), &Alloc_slottable_traffic[j], 1))
                {
                    // 如果此时隙内不包含接收或者发送节点, 则不会影响频分复用结果, 可以分配此时隙
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

            for (int ii = 1; ii <= SUBNET_NODE_FREQ_NUMBER; ii++)
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
                    for (k = 1; k <= SUBNET_NODE_FREQ_NUMBER; k++)
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
                    for (k = 1; k <= SUBNET_NODE_FREQ_NUMBER; k++)
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
    return Alloc_slottable_traffic;
}

// 下面需要将分配表转换为各节点对应的时隙表(未使用)
static mana_slot_traffic Generate_mana_slot_traffic(Alloc_slot_traffic *Alloc_slottable_traffic_temp)
{
    Alloc_slot_traffic *Alloc_slottable_traffic = (Alloc_slot_traffic *)MEM_malloc(sizeof(Alloc_slottable_traffic) * TRAFFIC_SLOT_NUMBER);
    // Alloc_slot_traffic Alloc_slottable_traffic[TRAFFIC_SLOT_NUMBER];
    // memset(&Alloc_slottable_traffic, 0, sizeof(Alloc_slottable_traffic) * TRAFFIC_SLOT_NUMBER);
    memcpy(Alloc_slottable_traffic, Alloc_slottable_traffic_temp, sizeof(Alloc_slottable_traffic) * TRAFFIC_SLOT_NUMBER);

    mana_slot_traffic slot_traffic_execution_new[SUBNET_NODE_FREQ_NUMBER][TRAFFIC_SLOT_NUMBER]; // 业务信道时隙表(执行阶段)
    memset(&slot_traffic_execution_new, 0, sizeof(mana_slot_traffic) * SUBNET_NODE_NUMBER * TRAFFIC_SLOT_NUMBER);

    for (int i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
    {
        for (int j = 0; j < TRAFFIC_SLOT_NUMBER; j++)
        {
            if (Alloc_slottable_traffic[j].multiplexing_num != FREQUENCY_DIVISION_MULTIPLEXING_NUMBER)
            {
                for (int k = 0; k < FREQUENCY_DIVISION_MULTIPLEXING_NUMBER - Alloc_slottable_traffic[j].multiplexing_num; k++)
                {
                    // Alloc_slottable_traffic[j].send_node 是 网管节点 的分配表的第 j 个时隙的发送节点序列
                    // send[k] 是其中的第 k 组链路需求的发送节点
                    // 需要找到20个时隙表中的第senk[k]个, 然后将其第[j]个时隙的state和send进行修改
                    // 修改为TX, 并将send指向recv节点
                    slot_traffic_execution_new[Alloc_slottable_traffic[j].send_node[k] - 1][j].state = DAATR_STATUS_TX;
                    slot_traffic_execution_new[Alloc_slottable_traffic[j].send_node[k] - 1][j].send_or_recv_node = Alloc_slottable_traffic[j].recv_node[k];

                    slot_traffic_execution_new[Alloc_slottable_traffic[j].recv_node[k] - 1][j].state = DAATR_STATUS_RX;
                    slot_traffic_execution_new[Alloc_slottable_traffic[j].recv_node[k] - 1][j].send_or_recv_node = Alloc_slottable_traffic[j].send_node[k];
                }
            }
        }
        // 将 slot_traffic_execution_new[i-1] 发给节点 i
    }
    return **slot_traffic_execution_new;
}

// 该函数由网络层在有业务要发送时进行调用
// 实现向业务信道队列和网管信道队列内添加待发送业务的目标
// node是生成业务的节点
bool MAC_NetworkLayerHasPacketToSend(Node *node, int interfaceIndex, msgFromControl *busin)
{
    /*
    1.检查busin的接收节点和优先级
    2.检查此时该节点的优先级队列是否已满
    3.(若满, 则将其放于下一优先级队列的top)
    4.(若未满, 则将其放于本优先级队列的last)
    5.更新此时的优先级 & waiting_time
    */
    int i, new_loc, loc;
    int flag = 0; // 表征是否成功将busin插入业务队列
    int temp_node;
    MacDataDaatr *macdata_daatr = (MacDataDaatr *)node->macData[interfaceIndex]->macVar;
    if (busin->packetLength == 73 && busin->msgType == 15 && busin->fragmentNum == 15)
    {
        // 如果是链路层自己的数据包, 那么不需要经过转发表
        loc = get_sequence_available_location(macdata_daatr, busin->destAddr, busin->priority);
    }
    // else if (busin->packetLength == 573 && busin->msgType == 14 && busin->fragmentNum == 14)
    // {
    //     // 如果是链路层自己的数据包, 那么不需要经过转发表
    //     loc = get_sequence_available_location(macdata_daatr, busin->destAddr, busin->priority);
    // }
    else
    {
        // 如果是网络层数据包, 那么需要查看路由表对应节点的队列是否已满
        temp_node = Search_Next_Hop_Addr(macdata_daatr, busin->destAddr);
        if (busin->msgType == 2)
        {
            temp_node = 0;
        }
        if (temp_node == 0) // 即不存在转发表的情况
        {

            // system("pause"); // 在此处停住说明目的地址不存在转发节点 可以注释掉
            if (busin->msgType != 2)
            {
                cout << "此时数据包的接收节点 " << busin->destAddr << " 不存在转发节点, 丢弃业务" << endl;
                return false;
            }
            else
            {
                // cout << "收到MSGTYPE=2的数据包 " << busin->srcAddr << "->" << busin->destAddr << endl;
                loc = get_sequence_available_location(macdata_daatr, busin->destAddr, busin->priority);
            }
        }
        else
        {
            loc = get_sequence_available_location(macdata_daatr, temp_node, busin->priority);
            // cout << "节点 " << node->nodeId << " 收到目的地址为: " << busin->destAddr << " 的数据包, 下一跳转发节点为 " << temp_node << endl;
            //  system("pause");
        }
    }

    // loc = get_sequence_available_location(macdata_daatr, busin->destAddr, busin->priority);

    if (loc == TRAFFIC_MAX_BUSINESS_NUMBER) // 这说明队列全满
    {
        printf("the sequence of node %d of priority %d is full!\n", busin->destAddr, busin->priority);

        if (busin->packetLength == 73 && busin->msgType == 15 && busin->fragmentNum == 15) // 这是频谱感知结果数据包
        {
            // 在此处, 说明优先级0的队列已经全满, 只能丢弃最后一个业务, 腾出第一个位置来放置频谱感知结果
            // loc - 1 = 9, 将 1-9 的业务后移一位
            Insert_mfc_to_queue(macdata_daatr, busin, busin->priority, loc - 1, 0, 0);
        }
        // else if (busin->packetLength == 573 && busin->msgType == 14 && busin->fragmentNum == 14) // 这是频谱感知结果数据包
        // {
        //     // 在此处, 说明优先级0的队列已经全满, 只能丢弃最后一个业务, 腾出第一个位置来放置接入回复结果
        //     // loc - 1 = 9, 将 1-9 的业务后移一位
        //     Insert_mfc_to_queue(macdata_daatr, busin, busin->priority, loc - 1, 0, 0);
        // }
        else if (busin->msgType == 2)
        {
            for (i = busin->priority + 1; i < TRAFFIC_CHANNEL_PRIORITY; i++)
            {
                new_loc = get_sequence_available_location(macdata_daatr, busin->destAddr, i);
                if (new_loc != TRAFFIC_MAX_BUSINESS_NUMBER)
                {
                    Insert_mfc_to_queue(macdata_daatr, busin, i, new_loc, 0, 0); // 不查看路由表
                    flag = 1;
                    break;
                }
                printf("the sequence of node %d of priority %d is full!\n", i, busin->priority);
            }
        }
        else
        {
            // 需要将busin加到busin->priority + 1或更低优先级的队列中去的首位
            // 依次检索之后每个优先级的业务队列
            for (i = busin->priority + 1; i < TRAFFIC_CHANNEL_PRIORITY; i++)
            {
                new_loc = get_sequence_available_location(macdata_daatr,
                                                          Search_Next_Hop_Addr(macdata_daatr, busin->destAddr),
                                                          i);
                if (new_loc != TRAFFIC_MAX_BUSINESS_NUMBER)
                {
                    Insert_mfc_to_queue(macdata_daatr, busin, i, new_loc, 0, 1);
                    if (busin->msgType == 3)
                    {
                        macdata_daatr->stats.msgtype3_recv++;
                    }
                    flag = 1;
                    break;
                }
                printf("the sequence of node %d of priority %d is full!\n", i, busin->priority);
            }
            if (flag == false)
            {
                cout << endl;
            }
        }
    }
    else
    {
        if (busin->packetLength == 73 && busin->msgType == 15 && busin->fragmentNum == 15) // 这是频谱感知结果数据包
        {
            // 将 1 - loc 的业务后移一位, 腾出第一个位置来放置频谱感知结果
            Insert_mfc_to_queue(macdata_daatr, busin, busin->priority, loc, 0, 0);
        }
        // else if (busin->packetLength == 573 && busin->msgType == 14 && busin->fragmentNum == 14) // 这是huifu
        // {
        //     // 将 1 - loc 的业务后移一位, 腾出第一个位置来放置频谱感知结果
        //     Insert_mfc_to_queue(macdata_daatr, busin, busin->priority, loc, 0, 0);
        // }
        else if (busin->msgType == 2)
        {
            Insert_mfc_to_queue(macdata_daatr, busin, busin->priority, loc, 1, 0);
        }
        else
        {
            Insert_mfc_to_queue(macdata_daatr, busin, busin->priority, loc, 1, 1);
            if (busin->msgType == 3)
            {
                macdata_daatr->stats.msgtype3_recv++;
            }
        }
        flag = 1;
    }

    return flag;
}

void MacDaatrLayer(Node *node, int interfaceIndex, Message *msg)
{
    MacDataDaatr *macdata_daatr = (MacDataDaatr *)node->macData[interfaceIndex]->macVar;

    int previousStatus = macdata_daatr->currentStatus;

    macdata_daatr->currentStatus = macdata_daatr->nextStatus;
    macdata_daatr->currentSlotId = macdata_daatr->nextSlotId;

#if 1
    switch (msg->eventType)
    {
    // mac层接收数据包事件
    // 涉及调整非网管节点 state_now , 更新节点位置信息
    case MSG_MAC_Receivemsg_From_Phy:
    {
        uint8_t *bit_seq = (uint8_t *)MESSAGE_ReturnPacket(msg); // 返回bit序列
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
        // if(macdata_daatr->state_now != Mac_Initialization)
        //{//如果当不为建链阶段
        //     int slot_num_temp = macdata_daatr->mana_slot_should_read;
        //	if(slot_num_temp == 0)
        //	{
        //	    slot_num_temp = MANAGEMENT_SLOT_NUMBER_OTHER_STAGE;
        //	}
        //	else
        //	{
        //		slot_num_temp--;
        //	}
        //	macdata_daatr->slot_management_other_stage[slot_num_temp].nodeId = mac_header2.srcAddr;
        // }
        if (mac_header2.backup == 1)
        { // 链路层数据包
            mac_converter2.set_type(8);
            mac_converter2 - mac_converter; // 网管信道消息类型
            uint8_t *temp_ptr = mac_converter2.get_sequence();
            Mana_Packet_Type packet_type;
            packet_type.type = temp_ptr[0];
            MacPacket_Daatr macpacket_daatr;
            switch (packet_type.type)
            {
            case 1:
            { // 随遇接入请求
                if (node->nodeId == macdata_daatr->mana_node && macdata_daatr->waiting_to_access_node == 0)
                { // 如果本节点是网管节点
                    cout << endl
                         << "网管节点 " << node->nodeId << " 已收到节点 " << mac_header2.srcAddr << " 接入请求 ";
                    float time = (float)(node->getNodeTime()) / 1000000;
                    cout << "Time : " << time << "ms" << endl;
                    macdata_daatr->access_state = DAATR_WAITING_TO_REPLY; // 网管节点已收到接入请求, 等待下一接入请求恢复时隙回复
                    macdata_daatr->waiting_to_access_node = mac_header2.srcAddr;
                }
                else if (node->nodeId == macdata_daatr->mana_node && macdata_daatr->waiting_to_access_node != 0)
                {
                    cout << "当前子网已有待接入节点, 待网管节点发送完全子网跳频图案后, 在进行接入" << endl;
                }
                break;
            }
            case 2:
            { // 同意随遇接入请求
                if (node->nodeId == mac_header2.destAddr)
                {
                    cout << endl
                         << "Node " << node->nodeId << "已收到网管节点同意接入请求回复 ";
                    float time = (float)(node->getNodeTime()) / 1000000;
                    cout << "Time : " << time << "ms" << endl;
                    mac_converter2.set_type(9);
                    mac_converter2 - mac_converter; // 网管节点接入请求
                    mac_converter2.daatr_0_1_to_struct();
                    mana_access_reply *access_reply = (mana_access_reply *)mac_converter2.get_struct();
                    int slot_location = 0;
                    for (i = 0; i < FREQUENCY_COUNT; i++)
                    {
                        macdata_daatr->frequency_sequence[mac_header2.srcAddr - 1][i] = access_reply->mana_node_hopping[i]; // 将网管节点的跳频序列赋值进去
                    }
                    for (i = 0; i < 1; i++)
                    { // 第一个时隙为飞行状态信息
                        slot_location = access_reply->slot_location[i];
                        cout << slot_location << " "; // 测试用
                        macdata_daatr->slot_management_other_stage[slot_location].nodeId = node->nodeId;
                        macdata_daatr->slot_management_other_stage[slot_location].state = DAATR_STATUS_FLIGHTSTATUS_SEND;
                    }
                    for (i; i < access_reply->slot_num; i++)
                    { // 剩余时隙为网络层业务时隙
                        slot_location = access_reply->slot_location[i];
                        cout << slot_location << " "; // 测试用
                        macdata_daatr->slot_management_other_stage[slot_location].nodeId = node->nodeId;
                        macdata_daatr->slot_management_other_stage[slot_location].state = DAATR_STATUS_NETWORK_LAYER_SEND;
                    }
                    macdata_daatr->access_state = DAATR_WAITING_TO_ACCESS; // 节点已收到网管节点同意请求, 等待接收全网跳频图案
                    macdata_daatr->mana_node = mac_header2.srcAddr;        // 此时的请求回复的源地址即为网管节点, 重新更新
                    Message *send_msg_access_waiting_time = MESSAGE_Alloc(node,
                                                                          MAC_LAYER,
                                                                          MAC_PROTOCOL_DAATR,
                                                                          MAC_DAATR_Subnet_Frequency_Waiting_Time); // 给等待接收全网跳频图案一个最大时间
                    MESSAGE_Send(node, send_msg_access_waiting_time, ACCESS_SUBNET_FREQUENCY_PARTTERN_WAITTING_MAX_TIME * MILLI_SECOND);

                    // 同步接入回复请求网管节点
                    macdata_daatr->slot_management_other_stage[9].nodeId = mac_header2.srcAddr;
                    macdata_daatr->slot_management_other_stage[21].nodeId = mac_header2.srcAddr;
                    macdata_daatr->slot_management_other_stage[34].nodeId = mac_header2.srcAddr;
                    macdata_daatr->slot_management_other_stage[46].nodeId = mac_header2.srcAddr;
                }
                break;
            }
            case 3:
            { // 拒绝随遇接入请求
                if (node->nodeId == mac_header2.destAddr)
                {
                    cout << endl
                         << "Node " << node->nodeId << "收到网管节点拒绝接入回复 ";
                    float time = (float)(node->getNodeTime()) / 1000000;
                    cout << "Time : " << time << "ms" << endl;
                    int num = rand() % (MAX_RANDOM_BACKOFF_NUM + 1);
                    macdata_daatr->access_state = DAATR_NEED_ACCESS; // 节点已收到网管节点拒绝接入请求, 重新等待接入
                    macdata_daatr->access_backoff_number = num;      // 随机回退

                    // 同步接入回复请求网管节点
                    macdata_daatr->slot_management_other_stage[9].nodeId = mac_header2.srcAddr;
                    macdata_daatr->slot_management_other_stage[21].nodeId = mac_header2.srcAddr;
                    macdata_daatr->slot_management_other_stage[34].nodeId = mac_header2.srcAddr;
                    macdata_daatr->slot_management_other_stage[46].nodeId = mac_header2.srcAddr;
                }
                break;
            }
            case 4:
            { // 普通节点飞行状态信息
                mac_converter2.set_type(3);
                mac_converter2 - mac_converter; // 飞行状态信息
                mac_converter2.daatr_0_1_to_struct();
                FlightStatus *flight_sta = (FlightStatus *)mac_converter2.get_struct();
                macpacket_daatr.node_position = *flight_sta;
                macpacket_daatr.node_position.nodeId = mac_header2.srcAddr;
                UpdatePosition(&macpacket_daatr, macdata_daatr);
                // if(macdata_daatr->if_mana_slottable_synchronization == true && macdata_daatr->if_receive_mana_flight == true)
                //{//如果需要进行网管信道时隙表同步
                //	macdata_daatr->mana_slottable_synchronization++;
                //	if(macdata_daatr->slot_management_other_stage[macdata_daatr->mana_slottable_synchronization].state == DAATR_STATUS_ACCESS_REQUEST
                //		|| macdata_daatr->slot_management_other_stage[macdata_daatr->mana_slottable_synchronization].state == DAATR_STATUS_ACCESS_REPLY
                //		|| macdata_daatr->slot_management_other_stage[macdata_daatr->mana_slottable_synchronization].nodeId == node->nodeId)
                //	{//如果时隙表当前时隙状态时接入请求或者接入回复或者是本时隙
                //		macdata_daatr->mana_slottable_synchronization++;
                //	}
                //	if(macdata_daatr->mana_slottable_synchronization != 25)
                //	{
                //		int slot_number = macdata_daatr->mana_slottable_synchronization;
                //		macdata_daatr->slot_management_other_stage[slot_number].nodeId = mac_header2.srcAddr;
                //	}
                //	else
                //	{//结束同步
                //		macdata_daatr->mana_slottable_synchronization = 0;
                //		macdata_daatr->if_mana_slottable_synchronization = false;
                //	}
                //
                // }
                break;
            }
            case 5:
            { // 网管节点飞行状态信息
                mac_converter2.set_type(3);
                mac_converter2 - mac_converter; // 飞行状态信息
                mac_converter2.daatr_0_1_to_struct();
                FlightStatus *flight_sta = (FlightStatus *)mac_converter2.get_struct();
                macpacket_daatr.node_position = *flight_sta;
                macpacket_daatr.node_position.nodeId = mac_header2.srcAddr;
                macdata_daatr->mana_node = mac_header2.srcAddr; // 更新网管节点
                                                                // if(macdata_daatr->if_mana_slottable_synchronization == true)
                //{//如果需要进行网管信道时隙表同步
                //		macdata_daatr->mana_slottable_synchronization = 0;
                //	macdata_daatr->if_receive_mana_flight = true;
                //	int slot_number = macdata_daatr->mana_slottable_synchronization;
                //	macdata_daatr->slot_management_other_stage[slot_number].nodeId = mac_header2.srcAddr;
                //}
                UpdatePosition(&macpacket_daatr, macdata_daatr);
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
                    cout << "节点 " << node->nodeId << " 此时未收到状态调整信息. Time: " << (float)(node->getNodeTime()) / 1000000 << endl;
                }

                else if (change_state->state == 1)
                {
                    macdata_daatr->state_now = Mac_Adjustment_Slot; // 调整本节点为时隙调整阶段
                    macdata_daatr->has_received_slottable = false;  // 初始未收到时隙表
                    cout << "节点 " << node->nodeId << " 收到网管节点广播消息, 并改变自己节点 state_now 为 Adjustment_Slot. Time: "
                         << (float)(node->getNodeTime()) / 1000000 << endl;
                    macdata_daatr->stats.slot_adjust_begin_time = (node->getNodeTime()) / 1000000; // 时隙调整阶段开始时间
                    // 在收到网管节点发来的状态调整信息后, 立刻将自己的波束指向网管节点, 等待接收时隙表
                    Node *dest_node_ptr = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, macdata_daatr->mana_node);
                    Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_RX, 0);
                }

                else if (change_state->state == 2)
                {
                    macdata_daatr->state_now = Mac_Adjustment_Freqency; // 调整本节点为时隙调整阶段
                    macdata_daatr->has_received_sequence = false;
                    cout << "节点 " << node->nodeId << " 收到网管节点广播消息, 并改变自己节点 state_now 为 Adjustment_Freqency. Time: "
                         << (float)(node->getNodeTime()) / 1000000 << endl;
                    macdata_daatr->stats.freq_adjust_begin_time = (node->getNodeTime()) / 1000000; // 时隙调整阶段开始时间
                    // 在收到网管节点发来的状态调整信息后, 立刻将自己的波束指向网管节点, 等待接收时隙表
                    Node *dest_node_ptr = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, macdata_daatr->mana_node);
                    Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_RX, 0);
                }

                else
                {
                    cout << "Incorrect Data!" << endl;
                    // system("pause");
                }
                // if(macdata_daatr->if_mana_slottable_synchronization == true && macdata_daatr->if_receive_mana_flight == true)
                //{//如果需要进行网管信道时隙表同步
                //	macdata_daatr->mana_slottable_synchronization++;
                //	if(macdata_daatr->slot_management_other_stage[macdata_daatr->mana_slottable_synchronization].state == DAATR_STATUS_ACCESS_REQUEST
                //		|| macdata_daatr->slot_management_other_stage[macdata_daatr->mana_slottable_synchronization].state == DAATR_STATUS_ACCESS_REPLY
                //		|| macdata_daatr->slot_management_other_stage[macdata_daatr->mana_slottable_synchronization].nodeId == node->nodeId)
                //	{//如果时隙表当前时隙状态时接入请求或者接入回复
                //		macdata_daatr->mana_slottable_synchronization++;
                //	}
                //	if(macdata_daatr->mana_slottable_synchronization != 25)
                //	{
                //		int slot_number = macdata_daatr->mana_slottable_synchronization;
                //		macdata_daatr->slot_management_other_stage[slot_number].nodeId = mac_header2.srcAddr;
                //	}
                //	else
                //	{//结束同步
                //		macdata_daatr->mana_slottable_synchronization = 0;
                //		macdata_daatr->if_mana_slottable_synchronization = false;
                //	}
                //
                // }
                break;
            }
            }
        }
        else if (mac_header2.backup == 0)
        {
            // 网络层数据包发送
            cout << "Node " << node->nodeId << " 收到网管信道数据包, 并向网络层传递" << endl;
            MacDaatr_struct_converter mac_converter_MFCs(100); // 用来盛放PDU中除去包头外的MFC的01序列
            mac_converter_MFCs - mac_converter;
            mac_converter_MFCs.set_length(frame_length - MAC_HEADER2_LENGTH);
            vector<uint8_t> *MFC_Trans_temp;
            MFC_Trans_temp = mac_converter_MFCs.Get_Single_MFC();
            msgFromControl *MFC_temp;
            MFC_temp = AnalysisLayer2MsgFromControl(MFC_Trans_temp);
            // 测试用
            Message *msg_send_network = MESSAGE_Alloc(node,
                                                      NETWORK_LAYER,
                                                      ROUTING_PROTOCOL_MMANET,
                                                      MSG_ROUTING_ReceiveMsgFromLowerLayer);
            MESSAGE_PacketAlloc(node, msg_send_network, MFC_Trans_temp->size(), TRACE_DAATR);
            memcpy(MESSAGE_ReturnPacket(msg_send_network), MFC_Trans_temp->data(), MFC_Trans_temp->size());
            delete MFC_Trans_temp;
            MESSAGE_Send(node, msg_send_network, 0);
        }
        MESSAGE_Free(node, msg); // 释放原有事件
        break;
    }

    // 涉及网管信道定时器事件, 子网内建链, 根据need_change_state更改state
    case Mac_Management_Channel_Period_Inquiry:
    {
        // 网管信道定期发送数据包事件
        // 抛出下一个20ms事件
        Message *send_msg = MESSAGE_Alloc(node,
                                          MAC_LAYER,
                                          MAC_PROTOCOL_DAATR,
                                          Mac_Management_Channel_Period_Inquiry);
        clocktype delay1 = MANA_SLOT_DURATION * MILLI_SECOND;
        MESSAGE_Send(node, send_msg, delay1);
        Node *node_ptr_to_node; // 指向各节点的指针
        int i = 0;
        if (macdata_daatr->state_now == Mac_Initialization)
        { // 如果当前阶段为建链阶段
            int slot_num = macdata_daatr->mana_slot_should_read;
            macdata_daatr->mana_slot_should_read++;
            if (macdata_daatr->mana_slot_should_read == 25)
            {
                macdata_daatr->mana_slot_should_read = 0;
            }
            macdata_daatr->ptr_to_mana_channel_period_inquiry = send_msg; // 网管信道定期查询事件指针指向该生成事件

            // 如果当前时隙为本节点时隙且为发送飞行状态信息
            if (macdata_daatr->slot_management[slot_num].nodeId == node->nodeId &&
                macdata_daatr->slot_management[slot_num].state == DAATR_STATUS_FLIGHTSTATUS_SEND)
            {
                cout << endl;
                cout << "[建链阶段-MANA] Node " << node->nodeId << " 广播 飞行状态信息 ";
                float time = (float)(node->getNodeTime()) / 1000000;
                cout << "Time : " << time << " ms" << endl;
                // 向本节点物理层传输网管节点参数（发）
                Management_Channel_Information temp_Management_Channel_Information;
                temp_Management_Channel_Information.SendOrReceive = 1;
                temp_Management_Channel_Information.transmitSpeedLevel = 1;
                Message *send_msg0 = MESSAGE_Alloc(node,
                                                   MAC_LAYER,
                                                   MAC_PROTOCOL_DAATR,
                                                   MAC_DAATR_ChannelParameter); // 生成消息
                MESSAGE_PacketAlloc(node, send_msg0, sizeof(temp_Management_Channel_Information), TRACE_DAATR);
                memcpy((Management_Channel_Information *)MESSAGE_ReturnPacket(send_msg0), &temp_Management_Channel_Information, sizeof(Management_Channel_Information));
                MESSAGE_Send(node, send_msg0, 0);
                // 发送飞行状态信息
                for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
                {
                    if (i != node->nodeId)
                    { // 向各节点（除本节点）外发送信息(广播)
                        // 发送自己节点的位置信息
                        Message *send_msg1 = MESSAGE_Alloc(node,
                                                           MAC_LAYER,
                                                           MAC_PROTOCOL_DAATR,
                                                           MSG_MAC_Receivemsg_From_Phy); // 生成消息
                        MacPacket_Daatr macpacket_daatr1;
                        MacHeader2 *mac_header2_ptr = new MacHeader2;
                        // macpacket_daatr1.mac_header = new MacHeader;
                        mac_header2_ptr->PDUtype = 1;
                        mac_header2_ptr->srcAddr = node->nodeId;
                        mac_header2_ptr->destAddr = 0x3ff;                 // 目的地址全1
                        mac_header2_ptr->packetLength = 41 + 1;            // 帧长度
                        mac_header2_ptr->backup = 1;                       // 备用字段为1
                        mac_header2_ptr->fragment_tail_identification = 1; // 分片尾标识
                        FlightStatus flight_sta = macdata_daatr->local_node_position_info;
                        Mana_Packet_Type packet_type;
                        if (macdata_daatr->node_type != Node_Management)
                        {
                            packet_type.type = 4; // 普通节点飞行状态信息通告
                        }
                        else
                        {
                            packet_type.type = 5; // 网管节点飞行状态信息通告
                        }
                        MacDaatr_struct_converter mac_converter1(2);
                        mac_converter1.set_struct((uint8_t *)mac_header2_ptr, 2);
                        mac_converter1.daatr_struct_to_0_1();
                        MacDaatr_struct_converter mac_converter2(3);
                        mac_converter2.set_struct((uint8_t *)&flight_sta, 3);
                        mac_converter2.daatr_struct_to_0_1();
                        MacDaatr_struct_converter mac_converter3(8);
                        mac_converter3.set_struct((uint8_t *)&packet_type, 8);
                        mac_converter3.daatr_struct_to_0_1();
                        mac_converter1 + mac_converter3 + mac_converter2; // 合包
                        uint8_t *frame_ptr = mac_converter1.get_sequence();
                        MESSAGE_PacketAlloc(node, send_msg1, mac_header2_ptr->packetLength, TRACE_DAATR);
                        memcpy((uint8_t *)MESSAGE_ReturnPacket(send_msg1), frame_ptr, mac_header2_ptr->packetLength);
                        node_ptr_to_node = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, i); // 获取该接收节点指针
                        MESSAGE_Send(node_ptr_to_node, send_msg1, 0);
                        delete mac_header2_ptr;
                    }
                }
                macdata_daatr->stats.mac_network_overhead += 42; // 网络开销
            }
            else
            { // 为接收时隙
                // 向本节点物理层传输网管节点参数（发）
                // cout << endl;
                // cout << "[建链阶段-MANA] Node " << node->nodeId << " RX 时隙 ";
                // float time = (float)(node->getNodeTime()) / 1000000;
                // cout << "Time : " << time << " ms" << endl;
                Management_Channel_Information temp_Management_Channel_Information;
                temp_Management_Channel_Information.SendOrReceive = 2;
                temp_Management_Channel_Information.transmitSpeedLevel = 1;
                Message *send_msg0 = MESSAGE_Alloc(node,
                                                   MAC_LAYER,
                                                   MAC_PROTOCOL_DAATR,
                                                   MAC_DAATR_ChannelParameter); // 生成消息
                MESSAGE_PacketAlloc(node, send_msg0, sizeof(temp_Management_Channel_Information), TRACE_DAATR);
                memcpy((Management_Channel_Information *)MESSAGE_ReturnPacket(send_msg0), &temp_Management_Channel_Information,
                       sizeof(Management_Channel_Information));
                MESSAGE_Send(node, send_msg0, 0);
            }
        }
        else
        { // 如果为其他阶段
            int slot_num = macdata_daatr->mana_slot_should_read;
            macdata_daatr->mana_slot_should_read++;
            if (macdata_daatr->mana_slot_should_read == 50)
            {
                macdata_daatr->mana_slot_should_read = 0;
                macdata_daatr->mana_channel_frame_num += 1; // 网管信道已完成一帧, 过去1s
            }
            if (macdata_daatr->slot_management_other_stage[slot_num].nodeId == node->nodeId || macdata_daatr->slot_management_other_stage[slot_num].state == 3)
            { // 此时隙为发送时隙或者此时隙为接入时隙
                Management_Channel_Information temp_Management_Channel_Information;
                temp_Management_Channel_Information.SendOrReceive = 1;
                temp_Management_Channel_Information.transmitSpeedLevel = 1;
                unsigned short slot_state = macdata_daatr->slot_management_other_stage[slot_num].state;
                switch (slot_state)
                {
                case 3:
                { // 接入请求
                    if (macdata_daatr->state_now != Mac_Access)
                    { // 节点未处于接入状态, 不需要广播接入请求
                        // cout << "[接入广播-MANA] Node ID " << node->nodeId << " 未处于接入状态, 非建链阶段不需要广播 网管信道 接入请求 ";
                        float time = (float)(node->getNodeTime()) / 1000000;
                        cout << "Time : " << time << "ms" << endl;
                        temp_Management_Channel_Information.SendOrReceive = 2;
                    }
                    else if (macdata_daatr->access_state == DAATR_NEED_ACCESS && macdata_daatr->state_now == Mac_Access)
                    { // 节点处于接入状态,且未发送接入请求, 需要广播接入请求
                        if (macdata_daatr->access_backoff_number == 0)
                        { // 如果等待退避数目时隙数为0
                            cout << endl;
                            cout << "[接入广播-MANA] Node ID " << node->nodeId << " 处于接入状态, 在此接入时隙 网管信道 广播接入请求 ";
                            float time = (float)(node->getNodeTime()) / 1000000;
                            cout << "Time : " << time << "ms" << endl;
                            for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
                            {
                                if (i != node->nodeId)
                                { // 向各节点（除本节点）外发送信息(广播)
                                    // 发送接入请求
                                    Message *send_msg1 = MESSAGE_Alloc(node,
                                                                       MAC_LAYER,
                                                                       MAC_PROTOCOL_DAATR,
                                                                       MSG_MAC_Receivemsg_From_Phy); // 生成消息
                                    MacPacket_Daatr macpacket_daatr1;
                                    MacHeader2 *mac_header2_ptr = new MacHeader2;
                                    mac_header2_ptr->PDUtype = 1;
                                    mac_header2_ptr->srcAddr = node->nodeId;
                                    mac_header2_ptr->destAddr = macdata_daatr->mana_node; // 目的地址为网管节点
                                    mac_header2_ptr->packetLength = 18;                   // 帧长度17 + 1
                                    mac_header2_ptr->backup = 1;                          // 备用字段为1
                                    mac_header2_ptr->fragment_tail_identification = 1;    // 分片尾标识
                                    Mana_Packet_Type packet_type;
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
                                    MESSAGE_PacketAlloc(node, send_msg1, 50, TRACE_DAATR);
                                    memcpy((uint8_t *)MESSAGE_ReturnPacket(send_msg1), frame_ptr, 50);
                                    node_ptr_to_node = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, i); // 获取接收节点指针
                                    MESSAGE_Send(node_ptr_to_node, send_msg1, 0);
                                    delete mac_header2_ptr;
                                }
                            }
                            macdata_daatr->stats.mac_network_overhead += 18;             // 网络开销
                            macdata_daatr->access_state = DAATR_HAS_SENT_ACCESS_REQUEST; // 断开节点已发送接入请求
                            Message *send_msg_access_waiting_time = MESSAGE_Alloc(node,
                                                                                  MAC_LAYER,
                                                                                  MAC_PROTOCOL_DAATR,
                                                                                  MAC_DAATR_Access_Request_Waiting_Time); // 给等待接入过程一个最大时间
                            MESSAGE_Send(node, send_msg_access_waiting_time, ACCESS_REQUREST_WAITING_MAX_TIME * MILLI_SECOND);
                        }
                        else
                        { // 等待退避数减1
                            macdata_daatr->access_backoff_number--;
                            cout << endl;
                            cout << "[接入广播-MANA] Node ID " << node->nodeId << " 处于退避状态, 还需要退避 "
                                 << (int)macdata_daatr->access_backoff_number << " 接入时隙";
                            float time = (float)(node->getNodeTime()) / 1000000;
                            cout << "Time : " << time << "ms" << endl;
                        }
                    }
                    else if (macdata_daatr->access_state == DAATR_HAS_SENT_ACCESS_REQUEST)
                    {
                        cout << endl;
                        cout << "[接入广播-MANA] Node ID " << node->nodeId << " 处于等待接收网管节点回复接入请求状态 ";
                        float time = (float)(node->getNodeTime()) / 1000000;
                        cout << "Time : " << time << "ms" << endl;
                    }
                    else if (macdata_daatr->access_state == DAATR_WAITING_TO_ACCESS)
                    {
                        cout << endl;
                        cout << "[接入广播-MANA] Node ID " << node->nodeId << " 已收到同意接入回复, 处于等待接收网管节点发送全网跳频图案状态 ";
                        float time = (float)(node->getNodeTime()) / 1000000;
                        cout << "Time : " << time << "ms" << endl;
                    }
                    break;
                }
                case 4:
                { // 接入回复
                    if (macdata_daatr->state_now == Mac_Access)
                    {
                        cout << "处于接入阶段，停止收发" << endl;
                        break;
                    }
                    if (macdata_daatr->access_state == DAATR_WAITING_TO_REPLY && macdata_daatr->state_now == Mac_Execution)
                    { // 如果网管节点有需要回复的节点
                        cout << endl;
                        cout << "[接入广播-MANA] Node ID " << node->nodeId << " 非建链阶段广播 网管信道 接入请求回复 ";
                        float time = (float)(node->getNodeTime()) / 1000000;
                        cout << "Time : " << time << "ms" << endl;
                        vector<uint8_t> *slot_location;
                        slot_location = MacDaatrSrearchSlotLocation(macdata_daatr->waiting_to_access_node, macdata_daatr);
                        mana_access_reply access_reply;
                        for (i = 0; i < FREQUENCY_COUNT; i++)
                        {
                            access_reply.mana_node_hopping[i] = macdata_daatr->frequency_sequence[node->nodeId][i]; // 将网管节点的跳频序列赋值进去
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
                        for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
                        {
                            if (i != node->nodeId)
                            { // 向各节点（除本节点）外发送信息(广播)
                                // 发送接入请求
                                Message *send_msg1 = MESSAGE_Alloc(node,
                                                                   MAC_LAYER,
                                                                   MAC_PROTOCOL_DAATR,
                                                                   MSG_MAC_Receivemsg_From_Phy); // 生成消息
                                MacPacket_Daatr macpacket_daatr1;
                                MacHeader2 *mac_header2_ptr = new MacHeader2;
                                mac_header2_ptr->PDUtype = 1;
                                mac_header2_ptr->srcAddr = node->nodeId;
                                mac_header2_ptr->destAddr = macdata_daatr->waiting_to_access_node; // 目的地址为接入节点
                                mac_header2_ptr->packetLength = 50;                                // 帧长度 17 + 1 + 32
                                mac_header2_ptr->backup = 1;                                       // 备用字段为1
                                mac_header2_ptr->fragment_tail_identification = 1;                 // 分片尾标识
                                Mana_Packet_Type packet_type;
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
                                MESSAGE_PacketAlloc(node, send_msg1, 50, TRACE_DAATR);
                                memcpy((uint8_t *)MESSAGE_ReturnPacket(send_msg1), frame_ptr, 50);
                                node_ptr_to_node = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, i); // 获取接收节点指针
                                MESSAGE_Send(node_ptr_to_node, send_msg1, 0);
                                delete mac_header2_ptr;
                            }
                        }
                        macdata_daatr->stats.mac_network_overhead += 50; // 网络开销
                        delete slot_location;
                        macdata_daatr->access_state = DAATR_WAITING_TO_SEND_HOPPING_PARTTERN;

                        // 生成特殊的MFC数据包, 放入队列首位
                        // int j;
                        // subnet_frequency_parttern subnet_parttern_str;
                        // for (i = 0; i < SUBNET_NODE_NUMBER; i++)
                        //{
                        //    for (j = 0; j < FREQUENCY_COUNT; j++)
                        //    {
                        //        subnet_parttern_str.subnet_node_hopping[i][j] = macdata_daatr->frequency_sequence[i][j];
                        //    }
                        //}
                        // MacDaatr_struct_converter subnet_parttern_ptr(10);
                        // subnet_parttern_ptr.set_struct((uint8_t *)&subnet_parttern_str, 10);
                        // subnet_parttern_ptr.daatr_struct_to_0_1();
                        //// subnet_parttern_ptr.daatr_0_1_to_struct();
                        // uint8_t *bit_seq = subnet_parttern_ptr.get_sequence();
                        //  vector<uint8_t> *SpecData = new vector<uint8_t>;
                        //  for (int i = 0; i < 563; i++)
                        //  {
                        //      SpecData->push_back(bit_seq[i]);
                        //  }

                        // msgFromControl *MFC_temp = new msgFromControl;
                        // MFC_temp->srcAddr = node->nodeId;
                        // MFC_temp->destAddr = macdata_daatr->waiting_to_access_node; // 给网管节点发送
                        // MFC_temp->backup = 1;                                       // 标识本层MFC数据包
                        // MFC_temp->data = (char *)SpecData;
                        // MFC_temp->priority = 0; // 最高优先级
                        // MFC_temp->packetLength = 573;
                        // MFC_temp->msgType = 14;
                        // MFC_temp->fragmentNum = 14; // 以上三项都是15标识频谱感知结果

                        // cout << "节点 " << node->nodeId << " 将结果存入最高优先级的业务队列中. Time: " << (float)(node->getNodeTime()) / 1000000 << endl;
                        // MAC_NetworkLayerHasPacketToSend(node, 0, MFC_temp); // 将此业务添加进业务信道队列

                        // macdata_daatr->waiting_to_access_node = 0;
                    }
                    else if (macdata_daatr->access_state == DAATR_WAITING_TO_REPLY && macdata_daatr->state_now != Mac_Execution)
                    { // 不处于执行阶段, 拒绝接入
                        cout << endl;
                        cout << "[接入广播-MANA] Node ID " << node->nodeId << " 非建链阶段广播 网管信道 拒绝接入请求回复 ";
                        float time = (float)(node->getNodeTime()) / 1000000;
                        cout << "Time : " << time << "ms" << endl;
                        for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
                        {
                            if (i != node->nodeId)
                            { // 向各节点（除本节点）外发送信息(广播)
                                // 发送接入请求
                                Message *send_msg1 = MESSAGE_Alloc(node,
                                                                   MAC_LAYER,
                                                                   MAC_PROTOCOL_DAATR,
                                                                   MSG_MAC_Receivemsg_From_Phy); // 生成消息
                                MacPacket_Daatr macpacket_daatr1;
                                MacHeader2 *mac_header2_ptr = new MacHeader2;
                                mac_header2_ptr->PDUtype = 1;
                                mac_header2_ptr->srcAddr = node->nodeId;
                                mac_header2_ptr->destAddr = macdata_daatr->waiting_to_access_node; // 目的地址为接入节点
                                mac_header2_ptr->packetLength = 18;                                // 帧长度17+1
                                mac_header2_ptr->backup = 1;                                       // 备用字段为1
                                mac_header2_ptr->fragment_tail_identification = 1;                 // 分片尾标识
                                Mana_Packet_Type packet_type;
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
                                MESSAGE_PacketAlloc(node, send_msg1, 50, TRACE_DAATR);
                                memcpy((uint8_t *)MESSAGE_ReturnPacket(send_msg1), frame_ptr, 50);
                                node_ptr_to_node = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, i); // 获取接收节点指针
                                MESSAGE_Send(node_ptr_to_node, send_msg1, 0);
                                delete mac_header2_ptr;
                            }
                        }
                        macdata_daatr->stats.mac_network_overhead += 18; // 网络开销
                        macdata_daatr->waiting_to_access_node = 0;
                    }
                    else
                    { // 网管节点无需回复
                        cout << endl;
                        cout << "无接入节点, 网管节点无需回复 ";
                        float time = (float)(node->getNodeTime()) / 1000000;
                        cout << "Time : " << time << "ms" << endl;
                    }
                    break;
                }
                case 5:
                { // 飞行状态信息通告
                    if (macdata_daatr->state_now == Mac_Access)
                    {
                        cout << "处于接入阶段，停止收发" << endl;
                        break;
                    }
                    cout << endl;
                    cout << "[执行阶段-MANA] Node " << node->nodeId << " 广播 飞行状态信息 ";
                    float time = (float)(node->getNodeTime()) / 1000000;
                    cout << "Time : " << time << " ms" << endl;
                    for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
                    {
                        if (i != node->nodeId)
                        { // 向各节点（除本节点）外发送信息(广播)
                            // 发送自己节点的位置信息
                            Message *send_msg1 = MESSAGE_Alloc(node,
                                                               MAC_LAYER,
                                                               MAC_PROTOCOL_DAATR,
                                                               MSG_MAC_Receivemsg_From_Phy); // 生成消息
                            MacPacket_Daatr macpacket_daatr1;
                            MacHeader2 *mac_header2_ptr = new MacHeader2;
                            // macpacket_daatr1.mac_header = new MacHeader;
                            mac_header2_ptr->PDUtype = 1;
                            mac_header2_ptr->srcAddr = node->nodeId;
                            mac_header2_ptr->destAddr = 0x3ff;                 // 目的地址全1
                            mac_header2_ptr->packetLength = 42;                // 帧长度
                            mac_header2_ptr->backup = 1;                       // 备用字段为1
                            mac_header2_ptr->fragment_tail_identification = 1; // 分片尾标识
                            FlightStatus flight_sta = macdata_daatr->local_node_position_info;
                            Mana_Packet_Type packet_type;
                            if (macdata_daatr->node_type != Node_Management)
                            {
                                packet_type.type = 4; // 普通节点飞行状态信息通告
                            }
                            else
                            {
                                packet_type.type = 5; // 网管节点飞行状态信息通告
                            }
                            MacDaatr_struct_converter mac_converter1(2);
                            mac_converter1.set_struct((uint8_t *)mac_header2_ptr, 2);
                            mac_converter1.daatr_struct_to_0_1();
                            MacDaatr_struct_converter mac_converter2(3);
                            mac_converter2.set_struct((uint8_t *)&flight_sta, 3);
                            mac_converter2.daatr_struct_to_0_1();
                            MacDaatr_struct_converter mac_converter3(8);
                            mac_converter3.set_struct((uint8_t *)&packet_type, 8);
                            mac_converter3.daatr_struct_to_0_1();
                            mac_converter1 + mac_converter3 + mac_converter2; // 合包
                            uint8_t *frame_ptr = mac_converter1.get_sequence();
                            MESSAGE_PacketAlloc(node, send_msg1, mac_header2_ptr->packetLength, TRACE_DAATR);
                            memcpy((uint8_t *)MESSAGE_ReturnPacket(send_msg1), frame_ptr, mac_header2_ptr->packetLength);
                            node_ptr_to_node = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, i); // 获取接收节点指针
                            MESSAGE_Send(node_ptr_to_node, send_msg1, 0);
                            delete mac_header2_ptr;
                        }
                    }
                    macdata_daatr->stats.mac_network_overhead += 42; // 网络开销
                    break;
                }
                case 6:
                {
                    // 网管节点通告消息
                    if (macdata_daatr->state_now == Mac_Access)
                    {
                        cout << "处于接入阶段，停止收发" << endl;
                        break;
                    }
                    cout << endl;
                    cout << "[执行阶段-MANA] Node " << node->nodeId << " 广播 网管节点通告消息 ";
                    float time = (float)(node->getNodeTime()) / 1000000;
                    cout << "Time : " << time << " ms" << endl;
                    // 发送网管节点通告消息
                    int state = 0;
                    if (macdata_daatr->need_change_stage == 0)
                    { // 若当前不转变状态
                        state = 0;
                    }
                    else if (macdata_daatr->need_change_stage == 1)
                    { // 若转变为时隙调整阶段
                        if (macdata_daatr->state_now != Mac_Adjustment_Slot &&
                            macdata_daatr->state_now != Mac_Adjustment_Freqency)
                        {
                            // 若当前状态不为时隙调整阶段且不为时隙调整阶段
                            state = 1;
                            cout << "网管节点改变自己节点 state_now 为 Adjustment_Slot" << endl;
                            macdata_daatr->state_now = Mac_Adjustment_Slot; // 调整网管节点（本节点）为时隙调整阶段
                            macdata_daatr->need_change_stage = 0;           // 已转变状态, 状态位复原
                            macdata_daatr->has_received_slottable = false;  // 初始未收到时隙表
                        }
                    }
                    else if (macdata_daatr->need_change_stage == 2)
                    { // 若转变为频率调整阶段
                        if (macdata_daatr->state_now != Mac_Adjustment_Slot && macdata_daatr->state_now != Mac_Adjustment_Freqency)
                        { // 若当前状态不为时隙调整阶段且不为时隙调整阶段
                            state = 2;
                            cout << "网管节点改变自己节点 state_now 为 Mac_Adjustment_Freqency" << endl;
                            macdata_daatr->state_now = Mac_Adjustment_Freqency; // 调整网管节点（本节点）为频率调整阶段
                            macdata_daatr->need_change_stage = 0;               // 已转变状态, 状态位复原
                            macdata_daatr->has_received_sequence = false;
                        }
                    }
                    else if (macdata_daatr->need_change_stage == 3)
                    {                                                   // 若在转变时隙调整阶段过程中发生频率调整阶段
                        macdata_daatr->state_now = Mac_Adjustment_Slot; // 调整网管节点（本节点）为时隙调整阶段
                        state = 1;
                        macdata_daatr->need_change_stage = 2; // 转变成需要进入频率调整阶段
                    }
                    else if (macdata_daatr->need_change_stage == 4)
                    {                                                       // 若在转变频率调整阶段过程中发生时隙调整阶段, 需要先进入频率调整阶段
                        macdata_daatr->state_now = Mac_Adjustment_Freqency; // 调整网管节点（本节点）为时隙调整阶段
                        state = 2;
                        macdata_daatr->need_change_stage = 1; // 转变成需要进入频率调整阶段
                    }
                    for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
                    {
                        if (i != node->nodeId)
                        { // 向各节点（除本节点）外发送信息
                          // 发送网管节点通告消息
                            Message *send_msg1 = MESSAGE_Alloc(node,
                                                               MAC_LAYER,
                                                               MAC_PROTOCOL_DAATR,
                                                               MSG_MAC_Receivemsg_From_Phy); // 生成消息
                            MacPacket_Daatr macpacket_daatr1;
                            MacHeader2 *mac_header2_ptr = new MacHeader2;
                            // macpacket_daatr1.mac_header = new MacHeader;
                            mac_header2_ptr->PDUtype = 1;
                            mac_header2_ptr->srcAddr = node->nodeId;
                            mac_header2_ptr->destAddr = 0x3ff;                 // 目的地址全1
                            mac_header2_ptr->packetLength = 18 + 1;            // 帧长度
                            mac_header2_ptr->backup = 1;                       // 链路层消息
                            mac_header2_ptr->fragment_tail_identification = 1; // 分片尾标识
                            if_need_change_state change_state;
                            change_state.state = state;
                            Mana_Packet_Type packet_type;
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
                            MESSAGE_PacketAlloc(node, send_msg1, mac_header2_ptr->packetLength, TRACE_DAATR);
                            memcpy((uint8_t *)MESSAGE_ReturnPacket(send_msg1), frame_ptr, mac_header2_ptr->packetLength);
                            // 获取该接收节点指针
                            node_ptr_to_node = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, i);
                            MESSAGE_Send(node_ptr_to_node, send_msg1, 0);
                            delete mac_header2_ptr;
                        }
                    }
                    macdata_daatr->stats.mac_network_overhead += 19; // 网络开销
                    break;
                }
                case 7:
                { // 网络层业务发送
                    if (macdata_daatr->state_now == Mac_Access)
                    {
                        cout << "处于接入阶段，停止收发" << endl;
                        break;
                    }
                    cout << endl;
                    cout << "[执行阶段-MANA] Node ID " << node->nodeId << " 广播 网络层控制消息 ";
                    float time = (float)(node->getNodeTime()) / 1000000;
                    cout << "Time : " << time << " ms" << endl;
                    msgFromControl buss_to_be_sent;
                    bool flag;
                    flag = getLowChannelBusinessFromQueue(&buss_to_be_sent, node, macdata_daatr);
                    if (flag == true)
                    { // 如果业务队列不为空, 有待发送业务
                        int mfc_length = buss_to_be_sent.packetLength;
                        int packet_length = 0;
                        packet_length = mfc_length + MAC_HEADER2_LENGTH; // 总包长(单位：byte)
                        for (i = 1; i <= SUBNET_NODE_NUMBER; i++)
                        {
                            if (i != node->nodeId)
                            { // 向各节点（除本节点）外发送信息
                              // 发送网管节点通告消息
                                Message *send_msg1 = MESSAGE_Alloc(node,
                                                                   MAC_LAYER,
                                                                   MAC_PROTOCOL_DAATR,
                                                                   MSG_MAC_Receivemsg_From_Phy); // 生成消息
                                MacPacket_Daatr macpacket_daatr1;
                                MacHeader2 *mac_header2_ptr = new MacHeader2;
                                mac_header2_ptr->PDUtype = 1;
                                mac_header2_ptr->srcAddr = node->nodeId;
                                mac_header2_ptr->destAddr = 0x3ff;                 // 目的地址全1
                                mac_header2_ptr->packetLength = packet_length;     // 帧长度
                                mac_header2_ptr->backup = 0;                       // 备用字段为0,网络层业务
                                mac_header2_ptr->fragment_tail_identification = 1; // 分片尾标识
                                vector<uint8_t> *MFC_Trans_temp;
                                MFC_Trans_temp = PackMsgFromControl(&buss_to_be_sent); // 将MFC转换为01序列(使用vector盛放)
                                MacDaatr_struct_converter mac_converter1(2);
                                mac_converter1.set_struct((uint8_t *)mac_header2_ptr, 2);
                                mac_converter1.daatr_struct_to_0_1();
                                MacDaatr_struct_converter mac_converter2(255); // 设定混合类型的converter, 用来容纳MFC转换的01序列的seq
                                mac_converter2.set_length(buss_to_be_sent.packetLength);
                                mac_converter2.daatr_MFCvector_to_0_1(MFC_Trans_temp, buss_to_be_sent.packetLength);
                                mac_converter1 + mac_converter2; // 合包
                                uint8_t *frame_ptr = mac_converter1.get_sequence();
                                MESSAGE_PacketAlloc(node, send_msg1, mac_header2_ptr->packetLength, TRACE_DAATR);
                                memcpy((uint8_t *)MESSAGE_ReturnPacket(send_msg1), frame_ptr, mac_header2_ptr->packetLength);
                                // 获取该接收节点指针
                                node_ptr_to_node = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, i);
                                MESSAGE_Send(node_ptr_to_node, send_msg1, 0);
                                delete MFC_Trans_temp;
                                delete mac_header2_ptr;
                            }
                        }
                        // 向网络层通知全网通告消息已发送
                        IdentityStatus inditity_message;
                        inditity_message.isBroadcast = true;
                        inditity_message.seq = buss_to_be_sent.seq;
                        Message *send_inditity = MESSAGE_Alloc(node,
                                                               NETWORK_LAYER,
                                                               NETWORK_PROTOCOL_IDENTITYUPDATEANDMAINTENANCE,
                                                               MSG_NETWORK_IDENTITYUPDATEANDMAINTENANCE_ReceiveIdentityStatus); // 生成消息
                        MESSAGE_PacketAlloc(node, send_inditity, sizeof(IdentityStatus), TRACE_DAATR);
                        memcpy((IdentityStatus *)MESSAGE_ReturnPacket(send_inditity), &inditity_message, sizeof(IdentityStatus));
                        MESSAGE_Send(node, send_inditity, 0);
                    }
                    else
                    { // 如果业务队列为空, 则不发送
                        cout << "网管信道队列为空, 无业务发送" << endl;
                    }
                    break;
                }
                }
                // 发送网管信道参数
                Message *send_msg0 = MESSAGE_Alloc(node,
                                                   MAC_LAYER,
                                                   MAC_PROTOCOL_DAATR,
                                                   MAC_DAATR_ChannelParameter); // 生成消息
                MESSAGE_PacketAlloc(node, send_msg0, sizeof(temp_Management_Channel_Information), TRACE_DAATR);
                memcpy((Management_Channel_Information *)MESSAGE_ReturnPacket(send_msg0), &temp_Management_Channel_Information,
                       sizeof(Management_Channel_Information));
                MESSAGE_Send(node, send_msg0, 0);

                // system("pause");
            }
            else
            { // 为接收时隙
                // 向本节点物理层传输网管节点参数（收）
                Management_Channel_Information temp_Management_Channel_Information;
                temp_Management_Channel_Information.SendOrReceive = 2;
                temp_Management_Channel_Information.transmitSpeedLevel = 1;
                Message *send_msg0 = MESSAGE_Alloc(node,
                                                   MAC_LAYER,
                                                   MAC_PROTOCOL_DAATR,
                                                   MAC_DAATR_ChannelParameter); // 生成消息
                MESSAGE_PacketAlloc(node, send_msg0, sizeof(temp_Management_Channel_Information), TRACE_DAATR);
                memcpy((Management_Channel_Information *)MESSAGE_ReturnPacket(send_msg0), &temp_Management_Channel_Information,
                       sizeof(Management_Channel_Information));
                MESSAGE_Send(node, send_msg0, 0);
            }
        }
        MESSAGE_Free(node, msg); // 释放原有事件
                                 // system("pause");
        break;
    }

    case MAC_DAATR_Access_Request_Waiting_Time:
    {
        if (macdata_daatr->access_state == DAATR_HAS_SENT_ACCESS_REQUEST)
        { // 如果节点尚未收到网管节点请求接入回复
            cout << endl;
            cout << "Node " << node->nodeId << " ";
            cout << "等待接入请求回复时间超过阈值, 结束等待阶段, 重新进入调整状态, 执行随机退避策略" << endl;
            int num = rand() % (MAX_RANDOM_BACKOFF_NUM + 1);
            macdata_daatr->access_state = DAATR_NEED_ACCESS;
            macdata_daatr->access_backoff_number = num;
        }
        MESSAGE_Free(node, msg);
        break;
    }

    case MAC_DAATR_Subnet_Frequency_Waiting_Time:
    {
        if (macdata_daatr->access_state == DAATR_WAITING_TO_ACCESS)
        { // 如果节点已收到同意接入, 但是未收到全网跳频图案, 此时可能出现丢包, 重新进入接入状态
            cout << endl;
            cout << "Node " << node->nodeId << " ";
            cout << "等待全网跳频图案接收时间时间超过阈值, 重新进入接入状态" << endl;
            macdata_daatr->access_state = DAATR_NEED_ACCESS;
        }
        MESSAGE_Free(node, msg);
        break;
    }

    // 涉及 业务信道根据时隙表进行发送的操作
    case MSG_MAC_Traffic_Channel_Period_Inquiry:
    {
        int i, j, k;
        double trans_speed, trans_data;
        int current_slot_Id = macdata_daatr->currentSlotId;
        int current_slot_state = macdata_daatr->slot_traffic_execution[current_slot_Id].state;
        bool all_link_flag = false;
        if (current_slot_Id % 20 < 8 && current_slot_Id % 20 > 15 && current_slot_Id < 62)
        {
            all_link_flag = true;
        }
        int slot_available = 1;
        int multi = 0;
        int skip_pkt = 0; // 表示这是大数据包(的个数), 无法放入此PDU, 跳过

        vector<msgFromControl> MFC_list_temp;

        // state_now 由节点的网管信道接收对应msg并更改相应标志位
        // 业务信道只需查看即可
        if (macdata_daatr->state_now == Mac_Execution || macdata_daatr->state_now == Mac_Initialization)
        {
            if (current_slot_state == DAATR_STATUS_TX)
            {
                // 业务信道此时发送
                if (DEBUG)
                {
                    // printf("Node %d traffic slot %d is in [send] slot to Node %d !!\n", node->nodeId, current_slot_Id, macdata_daatr->slot_traffic_execution[current_slot_Id].send_or_recv_node);
                    cout << "[SEND Slot " << current_slot_Id << "] " << node->nodeId << "->" << macdata_daatr->slot_traffic_execution[current_slot_Id].send_or_recv_node;
                    cout << "  Time: [" << (float)(node->getNodeTime()) / 1000000 << "]" << endl;
                }
                int dest_node = macdata_daatr->slot_traffic_execution[current_slot_Id].send_or_recv_node;
                if (dest_node == 0)
                {
                    MacDaatrUpdateTimer(node, macdata_daatr);
                    break;
                }
                /*if (macdata_daatr->Forwarding_Table[dest_node - 1][1] == 0 &&
                    macdata_daatr->traffic_channel_business[dest_node - 1].business[0][0].busin_data.backup == 0)*/
                if (dest_node > SUBNET_NODE_NUMBER)
                {
                    // cout << "到节点 " << dest_node << " 无链路可用,该节点转发变为空, 不再发送" << endl;
                    MacDaatrUpdateTimer(node, macdata_daatr);
                    // system("pause");
                    break;
                }
                if (macdata_daatr->access_state == DAATR_WAITING_TO_SEND_HOPPING_PARTTERN && macdata_daatr->waiting_to_access_node == dest_node && all_link_flag == true)
                // 接入阶段 向新节点发送新跳频图案
                {
                    int j;
                    subnet_frequency_parttern subnet_parttern_str;
                    for (i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
                    {
                        for (j = 0; j < FREQUENCY_COUNT; j++)
                        {
                            subnet_parttern_str.subnet_node_hopping[i][j] = macdata_daatr->frequency_sequence[i][j];
                        }
                    }
                    MacDaatr_struct_converter subnet_parttern_ptr(10);
                    subnet_parttern_ptr.set_struct((uint8_t *)&subnet_parttern_str, 10);
                    subnet_parttern_ptr.daatr_struct_to_0_1();
                    // subnet_parttern_ptr.daatr_0_1_to_struct();
                    uint8_t *bit_seq = subnet_parttern_ptr.get_sequence();

                    MacHeader *mac_header = new MacHeader;
                    mac_header->is_mac_layer = 0;
                    mac_header->backup = 1;
                    mac_header->PDUtype = 0;
                    mac_header->packetLength = 563 + 25; // MFC_Spec + 包头25
                    mac_header->destAddr = macdata_daatr->waiting_to_access_node;
                    mac_header->srcAddr = node->nodeId;
                    MacDaatr_struct_converter mac_converter1(1);
                    mac_converter1.set_struct((uint8_t *)mac_header, 1);
                    mac_converter1.daatr_struct_to_0_1();

                    mac_converter1 + subnet_parttern_ptr;
                    uint8_t *frame_ptr = mac_converter1.get_sequence();

                    Message *send_msg = MESSAGE_Alloc(node,
                                                      MAC_LAYER,
                                                      MAC_PROTOCOL_DAATR,
                                                      MSG_MAC_SendAccessNewSeqence); // 生成消息
                    MESSAGE_PacketAlloc(node, send_msg, mac_header->packetLength, TRACE_DAATR);
                    memcpy((uint8_t *)MESSAGE_ReturnPacket(send_msg), frame_ptr, mac_header->packetLength);
                    Node *dest_node_ptr = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, macdata_daatr->waiting_to_access_node);
                    MESSAGE_Send(dest_node_ptr, send_msg, 0);

                    macdata_daatr->access_state = DAATR_NO_NEED_TO_ACCESS;
                    macdata_daatr->waiting_to_access_node = 0;
                }

                // 正式运行用上面这行
                // int dest_node = macdata_daatr->traffic_channel_business[2 - 1].recv_node;
                // cout << "dest_node: " << dest_node << endl;
                trans_speed = calculate_slot_type_mana(macdata_daatr->traffic_channel_business[dest_node - 1].distance);
                trans_data = trans_speed * TRAFFIC_SLOT_DURATION / 8;

                trans_data -= MAC_HEADER1_LENGTH; // 减去PDU1的包头数据量, 才是真正一帧可传输的数据量
                // printf("per slot data speed: %f KByte/s\n", trans_data); // 测试用
                for (i = 0; i < TRAFFIC_CHANNEL_PRIORITY; i++)
                {
                    // i对应不同优先级
                    skip_pkt = 0;
                    for (j = 0; j < TRAFFIC_MAX_BUSINESS_NUMBER; j++)
                    {
                        if (macdata_daatr->traffic_channel_business[dest_node - 1].business[i][skip_pkt].priority == -1)
                        {
                            // cout << "traffic channel of pri " << i << " is empty" << endl;
                            break;
                        }

                        if (macdata_daatr->traffic_channel_business[dest_node - 1].business[i][0].busin_data.backup == 0)
                        // 第一个可发送业务是网络层下发的业务
                        // 向后遍历每一个业务, 查看1.是否继续发送会大于可传输量 2.是否下一个业务是链路层业务
                        // 有任何一个条件不满足, 则停止遍历, 并将此前的所有业务打包发送
                        {
                            if ((trans_data >= macdata_daatr->traffic_channel_business[dest_node - 1].business[i][skip_pkt].data_kbyte_sum) &&
                                judge_if_include_ReTrans(macdata_daatr->traffic_channel_business[dest_node - 1].business[i][skip_pkt].busin_data, MFC_list_temp) == false)
                            {
                                trans_data -= macdata_daatr->traffic_channel_business[dest_node - 1].business[i][skip_pkt].data_kbyte_sum;
                                multi += 1;
                                MFC_list_temp.push_back(macdata_daatr->traffic_channel_business[dest_node - 1].business[i][skip_pkt].busin_data);
                                traffic_business_forward_lead(macdata_daatr, dest_node, i, skip_pkt); // 此业务发送成功, 更新当前业务队列
                            }
                            else
                            {
                                skip_pkt += 1;
                                continue;
                            }
                        }
                        else
                        // 队列中第一个业务是链路层自行下发的业务
                        // 此时的macpacket的包头应该和mfc包头的destAddr是一致的
                        {
                            msgFromControl busin_data = macdata_daatr->traffic_channel_business[dest_node - 1].business[i][0].busin_data;
                            if (busin_data.packetLength == 73 && busin_data.msgType == 15 && busin_data.fragmentNum == 15) // 说明待发送的是频谱感知结果
                            {
                                Message *spectrum_msg = MESSAGE_Alloc(node,
                                                                      MAC_LAYER,
                                                                      MAC_PROTOCOL_DAATR,
                                                                      MAC_DAATR_Mana_Node_Recv_Spec_Sensing); // 生成消息

                                // 设置包头
                                MacHeader *mac_header = new MacHeader;
                                mac_header->is_mac_layer = 0;
                                mac_header->backup = 1;
                                cout << "节点 " << node->nodeId << " 向网管节点发送频谱感知结果. Time: " << (float)(node->getNodeTime()) / 1000000 << endl;
                                mac_header->PDUtype = 0;
                                mac_header->packetLength = 73 + 25; // MFC_Spec + 包头25
                                mac_header->destAddr = 1;
                                mac_header->srcAddr = node->nodeId;

                                // 包头的 converter
                                MacDaatr_struct_converter mac_converter_PDU1(1);
                                mac_converter_PDU1.set_struct((uint8_t *)mac_header, 1);
                                mac_converter_PDU1.daatr_struct_to_0_1();

                                // 包头后的内容
                                vector<uint8_t> *MFC_Spec_temp = new vector<uint8_t>;                              // 存储盛放频谱感知结果的MFC的vector序列
                                MFC_Spec_temp = PackMsgFromControl(&busin_data);                                   // 将MFC转换为 01 seq, 使用vector存放
                                MacDaatr_struct_converter mac_converter_Spec(255);                                 // 设定converter, 类别为255
                                                                                                                   // (由于没有header + MFC的type, 所以设为255)
                                mac_converter_Spec.set_length(busin_data.packetLength);                            // 设定此converter的长度为MFC的长度 73（固定长度）
                                mac_converter_Spec.daatr_MFCvector_to_0_1(MFC_Spec_temp, busin_data.packetLength); // 将MFC的vector加入converter的seq中

                                mac_converter_PDU1 + mac_converter_Spec; // 将两个包拼接进 PDU

                                uint8_t *frame_ptr = mac_converter_PDU1.get_sequence(); // 将拼接好的PDU的seq提出 待发送
                                MESSAGE_PacketAlloc(node, spectrum_msg, mac_header->packetLength, TRACE_DAATR);
                                memcpy(MESSAGE_ReturnPacket(spectrum_msg), frame_ptr, mac_header->packetLength);
                                Node *node_ptr_to_node = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, macdata_daatr->mana_node); // 获取该接收节点指针
                                traffic_business_forward_lead(macdata_daatr, dest_node, i, 0);

                                Attach_Status_Information_To_Packet(node, node_ptr_to_node, macdata_daatr, DAATR_STATUS_TX, 0);
                                MESSAGE_Send(node_ptr_to_node, spectrum_msg, 0);
                            }
                            // else if (busin_data.packetLength == 573 && busin_data.msgType == 14 && busin_data.fragmentNum == 14)
                            // {

                            // }
                            else
                            {
                                Message *data_msg = MESSAGE_Alloc(node,
                                                                  MAC_LAYER,
                                                                  MAC_PROTOCOL_DAATR,
                                                                  MSG_MAC_Control_Transmitting);

                                MacPacket_Daatr *macdata_pkt = new MacPacket_Daatr;
                                macdata_pkt->mac_header = new MacHeader;
                                macdata_pkt->mac_header->PDUtype = 0;
                                macdata_pkt->mac_header->is_mac_layer = 0;
                                macdata_pkt->mac_header->backup = 1;
                                macdata_pkt->mac_header->srcAddr = node->nodeId;

                                // 这里是直接从队列中拿出的, 所以不用查路由表
                                macdata_pkt->mac_header->destAddr = dest_node;
                                macdata_pkt->busin_data = busin_data;
                                cout << "busin_data.priority: " << busin_data.priority << endl;
                                cout << "busin_data.backup: " << busin_data.backup << endl;
                                cout << "busin_data.packetLength: " << busin_data.packetLength << endl;
                                cout << "busin_data.destAddr: " << busin_data.destAddr << endl;

                                int data_msg_packetSize = sizeof(MacPacket_Daatr);
                                MESSAGE_PacketAlloc(node, data_msg, data_msg_packetSize, TRACE_DAATR);
                                memcpy(MESSAGE_ReturnPacket(data_msg), macdata_pkt, sizeof(MacPacket_Daatr));
                                // 查询路由表
                                Node *dest_node_struct = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, dest_node); // 获取该接收节点指针
                                traffic_business_forward_lead(macdata_daatr, dest_node, i, 0);

                                Attach_Status_Information_To_Packet(node, dest_node_struct, macdata_daatr, DAATR_STATUS_TX, 0);
                                MESSAGE_Send(dest_node_struct, data_msg, 0);
                            }
                        }
                    }
                }

                if (MFC_list_temp.size() != 0)
                {
                    Message *data_msg = MESSAGE_Alloc(node,
                                                      MAC_LAYER,
                                                      MAC_PROTOCOL_DAATR,
                                                      MSG_MAC_Data_Transmitting);

                    MacHeader *mac_header = new MacHeader;
                    mac_header->is_mac_layer = 0;
                    mac_header->mac_backup = multi;
                    // cout << "节点 " << node->nodeId << " 向节点 " << dest_node << " 本次发送合包 " << multi << " 个" << endl;
                    mac_header->PDUtype = 0;
                    mac_header->packetLength = 25; // 设定PDU包头的初始长度
                    mac_header->destAddr = dest_node;
                    mac_header->srcAddr = node->nodeId;

                    vector<msgFromControl>::iterator mfc_t;
                    // 首先遍历所有待发送的MFC, 计算即将生成的PDU的长度, 更新包头位置的pktlength字段
                    for (mfc_t = MFC_list_temp.begin(); mfc_t != MFC_list_temp.end(); mfc_t++)
                    {
                        mac_header->packetLength += mfc_t->packetLength;
                    }

                    MacDaatr_struct_converter mac_converter_PDU1(1);         // 此 converter 用来盛放包头, 设置类别为 1
                    mac_converter_PDU1.set_struct((uint8_t *)mac_header, 1); // 将上述包头与converter相关联
                    mac_converter_PDU1.daatr_struct_to_0_1();                // 将包头转换为01序列

                    vector<msgFromControl>::iterator mfc_temp;             // 遍历待发送队列中的每个MFC
                    vector<uint8_t> *MFC_Trans_temp = new vector<uint8_t>; // 盛放MFC转换的01序列
                    int PDU_TotalLength = mac_converter_PDU1.get_length(); // PDU_TotalLength == mac_header->packetLength
                    for (mfc_temp = MFC_list_temp.begin(); mfc_temp != MFC_list_temp.end(); mfc_temp++)
                    {
                        MFC_Trans_temp = PackMsgFromControl(&(*mfc_temp)); // 将MFC转换为01序列(使用vector盛放)
                        MacDaatr_struct_converter mac_converter_temp(255); // 设定混合类型的converter, 用来容纳MFC转换的01序列的seq
                        mac_converter_temp.set_length(mfc_temp->packetLength);
                        mac_converter_temp.daatr_MFCvector_to_0_1(MFC_Trans_temp, mfc_temp->packetLength);

                        mac_converter_PDU1 + mac_converter_temp; // 将新生成的MFC的seq添加至PDU1包头之后
                        PDU_TotalLength += mfc_temp->packetLength;
                        MFC_Trans_temp->clear();
                        macdata_daatr->stats.traffic_output += (*mfc_temp).packetLength;
                        /*if((*mfc_temp).srcAddr == 0)
                        {macdata_daatr->stats.msgtype3_sent1++;}
                        if((*mfc_temp).destAddr == 0)
                        {macdata_daatr->stats.msgtype3_sent1++;}*/
                        // else if((*mfc_temp).msgType == 3 && dest_node == 2)
                        //{macdata_daatr->stats.msgtype3_sent2++;}
                        // else if((*mfc_temp).msgType == 3 && dest_node == 3)
                        //{macdata_daatr->stats.msgtype3_sent3++;}
                        macdata_daatr->stats.UDU_SentUnicast++;
                    }
                    MFC_list_temp.clear(); // 清空待发送队列

                    mac_converter_PDU1.set_length(PDU_TotalLength);
                    uint8_t *frame_ptr = mac_converter_PDU1.get_sequence();
                    MESSAGE_PacketAlloc(node, data_msg, PDU_TotalLength, TRACE_DAATR);
                    memcpy(MESSAGE_ReturnPacket(data_msg), frame_ptr, PDU_TotalLength); // 将首地址与msg相关联

                    // // 加入查询路由表的行为
                    Node *dest_node_struct = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, dest_node);

                    Attach_Status_Information_To_Packet(node, dest_node_struct, macdata_daatr, DAATR_STATUS_TX, 0);
                    MESSAGE_Send(dest_node_struct, data_msg, 0);
                    macdata_daatr->stats.PDU_SentUnicast++;
                    //  cout << "节点 " << node->nodeId << " 结束当前时隙发送任务, 调整业务队列的等待时间与优先级" << endl;
                }

                for (i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
                {
                    for (j = 0; j < TRAFFIC_CHANNEL_PRIORITY; j++)
                    {
                        for (k = 0; k < TRAFFIC_MAX_BUSINESS_NUMBER; k++)
                        {
                            if (macdata_daatr->traffic_channel_business[i].business[j][k].priority != -1)
                            {
                                macdata_daatr->traffic_channel_business[i].business[j][k].waiting_time += 1;
                            }
                        }
                    }
                }

                MFC_Priority_Adjustment(macdata_daatr);
            }

            else if (current_slot_state == DAATR_STATUS_RX)
            {
                // 业务信道接收
                if (DEBUG)
                {
                    cout << "[RECV Slot " << current_slot_Id << "] " << node->nodeId << "<-" << macdata_daatr->slot_traffic_execution[current_slot_Id].send_or_recv_node;
                    cout << "  Time: [" << (float)(node->getNodeTime()) / 1000000 << "]" << endl;
                }

                int dest_node = macdata_daatr->slot_traffic_execution[current_slot_Id].send_or_recv_node;
                if (dest_node == 0 || dest_node > SUBNET_NODE_NUMBER)
                {
                    MacDaatrUpdateTimer(node, macdata_daatr);
                    break;
                }

                Node *dest_node_struct = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, dest_node);
                Attach_Status_Information_To_Packet(node, dest_node_struct, macdata_daatr, DAATR_STATUS_RX, 0);
            }

            else
            {
                // 不收也不发
                if (DEBUG)
                {
                    printf("Node %d traffic channel %d is in IDLE slot\n", node->nodeId, current_slot_Id);
                }
            }

            MacDaatrUpdateTimer(node, macdata_daatr);
        }

        else if (macdata_daatr->state_now == Mac_Adjustment_Slot)
        {
            if (macdata_daatr->link_build_state == 0) // 在建链阶段读取完时隙表
            {
                MESSAGE_CancelSelfMsg(node, macdata_daatr->timerMsg);
                cout << "节点 " << node->nodeId << " 取消原有定时器事件, 等待进入执行阶段. Time: "
                     << (float)(node->getNodeTime()) / 1000000 << endl;
            }
            else
            {
                // 节点在 20 520 收到网管节点的通告消息, 更改自己的state_now
                MESSAGE_CancelSelfMsg(node, macdata_daatr->timerMsg);
                cout << "节点 " << node->nodeId << " 进入时域调整阶段, 取消原有定时器事件, Time: "
                     << (float)(node->getNodeTime()) / 1000000 << endl;
                // 调整波束指向 指向网管节点 等待新时隙表的发送
            }
        }

        else if (macdata_daatr->state_now == Mac_Adjustment_Freqency)
        {

            cout << "节点 " << node->nodeId << " 进入频率调整阶段, Time: " << (float)(node->getNodeTime()) / 1000000 << endl;
            MESSAGE_CancelSelfMsg(node, macdata_daatr->timerMsg);
        }

        else if (macdata_daatr->state_now == Mac_Access)
        {
            cout << "节点 " << node->nodeId << " 进入接入状态, Time: " << (float)(node->getNodeTime()) / 1000000 << endl;
            macdata_daatr->stats.access_begin_time = (node->getNodeTime()) / 1000000; // 随遇接入时间
            MacDaatrUpdateTimer(node, macdata_daatr);
            // MESSAGE_CancelSelfMsg(node, macdata_daatr->timerMsg);
        }

        else
        {
            cout << "now in this state" << endl;
            // system("pause");
        }
        break;
    }

    // 只有网管节点(nodeId == 1)可以收到这个事件
    // 由网管节点的网络层下发
    case MAC_DAATR_ReceiveTaskInstruct:
    {
        // 1. 收到此消息后判断接收节点是否为网管节点
        // 2. 网管节点通过网管信道广播消息, 通知子网内所有节点停止收发业务
        // 3. 各节点在收到上述广播消息后, 停止自己的业务发送, 清除原有的定时器事件, 等待网管节点下发新的时隙表
        // 4. 在广播的期间, 网管节点同时根据网络层下发的LA生成新的时隙表和各节点的分配表
        // 5. 等待网管信道一帧时长之后, 认为所有节点都已收到上述消息, 开始按照调整阶段的时隙表对生成的时隙表进行发送(使用业务信道)
        // 6. 各节点在收到时隙表后, 将其存储在自己的macdaatr结构体中, 并重新根据新时隙表设定定时器等, 随后开始按照时隙表收发
        cout << endl;
        cout << "网管节点收到上层链路构建需求!";
        cout << " Current time: " << (float)(node->getNodeTime()) / 1000000 << endl;

        if (node->nodeId != macdata_daatr->mana_node)
        {
            cout << "Incorrect Receiving Node! " << endl;
            // system("pause");
        }

        // 此时网管节点改变自己的daatr结构体中的标志位
        if (macdata_daatr->need_change_stage == 0 &&
            macdata_daatr->state_now != Mac_Adjustment_Slot &&
            macdata_daatr->state_now != Mac_Adjustment_Freqency)
        {
            // 当前状态稳定, 在这之前没有发生要调整状态的事件, 且不处于任何调整状态
            cout << "子网各节点即将进入时隙调整阶段!" << endl;
            macdata_daatr->need_change_stage = 1; // 即将进入时隙调整阶段
        }
        else if (macdata_daatr->need_change_stage == 2)
        {
            // 在这之前已经发生了要进入频率调整阶段, 但还未广播此信息, 也即未进入
            cout << "首先进入频率调整阶段, 在时隙调整阶段结束后进入时隙调整阶段! " << endl;
            macdata_daatr->need_change_stage = 4; // 设置为首先进入频率调整阶段, 在时隙调整阶段结束后进入时隙调整阶段
        }

        // 在网管节点的网管信道下一发送时隙到来时 向所有节点发送MSG_MAC_Receivemsg_From_Phy事件
        // 在消息数据包的备用字段中注明消息类型为 停止业务发送...
        // 子网内其他节点在收到此消息后完成相应操作 等待网管节点发送时隙表
        LinkAssignmentHeader *link_assignment_header = (LinkAssignmentHeader *)MESSAGE_ReturnPacket(msg);
        LinkAssignment *LintAssignment_data = (LinkAssignment *)(link_assignment_header + 1);
        int i, j, k;

        vector<LinkAssignment_single> LinkAssignment_Storage;
        // 将网络层发来的若干Linkassignment结构体(数组)分解为单个链路构建需求结构体 LinkAssignment_single_temp
        // 并将其存储进 LinkAssignment_Storage 内

        for (i = 0; i < link_assignment_header->linkNum; i++)
        {
            for (j = 0; j < (*(LintAssignment_data + i)).listNumber; j++)
            {
                LinkAssignment_single LinkAssignment_single_temp;
                LinkAssignment_single_temp.begin_node = (*(LintAssignment_data + i)).begin_node;
                LinkAssignment_single_temp.end_node = (*(LintAssignment_data + i)).end_node;
                LinkAssignment_single_temp.type = (*(LintAssignment_data + i)).type[j];
                LinkAssignment_single_temp.priority = (*(LintAssignment_data + i)).priority[j];
                LinkAssignment_single_temp.size = (*(LintAssignment_data + i)).size[j];

                LinkAssignment_single_temp.QOS = (*(LintAssignment_data + i)).QOS[j];
                LinkAssignment_single_temp.begin_time[3] = (*(LintAssignment_data + i)).begin_time[3][j];
                LinkAssignment_single_temp.end_time[3] = (*(LintAssignment_data + i)).end_time[3][j];
                LinkAssignment_single_temp.frequency = (*(LintAssignment_data + i)).frequency[j];

                int LA_num = 1;
                // cout << "number of LA_temp is " << LA_num << endl;
                for (k = 0; k < LA_num; k++)
                {
                    if (LinkAssignment_single_temp.size != 0)
                    {
                        LinkAssignment_Storage.push_back(LinkAssignment_single_temp);
                    }
                }
            }
        }

        vector<LinkAssignment_single>::iterator la;
        cout << "LinkAssignment_Storage Size: " << LinkAssignment_Storage.size() << endl;

        sort(LinkAssignment_Storage.begin(), LinkAssignment_Storage.end(), Compare_by_priority);
        // for (la = LinkAssignment_Storage.begin(); la != LinkAssignment_Storage.end(); la++)
        // {
        //      cout << (*la).begin_node << "->" << (*la).end_node << ": " << (*la).priority << endl;
        // }
        // cout << endl;

        // Alloc_slottable_traffic是网管节点生成的分配表
        Alloc_slot_traffic *Alloc_slottable_traffic = (Alloc_slot_traffic *)MEM_malloc(sizeof(Alloc_slot_traffic));
        Alloc_slottable_traffic = ReAllocate_Traffic_slottable(node, macdata_daatr, LinkAssignment_Storage);

        // 下面需要将分配表转换为各节点对应的时隙表
        // mana_slot_traffic slot_traffic_execution_new[SUBNET_NODE_NUMBER][TRAFFIC_SLOT_NUMBER];

        memset(&(macdata_daatr->slot_traffic_execution_new), 0, sizeof(mana_slot_traffic) * SUBNET_NODE_FREQ_NUMBER * TRAFFIC_SLOT_NUMBER);

        // **mana_slot_traffic_new = Generate_mana_slot_traffic(Alloc_slottable_traffic);
        // 将下发给各节点的时隙表进行初始化
        // 将 state 置为IDLE 将 recv/send 置为0
        for (int i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
        {
            for (int j = 0; j < TRAFFIC_SLOT_NUMBER; j++)
            {
                macdata_daatr->slot_traffic_execution_new[i][j].state = 0; // 本应全置为IDLE
            }
        }

        for (int i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
        {
            for (int j = 0; j < TRAFFIC_SLOT_NUMBER; j++)
            {
                if (Alloc_slottable_traffic[j].multiplexing_num != FREQUENCY_DIVISION_MULTIPLEXING_NUMBER)
                {
                    for (int k = 0; k < FREQUENCY_DIVISION_MULTIPLEXING_NUMBER - Alloc_slottable_traffic[j].multiplexing_num; k++)
                    {
                        // Alloc_slottable_traffic[j].send_node 是 网管节点 的分配表的第 j 个时隙的发送节点序列
                        // send[k] 是其中的第 k 组链路需求的发送节点
                        // 需要找到 20 个时隙表中的第 senk[k] 个, 然后将其第[j]个时隙的state和send进行修改
                        // 修改为 TX , 并将 send 指向 recv 节点
                        macdata_daatr->slot_traffic_execution_new[Alloc_slottable_traffic[j].send_node[k] - 1][j].state = DAATR_STATUS_TX;
                        macdata_daatr->slot_traffic_execution_new[Alloc_slottable_traffic[j].send_node[k] - 1][j].send_or_recv_node = Alloc_slottable_traffic[j].recv_node[k];

                        macdata_daatr->slot_traffic_execution_new[Alloc_slottable_traffic[j].recv_node[k] - 1][j].state = DAATR_STATUS_RX;
                        macdata_daatr->slot_traffic_execution_new[Alloc_slottable_traffic[j].recv_node[k] - 1][j].send_or_recv_node = Alloc_slottable_traffic[j].send_node[k];
                    }
                }
            }
        }

        // 网管节点保存子网内所有节点时隙表macdata_daatr->slot_traffic_execution_new
        // 在收到链路构建需求事件MAC_DAATR_ReceiveTaskInstruct之后 500ms 后发送时隙表
        Message *prepare_msg = MESSAGE_Alloc(node,
                                             MAC_LAYER,
                                             MAC_PROTOCOL_DAATR,
                                             MSG_MAC_ManaPrepareToSendSlottable);

        clocktype current_frametime = node->getNodeTime();
        clocktype delay;
        if (((current_frametime / 1000000) % 1000) < 20)
        {
            delay = (20 * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND) + 100 * MILLI_SECOND);
        }
        else if (((current_frametime / 1000000) % 1000) >= 20 && ((current_frametime / 1000000) % 1000) < 520)
        {
            delay = (520 * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND) + 100 * MILLI_SECOND);
        }
        else
        {
            delay = (1020 * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND) + 100 * MILLI_SECOND);
        }

        MESSAGE_Send(node, prepare_msg, delay);

        MESSAGE_Free(node, msg);
        break;
    }

    // 此时要进行时隙表的收发, 由网管节点发向各个节点
    case MSG_MAC_ManaPrepareToSendSlottable:
    {
        for (int dest_node = 1; dest_node <= SUBNET_NODE_NUMBER; dest_node++)
        {
            Message *slottable_msg_1 = MESSAGE_Alloc(node,
                                                     MAC_LAYER,
                                                     MAC_PROTOCOL_DAATR,
                                                     MSG_MAC_SendNewSlottableToEachNode);

            MacHeader *mac_header = new MacHeader;
            mac_header->is_mac_layer = 1;
            mac_header->backup = 4;
            mac_header->srcAddr = 1; // 由网管节点发送
            mac_header->srcAddr = dest_node;
            mac_header->PDUtype = 0;
            mac_header->packetLength = 25 + 550;

            MacDaatr_struct_converter mac_converter_PDU1(1);
            mac_converter_PDU1.set_struct((uint8_t *)mac_header, 1);
            mac_converter_PDU1.daatr_struct_to_0_1();

            MacDaatr_struct_converter mac_converter_Slottable(7);
            mac_converter_Slottable.set_length(550);
            Send_slottable *New_Slottable_ptr = new Send_slottable;
            for (int j = 0; j < TRAFFIC_SLOT_NUMBER; j++)
            {
                New_Slottable_ptr->slot_traffic_execution_new[j] = macdata_daatr->slot_traffic_execution_new[dest_node - 1][j];
            }
            mac_converter_Slottable.set_struct((uint8_t *)New_Slottable_ptr, 7);
            mac_converter_Slottable.daatr_struct_to_0_1();
            mac_converter_PDU1 + mac_converter_Slottable;
            uint8_t *frame_ptr1 = mac_converter_PDU1.get_sequence();
            MESSAGE_PacketAlloc(node, slottable_msg_1, mac_header->packetLength, TRACE_DAATR);
            memcpy((uint8_t *)MESSAGE_ReturnPacket(slottable_msg_1), frame_ptr1, mac_header->packetLength);

            Node *dest_node_str = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, dest_node);
            clocktype sending_delay_1 = (float)(0 * MILLI_SECOND + (dest_node - 1) * TRAFFIC_SLOT_DURATION * MILLI_SECOND);
            if (dest_node != macdata_daatr->mana_node)
            {
                cout << "[调整阶段] dest_node: " << dest_node << " 发送时隙表定时器延时: " << (float)(sending_delay_1) / 1000000;

                cout << " 预计收到新时隙表的时间: " << (float)(node->getNodeTime() + sending_delay_1) / 1000000 << endl;
            }

            Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_TX, sending_delay_1);
            // 收ACK的指示 在发送时隙表 50ms 后
            Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_RX, sending_delay_1 + 50 * MILLI_SECOND);
            MESSAGE_Send(dest_node_str, slottable_msg_1, sending_delay_1);

            delete mac_header;

            // 第二次时隙表
            {
                Message *slottable_msg_2 = MESSAGE_Alloc(node,
                                                         MAC_LAYER,
                                                         MAC_PROTOCOL_DAATR,
                                                         MSG_MAC_SendNewSlottableToEachNode);
                MacHeader *mac_header_repeat2 = new MacHeader;
                mac_header_repeat2->is_mac_layer = 2;
                mac_header_repeat2->backup = 4;
                mac_header_repeat2->srcAddr = 1; // 由网管节点发送
                mac_header_repeat2->srcAddr = dest_node;
                mac_header_repeat2->PDUtype = 0;
                mac_header_repeat2->packetLength = 25 + 550;

                MacDaatr_struct_converter mac_converter_PDU1_repeat2(1);
                mac_converter_PDU1_repeat2.set_struct((uint8_t *)mac_header_repeat2, 1);
                mac_converter_PDU1_repeat2.daatr_struct_to_0_1();

                MacDaatr_struct_converter mac_converter_Slottable_repeat2(7);
                mac_converter_Slottable_repeat2.set_length(550);
                Send_slottable *New_Slottable_ptr_repeat2 = new Send_slottable;
                for (int j = 0; j < TRAFFIC_SLOT_NUMBER; j++)
                {
                    New_Slottable_ptr_repeat2->slot_traffic_execution_new[j] = macdata_daatr->slot_traffic_execution_new[dest_node - 1][j];
                }
                mac_converter_Slottable_repeat2.set_struct((uint8_t *)New_Slottable_ptr_repeat2, 7);
                mac_converter_Slottable_repeat2.daatr_struct_to_0_1();

                mac_converter_PDU1_repeat2 + mac_converter_Slottable_repeat2;
                uint8_t *frame_ptr2 = mac_converter_PDU1_repeat2.get_sequence();
                MESSAGE_PacketAlloc(node, slottable_msg_2, mac_header_repeat2->packetLength, TRACE_DAATR);
                memcpy((uint8_t *)MESSAGE_ReturnPacket(slottable_msg_2), frame_ptr2, mac_header_repeat2->packetLength);
                clocktype sending_delay_2 = (float)(100 * MILLI_SECOND + (dest_node - 1) * TRAFFIC_SLOT_DURATION * MILLI_SECOND);
                Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_TX, sending_delay_2);
                Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_RX, sending_delay_2 + 50 * MILLI_SECOND);
                MESSAGE_Send(dest_node_str, slottable_msg_2, sending_delay_2);
                delete mac_header_repeat2;
            }

            // 第三次时隙表
            {
                Message *slottable_msg_3 = MESSAGE_Alloc(node,
                                                         MAC_LAYER,
                                                         MAC_PROTOCOL_DAATR,
                                                         MSG_MAC_SendNewSlottableToEachNode);
                MacHeader *mac_header_repeat3 = new MacHeader;
                mac_header_repeat3->is_mac_layer = 3;
                mac_header_repeat3->backup = 4;
                mac_header_repeat3->srcAddr = 1; // 由网管节点发送
                mac_header_repeat3->srcAddr = dest_node;
                mac_header_repeat3->PDUtype = 0;
                mac_header_repeat3->packetLength = 25 + 550;

                MacDaatr_struct_converter mac_converter_PDU1_repeat3(1);
                mac_converter_PDU1_repeat3.set_struct((uint8_t *)mac_header_repeat3, 1);
                mac_converter_PDU1_repeat3.daatr_struct_to_0_1();

                MacDaatr_struct_converter mac_converter_Slottable_repeat3(7);
                mac_converter_Slottable_repeat3.set_length(550);
                Send_slottable *New_Slottable_ptr_repeat3 = new Send_slottable;
                for (int j = 0; j < TRAFFIC_SLOT_NUMBER; j++)
                {
                    New_Slottable_ptr_repeat3->slot_traffic_execution_new[j] = macdata_daatr->slot_traffic_execution_new[dest_node - 1][j];
                }
                mac_converter_Slottable_repeat3.set_struct((uint8_t *)New_Slottable_ptr_repeat3, 7);
                mac_converter_Slottable_repeat3.daatr_struct_to_0_1();

                mac_converter_PDU1_repeat3 + mac_converter_Slottable_repeat3;
                uint8_t *frame_ptr3 = mac_converter_PDU1_repeat3.get_sequence();
                MESSAGE_PacketAlloc(node, slottable_msg_3, mac_header_repeat3->packetLength, TRACE_DAATR);
                memcpy((uint8_t *)MESSAGE_ReturnPacket(slottable_msg_3), frame_ptr3, mac_header_repeat3->packetLength);
                clocktype sending_delay_3 = (float)(200 * MILLI_SECOND + (dest_node - 1) * TRAFFIC_SLOT_DURATION * MILLI_SECOND);
                Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_TX, sending_delay_3);
                Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_RX, sending_delay_3 + 50 * MILLI_SECOND);
                MESSAGE_Send(dest_node_str, slottable_msg_3, sending_delay_3);
                delete mac_header_repeat3;
            }
        }

        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_ManaPrepareToSendSequence:
    {
        // 向各节点发送跳频图案
        for (int dest_node = 1; dest_node <= SUBNET_NODE_NUMBER; dest_node++)
        {
            Message *M_sequence_msg_1 = MESSAGE_Alloc(node,
                                                      MAC_LAYER,
                                                      MAC_PROTOCOL_DAATR,
                                                      MSG_MAC_SendNewSequenceToEachNode);

            MacHeader *mac_header = new MacHeader;
            mac_header->is_mac_layer = 1;
            mac_header->backup = 5;
            mac_header->srcAddr = 1; // 由网管节点发送
            mac_header->srcAddr = dest_node;
            mac_header->PDUtype = 0;
            mac_header->packetLength = 25 + 563;

            MacDaatr_struct_converter mac_converter_PDU1(1);
            mac_converter_PDU1.set_struct((uint8_t *)mac_header, 1);
            mac_converter_PDU1.daatr_struct_to_0_1();

            MacDaatr_struct_converter mac_converter_Sequence(10);
            mac_converter_Sequence.set_length(563);
            subnet_frequency_parttern *New_freq_sequence_ptr = new subnet_frequency_parttern;
            for (int j = 0; j < SUBNET_NODE_FREQ_NUMBER; j++)
            {
                for (int k = 0; k < FREQUENCY_COUNT; k++)
                {
                    New_freq_sequence_ptr->subnet_node_hopping[j][k] = macdata_daatr->frequency_sequence[j][k];
                }
            }
            mac_converter_Sequence.set_struct((uint8_t *)New_freq_sequence_ptr, 10);
            mac_converter_Sequence.daatr_struct_to_0_1();

            mac_converter_PDU1 + mac_converter_Sequence;
            uint8_t *frame_ptr1 = mac_converter_PDU1.get_sequence();
            MESSAGE_PacketAlloc(node, M_sequence_msg_1, mac_header->packetLength, TRACE_DAATR);
            memcpy((uint8_t *)MESSAGE_ReturnPacket(M_sequence_msg_1), frame_ptr1, mac_header->packetLength);
            Node *dest_node_str = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, dest_node);
            clocktype sending_delay_1 = (float)(0 * MILLI_SECOND + (dest_node - 1) * TRAFFIC_SLOT_DURATION * MILLI_SECOND);

            if (dest_node != macdata_daatr->mana_node)
            {
                cout << "[调整阶段] dest_node: " << dest_node << " 发送跳频图案定时器延时: " << (float)(sending_delay_1) / 1000000;
                cout << " 预计收到新跳频图案的时间: " << (float)(node->getNodeTime() + sending_delay_1) / 1000000 << endl;
            }

            Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_TX, sending_delay_1);
            // 收ACK的指示 在发送时隙表 50ms 后
            Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_RX, sending_delay_1 + 50 * MILLI_SECOND);
            MESSAGE_Send(dest_node_str, M_sequence_msg_1, sending_delay_1);

            delete mac_header;

            // 2nd
            {
                Message *M_sequence_msg_2 = MESSAGE_Alloc(node,
                                                          MAC_LAYER,
                                                          MAC_PROTOCOL_DAATR,
                                                          MSG_MAC_SendNewSequenceToEachNode);
                MacHeader *mac_header_repeat2 = new MacHeader;
                mac_header_repeat2->is_mac_layer = 2;
                mac_header_repeat2->backup = 5;
                mac_header_repeat2->srcAddr = 1; // 由网管节点发送
                mac_header_repeat2->srcAddr = dest_node;
                mac_header_repeat2->PDUtype = 0;
                mac_header_repeat2->packetLength = 25 + 563;

                MacDaatr_struct_converter mac_converter_PDU1_repeat2(1);
                mac_converter_PDU1_repeat2.set_struct((uint8_t *)mac_header_repeat2, 1);
                mac_converter_PDU1_repeat2.daatr_struct_to_0_1();

                MacDaatr_struct_converter mac_converter_Sequence_repeat2(10);
                mac_converter_Sequence_repeat2.set_length(563);
                subnet_frequency_parttern *New_freq_sequence_ptr_repeat2 = new subnet_frequency_parttern;
                for (int j = 0; j < SUBNET_NODE_FREQ_NUMBER; j++)
                {
                    for (int k = 0; k < FREQUENCY_COUNT; k++)
                    {
                        New_freq_sequence_ptr_repeat2->subnet_node_hopping[j][k] = macdata_daatr->frequency_sequence[j][k];
                    }
                }
                mac_converter_Sequence_repeat2.set_struct((uint8_t *)New_freq_sequence_ptr_repeat2, 10);
                mac_converter_Sequence_repeat2.daatr_struct_to_0_1();
                mac_converter_PDU1_repeat2 + mac_converter_Sequence_repeat2;
                uint8_t *frame_ptr2 = mac_converter_PDU1_repeat2.get_sequence();
                MESSAGE_PacketAlloc(node, M_sequence_msg_2, mac_header_repeat2->packetLength, TRACE_DAATR);
                memcpy((uint8_t *)MESSAGE_ReturnPacket(M_sequence_msg_2), frame_ptr2, mac_header_repeat2->packetLength);
                clocktype sending_delay_2 = (float)(100 * MILLI_SECOND + (dest_node - 1) * TRAFFIC_SLOT_DURATION * MILLI_SECOND);
                Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_TX, sending_delay_2);
                Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_RX, sending_delay_2 + 50 * MILLI_SECOND);
                MESSAGE_Send(dest_node_str, M_sequence_msg_2, sending_delay_2);
                delete mac_header_repeat2;
            }

            // 3rd
            {
                Message *M_sequence_msg_3 = MESSAGE_Alloc(node,
                                                          MAC_LAYER,
                                                          MAC_PROTOCOL_DAATR,
                                                          MSG_MAC_SendNewSequenceToEachNode);
                MacHeader *mac_header_repeat3 = new MacHeader;
                mac_header_repeat3->is_mac_layer = 3;
                mac_header_repeat3->backup = 5;
                mac_header_repeat3->srcAddr = 1; // 由网管节点发送
                mac_header_repeat3->srcAddr = dest_node;
                mac_header_repeat3->PDUtype = 0;
                mac_header_repeat3->packetLength = 25 + 563;

                MacDaatr_struct_converter mac_converter_PDU1_repeat3(1);
                mac_converter_PDU1_repeat3.set_struct((uint8_t *)mac_header_repeat3, 1);
                mac_converter_PDU1_repeat3.daatr_struct_to_0_1();

                MacDaatr_struct_converter mac_converter_Sequence_repeat3(10);
                mac_converter_Sequence_repeat3.set_length(563);
                subnet_frequency_parttern *New_freq_sequence_ptr_repeat3 = new subnet_frequency_parttern;
                for (int j = 0; j < SUBNET_NODE_FREQ_NUMBER; j++)
                {
                    for (int k = 0; k < FREQUENCY_COUNT; k++)
                    {
                        New_freq_sequence_ptr_repeat3->subnet_node_hopping[j][k] = macdata_daatr->frequency_sequence[j][k];
                    }
                }
                mac_converter_Sequence_repeat3.set_struct((uint8_t *)New_freq_sequence_ptr_repeat3, 10);
                mac_converter_Sequence_repeat3.daatr_struct_to_0_1();
                mac_converter_PDU1_repeat3 + mac_converter_Sequence_repeat3;
                uint8_t *frame_ptr3 = mac_converter_PDU1_repeat3.get_sequence();
                MESSAGE_PacketAlloc(node, M_sequence_msg_3, mac_header_repeat3->packetLength, TRACE_DAATR);
                memcpy((uint8_t *)MESSAGE_ReturnPacket(M_sequence_msg_3), frame_ptr3, mac_header_repeat3->packetLength);
                clocktype sending_delay_3 = (float)(200 * MILLI_SECOND + (dest_node - 1) * TRAFFIC_SLOT_DURATION * MILLI_SECOND);
                Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_TX, sending_delay_3);
                Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_RX, sending_delay_3 + 50 * MILLI_SECOND);
                MESSAGE_Send(dest_node_str, M_sequence_msg_3, sending_delay_3);
                delete mac_header_repeat3;
            }
        }
        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_SendLocalLinkStateTimer:
    {
        // if(macdata_daatr->state_now == Mac_Execution)
        //{
        cout << "Node " << node->nodeId << "Send Local Link Status To NetworkLayer" << endl;
        Send_Local_Link_Status(node, macdata_daatr);
        // SendLocalLinkMsg(node); //网络层测试函数
        MacDaatrInitialize_LocalLinkState_timer(node, macdata_daatr);
        //}
        // else
        //{
        //	MacDaatrInitialize_LocalLinkState_timer(node, macdata_daatr);
        //}
        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_End_Link_Build:
    { // 定时结束建链阶段, 进入执行阶段
        if (macdata_daatr->has_received_chain_building_request == false &&
            node->nodeId != macdata_daatr->mana_node)
        {
            macdata_daatr->state_now = Mac_Access; // 进入接入阶段
            if (node->nodeId == macdata_daatr->mana_node)
            {
                macdata_daatr->mana_node = 0;
                macdata_daatr->node_type = Node_Ordinary;
            }
            macdata_daatr->access_state = DAATR_NEED_ACCESS; // 需要发送接入请求

            cout << "[建链阶段] Node " << node->nodeId << " ";
            cout << " 未收到允许建链回复 进入接入状态";
            cout << " Time: " << (float)(node->getNodeTime()) / 1000000 << endl;
        }
        else
        {
            cout << endl;
            cout << "[建链阶段] Node " << node->nodeId << " ";
            cout << " 结束建链阶段, 进入执行阶段";
            cout << " Time: " << (float)(node->getNodeTime()) / 1000000 << endl;

            macdata_daatr->state_now = Mac_Execution;   // 进入执行阶段
            macdata_daatr->mana_slot_should_read = 0;   // 切换状态, 时隙表切换, 从时隙表开始进行
            macdata_daatr->mana_channel_frame_num += 1; // 已过去1s

            /////////////// 测试随遇接入用 ///////////////
            // if (node->nodeId == 5)
            //{
            //     macdata_daatr->state_now = Mac_Access;           // 进入接入阶段
            //     macdata_daatr->access_state = DAATR_NEED_ACCESS; // 需要发送接入请求
            // }
            /////////////////////////////////////////////

            if (macdata_daatr->link_build_state == 0)
            {
                // SendLocalLinkMsg(node );//网络层测试函数
                Send_Local_Link_Status(node, macdata_daatr);
                MacDaatrInitialize_LocalLinkState_timer(node, macdata_daatr);
                macdata_daatr->link_build_state = 1;
                // 第一次触发(从0到1标志已经建链完毕(包括建链和第一次调整), 可以实现数据包收发)
            }
            MacDaatrInitialize_traffic_Timer(node, macdata_daatr); // 业务信道定时器初始化
        }

        MESSAGE_Free(node, msg);
        break;
    }

    // 网管节点向各节点发送时隙表 各节点在此处接收
    case MSG_MAC_SendNewSlottableToEachNode:
    {
        int i;
        uint8_t *bit_seq = (uint8_t *)MESSAGE_ReturnPacket(msg);
        MacDaatr_struct_converter mac_converter(255);
        mac_converter.set_length(575);
        mac_converter.set_bit_sequence(bit_seq, 255);
        MacDaatr_struct_converter mac_converter_Slottable(7);
        mac_converter_Slottable - mac_converter;
        mac_converter_Slottable.daatr_0_1_to_struct();

        Send_slottable *slottable_ptr = (Send_slottable *)mac_converter_Slottable.get_struct();

        MacDaatr_struct_converter mac_converter_PDU1(1);
        mac_converter_PDU1 - mac_converter;
        mac_converter_PDU1.daatr_0_1_to_struct();
        MacHeader *mac_header = (MacHeader *)mac_converter_PDU1.get_struct();

        for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
        {
            macdata_daatr->slot_traffic_execution[i] = slottable_ptr->slot_traffic_execution_new[i];
        }

        // 各节点收到时隙表之后, 返回给网管节点一个ACK ( 50 ms )
        if (node->nodeId != macdata_daatr->mana_node)
        {
            Message *slottable_ACK_msg = MESSAGE_Alloc(node,
                                                       MAC_LAYER,
                                                       MAC_PROTOCOL_DAATR,
                                                       MSG_MAC_ReceiveACK);
            Node *dest_node_str = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, macdata_daatr->mana_node);

            MESSAGE_Send(dest_node_str, slottable_ACK_msg, 50 * MILLI_SECOND);
            Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_TX, 50 * MILLI_SECOND);
        }

        // 仅对第一次收到时隙表进行打印 并设定计时器进入执行阶段(仅为第一次收到时隙表的情况设定计时器)
        if (macdata_daatr->has_received_slottable == false)
        {
            Message *adjust_state_msg = MESSAGE_Alloc(node,
                                                      MAC_LAYER,
                                                      MAC_PROTOCOL_DAATR,
                                                      MSG_MAC_AllNodesEnterExecutionState);

            clocktype current_frametime = node->getNodeTime();
            clocktype delay;

            if (mac_header->is_mac_layer == 1) // 收到的是第一次的时隙表
            {
                delay = (1000 * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND));
            }
            else if (mac_header->is_mac_layer == 2) // 第二次接收到时隙表需要减去100
            {
                delay = ((1000 - 100) * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND));
            }
            else
            {
                delay = ((1000 - 200) * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND));
            }

            // if (((current_frametime / 1000000) % 1000) < 700) // 第一次收到时隙表的时间
            // {
            //     // 这说明所有节点可以在此1000ms结束前进入执行阶段
            //     delay = (1000 * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND));
            // }
            // else
            // {
            //     delay = (2000 * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND));
            // }
            cout << endl;
            cout << "[调整阶段] 节点 " << node->nodeId << " 收到新时隙表, Time: " << (float)(node->getNodeTime()) / 1000000;
            cout << " 进行初始化定时器操作, 通知进入执行阶段, delay = " << (float)delay / 1000000 << endl;

            for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
            {
                if (macdata_daatr->slot_traffic_execution[i].state == DAATR_STATUS_TX &&
                    macdata_daatr->slot_traffic_execution[i].send_or_recv_node != 0)
                {
                    cout << "|TX:" << macdata_daatr->slot_traffic_execution[i].send_or_recv_node;
                    macdata_daatr->TX_Slotnum++;
                }
                else if (macdata_daatr->slot_traffic_execution[i].state == DAATR_STATUS_RX &&
                         macdata_daatr->slot_traffic_execution[i].send_or_recv_node != 0)
                {
                    cout << "|RX:" << macdata_daatr->slot_traffic_execution[i].send_or_recv_node;
                }
                else
                {
                    cout << "|IDLE";
                }
            }
            cout << endl;

            macdata_daatr->has_received_slottable = true;

            // 用来接收时隙表的收状态
            Node *dest_node_str = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, macdata_daatr->mana_node);
            Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_RX, 100 * MILLI_SECOND);
            Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_RX, 200 * MILLI_SECOND);

            MESSAGE_Send(node, adjust_state_msg, delay);
        }

        // 生成射前时隙表的过程
        // ofstream fout1, fout2;
        // string stlotable_state_filename = "..\\..\\..\\..\\libraries\\user_models\\src\\Slottable\\Slottable_RXTX_State_" + to_string(static_cast<long long>(node->nodeId)) + ".txt";
        // // cout << stlotable_state_filename << endl;
        // string stlotable_node_filename = "..\\..\\..\\..\\libraries\\user_models\\src\\Slottable\\Slottable_RXTX_Node_" + to_string(static_cast<long long>(node->nodeId)) + ".txt";
        // fout1.open(stlotable_state_filename);
        // fout2.open(stlotable_node_filename);

        // if (!fout1.is_open())
        // {
        //     cout << "Could Not Open File1" << endl;
        // }
        // if (!fout2.is_open())
        // {
        //     cout << "Could Not Open File2" << endl;
        // }
        // // fout1 << node->nodeId << endl;
        // for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
        // {
        //     fout1 << macdata_daatr->slot_traffic_execution[i].state << ",";
        // }
        // // fout << endl;
        // for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
        // {
        //     if (macdata_daatr->slot_traffic_execution[i].state == DAATR_STATUS_TX && macdata_daatr->slot_traffic_execution[i].send_or_recv_node != 0)
        //     {
        //         fout2 << macdata_daatr->slot_traffic_execution[i].send_or_recv_node << ",";
        //     }
        //     else if (macdata_daatr->slot_traffic_execution[i].state == DAATR_STATUS_RX && macdata_daatr->slot_traffic_execution[i].send_or_recv_node != 0)
        //     {
        //         fout2 << macdata_daatr->slot_traffic_execution[i].send_or_recv_node << ",";
        //     }
        //     else
        //     {
        //         fout2 << 0 << ",";
        //     }
        // }
        // fout1.close();
        // fout2.close();
        // cout << endl;

        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_Read_Slottable_From_Txt:
    {
        // macdata_daatr->state_now = Mac_Adjustment_Slot;

        int i;
        string stlotable_state_filename = "..\\..\\..\\..\\libraries\\user_models\\src\\Slottable\\Slottable_RXTX_State_" + to_string(static_cast<long long>(node->nodeId)) + ".txt";
        string stlotable_node_filename = "..\\..\\..\\..\\libraries\\user_models\\src\\Slottable\\Slottable_RXTX_Node_" + to_string(static_cast<long long>(node->nodeId)) + ".txt";
        ifstream fout1(stlotable_state_filename);
        ifstream fout2(stlotable_node_filename);

        if (!fout1.is_open())
        {
            cout << "Could Not Open File1" << endl;
            system("pause");
        }
        if (!fout2.is_open())
        {
            cout << "Could Not Open File2" << endl;
            system("pause");
        }

        vector<int> RXTX_state;
        vector<int> RXTX_node;
        string str1, str2, temp;

        getline(fout1, str1);
        stringstream ss1(str1);
        while (getline(ss1, temp, ','))
        {
            RXTX_state.push_back(stoi(temp));
        }
        getline(fout2, str2);
        stringstream ss2(str2);
        while (getline(ss2, temp, ','))
        {
            RXTX_node.push_back(stoi(temp));
        }

        // cout << RXTX_node.size() << endl;
        // cout << RXTX_state.size() << endl;
        // for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
        // {
        //     cout << RXTX_state[i] << ": " << RXTX_node[i] << "|";
        // }
        // cout << endl;

        for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
        {
            macdata_daatr->slot_traffic_execution[i].state = RXTX_state[i];
            if (macdata_daatr->slot_traffic_execution[i].state == DAATR_STATUS_TX && RXTX_node[i] != 0)
            {
                macdata_daatr->slot_traffic_execution[i].send_or_recv_node = RXTX_node[i];
                macdata_daatr->TX_Slotnum++;
            }
            else if (macdata_daatr->slot_traffic_execution[i].state == DAATR_STATUS_RX && RXTX_node[i] != 0)
            {
                macdata_daatr->slot_traffic_execution[i].send_or_recv_node = RXTX_node[i];
            }
            else
            {
                macdata_daatr->slot_traffic_execution[i].send_or_recv_node = 0;
                // macdata_daatr->slot_traffic_execution[i].recv = 0;
            }
        }

        // Message *adjust_state_msg = MESSAGE_Alloc(node,
        //                                           MAC_LAYER,
        //                                           MAC_PROTOCOL_DAATR,
        //                                           MSG_MAC_AllNodesEnterExecutionState);
        // // clocktype delay = (SUBNET_NODE_NUMBER - node->nodeId + 1) * 2 * TRAFFIC_SLOT_DURATION * MILLI_SECOND;
        // clocktype delay = SUBNET_NODE_NUMBER * 2 * TRAFFIC_SLOT_DURATION * MILLI_SECOND;
        // // clocktype delay = 0;
        // cout << endl;
        // cout << "节点 " << node->nodeId << " 读取射前时隙表, Time: " << (float)(node->getNodeTime()) / 1000000;
        // cout << " 进行初始化定时器操作, 通知进入执行阶段, delay = " << (float)delay / 1000000 << endl;
        // MESSAGE_Send(node, adjust_state_msg, delay);

        cout << endl;
        cout << "节点 " << node->nodeId << " 读取射前时隙表, Time: " << (float)(node->getNodeTime()) / 1000000;
        cout << endl;
        for (i = 0; i < TRAFFIC_SLOT_NUMBER; i++)
        {
            if (macdata_daatr->slot_traffic_execution[i].state == DAATR_STATUS_TX && macdata_daatr->slot_traffic_execution[i].send_or_recv_node != 0)
            {
                cout << "|TX:" << macdata_daatr->slot_traffic_execution[i].send_or_recv_node;
            }
            else if (macdata_daatr->slot_traffic_execution[i].state == DAATR_STATUS_RX && macdata_daatr->slot_traffic_execution[i].send_or_recv_node != 0)
            {
                cout << "|RX:" << macdata_daatr->slot_traffic_execution[i].send_or_recv_node;
            }
            else
            {
                cout << "|IDLE";
            }
        }
        cout << endl;

        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_SendNewSequenceToEachNode:
    {
        int i;

        uint8_t *bit_seq = (uint8_t *)MESSAGE_ReturnPacket(msg);
        MacDaatr_struct_converter mac_converter(255);
        mac_converter.set_length(588);
        mac_converter.set_bit_sequence(bit_seq, 255);
        MacDaatr_struct_converter mac_converter_Sequence(10);
        mac_converter_Sequence - mac_converter;
        mac_converter_Sequence.daatr_0_1_to_struct();

        subnet_frequency_parttern *sequence_ptr = (subnet_frequency_parttern *)mac_converter_Sequence.get_struct();

        MacDaatr_struct_converter mac_converter_PDU1(1);
        mac_converter_PDU1 - mac_converter;
        mac_converter_PDU1.daatr_0_1_to_struct();
        MacHeader *mac_header = (MacHeader *)mac_converter_PDU1.get_struct();

        for (int i = 0; i < SUBNET_NODE_NUMBER; i++)
        {
            for (int j = 0; j < FREQUENCY_COUNT; j++)
            {
                macdata_daatr->frequency_sequence[i][j] = sequence_ptr->subnet_node_hopping[i][j];
                // 将接受到的网管节点发送的跳频序列存储到自己节点的调频序列中
            }
        }

        // 各节点收到时隙表之后, 返回给网管节点一个ACK ( 50 ms )
        if (node->nodeId != macdata_daatr->mana_node)
        {
            Message *sequence_ACK_msg = MESSAGE_Alloc(node,
                                                      MAC_LAYER,
                                                      MAC_PROTOCOL_DAATR,
                                                      MSG_MAC_ReceiveACK);
            Node *dest_node_str = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, macdata_daatr->mana_node);

            MESSAGE_Send(dest_node_str, sequence_ACK_msg, 50 * MILLI_SECOND);
            Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_TX, 50 * MILLI_SECOND);
        }

        // 仅对第一次收到时隙表进行打印 并设定计时器进入执行阶段
        if (macdata_daatr->has_received_sequence == false)
        {
            Message *adjust_state_msg = MESSAGE_Alloc(node,
                                                      MAC_LAYER,
                                                      MAC_PROTOCOL_DAATR,
                                                      MSG_MAC_AllNodesEnterExecutionState);

            clocktype current_frametime = node->getNodeTime();
            clocktype delay;

            if (mac_header->is_mac_layer == 1) // 收到的是第一次的时隙表
            {
                delay = (1000 * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND));
            }
            else if (mac_header->is_mac_layer == 2) // 第二次接收到时隙表需要减去100
            {
                delay = ((1000 - 100) * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND));
            }
            else
            {
                delay = ((1000 - 200) * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND));
            }

            // if (((current_frametime / 1000000) % 1000) < 700) // 第一次收到跳频图案的时间
            // {
            //     // 这说明所有节点可以在此1000ms结束前进入执行阶段
            //     delay = (1000 * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND));
            // }
            // else
            // {
            //     delay = (2000 * MILLI_SECOND - current_frametime % (1000 * MILLI_SECOND));
            // }
            cout << endl;
            cout << "[调整阶段] 节点 " << node->nodeId << " 收到新跳频图案, Time: " << (float)(node->getNodeTime()) / 1000000;
            cout << " 进行初始化定时器操作, 通知进入执行阶段, delay = " << (float)delay / 1000000 << endl;

            macdata_daatr->has_received_sequence = true;

            // 用来接收时隙表的 收状态
            Node *dest_node_str = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, macdata_daatr->mana_node);
            Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_RX, 100 * MILLI_SECOND);
            Attach_Status_Information_To_Packet(node, dest_node_str, macdata_daatr, DAATR_STATUS_RX, 200 * MILLI_SECOND);

            MESSAGE_Send(node, adjust_state_msg, delay);
        }

        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_SendAccessNewSeqence:
    {
        uint8_t *bit_seq = (uint8_t *)MESSAGE_ReturnPacket(msg);
        MacDaatr_struct_converter mac_converter(255);
        mac_converter.set_length(588);
        mac_converter.set_bit_sequence(bit_seq, 255);
        MacDaatr_struct_converter mac_converter_Sequence(10);
        mac_converter_Sequence - mac_converter;
        mac_converter_Sequence.daatr_0_1_to_struct();

        subnet_frequency_parttern *sequence_ptr = (subnet_frequency_parttern *)mac_converter_Sequence.get_struct();

        for (int i = 0; i < SUBNET_NODE_NUMBER; i++)
        {
            for (int j = 0; j < FREQUENCY_COUNT; j++)
            {
                macdata_daatr->frequency_sequence[i][j] = sequence_ptr->subnet_node_hopping[i][j];
                // 将接受到的网管节点发送的跳频序列存储到自己节点的调频序列中
            }
        }
        macdata_daatr->state_now = Mac_Execution;
        macdata_daatr->access_state = DAATR_NO_NEED_TO_ACCESS;
        macdata_daatr->stats.access_end_time = (node->getNodeTime()) / 1000000;
        // MacDaatrInitialize_traffic_Timer(node, macdata_daatr); // 业务信道定时器初始化

        // 暂时不用同步
        // macdata_daatr->if_mana_slottable_synchronization = true;//新接入节点需要作网管信道时隙表同步

        //////////////////测试接入节点网管信道时隙表与子网不同步
        // macdata_daatr->slot_management_other_stage[7].nodeId = 8;
        ///////////////////////////////

        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_AllNodesEnterExecutionState:
    {
        if (macdata_daatr->state_now == Mac_Adjustment_Slot)
        {
            macdata_daatr->stats.slot_adjust_end_time = (node->getNodeTime() / 1000000);
        }
        else if (macdata_daatr->state_now == Mac_Adjustment_Freqency)
        {
            macdata_daatr->stats.freq_adjust_end_time = (node->getNodeTime() / 1000000);
        }
        macdata_daatr->state_now = Mac_Execution;
        cout << "节点 " << node->nodeId << " 收到定时器事件, 通知进入执行阶段 Time: " << (float)(node->getNodeTime() / 1000000) << endl;

        if (macdata_daatr->link_build_state == 0)
        {
            // SendLocalLinkMsg(node );//网络层测试函数
            Send_Local_Link_Status(node, macdata_daatr);
            MacDaatrInitialize_LocalLinkState_timer(node, macdata_daatr);
            macdata_daatr->link_build_state = 1; // 第一次触发(从0到1标志已经建链完毕(包括建链和第一次调整), 可以实现数据包收发)
            float time = (float)(node->getNodeTime()) / 1000000;
            cout << "Time : " << time << " ms" << endl;
        }

        MacDaatrInitialize_traffic_Timer(node, macdata_daatr); // 业务信道定时器初始化
        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_ReceiveACK:
    {
        cout << "[调整阶段] 网管节点收到ACK信息 Time: " << (float)(node->getNodeTime()) / 1000000 << endl;

        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_Start_Link_Build:
    {
        macdata_daatr->link_build_times += 1; // 第一次建链

        int i = 0, j = 0;
        Node *dest_node_ptr; // 指向各节点的指针

        if (node->nodeId == macdata_daatr->mana_node) // 提醒网管节点的波束指向
        {
            // int temp_dest_node = macdata_daatr->slot_management[macdata_daatr->mana_slot_should_read - 3].nodeId;
            // int temp_flag = true;
            // if (temp_dest_node == SUBNET_NODE_NUMBER)
            // {
            //     temp_flag = false;
            // }
            // int current_dest_node = temp_dest_node;
            int current_dest_node = macdata_daatr->slot_management[macdata_daatr->mana_slot_should_read - 3].nodeId;
            // if (current_dest_node > SUBNET_NODE_NUMBER)
            if (current_dest_node != 0)
            {
                if (macdata_daatr->state_now == Mac_Initialization)
                {
                    dest_node_ptr = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, current_dest_node); // 获取该接收节点指针
                    Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_RX, 0 * MILLI_SECOND);
                    Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_TX, 12.5 * MILLI_SECOND);

                    if (current_dest_node < SUBNET_NODE_NUMBER)
                    {
                        Message *build_link_timer_msg = MESSAGE_Alloc(node,
                                                                      MAC_LAYER,
                                                                      MAC_PROTOCOL_DAATR,
                                                                      MSG_MAC_Start_Link_Build);
                        MESSAGE_Send(node, build_link_timer_msg, 20 * MILLI_SECOND);
                    }
                    else
                    {
                        Message *build_link_timer_msg = MESSAGE_Alloc(node,
                                                                      MAC_LAYER,
                                                                      MAC_PROTOCOL_DAATR,
                                                                      MSG_MAC_Start_Link_Build);
                        MESSAGE_Send(node, build_link_timer_msg, ((25 - SUBNET_NODE_NUMBER - 3) + 5) * MANA_SLOT_DURATION * MILLI_SECOND);
                    }
                }
            }
            // else
            // {
            //     if (macdata_daatr->state_now == Mac_Initialization)
            //     {
            //         // dest_node_ptr = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, current_dest_node); // 获取该接收节点指针
            //         // Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_RX, 0 * MILLI_SECOND);
            //         // Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_TX, 12.5 * MILLI_SECOND);

            //         Message *build_link_timer_msg = MESSAGE_Alloc(node,
            //                                                       MAC_LAYER,
            //                                                       MAC_PROTOCOL_DAATR,
            //                                                       MSG_MAC_Start_Link_Build);
            //         MESSAGE_Send(node, build_link_timer_msg, 100 * MILLI_SECOND);
            //     }
            // }
        }
        else
        {
            Message *build_link_msg_1 = MESSAGE_Alloc(node,
                                                      MAC_LAYER,
                                                      MAC_PROTOCOL_DAATR,
                                                      MAC_DAATR_LinkBuild_Request);
            Message *build_link_msg_2 = MESSAGE_Alloc(node,
                                                      MAC_LAYER,
                                                      MAC_PROTOCOL_DAATR,
                                                      MAC_DAATR_LinkBuild_Request);
            Message *build_link_msg_3 = MESSAGE_Alloc(node,
                                                      MAC_LAYER,
                                                      MAC_PROTOCOL_DAATR,
                                                      MAC_DAATR_LinkBuild_Request);
            BuildLink_Request build_link;
            build_link.nodeID = node->nodeId;
            build_link.if_build = 0; // 未允许加入

            MacDaatr_struct_converter daatr_convert(6);
            daatr_convert.set_struct((uint8_t *)&build_link, 6);
            daatr_convert.daatr_struct_to_0_1();

            uint8_t *frame_ptr1 = daatr_convert.get_sequence();
            uint8_t *frame_ptr2 = deepcopy(frame_ptr1, daatr_convert.get_length());
            uint8_t *frame_ptr3 = deepcopy(frame_ptr1, daatr_convert.get_length());

            MESSAGE_PacketAlloc(node, build_link_msg_1, daatr_convert.get_length(), TRACE_DAATR);
            MESSAGE_PacketAlloc(node, build_link_msg_2, daatr_convert.get_length(), TRACE_DAATR);
            MESSAGE_PacketAlloc(node, build_link_msg_3, daatr_convert.get_length(), TRACE_DAATR);

            memcpy((uint8_t *)MESSAGE_ReturnPacket(build_link_msg_1), frame_ptr1, daatr_convert.get_length());
            memcpy((uint8_t *)MESSAGE_ReturnPacket(build_link_msg_2), frame_ptr2, daatr_convert.get_length());
            memcpy((uint8_t *)MESSAGE_ReturnPacket(build_link_msg_3), frame_ptr3, daatr_convert.get_length());

            dest_node_ptr = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, macdata_daatr->mana_node); // 获取该接收节点指针

            MESSAGE_Send(dest_node_ptr, build_link_msg_1, 0 * MILLI_SECOND);
            MESSAGE_Send(dest_node_ptr, build_link_msg_2, 2.5 * MILLI_SECOND);
            MESSAGE_Send(dest_node_ptr, build_link_msg_3, 5 * MILLI_SECOND);
            // cout << "[建链阶段] 节点 " << node->nodeId << " 向网管节点发送建链请求 Time: " << (float)(node->getNodeTime()) / 1000000 << endl;

            Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_TX, 0);
            // Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_TX, 2.5 * MILLI_SECOND);
            // Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_TX, 5 * MILLI_SECOND);

            Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_RX, 12.5 * MILLI_SECOND);
            // Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_RX, 15 * MILLI_SECOND);
            // Attach_Status_Information_To_Packet(node, dest_node_ptr, macdata_daatr, DAATR_STATUS_RX, 17.5 * MILLI_SECOND);

            cout << endl;
            cout << "[建链阶段-Request] Node " << node->nodeId << " 发送建链请求 ";
            cout << "Time: " << (float)(node->getNodeTime()) / 1000000 << " ms" << endl;

            // 为第二次建链设定计时器(500ms)
            if (macdata_daatr->link_build_times <= 1)
            {
                Message *build_link_timermsg = MESSAGE_Alloc(node,
                                                             MAC_LAYER,
                                                             MAC_PROTOCOL_DAATR,
                                                             MSG_MAC_Start_Link_Build);
                MESSAGE_Send(node, build_link_timermsg, LINK_SETUP_INTERVAL * MILLI_SECOND);
            }
        }

        MESSAGE_Free(node, msg);
        break;
    }

    // 由子网内节点生成 网管节点收到此request消息
    case MAC_DAATR_LinkBuild_Request:
    {
        // 建链阶段网管节点接收非网管节点建链请求
        Node *dest_node_ptr;                                     // 指向各节点的指针
        uint8_t *bit_seq = (uint8_t *)MESSAGE_ReturnPacket(msg); // 返回bit序列
        MacDaatr_struct_converter mac_converter(6);
        mac_converter.set_bit_sequence(bit_seq, 6);
        mac_converter.daatr_0_1_to_struct();
        BuildLink_Request *build_requist = (BuildLink_Request *)mac_converter.get_struct();
        cout << "[建链阶段] 网管节点收到 Node " << build_requist->nodeID << " 的建链请求. ";
        float time = (float)(node->getNodeTime()) / 1000000;
        cout << "Time : " << time << " ms" << endl;

        // 在一段时间后进行Response报文的发送
        Message *send_msg0 = MESSAGE_Alloc(node,
                                           MAC_LAYER,
                                           MAC_PROTOCOL_DAATR,
                                           MAC_DAATR_LinkBuild_Response); // 生成消息
        BuildLink_Request build_link;
        build_link.nodeID = build_requist->nodeID;
        build_link.if_build = 1; // 允许建链
        mac_converter.set_struct((uint8_t *)&build_link, 6);
        mac_converter.daatr_struct_to_0_1();
        uint8_t *frame_ptr = mac_converter.get_sequence();
        MESSAGE_PacketAlloc(node, send_msg0, mac_converter.get_length(), TRACE_DAATR);
        memcpy((uint8_t *)MESSAGE_ReturnPacket(send_msg0), frame_ptr, mac_converter.get_length());
        dest_node_ptr = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, build_requist->nodeID); // 获取该接收节点指针
        MESSAGE_Send(dest_node_ptr, send_msg0, 12.5 * MILLI_SECOND);                                        // 相隔5个时隙
        MESSAGE_Free(node, msg);
        break;
    }

    case MAC_DAATR_LinkBuild_Response:
    {
        // 非网管节点接收来自网管节点的建链请求(使用业务信道)
        uint8_t *bit_seq = (uint8_t *)MESSAGE_ReturnPacket(msg); // 返回bit序列
        MacDaatr_struct_converter mac_converter(6);
        mac_converter.set_bit_sequence(bit_seq, 6);
        mac_converter.daatr_0_1_to_struct();
        BuildLink_Request *build_requist = (BuildLink_Request *)mac_converter.get_struct();
        if (build_requist->if_build == true)
        {
            // 建链成功
            // if (macdata_daatr->has_received_chain_building_request == false) // 仅在第一次收到时打印
            // {
            cout << "[建链阶段-ACK] 网管节点回应并允许 Node " << node->nodeId << " 建链请求! ";
            macdata_daatr->has_received_chain_building_request = true;
            float time = (float)(node->getNodeTime()) / 1000000;
            cout << "Time: " << time << " ms" << endl;
            // }
        }
        else
        {
            // 建链失败
            cout << "[建链阶段-NACK] 网管节点拒绝 Node " << node->nodeId << " 建链请求! ";
            float time = (float)(node->getNodeTime()) / 1000000;
            cout << "Time: " << time << " ms" << endl;
        }

        MESSAGE_Free(node, msg);
        // system("pause");
        break;
    }

    case MAC_DAATR_SpectrumSensing:
    {
        // 接收来自物理层的频谱感知结果
        Node *node_ptr_to_node;
        spectrum_sensing_struct *spec_sense_ptr = (spectrum_sensing_struct *)MESSAGE_ReturnPacket(msg);
        cout << "节点 " << node->nodeId << " 收到来自物理层的频谱感知结果. Time: " << (float)(node->getNodeTime()) / 1000000 << endl;
        for (int i = 0; i < TOTAL_FREQ_POINT; i++)
        {
            macdata_daatr->spectrum_sensing_node[i] = spec_sense_ptr->spectrum_sensing[i]; // 将接收到的频谱感知结果存储在macdata_daatr结构体中
        }
        if (node->nodeId == macdata_daatr->mana_node)
        {
            // 如果是网管节点, 则需要吧频谱感知结果存到总表里
            int i = 0;
            for (i = 0; i < TOTAL_FREQ_POINT; i++)
            {
                macdata_daatr->spectrum_sensing_sum[0][i] = spec_sense_ptr->spectrum_sensing[i];
            }
            macdata_daatr->spectrum_sensing_sum[0][TOTAL_FREQ_POINT] = 1;
        }
        else
        {
            // 在业务信道队列中添加此业务
            spectrum_sensing_struct *spec_sen = new spectrum_sensing_struct;
            for (int i = 0; i < TOTAL_FREQ_POINT; i++)
            {
                spec_sen->spectrum_sensing[i] = macdata_daatr->spectrum_sensing_node[i];
            }

            /*spectrum_sensing_struct spec_struct;
            for (int i = 0; i < TOTAL_FREQ_POINT; i++)
            {
                spec_struct.spectrum_sensing[i] = Randomly_Generate_0_1(0.8);
            }
            Message *spec_sense_msg = MESSAGE_Alloc(node,
                                                    MAC_LAYER,
                                                    MAC_PROTOCOL_DAATR,
                                                    MAC_DAATR_SpectrumSensing);
            MESSAGE_PacketAlloc(node, spec_sense_msg, sizeof(spectrum_sensing_struct), TRACE_DAATR);
            memcpy((spectrum_sensing_struct *)MESSAGE_ReturnPacket(spec_sense_msg),
                   &spec_struct, sizeof(spectrum_sensing_struct));
            MESSAGE_Send(node, spec_sense_msg, 700 * MILLI_SECOND);*/

            // 以下是将频谱感知结果转换为01序列, 放在MFC的data字段中, 并将MFC放入队列中等待发送
            MacDaatr_struct_converter mac_converter_Spec(5);
            mac_converter_Spec.set_struct((uint8_t *)spec_sen, 5);
            mac_converter_Spec.daatr_freq_sense_to_0_1();
            uint8_t *bit_seq = mac_converter_Spec.get_sequence(); // 63 byte

            vector<uint8_t> *SpecData = new vector<uint8_t>;
            for (int i = 0; i < 63; i++)
            {
                SpecData->push_back(bit_seq[i]);
            }

            msgFromControl *MFC_temp = new msgFromControl;
            MFC_temp->srcAddr = node->nodeId;
            MFC_temp->destAddr = macdata_daatr->mana_node; // 给网管节点发送
            MFC_temp->backup = 1;                          // 标识本层MFC数据包
            MFC_temp->data = (char *)SpecData;
            MFC_temp->priority = 0; // 最高优先级
            MFC_temp->packetLength = 73;
            MFC_temp->msgType = 15;
            MFC_temp->fragmentNum = 15; // 以上三项都是15标识频谱感知结果

            // Message *send_msg0 = MESSAGE_Alloc(node,
            //                                    MAC_LAYER,
            //                                    MAC_PROTOCOL_DAATR,
            //                                    MAC_DAATR_Mana_Node_Recv_Spec_Sensing); // 生成消息
            // MESSAGE_PacketAlloc(node, send_msg0, sizeof(msgFromControl), TRACE_DAATR);
            // memcpy((msgFromControl *)MESSAGE_ReturnPacket(send_msg0), MFC_temp, sizeof(msgFromControl));
            // node_ptr_to_node = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, 1); // 获取该接收节点指针
            // MESSAGE_Send(node_ptr_to_node, send_msg0, 2.5 * MILLI_SECOND);

            cout << "节点 " << node->nodeId << " 将结果存入最高优先级的业务队列中. Time: " << (float)(node->getNodeTime()) / 1000000 << endl;
            MAC_NetworkLayerHasPacketToSend(node, 0, MFC_temp); // 将此业务添加进业务信道队列
        }

        // system("pause");
        MESSAGE_Free(node, msg);
        break;
    }

    case MAC_DAATR_Mana_Node_Recv_Spec_Sensing:
    {                                                            // 网管节点接收子网内其他节点频谱感知结果
        uint8_t *bit_seq = (uint8_t *)MESSAGE_ReturnPacket(msg); // 收到的 01 序列

        MacDaatr_struct_converter mac_converter(255); // 255 的混合类型 PDU1 + MFC（data字段盛放已经转为 01 的 Spec）
        int PDU_TotalLength = mac_converter.get_sequence_length(bit_seq);
        mac_converter.set_length(PDU_TotalLength);
        mac_converter.set_bit_sequence(bit_seq, 255);

        MacDaatr_struct_converter mac_converter_Spec(99); // MFC（盛放Spec）
        mac_converter_Spec - mac_converter;               // mac_converter_Spec 的 seq 中存储 Spec 的 01 序列
        mac_converter_Spec.set_length(PDU_TotalLength - 25);

        vector<uint8_t> *MFC_vector_temp = new vector<uint8_t>;
        MFC_vector_temp = mac_converter_Spec.Get_Single_MFC(); // 将 MFC 转换为01序列的vector以便进行后续的转换
        msgFromControl *Spec_MFC = AnalysisLayer2MsgFromControl(MFC_vector_temp);
        vector<uint8_t> *sensing_vector = (vector<uint8_t> *)Spec_MFC->data; // 将data字段提取出来, 按照西电的标准是vector

        uint8_t *sensing_seq = new uint8_t[63];
        for (int i = 0; i < 63; i++)
        {
            sensing_seq[i] = (*sensing_vector)[i];
        }

        MacDaatr_struct_converter mac_converter_Spec_2(5);     // 存放 SpecSensing 的converter
        mac_converter_Spec_2.set_bit_sequence(sensing_seq, 5); // 此行为了兼容 type 5 的写法, 将01序列的频谱感知结果赋给converter
        mac_converter_Spec_2.daatr_0_1_to_freq_sense();        // 将频谱感知结果的01序列转换为结构体
        spectrum_sensing_struct *Spec_struct = (spectrum_sensing_struct *)mac_converter_Spec_2.get_struct();

        int sendnode = Spec_MFC->srcAddr;
        cout << "网管节点收到来自节点 " << sendnode << " 的频谱感知结果. Time: " << (float)(node->getNodeTime()) / 1000000 << endl;

        for (int i = 0; i < TOTAL_FREQ_POINT; i++)
        {
            macdata_daatr->spectrum_sensing_sum[sendnode - 1][i] = Spec_struct->spectrum_sensing[i];
        }
        macdata_daatr->spectrum_sensing_sum[sendnode - 1][TOTAL_FREQ_POINT] = 1;

        MESSAGE_Free(node, msg);
        break;
    }

    case MAC_DAATR_Mana_Judge_Enter_Freq_Adjustment_Stage:
    {
        // 网管节点定期判断是否进入频率调整阶段
        cout << "Begin To Judge If Enter Freq Adjustment ";
        float time = (float)(node->getNodeTime()) / 1000000;
        cout << "Time : " << time << " ms" << endl;
        Judge_If_Enter_Freq_Adjust(node, macdata_daatr); // 进行判断是否进入频率调整阶段
        Message *send_msg = MESSAGE_Alloc(node,
                                          MAC_LAYER,
                                          MAC_PROTOCOL_DAATR,
                                          MAC_DAATR_Mana_Judge_Enter_Freq_Adjustment_Stage);
        MESSAGE_Send(node, send_msg, SPEC_SENSING_PERIOD * MILLI_SECOND);
        macdata_daatr->ptr_to_period_judge_if_enter_adjustment = send_msg;
        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_Beam_maintenance:
    { // 波束维护模块
        Print_Wave_Info(macdata_daatr->local_node_position_info, macdata_daatr->subnet_other_node_position, node->nodeId);
        printf("Success to obtain Beam list! \n");
        MESSAGE_Free(node, msg);
        Message *wave_msg = MESSAGE_Alloc(node,
                                          MAC_LAYER,
                                          MAC_PROTOCOL_DAATR,
                                          MSG_MAC_Beam_maintenance);
        MESSAGE_Send(node, wave_msg, 400 * MILLI_SECOND);
        break;
    }

    case MAC_DAATR_ReceiveLocalFlightState:
    { // 接收来自上层传递的本地飞行状态事件
        FlightStatus *flight = (FlightStatus *)MESSAGE_ReturnPacket(msg);
        // cout << "!!!!!!!!!!!!!!!!!!!!!!" << endl;
        // cout << endl;
        // cout << "Node " << node->nodeId << " Receive Node Position from Upper Layer ";
        // float time = (float)(node->getNodeTime()) / 1000000;
        // cout << "Time : " << time << " ms" << endl;
        // cout << "SPEED X=" << flight->accelerateX << endl;
        // cout << "SPEED Y=" << flight->accelerateY << endl;
        // cout << "POSITION X = " << flight->positionX << endl;
        // cout << "POSITION Y = " << flight->positionY << endl;
        // cout << "POSITION Z = " << flight->positionZ << endl;
        // cout << "Node ID=" << flight->nodeId << endl;

        FlightStatus *node_position_data = (FlightStatus *)MESSAGE_ReturnPacket(msg);
        macdata_daatr->local_node_position_info.nodeId = node_position_data->nodeId;
        macdata_daatr->local_node_position_info.positionX = node_position_data->positionX;
        macdata_daatr->local_node_position_info.positionY = node_position_data->positionY;
        macdata_daatr->local_node_position_info.positionZ = node_position_data->positionZ;
        macdata_daatr->local_node_position_info.accelerateX = node_position_data->accelerateX;
        macdata_daatr->local_node_position_info.accelerateY = node_position_data->accelerateY;
        macdata_daatr->local_node_position_info.accelerateZ = node_position_data->accelerateZ;
        macdata_daatr->local_node_position_info.pitchAngle = node_position_data->pitchAngle;
        macdata_daatr->local_node_position_info.yawAngle = node_position_data->yawAngle;
        macdata_daatr->local_node_position_info.rollAngle = node_position_data->rollAngle;
        MESSAGE_Free(node, msg);
        // system("pause");
        break;
    }

    case MAC_DAATR_ReceiveTopology:
    {
        NetNeighList *net_neigh = (NetNeighList *)MESSAGE_ReturnPacket(msg);
        cout << "Receive topology from network layer" << endl;
        // printf("the num of matrixp[0][0] is %d\n", *(net_neigh->receive_topology)[0][0]);
        //  macdata_daatr->neigh_list.nodeAddr = net_neigh->nodeAddr;
        TGS_copy_node_topology(macdata_daatr, net_neigh->receive_topology); // 将接收到的网络拓扑结构复制到自己的拓扑结构矩阵中
        // printf("the num of matrixp[0][0] is %d\n", macdata_daatr->subnet_topology_matrix[0][0]);

        cout << "Node " << node->nodeId;
        printf(" Receive netneighlist ");
        float time = (float)(node->getNodeTime()) / 1000000;
        cout << "Time : " << time << " ms" << endl;

        MESSAGE_Free(node, msg);
        break;
    }

    case MAC_DAATR_ReceiveCPUFreq:
    {
        macdata_daatr->cpu_freq.cpu = ((CPUFrequency *)MESSAGE_ReturnPacket(msg))->cpu;
        cout << "Node " << node->nodeId;
        printf(" receive cpu frequency\n");
        float time = (float)(node->getNodeTime()) / 1000000;
        cout << "Current time is: " << time << " ms" << endl;

        MESSAGE_Free(node, msg);
        break;
    }

    case MAC_DAATR_SendOtherNodePosition:
    { // 向上层周期发送子网其他节点位置信息
        int i = 0;
        int node_number = mac_search_number_of_node_position(macdata_daatr); // getting node number
        node_number++;                                                       // 添加本节点飞行状态信息2023.8.17
        FlightStatus *NodePosition_test = new FlightStatus[node_number];     // allocate memory to Struct array
        for (i = 0; i < node_number - 1; i++)
        { // 将已收到的结构体信息赋值给要发送的结构体中
            NodePosition_test[i] = macdata_daatr->subnet_other_node_position[i];
        }
        NodePosition_test[i] = macdata_daatr->local_node_position_info;                               // 添加本节点飞行状态信息2023.8.17
        unsigned int fullPacketSize = sizeof(NetFlightStateMsg) + node_number * sizeof(FlightStatus); // packet的总大小
        char *pkt_nodeposition = (char *)MEM_malloc(fullPacketSize);                                  // 存储该packet的空间
        NetFlightStateMsg *node_position_header = (NetFlightStateMsg *)pkt_nodeposition;
        node_position_header->nodeNum = node_number; // 本packet包含多少节点
        FlightStatus *node_position = (FlightStatus *)(pkt_nodeposition + sizeof(NetFlightStateMsg));
        memcpy(node_position, NodePosition_test, node_number * sizeof(FlightStatus));
        Message *msg4 = MESSAGE_Alloc(node,
                                      NETWORK_LAYER,
                                      NETWORK_PROTOCOL_NETVIEW,
                                      MSG_NETWORK_NETVIEW_ReceiveNetFlightState);

        MESSAGE_PacketAlloc(node, msg4, fullPacketSize, TRACE_DAATR);
        msg4->packetSize = fullPacketSize;
        memcpy((FlightStatus *)MESSAGE_ReturnPacket(msg4), pkt_nodeposition, msg4->packetSize);
        MESSAGE_Send(node, msg4, 0 * SECOND);
        memset(macdata_daatr->subnet_other_node_position, 0, sizeof(macdata_daatr->subnet_other_node_position)); // 将数组全部设置为0
        cout << "Has sent subnet other node position to network layer" << endl;
        delete[] NodePosition_test;
        MESSAGE_Free(node, msg);
        Message *send_msg0 = MESSAGE_Alloc(node,
                                           MAC_LAYER,
                                           MAC_PROTOCOL_DAATR,
                                           MAC_DAATR_SendOtherNodePosition);
        MESSAGE_Send(node, send_msg0, SEND_NODE_POSITION_PERIOD * MILLI_SECOND);
        break;
    }

    case MSG_MAC_POOL_ReceiveResponsibilityConfiguration:
    { // 接收身份配置信息
        printf("[app]Receiving message of ReseiveResonsibilityConfiguration\n");
        NodeNotification *node_notify_ptr = (NodeNotification *)MESSAGE_ReturnPacket(msg);
        unsigned int temp_nodeResonsibility = ((NodeNotification *)MESSAGE_ReturnPacket(msg))->nodeResponsibility;
        if (macdata_daatr->state_now == Mac_Execution)
        {
            switch (node_notify_ptr->nodeResponsibility)
            {
            case 10:
            {
                macdata_daatr->node_type = Node_Ordinary;
                break;
            }
            case 1:
            {
                macdata_daatr->node_type = Node_Management;
                break;
            }
            case 5:
            {
                macdata_daatr->node_type = Node_Gateway;
                break;
            }
            }
            if (macdata_daatr->mana_node != node_notify_ptr->intragroupcontrolNodeId)
            { // 如果网管节点发生变化, 则需要修改网管信道时隙表
                int mana_node_pre = macdata_daatr->mana_node;
                int mana_node_now = node_notify_ptr->intragroupcontrolNodeId;
                if (node->nodeId == mana_node_pre)
                {
                    MESSAGE_CancelSelfMsg(node, macdata_daatr->timerMsg);
                }
                else if (node->nodeId == mana_node_now)
                {
                    Message *send_msg = MESSAGE_Alloc(node,
                                                      MAC_LAYER,
                                                      MAC_PROTOCOL_DAATR,
                                                      MAC_DAATR_Mana_Judge_Enter_Freq_Adjustment_Stage);
                    MESSAGE_Send(node, send_msg, SPEC_SENSING_PERIOD * MILLI_SECOND);
                    macdata_daatr->ptr_to_period_judge_if_enter_adjustment = send_msg;
                }
                if (node_notify_ptr->intragroupcontrolNodeId == node->nodeId)
                { // 如果本节点成为新网管节点
                    cout << "本节点 "
                         << "Node " << node->nodeId << " 成为新的网管节点" << endl;
                    vector<uint8_t> *ptr_temp;  // 本节点
                    vector<uint8_t> *ptr_temp1; // 网管节点
                    vector<uint8_t>::iterator ptr_it;
                    short num;
                    ptr_temp = MacDaatrSrearchSlotLocation(node->nodeId, macdata_daatr);              // 本节点
                    ptr_temp1 = MacDaatrSrearchSlotLocation(macdata_daatr->mana_node, macdata_daatr); // 网管节点
                    for (ptr_it = ptr_temp->begin(); ptr_it != ptr_temp->end(); ptr_it++)
                    { // 原网管节点分配到本节点原时隙位置
                        num = *ptr_it;
                        if (macdata_daatr->slot_management_other_stage[num].state != DAATR_STATUS_NETWORK_LAYER_SEND)
                        { // 如果不是上层消息发送时隙
                            macdata_daatr->slot_management_other_stage[num].nodeId = macdata_daatr->mana_node;
                        }
                    }
                    for (ptr_it = ptr_temp1->begin(); ptr_it != ptr_temp1->end(); ptr_it++)
                    { // 本节点时隙分配到原网管节点时隙位置
                        num = *ptr_it;
                        if (macdata_daatr->slot_management_other_stage[num].state != DAATR_STATUS_NETWORK_LAYER_SEND)
                        { // 如果不是上层消息发送时隙
                            macdata_daatr->slot_management_other_stage[num].nodeId = node->nodeId;
                        }
                    }
                    delete ptr_temp;
                    delete ptr_temp1;
                }
                else
                { // 如果本节点不是新网管节点
                    cout << "Node " << node_notify_ptr->intragroupcontrolNodeId << " 成为新的网管节点" << endl;
                    vector<uint8_t> *ptr_temp;  // 原网管节点
                    vector<uint8_t> *ptr_temp1; // 新网管节点
                    vector<uint8_t>::iterator ptr_it;
                    short num;
                    ptr_temp = MacDaatrSrearchSlotLocation(macdata_daatr->mana_node, macdata_daatr);                  // 原网管节点
                    ptr_temp1 = MacDaatrSrearchSlotLocation(node_notify_ptr->intragroupcontrolNodeId, macdata_daatr); // 新网管节点
                    for (ptr_it = ptr_temp->begin(); ptr_it != ptr_temp->end(); ptr_it++)
                    { // 新网管节点分配到网管节点时隙
                        num = *ptr_it;
                        if (macdata_daatr->slot_management_other_stage[num].state != DAATR_STATUS_NETWORK_LAYER_SEND)
                        { // 如果不是上层消息发送时隙
                            macdata_daatr->slot_management_other_stage[num].nodeId = node_notify_ptr->intragroupcontrolNodeId;
                        }
                    }
                    for (ptr_it = ptr_temp1->begin(); ptr_it != ptr_temp1->end(); ptr_it++)
                    { // 原网管节点分配到新网管节点的原时隙处
                        num = *ptr_it;
                        if (macdata_daatr->slot_management_other_stage[num].state != DAATR_STATUS_NETWORK_LAYER_SEND)
                        { // 如果不是上层消息发送时隙
                            macdata_daatr->slot_management_other_stage[num].nodeId = macdata_daatr->mana_node;
                        }
                    }
                    delete ptr_temp;
                    delete ptr_temp1;
                }
            }
            // macdata_daatr->mana_slot_should_read = 0;//重新进行网管信道时隙表轮询
            macdata_daatr->node_notification = *(node_notify_ptr);
            macdata_daatr->mana_node = node_notify_ptr->intragroupcontrolNodeId;
            macdata_daatr->gateway_node = node_notify_ptr->intergroupgatewayNodeId;
            macdata_daatr->standby_gateway_node = node_notify_ptr->reserveintergroupgatewayNodeId;
            cout << "The node" << node->nodeId << "Resonsibility of msgdata is" << temp_nodeResonsibility;
        }
        else if (macdata_daatr->state_now == Mac_Initialization)
        {
            cout << "当前子网处于建链阶段, 不接受节点身份配置信息" << endl;
        }
        else
        {
            cout << "当前子网处于调整阶段, 不接受节点身份配置信息" << endl;
        }
        MESSAGE_Free(node, msg);
        system("pause");
        break;
    }

    case MAC_DAATR_ReceiveForwardingTable:
    {
        int i, temp_node;
        printf("[MAC] 收到转发表 将原本转发表清空\n");
        Layer2_ForwardingTable_Msg *temp_Routing_table = ((Layer2_ForwardingTable_Msg *)MESSAGE_ReturnPacket(msg));
        vector<vector<uint16_t>> *recv_forwarding_table = new vector<vector<uint16_t>>;
        recv_forwarding_table = temp_Routing_table->Layer2_FT_Ptr;
        /// cout << "The Routing_table of msgdata is " << (*recv_forwarding_table)[1][1] << endl;

        if (recv_forwarding_table->size() == 0)
        {
            if (macdata_daatr->state_now == Mac_Execution)
            {
                macdata_daatr->Forwarding_Table_int_access_sign++;
                if (macdata_daatr->Forwarding_Table_int_access_sign == 3)
                {
                    macdata_daatr->access_state = DAATR_NEED_ACCESS;
                    macdata_daatr->state_now = Mac_Access;
                    if (node->nodeId == macdata_daatr->mana_node)
                    {
                        macdata_daatr->mana_node = 0;
                        macdata_daatr->node_type = Node_Ordinary;
                    }
                    cout << "收到全空转发表3次, 该节点进入接入状态" << endl;
                    macdata_daatr->Forwarding_Table_int_access_sign = 0;
                }
            }
        }
        else
        {
            macdata_daatr->Forwarding_Table_int_access_sign = 0;
        }
        if (macdata_daatr->state_now == Mac_Execution)
        {
            for (i = 0; i < SUBNET_NODE_NUMBER; i++)
            {
                macdata_daatr->Forwarding_Table[i][1] = 0; //
                                                           // 在收到新的路由表之后, 将原来的路由表的转发节点全部清零
            }
            for (i = 0; i < recv_forwarding_table->size(); i++)
            {
                // 如果此次传下来的路由表中有对应destnode,则将其赋值至对应位置
                // 如果没有的话, 就将其置零
                // 此外, 表中节点编号均为nodeId, 即为1-20, 并非0-19
                temp_node = (*recv_forwarding_table)[i][0];
                macdata_daatr->Forwarding_Table[temp_node - 1][1] = (*recv_forwarding_table)[i][1];
            }
            cout << "Node " << node->nodeId << " ForwardingTable: " << endl;
            for (i = 0; i < SUBNET_NODE_NUMBER; i++)
            {
                cout << macdata_daatr->Forwarding_Table[i][0] << "  " << macdata_daatr->Forwarding_Table[i][1] << endl;
            }
        }
        MESSAGE_Free(node, msg);
        // system("pause");
        break;
    }

    case MAC_DAATR_PassPacketToNetwork:
    {
        printf("[mac] Receiving message of PassPacketToNetwork\n");
        unsigned int temp_priority = ((msgFromControl *)MESSAGE_ReturnPacket(msg))->priority;
        printf("The priority of msgdata is %d\n", temp_priority);
        MESSAGE_Free(node, msg);
        // system("pause");
        break;
    }

    case MSG_NETWORK_ReceiveNetworkPkt:
    {
        // printf("[MAC] 收到网络层下发的数据包MFC\n");

        ////////////////此处接收到网络层vector型0/1消息, 需要转换为msgfromcontrol结构体类型
        char *from_network_MFC = MESSAGE_ReturnPacket(msg);
        vector<uint8_t> curPktPayload(from_network_MFC, from_network_MFC + MESSAGE_ReturnPacketSize(msg));
        // cout << "MAC层收到包的头指针地址为" << (int *)&(curPktPayload[0]) << endl;
        msgFromControl *MFC_temp = new msgFromControl();
        MFC_temp = AnalysisLayer2MsgFromControl(&curPktPayload); // 解析数组, 还原为包结构体类型
                                                                 //////////////////////////////////////////////

        // msgFromControl *MFC_temp = (msgFromControl *)MESSAGE_ReturnPacket(msg);
        // cout << MFC_temp->msgType << endl;
        if (MFC_temp->msgType == 1) // 网管信道消息
        {
            MacDaatNetworkHasLowFreqChannelPacketToSend(node, 0, MFC_temp);
        }
        else // 业务信道消息
        {
            unsigned int temp_destAddr = MFC_temp->destAddr;
            unsigned int temp_sourceAddr = MFC_temp->srcAddr;
            if (temp_destAddr == 0)
            {
                cout << endl;
            }
            // cout << "The destAddr from network layer packet is " << temp_destAddr << endl;
            // cout << "The srcAddr from network layer packet is " << temp_sourceAddr << endl;

            if (MFC_temp->backup == 0)
            {
                if (MFC_temp->msgType == 3 &&
                    macdata_daatr->Forwarding_Table[MFC_temp->destAddr - 1][1] == 0)
                {
                    cout << "无高速链路, 丢弃数据包 (msgType = 3)" << endl;
                    MESSAGE_Free(node, msg);
                    break;
                }
                if (MFC_temp->msgType == 3)
                {
                    macdata_daatr->stats.msgtype3_recv_network++;
                }
                MAC_NetworkLayerHasPacketToSend(node, interfaceIndex, MFC_temp);
                delete MFC_temp;
                macdata_daatr->stats.UDU_GotUnicast++;
                // cout << "MAC 将数据包放入队列. " << endl;
            }
            else
            {
                cout << "收到发给MAC的控制信息! " << endl;
            }
        }

        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_Attach_Information_To_PHY:
    {
        Status_Information_To_Phy *Status_Information = (Status_Information_To_Phy *)MESSAGE_ReturnPacket(msg);

        if (Status_Information->SendOrReceive == DAATR_STATUS_TX)
        {
            //  cout << "{TX} Node {" << node->nodeId << "}  -> " << Status_Information->dest_node
            //    << " Time: " << (float)(node->getNodeTime()) / 1000000 << " ms" << endl;
        }
        else
        {
            // cout << "{RX} Node {" << node->nodeId << "}  <- " << Status_Information->dest_node
            //  << " Time: " << (float)(node->getNodeTime()) / 1000000 << " ms" << endl;
        }
        // cout << "天线编号 antennaID: " << Status_Information->SendOrReceive.antennaID << endl;
        // cout << "波束方位角 azimuth: " << Status_Information->SendOrReceive.azimuth << endl;
        // cout << "波束俯仰角 pitchAngle: " << Status_Information->SendOrReceive.pitchAngle << endl;
        // cout << "传输速率档位 transmitSpeedLevel: " << Status_Information->transmitSpeedLevel << endl;
        // cout << "跳频图案 frequency_hopping: " << Status_Information->SendOrReceive << endl;

        MESSAGE_Free(node, msg);
        break;
    }

    case MAC_DAATR_ReceiveTimeSyn:
    {
        printf("[app]Receiving message of ReceiveTimeSyn\n");
        short int *temp_time_syn = ((short int *)MESSAGE_ReturnPacket(msg));
        printf("The size of msgdata is %u\n", *temp_time_syn);
        MESSAGE_Free(node, msg);
        break;
    }

    case MAC_DAATR_ChannelParameter:
    { // 物理层接收网管信道参数事件(此事件应放置于物理层中, 目前为测试放置于mac层中)

        Management_Channel_Information *temp_Management_Channel_Information =
            ((Management_Channel_Information *)MESSAGE_ReturnPacket(msg));
        // cout << "信道收发状态为 " << temp_Management_Channel_Information->SendOrReceive << endl;
        MESSAGE_Free(node, msg);
        break;
    }

    case MAC_DAATR_TransParameter:
    {
        printf("[mac]Receiving message of TransParameter\n");
        Status_Information_To_Phy *temp_Status_Information_To_Phy =
            ((Status_Information_To_Phy *)MESSAGE_ReturnPacket(msg));
        printf("The frequency_hopping of msgdata is %u\n", temp_Status_Information_To_Phy->frequency_hopping[1]);
        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_Data_Transmitting:
    {
        // printf("[MAC]节点 %d 业务信道 收到数据包!\n", node->nodeId);
        macdata_daatr->stats.PDU_GotUnicast++;

        uint8_t *bit_seq = (uint8_t *)MESSAGE_ReturnPacket(msg);
        MacDaatr_struct_converter mac_converter_header(255);
        int PDU_TotalLength = mac_converter_header.get_sequence_length(bit_seq); // 读取bitseq的相应字段, 得到整个PDU的长度
        mac_converter_header.set_length(PDU_TotalLength);
        mac_converter_header.set_bit_sequence(bit_seq, 255); // 将bitseq和长度赋给相应converter

        MacDaatr_struct_converter mac_converter_PDU(1);                          // 需要得到上述bitseq中属于包头的部分
        mac_converter_PDU - mac_converter_header;                                // 使用重载的-运算符得到
        mac_converter_PDU.daatr_0_1_to_struct();                                 // 将包头部分的01序列转换为结构体
        MacHeader *mac_header_ptr = (MacHeader *)mac_converter_PDU.get_struct(); // 通过强制类型转换将uint8*转换为MacHeader格式
        MacHeader mac_header;
        mac_header = *mac_header_ptr;
        int Total_MFC_num;
        if (mac_header.is_mac_layer == 0)
        {
            Total_MFC_num = mac_header.mac_backup; // 读取backup字段得到PDU中的MFC数量
        }
        else
        {
            cout << "ERROR PDUHeader is_mac_layer value" << endl;
            system("pause");
        }

        MacDaatr_struct_converter mac_converter_MFCs(99); // 用来盛放PDU中除去包头外的若干紧邻的MFC的01序列
        mac_converter_MFCs - mac_converter_header;
        mac_converter_MFCs.set_length(PDU_TotalLength - 25);

        vector<uint8_t> *MFC_vector_temp = new vector<uint8_t>;
        msgFromControl *MFC_temp = new msgFromControl;
        int mfc_count_upper = 0, mfc_count_trans = 0;
        for (int i = 0; i < Total_MFC_num; i++)
        {
            MFC_vector_temp = mac_converter_MFCs.Get_Single_MFC();
            MFC_temp = AnalysisLayer2MsgFromControl(MFC_vector_temp); // 将vector形式的01序列转为MFC
            if (MFC_temp->destAddr == node->nodeId)
            {
                // 传给上层
                Message *msg_send_network = MESSAGE_Alloc(node,
                                                          NETWORK_LAYER,
                                                          ROUTING_PROTOCOL_MMANET,
                                                          MSG_ROUTING_ReceiveMsgFromLowerLayer);

                vector<uint8_t> *MFC_ToNetwork = new vector<uint8_t>;
                MFC_ToNetwork = PackMsgFromControl(MFC_temp);
                if (MFC_temp->msgType == 3 && MFC_temp->srcAddr == 1)
                {
                    macdata_daatr->stats.msgtype3_sent_network1++;
                }
                else if (MFC_temp->msgType == 3 && MFC_temp->srcAddr == 2)
                {
                    macdata_daatr->stats.msgtype3_sent_network2++;
                }
                else if (MFC_temp->msgType == 3 && MFC_temp->srcAddr == 3)
                {
                    macdata_daatr->stats.msgtype3_sent_network3++;
                }
                else if (MFC_temp->msgType != 2 && MFC_temp->msgType != 4)
                {
                    cout << MFC_temp->msgType << endl;
                }
                MESSAGE_PacketAlloc(node, msg_send_network, MFC_ToNetwork->size(), TRACE_DAATR);
                memcpy(MESSAGE_ReturnPacket(msg_send_network), MFC_ToNetwork->data(), MFC_ToNetwork->size());
                MESSAGE_Send(node, msg_send_network, 0);

                // MESSAGE_PacketAlloc(node, msg_send_network, sizeof(MFC_temp), TRACE_DAATR);
                // msg_send_network->packetSize = sizeof(msgFromControl);
                // memcpy(MESSAGE_ReturnPacket(msg_send_network), &MFC_temp, msg_send_network->packetSize);
                // MESSAGE_Send(node, msg_send_network, 0);

                // delete control_to_network_ptr;
                // MEM_free(pktStart);
                mfc_count_upper++;
                // cout << "MAC 收到本层数据包并传往上层. " << mfc_count_upper << " / " << Total_MFC_num << endl;
            }
            else
            {
                // 查询路由表 mfc_temp->destaddr --->>> macpacket->destaddr
                // 更改MFC的destAddr, 并将其放入对应的业务信道队列中

                MAC_NetworkLayerHasPacketToSend(node, interfaceIndex, MFC_temp);
                mfc_count_trans++;
                cout << "MAC 收到非本节点消息 传向对应节点. " << mfc_count_trans << " / " << Total_MFC_num << endl;
            }
        }
        MESSAGE_Free(node, msg);
        break;
    }

    case MSG_MAC_PktCounter:
    {
        extern int appDateToLayer;
        extern int receivedAppDate;
        cout << "---------[ Node " << node->nodeId << " ] Pkt_Stats ---------" << endl;
        cout << "Recv PDU num: " << macdata_daatr->stats.PDU_GotUnicast << endl;
        cout << "Sent PDU num: " << macdata_daatr->stats.PDU_SentUnicast << endl;
        cout << "Recv UDU num: " << macdata_daatr->stats.UDU_GotUnicast << endl;
        cout << "Sent UDU num: " << macdata_daatr->stats.UDU_SentUnicast << endl;
        // cout << "网络层下发msgtype3的包数量： " << macdata_daatr->stats.msgtype3_recv_network << endl;
        // cout << "网络层下发msgtype3的包并进入队列的包数量： " << macdata_daatr->stats.msgtype3_recv << endl;
        // cout << "网络层下发msgtype3的包并发送给节点1的包数量： " << macdata_daatr->stats.msgtype3_sent1 << endl;
        // cout << "网络层下发msgtype3的包并发送给节点2的包数量： " << macdata_daatr->stats.msgtype3_sent2 << endl;
        // cout << "网络层下发msgtype3的包并发送给节点3的包数量： " << macdata_daatr->stats.msgtype3_sent3 << endl;
        // cout << "节点1网络层成功传输msgtype3的包并发送的包数量： " << macdata_daatr->stats.msgtype3_sent_network1 << endl;
        // cout << "节点2网络层成功传输msgtype3的包并发送的包数量： " << macdata_daatr->stats.msgtype3_sent_network2 << endl;
        // cout << "节点3网络层成功传输msgtype3的包并发送的包数量： " << macdata_daatr->stats.msgtype3_sent_network3 << endl;
        // cout << "网络层发给链路层的业务数据包总个数为：" << appDateToLayer << endl;
        // cout << "网络层收到链路层转发回的业务数据包总个数为：" << receivedAppDate << endl;
        cout << "Access Begin Time: " << macdata_daatr->stats.access_begin_time << " ms" << endl;
        cout << "Access End Time: " << macdata_daatr->stats.access_end_time << " ms" << endl;
        cout << "SLot Adjust Begin Time: " << macdata_daatr->stats.slot_adjust_begin_time << " ms" << endl;
        cout << "SLot Adjust End Time: " << macdata_daatr->stats.slot_adjust_end_time << " ms" << endl;
        cout << "Freq Adjust Begin Time: " << macdata_daatr->stats.freq_adjust_begin_time << " ms" << endl;
        cout << "Freq Adjust End Time: " << macdata_daatr->stats.freq_adjust_end_time << " ms" << endl;
        cout << "Traffic Throughput: " << macdata_daatr->stats.traffic_output << " Byte" << endl;
        cout << "网络开销: " << macdata_daatr->stats.mac_network_overhead << " Byte" << endl;
        if (macdata_daatr->mana_node == node->nodeId)
        {
            cout << "更新前全子网跳频图案干扰比例 " << macdata_daatr->stats.subnet_freq_interfer_ratio_before << "%" << endl;
            cout << "更新后全子网跳频图案干扰比例 " << macdata_daatr->stats.subnet_freq_interfer_ratio << "%" << endl;
            for (int i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
            {
                cout << "更新前节点 " << i + 1 << " 跳频图案干扰比例 " << macdata_daatr->stats.node_freq_interfer_ratio_before[i] << "%" << endl;
                cout << "更新后节点 " << i + 1 << " 跳频图案干扰比例 " << macdata_daatr->stats.node_freq_interfer_ratio[i] << "%" << endl;
            }
            for (int i = 0; i < SUBNET_NODE_FREQ_NUMBER; i++)
            {
                cout << "更新前节点 " << i + 1 << " 窄带干扰比例 " << macdata_daatr->stats.node_narrow_band_interfer_ratio_before[i] << "%" << endl;
                cout << "更新后节点 " << i + 1 << " 窄带干扰比例 " << macdata_daatr->stats.node_narrow_band_interfer_ratio[i] << "%" << endl;
            }
            cout << "存在窄带干扰的节点对： " << endl;
            vector<int> *temp;
            for (int i = 0; i < macdata_daatr->stats.narrow_band_interfer_nodeID.size(); i++)
            {
                temp = &(macdata_daatr->stats.narrow_band_interfer_nodeID.at(i));
                cout << temp->at(0) << "--" << temp->at(1) << "  ";
            }
            cout << endl;
        }

        cout << "----------------------------------------" << endl;
        /*macdata_daatr->stats.PDU_GotUnicast = 0;
    macdata_daatr->stats.PDU_SentUnicast = 0;
    macdata_daatr->stats.UDU_GotUnicast = 0;
    macdata_daatr->stats.UDU_SentUnicast = 0;*/
        if (node->nodeId == macdata_daatr->mana_node)
        {
            system("pause");
        }
        MESSAGE_Free(node, msg);
        // MacDaatrInitialize_pkt_Counter(node, macdata_daatr); // 此处会清零之前数据!!!!!
        Message *pkt_counter_timerMsg = MESSAGE_Alloc(node, MAC_LAYER, MAC_PROTOCOL_DAATR, MSG_MAC_PktCounter);
        MESSAGE_Send(node, pkt_counter_timerMsg, SIMULATION_RESULT_SHOW_PERIOD * MILLI_SECOND);
        break;
    }

    case MSG_MAC_Control_Transmitting:
    {
        printf("[MAC]节点 %d 收到本层控制信息!\n", node->nodeId);
        MacPacket_Daatr *macpacket_daatr_data_temp = (MacPacket_Daatr *)MESSAGE_ReturnPacket(msg);
        cout << "destAddr: " << macpacket_daatr_data_temp->mac_header->destAddr << endl;
        if (macpacket_daatr_data_temp->mac_header->destAddr == node->nodeId)
        {
            // 是本节点自己的MAC层消息
            cout << "Received my MacControl data!" << endl;
        }
        else
        {
            Message *data_msg = MESSAGE_Alloc(node,
                                              MAC_LAYER,
                                              MAC_PROTOCOL_DAATR,
                                              MSG_MAC_Control_Transmitting);

            msgFromControl busin_data = macpacket_daatr_data_temp->busin_data;
            delete macpacket_daatr_data_temp;
            MESSAGE_Free(node, msg);

            MacPacket_Daatr macdata_pkt;
            macdata_pkt.mac_header = new MacHeader;
            macdata_pkt.mac_header->PDUtype = 0;
            macdata_pkt.mac_header->srcAddr = node->nodeId;

            // 查询路由表 busin->destaddr --->>> macpacket->destaddr
            macdata_pkt.mac_header->destAddr = busin_data.destAddr;
            macdata_pkt.busin_data = busin_data;

            int data_msg_packetSize = sizeof(MacPacket_Daatr);
            MESSAGE_PacketAlloc(node, data_msg, data_msg_packetSize, TRACE_DAATR);
            memcpy((MacPacket_Daatr *)MESSAGE_ReturnPacket(data_msg), &macdata_pkt, sizeof(MacPacket_Daatr));

            Node *dest_node_struct = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash, macdata_pkt.mac_header->destAddr);

            MESSAGE_Send(dest_node_struct, data_msg, 0);
        }
        break;
    }

    default:
    {
        cout << "Receive Unknown Event : " << msg->eventType << endl;
        MESSAGE_Free(node, msg);
        break;
    }
    }
#endif
}

void MacDaatrNetworkLayerHasPacketToSend(Node *node, MacDataDaatr *macdata_daatr)
{
}

void MacDaatrReceivePacketFromPhy(Node *node, MacDataDaatr *macdata_daatr, Message *msg)
{
}

void MacDaatrReceivePhyStatusChangeNotification(Node *node, MacDataDaatr *macdata_daatr, PhyStatusType oldPhyStatus, PhyStatusType newPhyStatus)
{
}

void MacDaatrFinalize(Node *node, int interfaceIndex)
{
}
