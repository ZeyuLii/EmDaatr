#ifndef NETWORK_H
#define NETWORK_H
#include "common_struct.h"
#include <list>
#include <signal.h>

/*
*
********************************************************************************
函数名：NetReceiveFromSvc
功能：网管模块从服务层接收数据子线程的回调函数
********************************************************************************
*
*/
void *NetReceiveFromSvc(void *arg);

/*
*
********************************************************************************
函数名：NetReceiveFromRouting
功能：网管模块从网络分布式管控模块接收数据子线程的回调函数
********************************************************************************
*
*/
void *NetReceiveFromRouting(void *arg);

/*
*
********************************************************************************
函数名：NetReceiveFromMac
功能：网管模块从链路层接收数据子线程的回调函数
********************************************************************************
*
*/
void *NetReceiveFromMac(void *arg);

void PackRingBuffer(uint8_t buffer[], void *data, uint16_t size, uint8_t type);

uint64_t analysisPtr(const vector<uint8_t> &msgPtr);

void SetTimerUser2(long sc, long ms, int number, void *ptr, int structSize);

void timer_callbackUser2(int signum, siginfo_t *si, void *uc);

/*------------------------network_translinkconfig.h------------------------------------*/
// 传输链路配置模块数据结构
struct LinkConfigData {
    NodeAddress nodeId;
    // unsigned char nodeIdentity;
    vector<vector<nodeNetForwardingTable *>> *netRTtablePtr;
    list<LinkAssignment *> linkAllocList;
    // LinkAssignment *linkAssList;
    //************ 新增链路运行状态***********************
    unsigned int linkStatus; // 标志链路运行状态
    //****************************************************
    LinkConfigData() { this->netRTtablePtr = nullptr; }
};

void LinkConfigInit(LinkConfigData *linkConfigPtr);

void HandleReceivedRTtable(const vector<uint8_t> &NetRT_EntryMsgPtr);

void CleanRTtabFromRout(vector<vector<nodeNetForwardingTable *>> *rtTablePtr);

void PrintRTTable(vector<vector<nodeNetForwardingTable *>> *rTtablePtr);

void HandleReceivedTaskInstruct(const vector<uint8_t> &task_MsgPtr);

void PrintFullRTPath(vector<uint16_t> *relayNodesPtr);

void BreakFullRoute(vector<uint16_t> *relayNodesPtr, TaskAssignment origTask);

void PrintLinkAssignment(LinkAssignment *taskPtr);

LinkAssignment *FindSameLinkInList(list<LinkAssignment *> *linkListPtr, unsigned short begin_node, unsigned short end_node);

void PrintTaskAssignment(TaskAssignment *taskPtr);

void PrintLinkAssList(list<LinkAssignment *> *linkListPtr);

void HandleControlMsgFromRoute(const vector<uint8_t> &control_MsgPtr);

void HandleTaskListFromOther(msgFromControl *controlMsgPtr);

vector<uint8_t> *PackMsgFromControl(msgFromControl *packet);

uint16_t CRC16(vector<uint8_t> *buffer);

msgFromControl *AnalysisLayer2MsgFromControl(vector<uint8_t> *dataPacket);

vector<uint8_t> *PackDataOfMsgFromControl(char *payloadStart);

char *AnalysisDataInMsgFromControl(vector<uint8_t> *dataPacket);

void TurnFloatToUint(float size, vector<uint8_t> *buffer);

void ClearTaskList(list<LinkAssignment *> *taskListPtr);

void CheckTaskListTimeout();

// 新增 链路运行状态接口
void UpdateLinkStatus(const vector<uint8_t> &linkStatusPtr);
void TestSendLinkStatus(); // 测试

// 新增输出到文件以及网络状态视图
void FileInit(const string filename);
void OutputTaskListToTxt(list<LinkAssignment *> *linkListPtr, const string filename);

// 链路构建需求汇总时msgFromControl的封装
vector<uint8_t> *LinkConfigAddHeader_controlmsg(NodeAddress destId, char *pktStart, int payLoadSize, int moduleName, int eventType);
/*------------------------network_translinkconfig.h------------------------------------*/

/*------------------------network_netview.h------------------------------------*/
// 转化坐标系的参数//
#define MY_PI 3.14159265359
#define Daita_h 0
static const double A = 6378137.0;
static const double INV_F = 298.257223563;
static const double F = 1.0 / INV_F;

// eccentricity
static const double E2 = 2.0 * F - F * F;

// 自身网络视图存储的一个邻居链路
struct LinkQuality {
    unsigned int nodeId;
    float linkUsageRate;
    unsigned short linkDelay;
    LinkQuality() {}
    LinkQuality(unsigned int id) : nodeId(id) {}
};

struct NodeSpeed {
    float speedX;
    float speedY;
    float speedZ;
};

struct NodePosition1 {
    float positionX;
    float positionY;
    float positionZ;
};

// 自身网络视图存储的一个节点及其邻居
struct NodeCondition {
    unsigned int nodeId;

    // from route
    unsigned int nodeType; // 节点身份
    list<LinkQuality *> neighborList;
    unsigned int queueLength;
    unsigned int usedLength;
    float queueLoadRate;

    // from MAC
    NodeSpeed thisSpeed;
    NodePosition1 thisPos;

    NodeCondition(unsigned int id) {
        nodeId = id;
        nodeType = NODENORMAL; // 节点身份构造默认为普通节点
    }
    NodeCondition() {
        nodeType = NODENORMAL; // 节点身份构造默认为普通节点
    }
};

// 网络状态视图模块数据结构
struct NetViewData {
    unsigned int nodeId;
    // 群内网络状态视图
    list<NodeCondition *> netViewList;

    // list<NodeNotification*> nodeResponse;

    FlightStatus *netPosPtr;
    unsigned int nodeNum; // 指示群内节点数组，用于读位置速度等

    // 初始化函数
    NetViewData() {
        netPosPtr = NULL;
        nodeNum = 0;
        printf("Node init NetView module \n");
    }
};

void NetViewInit();

void HandleReceivedTopology(const vector<uint8_t> &TopologyMsgPtr);

void CleanNetViewTopology(list<NodeCondition *> *nodeList);

bool sortNeighborByNodeId(LinkQuality *neighbor1, LinkQuality *neighbor2);

bool sortByNodeId(NodeCondition *node1, NodeCondition *node2);

void PrintNodeListAndNeiborWithId(list<NodeCondition *> *nodeListPtr);

void HandleReceivedNetPos(const vector<uint8_t> &NetPosMsgPtr);

void ReadNetPos();

NodeCondition *GetANodeConditionById(list<NodeCondition *> *nodeList, unsigned int nodeId);

void HandleLocalFlightState(const vector<uint8_t> &LocalFlightStateMsgPtr);

void HandleIdentityUpdate(NodeNotification *nodeReponseMsgPtr);
/*------------------------network_netview.h------------------------------------*/

/*------------------------network_identityUpdateAndMaintenance.h------------------------------------*/
#define INPUTNODEFILE "dataFile.txt"
#define INPUTPOSSPEEDFILE "011.txt"

// 天基网关新加
#define EARTH_RADIUS 6375000.0
#define ERROR_ANGLE 5.0
#define a_LBH 6378137.0
#define f (1 / 298.257)
#define e2 (f * (2 - f))

// 更新因子
struct UpdateFactor {
    unsigned int nodeId;
    unsigned int groupId;
    float nodeDistance;
    float nodeSpeed;
    float nodePayLoad;
    float nodePriorityFactor;

    UpdateFactor() {
        nodeId = 0;
        groupId = 0;
        nodeDistance = 0.0;
        nodeSpeed = 0.0;
        nodePayLoad = 0.0;
        nodePriorityFactor = 0.0;
    };
    // UpdateFactor(unsigned int id):nodeId(id){};
};

struct Satellite {
    float SATELLITE_longitude;
    float SATELLITE_latitude;
    float SATELLITE_high;
    float SATELLITE_posx;
    float SATELLITE_posy;
    float SATELLITE_posz;
    float SATELLITE_xspeed;
    float SATELLITE_yspeed;
    float SATELLITE_zspeed;
};

struct LBH_Coordinate {
    float longitude;
    float latitude;
    float sate_Height;

    LBH_Coordinate() {
        longitude = 0.0;
        latitude = 0.0;
        sate_Height = 0.0;
    }
};

struct SatebasedUpd_Factor {
    unsigned int nodeId;
    unsigned int groupId;
    float covertime;
    SatebasedUpd_Factor() {
        nodeId = 0;
        groupId = 0;
        covertime = 0.0;
    }
};

// 新加身份通知改为广播 && 身份通知合并
// 身份配置模块的数据结构（存储的临时变量，以及没有通知全网的身份）
struct IdentityData {
    unsigned int nodeId;
    // 最新一轮群内身份temp
    unsigned int groupID_Temp;
    unsigned int mastercontrol_Temp;                // 总控制节点
    unsigned int reservemastercontrol_Temp;         // 总控制节点备份
    unsigned int intragroupcontrol_Temp;            // 所属群控制节点ID
    unsigned int reserveintracontrol_Temp;          // 所属群备用控制
    unsigned int timereference_Temp;                // 时间基准网关
    unsigned int reservetimereference_Temp;         // 备用时间基准网关
    unsigned int intergroupgateway_Temp;            // 所属群网关节点ID
    unsigned int reserveintergroupgateway_Temp;     // 所属群备用网关
    unsigned int satellitebasedgateway_Temp;        // 所属群星基网关
    unsigned int reservesatellitebasedgateway_Temp; // 所属群备用星基网关
    // 新加 身份配置信息状态指示
    unsigned int SerialNum; // 当前本轮待广播身份序列号
    bool IsBroadcast;       // 是否广播成功
    // 新加 确定真实群节点数目
    unsigned int nodeNumInGroup; // 初始定义为群最大节点数目
    // 新加 毁伤
    list<NodeAddress> damaged_Processed; // 已处理毁伤列表
    list<NodeAddress> damaged_Pending;   // 待处理毁伤列表
    // 新增 群内网络状态视图
    list<NodeCondition *> netViewList;
    // 新增 计算更新时间
    clocktype Identity_Trigger;     // 触发身份开始计算的时间
    clocktype delay_IdentityUpdate; // 身份更新完成的时间
    bool Is_IdentityUpdate;         // 关键节点身份是否变化
    // 新增测试
    vector<NodeAddress> damagedTest;
    // 新增 判断是否进行了身份计算
    bool If_calculated;
    // 新增 记录路由表全体成员
    vector<NodeAddress> *netNodes;

    // 初始化函数
    IdentityData() {
        groupID_Temp = 0;
        mastercontrol_Temp = 0;
        reservemastercontrol_Temp = 0;
        intragroupcontrol_Temp = 0;
        reserveintracontrol_Temp = 0;
        timereference_Temp = 0;
        reservetimereference_Temp = 0;
        intergroupgateway_Temp = 0;
        reserveintergroupgateway_Temp = 0;
        satellitebasedgateway_Temp = 0;
        reservesatellitebasedgateway_Temp = 0;
        SerialNum = 0;
        IsBroadcast = true;
        nodeNumInGroup = NODENUMINGROUP;
        If_calculated = false;
        netNodes = nullptr;
    }
    //********************意外退出了，百度说查析构，暂加一个析构函数********
    ~IdentityData() { cout << "IdentityData 析构函数调用 " << endl; }
};

void NetUpdateAndMaintenanceInit(); // 起始化

void PrintIdentity();

void ReadNetResponseFromText(); // 从text中读取初始身份

void IntraGroupUpdate(); // 节点身份主要更新函数

void ReceiveAMsgFromOtherNodes(char *msgPtr); // 节点接收数据包的函数

void ReceiveDamagedNodesmsg(
    const vector<uint8_t> &DamagedNodeMsgPtr); /// 接收传过来的损毁节点信息并在当前节点进行删除无身份节点并向新的控制节点传送该消息

void ControlNodeDealWithDamage(
    msgFromControl *controlMsgPtr); // 新的控制节点接收到整理后的消息，进行一个去重复和转移相关有身份节点的操作?  目前的问题什么时候传送

bool sortByNodePriorityFactor(UpdateFactor node1, UpdateFactor node2); // 按照优先级排序

float evaluatePriorityFactor(float nodeDistance, float nodeSpeed, float nodePayLoad, float maxDistance); // 计算各节点优先级

float distanceAll(NodeCondition *NodePtr, list<NodeCondition *> *ptr); // distanceAll用于计算群内某点到群内其他点的距离之和

float speedAll(NodeCondition *NodePtr, list<NodeCondition *> *ptr); // speedAll用于计算群内某点相对于群平均速度的偏离程度

float nodePayLoadAll(NodeCondition *NodePtr, list<NodeCondition *> *ptr); // nodePayLoadAll用于计算群内某点负载，由节点排队率和链路占用率计算

// 天基网关新加
void SpacebasedUpdate();

float Calculate_Covertime();

bool sortByCoverTime();

// LBH_Coordinate SpaceToLBH();

// CRC新加
vector<uint8_t> *TransPacketToVec_Broadcast(char *MsgToSendPtr);

char *TransVecToPacket(vector<uint8_t> *);

void TurnUnsignedToUint(unsigned int member, vector<uint8_t> *buffer);

unsigned int TurnUintToUnsigned(vector<uint8_t> *vect, int index);

void TurnAddrToUint(NodeAddress address, vector<uint8_t> *buffer);

vector<uint8_t> *TransDamageToVec_1(char *MsgToSendPtr); // 传值

char *TransVecToDamage_1(vector<uint8_t> *vect); // 传值

// 新加身份配置信息状态接口
void ControlNodeDealWithIdentityStatus(const vector<uint8_t> &IdentityStatusMsgPtr);

void InformIdentityToOtherModules();

vector<uint8_t> *TransPacketToVec_Broadcast(char *MsgToSendPtr); // 关于广播包的CRC

char *TransVecToPacket_Broadcast(vector<uint8_t> *vect); // 广播包CRC解包

// 新增毁伤
vector<uint8_t> *MsgFromControl_ConvertDamage(NodeAddress destId, void *data, int moduleName, int eventType); // 毁伤消息汇聚

void IntraGroupControlUpdate(list<NodeCondition *> *ptr); // 控制节点更新函数

bool IsInDamagedList(list<NodeAddress> *damaged_List, NodeAddress nodeId); // 判断节点是否在毁伤列表

void CheckDamagedNodeClock(DamagedNode *damagedNodePtr); // 待处理毁伤的时钟到期，从待处理毁伤列表中删除

void HandleDamageMsg(NodeAddress damaged_node);

bool IsIdentityUpdate(NodeBroadcast *contentPtr);

void HandleNodeOffline();

// 身份信息定时下发
void HandleIdentityBroadcastTiming();

// 关于节点身份上报
// int Stats_ProGetDataManager ();
// vector<ParamDataStr>* OutputIdentityShow();
// void FillInNodeAtIdentityTable();
/*------------------------network_identityUpdateAndMaintenance.h------------------------------------*/
#endif