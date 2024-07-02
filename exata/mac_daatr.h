#ifndef MAC_DAATR_H
#define MAC_DAATR_H

// 约定网管节点id为1, 网关节点id为2, 其余3-20节点为普通节点

// 子网信息
#define GATEWAY 1             // 此处为判断是否为网关节点, 若为网关节点, 则置1, 若不是, 置0, 默认为0
#define SUBNET_NODE_NUMBER 20 // 当前节点可与相同子网通信的数量(可变)
#define FREQUENCY_DIVISION_MULTIPLEXING_NUMBER 10 // 子网内通信频分复用允许最大复用数(1/2,向下取整)
#define SUBNET_NUM 10         // 子网数
#define SUBNET_NODE_FREQ_NUMBER 20 //跳频图案可支持最大节点数（不可变！！！！）

// 网管信道
#define MANA_SLOT_DURATION 20                 // 网管信道时隙时长(单位: ms)
#define MANAGEMENT_SLOT_NUMBER_LINK_BUILD 25  // 建链阶段网管信道时隙表包含时隙数
#define MANAGEMENT_SLOT_NUMBER_OTHER_STAGE 50 // 其他阶段阶段网管信道时隙表包含时隙数
#define MANAGEMENT_ACCESS_SLOT_NUMBER 2       // 网管信道接入时隙数
#define MANA_CHANNEL_PRIORITY 10              // 网管信道业务优先级数量
#define MANA_MAX_BUSINESS_NUMBER 10           // 网管信道每一个优先级可允许容纳的业务量
#define MANA_CHANNEL_SPEED 4                  // 网管信道信息传输速率(单位: kbps)
#define END_LINK_BUILD_TIME 1000              // 结束建链阶段时间（单位：ms）

// 随遇接入相关
#define ACCESS_REQUREST_WAITING_MAX_TIME 200                    // 节点等待接入请求回复最长时间（单位：ms）
#define ACCESS_SUBNET_FREQUENCY_PARTTERN_WAITTING_MAX_TIME 3000 // 等待接收全网跳频图案的最大时间
#define MAX_RANDOM_BACKOFF_NUM 7                                // 最大接入退避时隙

// 业务信道
#define TRAFFIC_SLOT_DURATION 2.5                 // 业务信道时隙时长(单位: ms)
#define TRAFFIC_SLOT_NUMBER 400                   // 业务信道时隙表包含时隙数(执行阶段)
#define TRAFFIC_CHANNEL_PRIORITY 4                // 业务信道业务优先级数量

#define TRAFFIC_MAX_BUSINESS_NUMBER 60            // 业务信道每一个优先级可允许容纳的业务量

#define MAX_WAITING_TIME 100000              // 数据包在队列中的最大等待时间(ms)
#define LINK_SETUP_GUARD_TIME 100          // 建链阶段保护时间(在网管节点开始广播(20ms)后80ms开始业务信道的建链)
#define LINK_SETUP_INTERVAL 500            // 两次建链间的间隔时间(指从第一次开始发建链请求到第二次开始)
#define SENDING_SLOTTABLE_PREPARE_TIME 500 // 在收到链路构建需求事件后 发送时隙表

// 频谱感知与跳频序列
#define NSUM 50                               // 与产生正态分布数有关, 该值越大, 符合正态分布精度值越大(中心极限定理法)
#define INTERFEREENCE_FREQUENCY_THRESHOLD 0.01 // 使用频段干扰比例, 若超过此门限, 则进入频率调整阶段
#define TOTAL_FREQ_POINT 501                  // 总频点
#define M_SEQUENCE_LENGTH 9
#define FREQUENCY_COUNT 25
#define MIN_FREQUENCY 0
#define MAX_FREQUENCY 500
#define MIN_FREQUENCY_DIFFERENCE 15
#define NARROW_BAND_WIDTH 30

// 时延与周期相关
#define BEAM_MATAINMENCE_PERIOD 930        // 波束维护周期(单位: ms)
#define SPEC_SENSING_PERIOD 1000            // 判断是否进入频率调整阶段周期(单位: ms)
#define SIMULATION_RESULT_SHOW_PERIOD 1000 // 显示仿真结果周期(单位: ms)
#define LINK_STATE_PERIOD 3000             // 发送链路状态信息周期(单位: ms)
#define SEND_NODE_POSITION_PERIOD 8000     // 向网络层上传节点位置信息周期(单位: ms)

// 波束维护模块
#define RADIS_A 6378137.0    //(单位m)基准椭球体长半径
#define RADIS_B 6356752.3142 //(单位m)基准椭球体短半径

// 其他
#define TGS_THRESHOLD 80
#define MAC_HEADER1_LENGTH 25 // 链路层PDU1大小(单位: byte)
#define MAC_HEADER2_LENGTH 17 // 链路层PDU2大小（单位：byte）

#include <stdint.h>

enum
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

enum MacState
{
    // Mac层节点当前所处状态  分为初始化(Mac_Initialization)、调整(Mac_Adjustment)、执行(Mac_Execution)
    Mac_Initialization = 0,
    Mac_Adjustment_Slot,     // 时隙调整
    Mac_Adjustment_Freqency, // 频率调整
    Mac_Execution,
    Mac_Access // 接入阶段
};

enum NodeType
{
    // 节点类型  分为网管节点(Node_Management)、网关节点(Node_Gateway)、普通节点(Node_Ordinary)
    Node_Management = 0,
    Node_Gateway,
    Node_Standby_Gateway,
    Node_Ordinary
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

typedef struct MacHeader
{ // Mac层包头PDU1
    unsigned int PDUtype : 1;
    unsigned int is_mac_layer : 2; // 0上层数据包+频谱感知; 1本层数据; 2-3重传包(时隙表&跳频图案)
    unsigned int backup : 5;
    unsigned short packetLength;
    unsigned int srcAddr : 10;
    unsigned int destAddr : 10;
    unsigned short seq;
    unsigned long long mac_backup : 44;
    unsigned int mac_headerChecksum : 8;
    unsigned long long mac_pSynch;
    unsigned int mac : 24;
} MacHeader;

typedef struct MacHeader2
{                                                    // Mac层包头PDU2
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

typedef struct nodeLocalNeighListPacket
{
    vector<nodeLocalNeighList *> *node_link_state;
} nodeLocalNeighListPacket;

typedef struct Management_Channel_Information
{
    unsigned short SendOrReceive;      // 当前收发指示(数值, 1表示发送, 2表示接收, 0表示不收不发)
    unsigned short transmitSpeedLevel; // 传输速率档位(数值=1)
} Management_Channel_Information;

typedef struct Status_Information_To_Phy
{
    unsigned short SendOrReceive;                      // 当前收发指示(数值, 1表示发送, 2表示接收, 0表示不收不发)
    unsigned short antennaID;                          // 天线编号, 取值范围[1-5](5根天线)
    short int azimuth;                                 // 波束方位角, 值域为[-32768~32767], 物理量范围为[-90°, 90°], 量化精度为180°/216
    short int pitchAngle;                              // 波束俯仰角, 值域为[-32768~32767], 物理量范围为[-90°, 90°], 量化精度为180°/216
    unsigned short transmitSpeedLevel;                 // 传输速率档位, 取值范围为[1-7]
    unsigned short frequency_hopping[FREQUENCY_COUNT]; // 跳频图案(当前发表或接收跳频序列), 每个元素取值范围为[1-1024]

    unsigned short dest_node; // 仅调试使用 (正式使用时可删)
} Status_Information_To_Phy;

typedef struct LinkAssignment_single
{
    unsigned short begin_node;    // 链路发送节点ID
    unsigned short end_node;      // 链路接收节点ID
    unsigned short type;          // 业务类型, 取值范围[1-16]
    unsigned short priority;      // 业务优化级, 取值范围为[1-16]
    float size;                   // 业务量大小(单位: Byte); 物理量范围为[0-1MB], 取值范围为[0-1048576]
    unsigned short QOS;           // QOS需求; 物理量范围为[0-5s], 取值范围为[0-2000], 时延=取值*2.5ms,
    unsigned short begin_time[3]; // 业务开始时间(每行第一位为时, 取值范围为[1-24], 第二位为分, 取值范围为[0-60], 第三位为秒, 取值范围为[0-60]);
    unsigned short end_time[3];   // 业务结束时间(每行第一位为时, 取值范围为[1-24], 第二位为分, 取值范围为[0-60], 第三位为秒, 取值范围为[0-60])
    unsigned short frequency;     // 业务频率, 物理量范围为[2.5ms/次-5s/次], 取值范围为[1-2000], 业务频率=取值*2.5ms/次

    int data_slot_per_frame; // 指示该LA每帧需要的时隙数
    double distance;         // 指示收发节点间的距离, 通过Save_Position数组实现
} LinkAssignment_single;

typedef struct BuildLink_Request
{
    // 建链请求数据包
    unsigned short nodeID;     // 请求建链节点ID
    unsigned int if_build : 6; // 建链请求是否被允许  1:允许建链  0: 不允许建链
} BuildLink_Request;

typedef struct if_need_change_state
{
    // 是否进入调整状态, 此结构体由网管节点通过网管信道广播发送
    unsigned int state; // 0:当前保持原状态  1: 进入时隙调整阶段  2: 进入频率调整阶段  3: 暂无意义
} if_need_change_state;

typedef struct spectrum_sensing_struct
{
    // 频谱感知结果结构体
    unsigned int spectrum_sensing[TOTAL_FREQ_POINT]; // 频谱感知结果(为一串0、1序列, 0代表此频段有干扰, 1代表此频段无干扰)
} spectrum_sensing_struct;

struct net_nei_one
{
    unsigned short nodeID;
    unsigned int nodeID_1[SUBNET_NODE_NUMBER - 1];
    unsigned int sec_num_1[SUBNET_NODE_NUMBER - 1];
};

typedef struct CPUFrequency
{
    int cpu;
} CPUFrequency;

typedef struct subnet_frequency_parttern
{
    unsigned int mana_node_hopping[500];                                   // 网管节点跳频图案
    unsigned int subnet_node_hopping[SUBNET_NODE_FREQ_NUMBER][FREQUENCY_COUNT]; // 子网各节点跳频图案
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

typedef struct mana_slot_management
{
    // 时频资源管理模块网管信道时隙表
    unsigned short state; // 时隙状态：|0:可发送飞行状态|1:可发送上层网络业务|2:发送网管节点通告消息|3:发送接入请求|4:接收接入请求
    //(废弃) 时隙状态:|0:可发送TX|1:可接收RX|2:不可收发|3:未定义|
    unsigned short nodeId; // 此时隙分配的节点ID
} mana_slot_management;

typedef struct mana_slot_traffic
{
    // 时频资源管理模块业务信道时隙表
    unsigned int state : 1; // 时隙状态:|0:可发送TX|1:可接收RX|2:不可收发|3:未定义|
    // int send; // 若时隙状态为接收, 此处存放发送节点ID
    // int recv; // 若时隙状态为可发送, 此处存放接收节点ID
    unsigned short send_or_recv_node : 10;
} mana_slot_traffic;

typedef struct Send_slottable
{
    mana_slot_traffic slot_traffic_execution_new[TRAFFIC_SLOT_NUMBER];
} Send_slottable;

typedef struct Send_freq_sequence
{
    // 网管节点向各节点发送跳频序列结构体
    unsigned int frequency_sequence[SUBNET_NODE_NUMBER][FREQUENCY_COUNT];
} Send_freq_sequence;

typedef struct Mana_Packet_Type
{                 // 网管信道数据包类型
    uint8_t type; // 0：无定义  1：随遇接入请求  2：同意随遇接入请求回复  3：拒绝接入请求恢复  4：普通节点飞行状态信息  5：网管节点飞行状态信息  6：网管节点通告消息
} Mana_Packet_Type;

typedef struct mana_business_mana
{
    int priority;
    double data_kbyte_sum;
    int waiting_time;
    msgFromControl busin_data;
} mana_business_mana;

typedef struct mana_business_traffic
{                                                                                       // 业务信道业务
    int recv_node;                                                                      // 下一跳节点
    mana_business_mana business[TRAFFIC_CHANNEL_PRIORITY][TRAFFIC_MAX_BUSINESS_NUMBER]; // 接收节点对应的各优先级业务
    double distance;                                                                    // 两节点距离(收发节点)
    int state;                                                                          // 0: 子网内通信  1: 子网间通信
} mana_business_traffic;

typedef struct A_ang
{
    float el;    // 天线俯仰角
    float az;    // 天线方位角
    float b_ang; // 波束角(波束指向向量与y轴正向的夹角)
    float dys;   // 天线坐标系下的dys
    int index;
} A_ang;

typedef struct struct_daatr_stats_str
{
    int PDU_GotUnicast;  // 发出的PDU数量
    int PDU_SentUnicast; // 收到的PDU数量
    int UDU_GotUnicast;  // 发出的UDU数量
    int UDU_SentUnicast; // 收到的上层的UDU数量

	uint64_t access_begin_time;//随遇接入开始时间(单位：ms)
	uint64_t access_end_time;//随遇接入结束时间(单位：ms)
	uint64_t slot_adjust_begin_time;//时隙调整开始时间(单位：ms)
	uint64_t slot_adjust_end_time;//时隙调整结束时间(单位：ms)
	uint64_t freq_adjust_begin_time;//频率调整开始时间(单位：ms)
	uint64_t freq_adjust_end_time;//频率调整结束时间(单位：ms)
	uint64_t traffic_output;//业务信道吞吐量(单位：byte)
	uint64_t mac_network_overhead;//mac层网络开销(单位：byte)
	float node_freq_interfer_ratio_before[SUBNET_NODE_FREQ_NUMBER];//跳频序列调整前 子网各节点频谱干扰比例
	float subnet_freq_interfer_ratio_before;//跳频序列调整前子网频谱干扰比例 
	float node_freq_interfer_ratio[SUBNET_NODE_FREQ_NUMBER];//子网各节点频谱干扰比例
	float subnet_freq_interfer_ratio;//子网频谱干扰比例
	float node_narrow_band_interfer_ratio_before[SUBNET_NODE_FREQ_NUMBER];//更新前子网各节点窄带干扰比例
	float node_narrow_band_interfer_ratio[SUBNET_NODE_FREQ_NUMBER];//子网各节点窄带干扰比例
	vector<vector<int>> narrow_band_interfer_nodeID;//子网有窄带干扰的节点ID组合

	int msgtype3_recv_network;
	int msgtype3_recv; //
	int msgtype3_sent1;//
	int msgtype3_sent2;//
	int msgtype3_sent3;//
	int msgtype3_sent_network1;//
	int msgtype3_sent_network2;//
	int msgtype3_sent_network3;//
    int Missed_pkts; // 因为长度过大而丢弃的包
} DaatrStats;

typedef struct mana_channel_business
{                              // 网管信道业务队列
    int state;                 // 业务状态（0：此位置未被占用  1：此位置被占用）
    msgFromControl busin_data; // 网络层业务
} mana_channel_business;

typedef struct mana_access_reply
{                                                    // 网管节点接入请求回复
    unsigned int mana_node_hopping[FREQUENCY_COUNT]; // 网管节点跳频图案
    unsigned char slot_location[5];                  // 节点位置(0-49)(60为空，忽略即可)
    unsigned char slot_num;                          // 分配的时隙数量
} mana_access_reply;

typedef struct patam_
{
   int nodeId;//节点ID
   double value;//干扰比例%
} ParamData;

typedef struct patamstr_
{
	int nodeId;//节点ID
	string value;//与此节点互干扰的节点ID字符串，中间以“,”隔开，例：“2，3 ”
} ParamDataStr;



/*
*
********************************************************************************
类名: MacDaatr_struct_converter
功能: 将结构体或 0/1 序列转化为 0/1 序列或结构体
备注: 构造函数输入值为转化类型, 默认为 0, 其中:
0: 不做任何转换;
1: PDU1;
2: PDU2;
3: 飞行状态信息;
4: 网管节点通告消息;
5: 频谱感知结果;
6: 建链请求;
255: 混合消息(包括前几类的和, 同时可能包括网络层数据包), 类型 5 结构体指针为空
********************************************************************************
*
*/
class MacDaatr_struct_converter
{
private:
    uint8_t *bit_sequence_ptr_;
    uint8_t *Daatr_struct_ptr_;
    unsigned char type_;
    int length_; // 比特序列长度, 单位: 字节

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
    int get_length();                                                 // 返回长度
    int get_sequence_length(uint8_t *bit_seq);                        // 返回此比特序列长度(从包头得到) type: 0 PDU1包  1 PDU2
    int get_PDU2_sequence_length(uint8_t *pkt);                       // 返回PDU1包头类型数据包长度
    // bool daatr_MFC_to_0_1(msgFromControl *packet);
    // bool daatr_MFC_to_0_1(msgFromControl *packet);
    bool daatr_MFCvector_to_0_1(vector<uint8_t> *MFC_vector_temp, int packetlength);
    vector<uint8_t> *daatr_0_1_to_MFCvector();
    vector<uint8_t> *Get_Single_MFC();

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

typedef struct struct_mac_daatr_str
{
    // 总体相关
    MacData *myMacData;
    DaatrStats stats;
    clocktype timerExpirationTime;

    // 节点信息
    enum NodeType node_type;      // 当前节点类型(默认为普通节点)
    enum NodeAccess access_state; // 当前节点接入状态
    enum MacState state_now;      // 当前节点状态(默认为初始化状态)

    NodeNotification node_notification; // 节点身份配置信息
    CPUFrequency cpu_freq;              // CPU频率

    // 随遇接入相关
    unsigned char access_backoff_number; // 接入退避数目
    unsigned int waiting_to_access_node; // 等待接入的节点ID(网管节点专属)

    // 节点位置信息
    // net_nei_one nei_one;
    FlightStatus subnet_other_node_position[SUBNET_NODE_FREQ_NUMBER - 1]; // 子网其他节点位置信息
    FlightStatus local_node_position_info;                           // 本地飞行状态信息

    // 子网信息
    int mana_node;                                                      // 本子网网管节点ID
    int gateway_node;                                                   // 本子网网关节点ID
    int standby_gateway_node;                                           // 本子网备用网关节点ID
    int subnet_topology_matrix[SUBNET_NODE_NUMBER][SUBNET_NODE_NUMBER]; // 拓扑矩阵
    uint16_t Forwarding_Table[SUBNET_NODE_FREQ_NUMBER][2];//转发表
	char Forwarding_Table_int_access_sign;//收到空转发表数量
    // vector<nodeLocalNeighList *> node_local_neigh_list_ptr; // 链路状态指针

    // 业务信道相关
    int currentStatus;
    int currentSlotId;
    int nextStatus;
    int nextSlotId;
    int numSlotsPerFrame;
    int TX_Slotnum;
    clocktype slotDuration;
    Message *timerMsg;

    mana_slot_traffic slot_traffic_execution[TRAFFIC_SLOT_NUMBER];      // 业务信道时隙表(执行阶段)
    mana_slot_traffic slot_traffic_initialization[TRAFFIC_SLOT_NUMBER]; // 业务信道时隙表(建链阶段)
    mana_business_traffic traffic_channel_business[SUBNET_NODE_FREQ_NUMBER]; // 业务信道的待发送队列

    mana_slot_traffic slot_traffic_execution_new[SUBNET_NODE_FREQ_NUMBER][TRAFFIC_SLOT_NUMBER]; // 存储子网内所有节点的业务信道时隙表

    // 网管信道相关
    Message *ptr_to_mana_channel_period_inquiry;                                          // 指向网管信道定期查询事件指针
    mana_slot_management slot_management[MANAGEMENT_SLOT_NUMBER_LINK_BUILD];              // 建链阶段网管信道时隙表
    mana_slot_management slot_management_other_stage[MANAGEMENT_SLOT_NUMBER_OTHER_STAGE]; // 其他阶段网管信道时隙表
    mana_channel_business mana_channel_business_queue[MANA_MAX_BUSINESS_NUMBER]; // 网管信道待发送业务队列
    unsigned int mana_channel_frame_num;                                         // 网管信道已过去几帧（一帧以50个时隙为基准，即一帧为1s）
    int mana_slot_should_read;                                                   // 当前应查看网管信道时隙表第几个时隙
	int mana_slottable_synchronization; //网管信道时隙表同步
	bool if_mana_slottable_synchronization;
	bool if_receive_mana_flight;//是否收到网管节点飞行状态信息


    // 网管信道信令flag
    bool has_received_chain_building_request; // 建链请求是否已收到一次
    bool has_received_slottable;              // 时隙表是否已收到一次
    bool has_received_slottable_ACK;          // 时隙表ACK是否已收到一次
    bool has_received_sequence;
    bool has_received_sequence_ACK;
    int link_build_times;   // 是否已经建链一次
    char need_change_stage; // 网管节点的阶段转换标志(默认为0) 1 表示时域调整 2 表示频域调整
    bool link_build_state;  // MAC层是否可以收发

    // 频域信息
    unsigned int frequency_sequence[SUBNET_NODE_FREQ_NUMBER][FREQUENCY_COUNT];        // 生成子网内调频序列(仅网管节点有)
    unsigned int subnet_frequency_sequence[TOTAL_FREQ_POINT];                    // 子网所使用频段(共501频点, 对应频点值为1代表使用此频点, 0为不使用)(网管节点独有)
    unsigned int spectrum_sensing_node[TOTAL_FREQ_POINT];                        // 本节点频谱感知结果
    unsigned int spectrum_sensing_sum[SUBNET_NODE_FREQ_NUMBER][TOTAL_FREQ_POINT + 1]; // 总频谱感知结果, 只存在于网管节点.最后一列每一行代表一个节点, 若为0, 则说明没有接收到此节点的频谱感知结果, 若为1, 则说明接收到了
    unsigned int unavailable_frequency_points[TOTAL_FREQ_POINT];                 // 不可用频点集合(1: 频点未干扰 0: 频点受到干扰)
	Message *ptr_to_period_judge_if_enter_adjustment;//定期判断是否进入频率调整阶段指针
} MacDataDaatr;

bool MAC_NetworkLayerHasPacketToSend(Node *node, int interfaceIndex, msgFromControl *busin);

void MacDaatrLayer(Node *node, int interfaceIndex, Message *msg);

void MacDaatrInit(Node *node,
                  int interfaceIndex,
                  const NodeInput *nodeInput,
                  const int subnetListIndex,
                  const int numNodesInSubnet);

void MacDaatrReceivePacketFromPhy(Node *node, MacDataDaatr *macdata_daatr, Message *msg);

void MacDaatrNetworkLayerHasPacketToSend(Node *node, MacDataDaatr *macdata_daatr);

void MacDaatrReceivePhyStatusChangeNotification(Node *node,
                                                MacDataDaatr *macdata_daatr,
                                                PhyStatusType oldPhyStatus,
                                                PhyStatusType newPhyStatus);

void MacDaatrFinalize(Node *node, int interfaceIndex);
int Search_Next_Hop_Addr(MacDataDaatr *macdata_daatr, int destAddr);
bool MacDaatr_judge_if_could_send_config_or_LinkRequist(Node* node, int interfaceIndex);
#endif