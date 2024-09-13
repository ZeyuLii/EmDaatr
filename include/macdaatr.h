#ifndef __MACDAATR_H
#define __MACDAATR_H

#include <thread>
#include <mutex>
#include <cstdint>
#include <iostream>
#include <condition_variable>
#include <unordered_map>
#include <map>
#include <set>
#include <arpa/inet.h>
#include <signal.h>

#include "common_struct.h" // define namespace

// 嵌入式Daatr 相关配置信息
#define HIGH_FREQ_SLOT_LEN 2.5                                           // 业务信道时隙长度（单位：ms）
#define LOW_FREQ_SLOT_LEN 20                                             // 网管信道时隙长度（单位：ms）
#define TRIGGER_LEN 0.5                                                  // 触发时长（单位：ms）
#define HIGH_FREQ_CHANNEL_TRIGGER_LEN (HIGH_FREQ_SLOT_LEN / TRIGGER_LEN) // 业务信道触发次数
#define LOW_FREQ_CHANNEL_TRIGGER_LEN (LOW_FREQ_SLOT_LEN / TRIGGER_LEN)   // 网管信道触发次数
#define TIME_PRECISION 500                                               // 时间精确度（100us）(与底层中断相关，不要随便改变！！！！！！)

// 子网信息
#define SUBNET_NODE_NUMBER_MAX 20                // 子网最大节点数
#define FREQUENCY_DIVISION_MULTIPLEXING_NUMBER 1 // 子网内通信频分复用允许最大复用数(1/2,向下取整)
#define FULL_CONNECTION_NODE_NUMBER 10           // 当前使用的全连接时隙表的节点数
#define SUBNET_NUM 10                            // 子网数
#define END_LINK_BUILD_TIME 1000                 // 结束建链阶段时间（单位：ms）

// 低频信道相关配置
#define MANAGEMENT_SLOT_NUMBER_LINK_BUILD 25  // 建链阶段网管信道时隙表包含时隙数
#define MANAGEMENT_SLOT_NUMBER_OTHER_STAGE 50 // 其他阶段阶段网管信道时隙表包含时隙数
#define MANA_MAX_BUSINESS_NUMBER 10           // 网管信道每一个优先级可允许容纳的业务量

// 随遇接入相关
#define ACCESS_REQUREST_WAITING_MAX_TIME 200                    // 节点等待接入请求回复最长时间（单位：ms）
#define ACCESS_SUBNET_FREQUENCY_PARTTERN_WAITTING_MAX_TIME 3000 // 等待接收全网跳频图案的最大时间
#define MAX_RANDOM_BACKOFF_NUM 7                                // 最大接入退避时隙

// 高频信道相关配置信息
#define TRAFFIC_SLOT_NUMBER 400            // 业务信道时隙表包含时隙数(执行阶段)
#define TRAFFIC_CHANNEL_PRIORITY 4         // 业务信道业务优先级数量
#define TRAFFIC_MAX_BUSINESS_NUMBER 60     // 业务信道每一个优先级可允许容纳的业务量
#define MAX_WAITING_TIME 100000            // 数据包在队列中的最大等待时间(ms)
#define LINK_SETUP_GUARD_TIME 100          // 建链阶段保护时间(在网管节点开始广播(20ms)后80ms开始业务信道的建链)
#define LINK_SETUP_INTERVAL 500            // 两次建链间的间隔时间(指从第一次开始发建链请求到第二次开始)
#define SENDING_SLOTTABLE_PREPARE_TIME 500 // 在收到链路构建需求事件后 发送时隙表

// 时延与周期相关
#define BEAM_MATAINMENCE_PERIOD 930        // 波束维护周期(单位: ms)
#define SPEC_SENSING_PERIOD 5000           // 判断是否进入频率调整阶段周期(单位: ms)
#define SIMULATION_RESULT_SHOW_PERIOD 1000 // 显示仿真结果周期(单位: ms)
#define LINK_STATE_PERIOD 1000             // 发送链路状态信息周期(单位: ms)
#define SEND_NODE_POSITION_PERIOD 1000     // 向网络层上传节点位置信息周期(单位: ms)

// 频谱感知与跳频序列
#define NSUM 50                                // 与产生正态分布数有关, 该值越大, 符合正态分布精度值越大(中心极限定理法)
#define INTERFEREENCE_FREQUENCY_THRESHOLD 0.10 // 使用频段干扰比例, 若超过此门限, 则进入频率调整阶段
#define TOTAL_FREQ_POINT 501                   // 总频点
#define M_SEQUENCE_LENGTH 9
#define FREQUENCY_COUNT 25
#define MIN_FREQUENCY 0
#define MAX_FREQUENCY 500
#define MIN_FREQUENCY_DIFFERENCE 15
#define NARROW_BAND_WIDTH 30

// 波束维护模块
#define RADIS_A 6378137.0    //(单位m)基准椭球体长半径
#define RADIS_B 6356752.3142 //(单位m)基准椭球体短半径
#define PI 3.1415926

#define MAC_HEADER1_LENGTH 25 // 链路层PDU1大小(单位:byte)
#define MAC_HEADER2_LENGTH 17 // 链路层PDU2大小(单位:byte)

enum MacState
{
    // Mac层节点当前所处状态  分为初始化(Mac_Initialization)、调整(Mac_Adjustment)、执行(Mac_Execution)
    Mac_Initialization = 0,
    Mac_Adjustment_Slot,      // 时隙调整
    Mac_Adjustment_Frequency, // 频率调整
    Mac_Execution,
    Mac_Access // 接入阶段
};

enum SlotState
{
    DAATR_STATUS_TX,
    DAATR_STATUS_RX,
    DAATR_STATUS_IDLE,
    DAATR_STATUS_ACCESS_REQUEST,    // 接入请求
    DAATR_STATUS_ACCESS_REPLY,      // 接入回复
    DAATR_STATUS_FLIGHTSTATUS_SEND, // 飞行状态信息发送
    DAATR_STATUS_MANA_ANNOUNCEMENT, // 网管节点通告消息
    DAATR_STATUS_NETWORK_LAYER_SEND // 网络层消息发送
};

enum NodeType
{
    // 节点类型  分为网管节点(Node_Management)、网关节点(Node_Gateway)、普通节点(Node_Ordinary)
    Node_Management = 0,  // 网管节点
    Node_Gateway,         // 网关节点
    Node_Standby_Gateway, // 备用网关节点
    Node_Ordinary         // 普通节点
};

enum NodeAccess
{                                          // 接入控制类型
    DAATR_NO_NEED_TO_ACCESS,               // 未处于接入状态
    DAATR_NEED_ACCESS,                     // 节点断开，需要接入子网
    DAATR_HAS_SENT_ACCESS_REQUEST,         // 断开节点已发送接入请求
    DAATR_WAITING_TO_ACCESS,               // 断开节点收到同意接入，等待接收全网跳频图案
    DAATR_WAITING_TO_REPLY,                // 网管节点收到接入请求，等待回复
    DAATR_WAITING_TO_SEND_HOPPING_PARTTERN // 网管节点已经发送同意接入，但尚未发送全网跳频图案
};

// 时频资源管理模块低频网管信道时隙表
typedef struct low_freq_slot
{
    uint16_t state;  // 时隙状态：|0:可发送飞行状态|1:可发送上层网络业务|2:发送网管节点通告消息|3:发送接入请求|4:接收接入请求
    uint16_t nodeId; // 此时隙分配的节点ID
} low_freq_slot;

// 低频网管信道业务队列
typedef struct Low_Freq_Channel_Business
{
    int state;                 // 业务状态（0：此位置未被占用  1：此位置被占用）
    msgFromControl busin_data; // 网络层业务
} Low_Freq_Channel_Business;

typedef struct Low_Freq_Channel_Information
{
    unsigned short SendOrReceive;      // 当前收发指示(数值, 1表示发送, 2表示接收, 0表示不收不发)
    unsigned short transmitSpeedLevel; // 传输速率档位(数值=1)
} Low_Freq_Channel_Information;

// 网管信道数据包类型
typedef struct Low_Freq_Packet_Type
{
    uint8_t type; // 0：无定义  1：随遇接入请求  2：同意随遇接入请求回复  3：拒绝接入请求恢复  4：普通节点飞行状态信息  5：网管节点飞行状态信息  6：网管节点通告消息
} Low_Freq_Packet_Type;

// 网管节点接入请求回复
typedef struct mana_access_reply
{
    unsigned int mana_node_hopping[FREQUENCY_COUNT]; // 网管节点跳频图案
    unsigned char slot_location[5];                  // 节点位置(0-49)(60为空，忽略即可)
    unsigned char slot_num;                          // 分配的时隙数量
} mana_access_reply;

// 建链请求数据包
typedef struct BuildLink_Request
{
    unsigned short nodeID;     // 请求建链节点ID
    unsigned int if_build : 6; // 建链请求是否被允许  1:允许建链  0: 不允许建链
} BuildLink_Request;

typedef struct LinkAssignment_single
{
    unsigned short begin_node; // 链路发送节点ID
    unsigned short end_node;   // 链路接收节点ID
    unsigned short type;       // 业务类型, 取值范围[1-16]
    unsigned short priority;   // 业务优化级, 取值范围为[1-16]
    float size;                // 业务量大小(单位: Byte); 物理量范围为[0-1MB], 取值范围为[0-1048576]
    unsigned short QOS;        // QOS需求; 物理量范围为[0-5s], 取值范围为[0-2000], 时延=取值*2.5ms,
    unsigned short frequency;  // 业务频率, 物理量范围为[2.5ms/次-5s/次], 取值范围为[1-2000], 业务频率=取值*2.5ms/次

    int data_slot_per_frame; // 指示该LA每帧需要的时隙数
    double distance;         // 指示收发节点间的距离, 通过Save_Position数组实现
} LinkAssignment_single;

// 是否进入调整状态, 此结构体由网管节点通过网管信道广播发送
typedef struct if_need_change_state
{
    unsigned int state; // 0:当前保持原状态  1: 进入时隙调整阶段  2: 进入频率调整阶段  3: 暂无意义
} if_need_change_state;

// 频谱感知结果结构体
typedef struct spectrum_sensing_struct
{
    unsigned int spectrum_sensing[TOTAL_FREQ_POINT]; // 频谱感知结果(为一串0、1序列, 0代表此频段有干扰, 1代表此频段无干扰)
} spectrum_sensing_struct;

typedef struct A_ang
{
    float el;    // 天线俯仰角
    float az;    // 天线方位角
    float b_ang; // 波束角(波束指向向量与y轴正向的夹角)
    float dys;   // 天线坐标系下的dys
    int index;
} A_ang;

typedef struct subnet_frequency_parttern
{
    unsigned int mana_node_hopping[500];                                       // 网管节点跳频图案
    unsigned int subnet_node_hopping[SUBNET_NODE_NUMBER_MAX][FREQUENCY_COUNT]; // 子网各节点跳频图案
} subnet_frequency_parttern;

// 时隙分配表(G 4-9)
// 格式为 [时隙X 链路列表]
typedef struct Alloc_slot_traffic
{
    unsigned short send_node[FREQUENCY_DIVISION_MULTIPLEXING_NUMBER]; // 该时隙分配的发送节点ID, 数目为该子网内的可频分复用数目
    unsigned short recv_node[FREQUENCY_DIVISION_MULTIPLEXING_NUMBER]; // 该时隙分配的接收节点ID, 数目为该子网内的可频分复用数目
    // 发送节点和接收节点是一一对应的, 表现在数组上即位置的相同(例如node_send[0]与node_recv[0]为一对)
    int subnet_ID;        // 从属子网的ID
    int attribute;        // 时隙属性。0:下级网业务时隙 1:不可与网关节点通信的时隙 -1:未分配时隙
    int multiplexing_num; // 剩余可复用数
} Alloc_slot_traffic;

// 时频资源管理模块业务信道时隙表
typedef struct highFreqSlottableUnit
{
    unsigned int state : 1; // 时隙状态:|0:可发送TX|1:可接收RX|2:不可收发|3:未定义|
    unsigned short send_or_recv_node : 10;
} highFreqSlottableUnit;

typedef struct highFreqSlottable
{
    highFreqSlottableUnit slot_traffic_execution_new[TRAFFIC_SLOT_NUMBER];
} highFreqSlottable;

typedef struct highFreqBusiness
{
    int priority;
    double data_kbyte_sum;
    int waiting_time;
    msgFromControl busin_data;
} highFreqBusiness;

typedef struct highFreqBusinessQueue
{                                                                                     // 业务信道业务
    int recv_node;                                                                    // 下一跳节点
    highFreqBusiness business[TRAFFIC_CHANNEL_PRIORITY][TRAFFIC_MAX_BUSINESS_NUMBER]; // 接收节点对应的各优先级业务
    // set<highFreqBusiness> businessSet[TRAFFIC_CHANNEL_PRIORITY];
    double distance; // 两节点距离(收发节点)
    int state;       // 0: 子网内通信  1: 子网间通信
} highFreqBusinessQueue;

// Mac层包头PDU1
typedef struct MacHeader
{
    unsigned int PDUtype : 1;
    unsigned int is_mac_layer : 2; // 0 上层数据包+频谱感知; 1 本层数据; 2-3 重传包 (时隙表&跳频图案)
    unsigned int backup : 5;       // 1建链请求 2建链回复 4时隙表 5跳频图案 6调整ACK 7频谱干扰
    unsigned short packetLength;
    unsigned int srcAddr : 10;
    unsigned int destAddr : 10;
    unsigned short seq;
    unsigned long long mac_backup : 44;
    unsigned int mac_headerChecksum : 8;
    unsigned long long mac_pSynch;
    unsigned int mac : 24;
} MacHeader;

// Mac层包头PDU2
typedef struct MacHeader2
{
    unsigned int PDUtype : 1;                        // PDU类型
    unsigned short packetLength;                     // 长度
    unsigned int srcAddr : 10;                       // 源地址
    unsigned int destAddr : 10;                      // 目的地址
    unsigned short slice_head_identification : 1;    // 分头片标识
    unsigned short fragment_tail_identification : 1; // 分片尾标识
    unsigned int backup : 1;                         // 备用字段
    unsigned int mac_headerChecksum : 8;             // 头部校验和
    unsigned long long mac_pSynch;                   // 密码同步
    unsigned int mac : 24;                           // 消息认证码
} MacHeader2;

typedef struct MacPacket_Daatr
{
    MacHeader *mac_header;      // Mac层包头
    FlightStatus node_position; // Mac层数据
    msgFromControl busin_data;
    vector<msgFromControl> MFC_list;
} MacPacket;

// class Business

// mac层daatr类
class MacDaatr
{
    // 私有信息存储区，此部分信息需要额外设置访问函数
private:
    // 网管信道时隙表
    Low_Freq_Channel_Business low_freq_channel_business_queue[MANA_MAX_BUSINESS_NUMBER]; // 低频网管信道待发送业务队列
    // 业务信道时隙表

    // 共有信息存储区，此部分信息可以直接访问
public:
    /*节点信息*/
    uint16_t nodeId;                                    // 节点Id
    uint16_t subnetId;                                  // 子网号
    uint16_t subnet_node_num;                           // 当前在网节点数量
    unordered_map<uint16_t, string> in_subnet_id_to_ip; // 子网内节点Id对应的IP地址
    mutex lock_Mac_to_Net;                              // Mac->Net写锁
    MacState state_now;                                 // 当前节点状态
    uint64_t time;                                      // 协议栈绝对时间（单位us，精确度100us）
    NodeType node_type;                                 // 节点身份

    /*子网信息*/
    int mana_node;            // 本子网网管节点ID
    int gateway_node;         // 本子网网关节点ID
    int standby_gateway_node; // 本子网备用网关节点ID

    /*管控线程相关*/
    mutex lock_central_control_thread;             // 管控线程访问锁
    condition_variable central_control_thread_var; // 管控线程条件变量
    int clock_trigger;                             // 时帧触发数目

    /*低频信道线程相关*/
    int mac_daatr_low_freq_socket_fid; // daatr协议低频信道socket句柄
    mutex lock_mana_channel;           // 网管信道待发送队列访问锁
    mutex lowthreadmutex;
    condition_variable lowthreadcondition_variable;
    uint8_t low_slottable_should_read;                                                 // 低频信道时隙位置(时隙表最大时隙数)
    low_freq_slot low_freq_link_build_slot_table[MANAGEMENT_SLOT_NUMBER_LINK_BUILD];   // 建链阶段网管信道时隙表
    low_freq_slot low_freq_other_stage_slot_table[MANAGEMENT_SLOT_NUMBER_OTHER_STAGE]; // 其他阶段网管信道时隙表
    uint8_t need_change_state;                                                         // 网管节点的阶段转换标志(默认为0) 1 表示时域调整 2 表示频域调整
    bool if_receive_mana_flight_frame[SUBNET_NODE_NUMBER_MAX];                         // 是否在本帧内收到新的其他节点的飞行状态信息

    /*高频信道线程相关*/
    uint32_t currentSlotId; // 当前高频信道所处的时隙idx (由::UpdateTimer函数进行更新)
    uint32_t currentStatus;
    int mac_daatr_high_freq_socket_fid;               // daatr协议高频信道socket句柄
    mutex lock_buss_channel;                          // 业务信道待发送队列访问锁
    mutex highThreadSendMutex;                        // 控制高频信道的发送线程互斥量  unique_lock<mutex> highThreadSendLock(highThreadSendMutex);
    condition_variable highThread_condition_variable; // 高频信道条件变量 用来在daatrctr中每2.5ms唤醒高频信道发送线程
    uint32_t receivedSlottableTimes;                  // 时隙表收到次数
    uint32_t receivedSequenceTimes;
    uint32_t slottableTimes;
    uint32_t sequenceTimes;
    uint32_t stACKTimes;
    uint32_t seqACKTimes;
    bool has_received_sequence;
    bool receivedChainBuildingRequest;
    uint32_t blTimes;                                     // 已经发送建链请求的数量 与MacHeader seq相关
    uint16_t Forwarding_Table[SUBNET_NODE_NUMBER_MAX][2]; // 转发表

    highFreqSlottableUnit slottable_execution[TRAFFIC_SLOT_NUMBER];                                // 业务信道时隙表(执行阶段)
    highFreqSlottableUnit slottable_setup[TRAFFIC_SLOT_NUMBER];                                    // 业务信道时隙表(建链阶段)
    highFreqSlottableUnit slottable_adjustment[TRAFFIC_SLOT_NUMBER];                               // 业务信道时隙表(调整阶段)
    highFreqSlottableUnit slot_traffic_execution_new[SUBNET_NODE_NUMBER_MAX][TRAFFIC_SLOT_NUMBER]; // 存储子网内所有节点的业务信道时隙表

    highFreqBusinessQueue businessQueue[SUBNET_NODE_NUMBER_MAX]; // 业务信道的待发送队列

    // 缺少bool has_received_sequence_ACK;  523

    /*随遇接入相关*/
    NodeAccess access_state;         // 当前节点接入状态
    uint8_t access_backoff_number;   // 接入退避数目
    uint16_t waiting_to_access_node; // 等待接入的节点ID(网管节点专属)

    // 频域信息
    unsigned int freqSeq[SUBNET_NODE_NUMBER_MAX][FREQUENCY_COUNT];                   // 生成子网内调频序列(仅网管节点有)
    unsigned int subnet_frequency_sequence[TOTAL_FREQ_POINT];                        // 子网所使用频段(共501频点, 对应频点值为1代表使用此频点, 0为不使用)(网管节点独有)
    unsigned int spectrum_sensing_node[TOTAL_FREQ_POINT];                            // 本节点频谱感知结果
    unsigned int spectrum_sensing_sum[SUBNET_NODE_NUMBER_MAX][TOTAL_FREQ_POINT + 1]; // 总频谱感知结果, 只存在于网管节点.最后一列每一行代表一个节点,
    // 若为0, 则说明没有接收到此节点的频谱感知结果, 若为1, 则说明接收到了
    unsigned int unavailable_frequency_points[TOTAL_FREQ_POINT]; // 不可用频点集合(1: 频点未干扰 0: 频点受到干扰)

    /*飞行状态信息*/
    FlightStatus subnet_other_node_position[SUBNET_NODE_NUMBER_MAX - 1]; // 子网其他节点位置信息
    FlightStatus local_node_position_info;                               // 本地飞行状态信息

    // 中断与嵌入式相关
    bool start_irq;
    bool init_send;

    // 网络层到MAC层缓存区读线程
    mutex nTmBufferReadmutex;                                           // 控制网络层到MAC层缓存区读线程互斥量
    condition_variable networkToMacBufferReadThread_condition_variable; // 网络层到MAC层缓存区条件变量
    // unique_lock<mutex> nTmBufferReadlock(daatr_str.nTmBufferReadmutex);

    // 掉链标志位
    bool node_is_chain_drop;
    // 类函数
public:
    // 初始化相关
    MacDaatr();
    void macParameterInitialization(uint32_t idx);

    // 管控线程
    // void macDaatrControlThread(int signum, siginfo_t *info, void *context);

    // Socket 相关
    int macDaatrCreateUDPSocket(string ip, int port, bool if_not_block);
    void initializeNodeIP(sockaddr_in &receiver, uint16_t dest_node, int port);

    // 频域相关
    // void MacDaatrInitialize_Frequency_Seqence();

    // 业务信道相关函数
    void LoadSlottable_setup(); // 读取建链阶段时隙表
    void LoadSlottable_Execution();
    void LoadSlottable_Adjustment();

    void highFreqSendThread();                                                      // 高频信道发送线程函数
    void macDaatrSocketHighFreq_Send(uint8_t *data, uint32_t len, uint16_t nodeId); // 高频信道Socket发送
    void macDaatrSocketHighFreq_Recv(bool IF_NOT_BLOCKED);                          // 高频信道Socket接收数据包
    void highFreqChannelHandle(uint8_t *bit_seq, uint64_t len);                     // 高频信道处理数据包

    bool MAC_NetworkLayerHasPacketToSend(msgFromControl *busin);

    // 网管信道相关函数
    // void lowFreqSendThread();
    bool MacDaatNetworkHasLowFreqChannelPacketToSend(msgFromControl *busin);
    bool getLowChannelBusinessFromQueue(msgFromControl *busin);
    void macDaatrSocketLowFreq_Send(uint8_t *data, uint32_t len);
    void macDaatrSocketLowFreq_Recv(bool IF_NOT_BLOCKED);

    // 层间缓冲区操作函数 (W)
    void macToNetworkBufferHandle(void *data, uint8_t type, uint16_t len);
    void processPktFromNet(msgFromControl *MFC_temp);
    void processNodeNotificationFromNet(NodeNotification *temp);
    void processLinkAssignmentFromNet(LinkAssignment *LinkAssignment_data, int linkNum);
    // 层间缓冲区操作函数 (R)
    void networkToMacBufferHandle(uint8_t *rBuffer_mac);
    void networkToMacBufferReadThread();
};

uint64_t getTime();
void printTime_ms();
void printTime_us();
void printTime_s();
#endif