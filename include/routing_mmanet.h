#ifndef MMANET_H
#define MMANET_H

#include "common_struct.h"
#include <signal.h>
#include <vector>
using namespace std;

#define MMANET_MFC_DATASIZE 145             // msgFromControl的data字节数
#define MMANET_TOUPPERMSG_LIFECYCLE 1000000 // 重组后的格式化消息的生存周期, 单位为微秒
#define INF 0xffff                          // 表示无限大，用于邻接矩阵的初始跳数

typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

// #define ANY_DEST        0xffff //16位地址
#define ANY_DEST 0b1111111111 // 10位地址

#define INPUT_N 5 // 与 参数文件形成的LSTM模型中参数（lstm_module.outNodeNum）一致

//-----------------------------------------------------------------------------------------------------------
// 以下部分代码定义了各种所需结构体

/*
*
********************************************************************************
结构体名：NodeTime_lag
结构体:
功能：节点与对应的时间间隔
********************************************************************************
*
*/
struct NodeTime_lag {
    uint16_t nodeAddr;
    vector<double> timelag;
};

/*
*
********************************************************************************
结构体名：nodeLocalForwardingTable
结构体: 本地路由表
功能：定义本地路由表内容
********************************************************************************
*
*/
struct nodeLocalForwardingTable {
    uint16_t destAddr; // 目的地址
    uint16_t nextHop;  // 下一跳地址
    uint16_t hop;      // 跳数
    uint16_t delay;    // 时延
};

/*
*
********************************************************************************
结构体名：routingTableMessage
结构体: 路由表信息
功能：记录Dijkstra算法计算后的信息，便于整理路由表时使用
********************************************************************************
*
*/
struct routingTableMessage {
    vector<int> dist; // dist[i]记录节点i到源节点最短路径的长度
    vector<int> path; // 记录源节点到目的节点的完整路径（不包括源节点）
    int count;        // path的长度
};

/*
*
********************************************************************************
结构体名：sn_nodeNetNeighList
结构体: 全网邻居表+sn定义
功能：记录全网邻居表及对应的sn号的格式
********************************************************************************
*
*/
struct sn_nodeNetNeighList {
    uint16_t nodeAddr; // 节点地址，表明是谁的地址
    unsigned int SN;
};

struct tst_mapping_table {
    NodeAddress nodeAddr;
    NodeAddress Node_ID;
};

/*
*
********************************************************************************
结构体名：formatMessageControl
结构体: 重组后格式化消息的管理
功能：管理重组后的格式化消息的重复发送
********************************************************************************
*
*/
struct formatMessageControl {
    clocktype lastMsgFromControlTimer; // 用于存储接收一条格式化消息的业务数据包的更新时间
    bool reorganizationCompleted;      // 用于存储该条格式化消息是否已经重组完成且发送
};

/*
*
********************************************************************************
结构体名：MMANETData
结构体: routing_mmanet模块所需的数据类型
功能：管理routing_mmanet模块节点各自所需的数据
********************************************************************************
*
*/
typedef struct struct_network_mmanet_str {
    // clocktype LastHelloBroadCastTime;//记录上一次Hello包发送时间以确定本次Hello包是否该发送
    int32_t netDiameter;
    clocktype nodeTraversalTime;
    clocktype MMANETRouteTimeout;
    clocktype LastHelloBroadCastTime; // 记录上一次Hello包发送时间以确定本次Hello包是否该发送
    // MMANETInterfaceInfo* iface;

    bool ReceiveLocalLinkMsg_Flg; // 用于判断是否开始发送本地链路状态信息
    bool BC_Process_Init_Flg;     // 用于判断是否已经开启了广播流程的标志位
    bool Hello_Init_Flg;          // 用于判断hello包是否重复
    bool Hello_Ack_Flg;           // 用于判断此时是否是hello包探测时期，在实现非同步的时候要用
    bool time_Flg;                // 用于判断是否为第一次记录时间
    bool List_Flg;                // 用于判读邻接表是否改变
    bool Msg_Flg;                 // 用于判断接收到的其它邻接表是否改变
    bool NetConFlag;              // 用于判断节点网络是否构建成功
    bool Lstm_Flg;                // 作为预测开启的开关
    unsigned int seq_Num;         // 给业务包分配序列号
    unsigned int signalOverhead;  // 本节点的信令开销

    uint16_t nodeIdentity;                   // 存储节点类型(身份)
    uint16_t nodeAddr;                       // 节点地址
    uint16_t groupID;                        // 所属群ID
    uint16_t intragroupcontrolNodeId;        // 所属群控制节点ID
    uint16_t reserverdintracontrolNodeId;    // 所属群备用控制节点ID
    uint16_t intergroupgatewayNodeId;        // 所属群间网关节点ID
    uint16_t resrvedintergroupgatewayNodeId; // 备用网关节点ID

    vector<nodeLocalNeighList *> neighborList;       // 存储本地节点邻居表
    vector<nodeLocalNeighList *> disu_neighborList;  // 存储低速本地节点邻居表
    vector<nodeLocalNeighList *> gaosu_neighborList; // 存储高速本地节点邻居表
    vector<nodeLocalNeighList *> temp_neighborList;  // 临时存储本地节点邻居表，用于表项更新
    vector<nodeNetNeighList *> NetNeighList;         // 存储高速全网邻居表
    vector<nodeNetNeighList *> disu_NetNeighList;    // 存储低速全网邻居表
    vector<vector<uint16_t>> neighMatrix;            // 存储邻接矩阵
    vector<uint16_t> NodeAddrToIndex;                // 邻接矩阵中下标与节点地址的映射表

    vector<vector<uint16_t>> Layer2_ForwardingTable;      // 发给层2的转发表
    vector<nodeLocalForwardingTable *> LFT_Entry;         // 用于节点存储本地路由表
    vector<vector<nodeNetForwardingTable *>> NetRT_Entry; // 用于节点存储全网路由表
    vector<NodeAddress> DamagedNodes;                     // 用于存储毁伤节点表

    vector<vector<msgFromControl *>> ToNetwork_msgFromControl; // 用于存储链路层发送的业务数据包以便进行重组
    vector<formatMessageControl> FormatMessageControl;         // 用于管理重组后的格式化消息的重复发送
    vector<vector<msgFromControl *>> BC_msgFromControl;        // 用于存储接收广播数据包(高速的）
    vector<vector<msgFromControl *>> BC_msgFromControl2;       // 用于存储接收广播数据包（低速的）
    vector<msgFromControl *> one_msgFromComtrol;               // 用于存储接收新的广播数据(高速的）
    vector<msgFromControl *> one_msgFromComtrol2;              // 用于存储接收新的广播数据（低速的）
    vector<nodeLocalNeighList *> cur_neighborList;             // 用于接收链路层本地链路信息
    vector<nodeLocalNeighList *> old_neighborList;             // 用于存储旧的链路层本地链路信息

    // 以下为测试用映射表
    vector<tst_mapping_table *> mapping_table;
    // 使用哈希表，存储每次网内节点，以判断哪些节点毁伤
    vector<uint16_t> old_node_set;

    // 泛洪机制
    // 对于高速
    unsigned int sn;
    vector<sn_nodeNetNeighList *> sn_NetNeighList;
    // 对于低速
    unsigned int disu_sn;
    vector<sn_nodeNetNeighList *> disu_sn_NetNeighList;

} MMANETData;

//--------------------------------------------------------------------------------------------------
// 以上部分代码定义了各种所需结构体

/*
*
********************************************************************************
结构体名： MMANETInterfaceInfo;
函数: MMANET接口信息
功能：设置MMANET所使用的接口信息格式
备注：参考AodvInterfaceInfo
********************************************************************************
*
*/
// typedef struct str_mmanet_interface_info
// {
// 	Address address;
// 	UInt32 ip_version;
// 	//以下两个flg不清楚有没有用，先放着
// 	//BOOL MMANETeligible;
// 	BOOL MMFlag;

// } MMANETInterfaceInfo;

/*
*
********************************************************************************
函数名：RoutingReceiveFromSvc
功能：路由模块从服务层接收数据子线程的回调函数
********************************************************************************
*
*/
void *RoutingReceiveFromSvc(void *arg);

/*
*
********************************************************************************
函数名：RoutingReceiveFromNet
功能：路由模块从网络分布式管控模块接收数据子线程的回调函数
********************************************************************************
*
*/
void *RoutingReceiveFromNet(void *arg);

/*
*
********************************************************************************
函数名：RoutingReceiveFromMac
功能：路由模块从链路层接收数据子线程的回调函数
********************************************************************************
*
*/
void *RoutingReceiveFromMac(void *arg);

/*
*
********************************************************************************
函数名：MMANETInit
功能：对routing_mmanet模块数据结构MMANETData进行初始化
输入：MMANETData结构体指针
********************************************************************************
*
*/
void MMANETInit(MMANETData *mmanet);

/*
*
********************************************************************************
函数名：HandleFormatMsgFromSvc
功能：路由模块从上层（服务层）接收格式化消息，并对其进行相应处理
输入：接收到的格式化消息vector<uint8_t>数组
********************************************************************************
*
*/
void HandleFormatMsgFromSvc(vector<uint8_t> curDataPacket);

/*
*
********************************************************************************
函数名：GetMsgFromControl
功能：根据格式化消息得到分片数并生成对应个数业务数据包,以数组的形式存储
输入：格式化消息指针
********************************************************************************
*
*/
vector<msgFromControl *> *GetMsgFromControl(vector<uint8_t> *dataPacket);

/*
*
********************************************************************************
函数名：PackMsgFromControl
功能：封装业务数据包并添加CRC校验位，形成字节流
输入：业务数据包指针
********************************************************************************
*
*/
vector<uint8_t> *PackMsgFromControl(msgFromControl *packet);

/*
*
********************************************************************************
函数名：PackRingBuffer
功能：将数据包封装成层间接口数据帧，以便进行发送
输入：存放封装结果的uint8_t数组, 待写入数据的vector<uint8_t>数组指针， 包头的中的数据类型字段
********************************************************************************
*
*/
void PackRingBuffer(uint8_t buffer[], vector<uint8_t> *vec, uint8_t type);

/*
*
********************************************************************************
函数名：HandleMfcFromMac
功能：路由模块收到链路层发来的网络数据包，并对其进行相应处理
输入：接收到的网络数据包vector<uint8_t>数组引用
********************************************************************************
*
*/
void HandleMfcFromMac(vector<uint8_t> &curPktPayload);

/*
*
********************************************************************************
函数名：AnalysisLayer2MsgFromControl
功能：将uint8_t数组内对应的内容存入msgFromControl结构体
输入：待解析的uint8_t数组
********************************************************************************
*
*/
msgFromControl *AnalysisLayer2MsgFromControl(vector<uint8_t> *dataPacket);

/*
*
********************************************************************************
函数名：CRC16
功能：使用CRC-16/MODBUS计算CRC校验码
输入：uint8_t数组
********************************************************************************
*
*/
uint16_t CRC16(vector<uint8_t> *buffer);

/*
*
********************************************************************************
函数名：getCurrentTime
功能：通过gettimeofday获取当前时间，并转换为clocktype类型保存，单位为微秒
输入：无
********************************************************************************
*
*/
clocktype getCurrentTime();

/*
*
********************************************************************************
函数名：SendFromatMessageToUpperLayer
功能：将分片已经完全接收到的格式化消息进行重组并发送给服务层
输入：该格式化消息在存储数组中的下标号
********************************************************************************
*
*/
void SendFromatMessageToUpperLayer(int k);

/*
*
********************************************************************************
函数名：CMP
功能：按照分片序号将业务数据包进行升序排列
输入：需要比较的两个业务数据包指针
********************************************************************************
*
*/
bool CMP(msgFromControl *a, msgFromControl *b);

/*
*
********************************************************************************
函数名：SetTimer
功能：设置一个定时器
输入：所设置定时器的秒数，毫秒数，定时器的编号（自己定义，以便在回调函数区分）
********************************************************************************
*
*/
void SetTimer(long sc, long ms, int number);

/*
*
********************************************************************************
函数名：timer_callback
功能：SIGUSR1信号的回调函数
输入：捕捉到信号时，内核自动调用
********************************************************************************
*
*/
void timer_callback(int signum, siginfo_t *si, void *uc);

/*
*
********************************************************************************
函数名：SetTimer2
功能：设置一个定时器,设置T2定时器
输入：所设置定时器的秒数，毫秒数，定时器的编号（自己定义，以便在回调函数区分）
********************************************************************************
*
*/
void SetTimer2(long sc, long ms, int number);

/*
*
********************************************************************************
函数名：timer_callback2
功能：SIGUSR2信号的回调函数
输入：捕捉到信号时，内核自动调用
********************************************************************************
*
*/
void timer_callback2(int signum, siginfo_t *si, void *uc);

/*
*
********************************************************************************
函数名：Count_List
功能：根据全网邻居表计算路由表，并将其他模块所需信息发送给对应模块
输入：
********************************************************************************
*
*/
void Count_List();

/*
*
********************************************************************************
函数名：FillNodeAddrToIndexTable
功能：构建邻接矩阵中下标与节点地址的映射表
输入：
********************************************************************************
*
*/
void FillNodeAddrToIndexTable();

/*
*
********************************************************************************
函数名：FillNeighMatrix
功能：通过存储的全网邻居表构建邻接矩阵
输入：
********************************************************************************
*
*/
void FillNeighMatrix();

/*
*
********************************************************************************
函数名：ReturnSNinLocalNeighMatrix
功能：根据节点地址确定其在自己存储的邻接矩阵中的下标
输入：节点地址
输出：横坐标（也是纵坐标，因为矩阵对称）
********************************************************************************
*
*/
int ReturnSNinLocalNeighMatrix(uint16_t nodeAddr);

/*
*
********************************************************************************
函数名：ReturnAddrinLocalNeighMatrix
功能：根据邻接矩阵中的下标得到该下标对应的节点地址
输入：横坐标（也是纵坐标）
输出：对应的节点地址
********************************************************************************
*
*/
int ReturnAddrinLocalNeighMatrix(size_t coor);

/*
*
********************************************************************************
函数名：DijkstraAlgorithm
功能：根据源节点的邻接矩阵计算源节点到其他各节点的最短路径，并将路由信息填入本地
或者全网路由表
********************************************************************************
*
*/
void DijkstraAlgorithm();

/*
*
********************************************************************************
函数名：FillLocalForwardingTable
功能：根据Dijkstra算法计算后的信息，填入本地路由表表项
********************************************************************************
*
*/
void FillLocalForwardingTable(routingTableMessage *newRTM, int dest_coor);

/*
*
********************************************************************************
函数名：FillNetForwardingTable
功能：根据Dijkstra算法计算后的信息，填入全网路由表表项
********************************************************************************
*
*/
void FillNetForwardingTable(routingTableMessage *newRTM, int src_coor, int dest_coor);

/*
*
********************************************************************************
函数名：CMP_NetRT_Entry
功能：同一源节点的路由项按照其目的节点地址大小进行升序排列
输入：需要比较的两个nodeNetForwardingTable指针
********************************************************************************
*
*/
bool CMP_NetRT_Entry(nodeNetForwardingTable *a, nodeNetForwardingTable *b);

/*
*
********************************************************************************
函数名：CleanRoutingTable
功能：清空上一次生成的本地路由表和全网路由表
********************************************************************************
*
*/
void CleanRoutingTable();

/*
*
********************************************************************************
函数名：FillToLayer2_ForwardingTable
功能：填充发给层2的转发表（目的地址+下一跳）
********************************************************************************
*
*/
void FillToLayer2_ForwardingTable();

/*
*
********************************************************************************
函数名：SendForwardingTable
功能：发送全网路由表给网管模块
********************************************************************************
*
*/
void SendForwardingTable();

/*
*
********************************************************************************
函数名：SendTopologyTable
功能：发送全网邻居表（拓扑表）给网管模块
********************************************************************************
*
*/
void SendTopologyTable();

/*
*
********************************************************************************
函数名：FindDamagedNodes
功能：找损伤节点
********************************************************************************
*
*/
void FindDamagedNodes();

/*
*
********************************************************************************
函数名：SendDamageReport
功能：发送毁伤感知信息给网管模块
********************************************************************************
*
*/
void SendDamageReport();

/*
*
********************************************************************************
函数名：SendLayer2_ForwardingTable
功能：发送转发表信息给链路层
********************************************************************************
*
*/
void SendLayer2_ForwardingTable();

/*
*
********************************************************************************
函数名：UpdateNetNeighList
功能：更新全网拓扑表，保持一致性
输入：最新的全网邻居表条目
********************************************************************************
*
*/
void UpdateNetNeighList(nodeNetNeighList *newNetNeighListMember);

/*
*
********************************************************************************
函数名：CMP_neighborList
功能：按照节点序号将本地邻居表进行升序排列
输入：需要比较的两个本地邻居表指针
********************************************************************************
*
*/
bool CMP_neighborList(nodeLocalNeighList *a, nodeLocalNeighList *b);

/**
*
********************************************************************************
事件：MSG_ROUTING_ReceiveLocalLinkMsg
事件描述：从链路层接收链路状态信息
处理：
备注：
********************************************************************************
*
*/
void MSG_ROUTING_ReceiveLocalLinkMsg(vector<uint8_t> &curPktPayload, uint16_t len);

/*
*
********************************************************************************
函数名：CMP_NetNeighList
函数:
功能：按照节点序号将全网邻居表进行升序排列
输入：需要比较的两个本地全网邻居表指针
备注：
********************************************************************************
*
*/
bool CMP_NetNeighList(nodeNetNeighList *a, nodeNetNeighList *b);

/*
*
********************************************************************************
函数名：MMANETBroadcastHello
函数: MMANET开始Hello流程
功能：参考Aodv，开始Hello流程
输入：节点指针，MMANETData
返回：无
备注：测试用函数会通过向每一个存在的节点发送测试用Hello包消息已完成广播流程
********************************************************************************
*
*/
void MMANETBroadcastHello();

/*
*
********************************************************************************
函数名：MMANETSendPacket
函数: MMANET向链路层发包
功能：将网络层产生的封装完毕网管数据包等交给链路层以在网络中发送
输入：节点指针，Msg指针，源地址，目标地址，下一跳地址
返回：无
备注：测试用函数的源地址目标地址暂时以节点号代替，之后可以更改，此外，测试用函数
默认场景中节点间均一跳可达，因此下一跳地址都是目的地址，但广播时目的置全一地址，
因此别忘了判断是否是广播
********************************************************************************
*
*/
void MMANETSendPacket(vector<uint8_t> *curPktPayload, NodeAddress srcAddr, NodeAddress destAddr, int i);

/*
*
********************************************************************************
函数名：MMANETBroadcastMessage
函数: MMANET开始广播流程
功能：参考Aodv，开始广播流程
输入：节点指针，MMANETData
返回：无
备注：测试用函数会通过向每一个存在的节点发送测试用广播包消息已完成广播流程
********************************************************************************
*
*/
void MMANETBroadcastMessage(char *NeighList, int a);

/*
*
********************************************************************************
函数名：saveRESULTfile
函数:
功能：将结果输出为文本形式
输入：
备注：
********************************************************************************
*
*/
void saveRESULTfile(vector<double> data1, vector<double> data2, vector<double> data3, string filename, MMANETData *mmanet);

/*
*
********************************************************************************
函数名：AnalysisLayer2MsgLocalLinkState
功能：将字节流转回成链路数据包格式
********************************************************************************
*
*/
ReceiveLocalLinkState_Msg *AnalysisLayer2MsgLocalLinkState(vector<uint8_t> *dataPacket);

void *SvcToAll(void *arg);

#endif