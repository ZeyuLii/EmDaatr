#ifndef __STRUCT_CONVERTER_H__
#define __STRUCT_CONVERTER_H__

#include <stdint.h>
#include <stddef.h>
#include <vector>
#include "common_struct.h"

class MacDaatr_struct_converter
{
private:
    uint8_t *bit_sequence_ptr_;
    uint8_t *Daatr_struct_ptr_;
    unsigned char type_;
    uint32_t length_; // 比特序列长度, 单位: 字节

public:
    MacDaatr_struct_converter();                   // 默认构造函数
    MacDaatr_struct_converter(unsigned char type); // 构造函数
    ~MacDaatr_struct_converter();                  // 析构函数

    bool set_type(unsigned char type);                                // 设置转换结构体类型
    bool set_length(int length);                                      // 设置比特序列长度
    bool set_struct(uint8_t *daatr_struc, unsigned char type);        // 设置结构体
    bool set_bit_sequence(uint8_t *bit_sequence, unsigned char type); // 设置0/1序列
    uint8_t *get_sequence();                                          // 返回0/1序列
    uint8_t *get_struct();                                            // 返回结构体
    char get_type();                                                  // 返回结构体类型
    uint32_t get_length();                                                 // 返回长度
    int get_sequence_length(uint8_t *bit_seq);                        // 返回此比特序列长度(从包头得到) type: 0 PDU1包  1 PDU2
    int get_PDU2_sequence_length(uint8_t *pkt);                       // 返回PDU1包头类型数据包长度
    // bool daatr_MFC_to_0_1(msgFromControl *packet);
    // bool daatr_MFC_to_0_1(msgFromControl *packet);
    bool daatr_MFCvector_to_0_1(std::vector<uint8_t> *MFC_vector_temp, int packetlength);
    std::vector<uint8_t> *daatr_0_1_to_MFCvector();
    std::vector<uint8_t> *Get_Single_MFC();

    bool daatr_PDU1_to_0_1(); // PDU1转化为0/1序列
    bool daatr_PDU2_to_0_1(); // PDU2转化为0/1序列
    bool daatr_0_1_to_PDU1(); // 0/1序列转化为PDU1
    bool daatr_0_1_to_PDU2(); // 0/1序列转化为PDU2

    bool daatr_flight_to_0_1(); // 飞行状态信息转化为0/1序列
    bool daatr_0_1_to_flight(); // 0/1序列转化为飞行状态信息

    bool daatr_mana_info_to_0_1(); // 网管节点通告消息转化为0/1序列
    bool daatr_0_1_to_mana_info(); // 0/1序列转化网管节点通告消息

    bool daatr_freq_sense_to_0_1(); // 频谱感知结果转化为0/1序列
    bool daatr_0_1_to_freq_sense(); // 0/1序列转化为频谱感知结果

    bool daatr_build_req_to_0_1(); // 建链请求转化为0/1序列
    bool daatr_0_1_to_build_req(); // 0/1序列转化为建链请求

    bool daatr_mana_packet_type_to_0_1(); // 网管信道数据类型转换为0/1序列
    bool daatr_0_1_to_mana_packet_type(); // 0/1序列转换为网管信道数据类型

    bool daatr_access_reply_to_0_1(); // 网管节点接入请求回复转换为0/1
    bool daatr_0_1_to_access_reply(); // 0/1序列转换为网管节点接入请求

    bool daatr_slottable_to_0_1();
    bool daatr_0_1_to_slottable();

    bool daatr_subnet_frequency_parttern_to_0_1();
    bool daatr_0_1_to_subnet_frequency_parttern();

    void daatr_print_result(); // 打印结构体和0/1序列

    bool daatr_struct_to_0_1(); // 结构体转化为0/1序列
    bool daatr_0_1_to_struct(); // 0/1序列转化为结构体

    bool daatr_add_0_to_full(double speed);
    bool daatr_mana_channel_add_0_to_full(); // 网管信道补零操作（网管信道速率：20kpbs）

    MacDaatr_struct_converter &operator+(const MacDaatr_struct_converter &packet1);
    MacDaatr_struct_converter &operator-(const MacDaatr_struct_converter &packet1);
    MacDaatr_struct_converter &operator=(const MacDaatr_struct_converter &packet1);
};

msgFromControl *AnalysisLayer2MsgFromControl(std::vector<uint8_t> *dataPacket);
uint16_t CRC16(std::vector<uint8_t> *buffer);
std::vector<uint8_t> *PackMsgFromControl(msgFromControl *packet);
uint8_t *deepcopy(uint8_t *frame_ptr, int length);


#endif