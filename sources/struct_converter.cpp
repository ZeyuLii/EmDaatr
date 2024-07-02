#include "struct_converter.h"
#include "common_struct.h"
#include "macdaatr.h"
#include <cmath>

using namespace std;
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
uint32_t MacDaatr_struct_converter::get_length()
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
        Low_Freq_Packet_Type *ptr_temp = new Low_Freq_Packet_Type;
        Low_Freq_Packet_Type *ptr_temp2 = (Low_Freq_Packet_Type *)daatr_struc;
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
    Low_Freq_Packet_Type *mana_type_ptr_temp = (Low_Freq_Packet_Type *)Daatr_struct_ptr_;
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
    Low_Freq_Packet_Type *struct_ptr_temp = new Low_Freq_Packet_Type;
    memset(struct_ptr_temp, 0, sizeof(Low_Freq_Packet_Type));
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
    for (i = 0; i < SUBNET_NODE_NUMBER_MAX; i++)
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
            Low_Freq_Packet_Type *struct_ptr_temp = (Low_Freq_Packet_Type *)Daatr_struct_ptr_;
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
            for (i = 0; i < SUBNET_NODE_NUMBER_MAX; i++)
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

/*********************************************************网络层数据包转换函数*****************************************************************************/

// 函数名: AnalysisLayer2MsgFromControl
// 函数: 解析层2发来的业务数据包
// 功能: 将数组内对应的内容存入msgFromControl结构体
// 输入: uint8_t数组
msgFromControl *AnalysisLayer2MsgFromControl(vector<uint8_t> *dataPacket)
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
uint16_t CRC16(vector<uint8_t> *buffer)
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
vector<uint8_t> *PackMsgFromControl(msgFromControl *packet)
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

uint8_t *deepcopy(uint8_t *frame_ptr, int length)
{
    uint8_t *frame_ptr_new = new uint8_t[length];
    for (int i = 0; i < length; i++)
    {
        frame_ptr_new[i] = frame_ptr[i];
    }
    return frame_ptr_new;
}