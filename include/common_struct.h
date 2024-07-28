#ifndef __COMMON_STRUCT_H
#define __COMMON_STRUCT_H

#include <vector>
#include <string.h>
#include <list>
#include <cstdint>
#include <mutex>

using namespace std;

#define NODENUMINGROUP 20
#define LINKMAXINRROUP 19
// 主要定义来自路由模块和链路模块的数据结构
#define BUSSINE_TYPE_NUM 16

// 节点身份枚举
enum nodeResponsibility
{
	MASTERCONTROL = 0,				  // 总控制节点
	INTRAGROUPCONTROL = 1,			  // 群内控制节点
	RESEVERMASTERCONTROL = 2,		  // 总控制节点备份
	RESERVEINTRAGROUPCONTROL = 3,	  // 群内控制节点备份
	TIMEREFERENCE = 4,				  // 时间基准节点
	INTERGROUPGATEWAY = 5,			  // 群间网关节点
	RESERVETIMEREFERENCE = 6,		  // 时间基准节点备份
	RESERVEINTERGROUPFATEWAY = 7,	  // 群间网关节点备份
	SATELLITEBASEDGATEWAY = 8,		  // 天基网关节点
	RESERVESATELLITEBASEDGATEWAY = 9, // 天基网关节点备份
	NODENORMAL = 10,				  // 群内普通节点
	ISOLATION = 11,					  // 孤立节点（网络拓扑无法发现该节点）
};

// 路由模块存储的本地邻居表
struct nodeLocalNeighList
{

	uint16_t nodeAddr; // 邻居节点地址
	// NodeAddress nodeAddr;//邻居节点地址
	// uint16_t Delay;
	// clocktype Delay;
	uint16_t Capacity; // 链路容量
	uint16_t Queuelen; // 队列长度
	float Load;
};
// 路由模块存储的全网邻居表
struct nodeNetNeighList
{
	uint16_t nodeAddr;
	vector<nodeLocalNeighList *> localNeighList;
};
// 节点损伤信息
//  struct DamagedNodes_Msg {
//  	vector<NodeAddress>* DamagedNodes;//vector指针，指向毁伤节点表
//  };

// 链路模块传来的节点速度和位置
struct NodePosAndSpeed
{
	unsigned int nodeId;
	// 位置
	float posX;
	float posY;
	float posZ;
	// 速度
	float speedX;
	float speedY;
	float speedZ;
};

// 智能路由表项
struct nodeNetForwardingTable
{
	uint16_t source;			 // 源地址
	uint16_t dest;				 // 目的地址
	uint16_t hop;				 // 跳数
	uint16_t delay;				 // 时延
	uint16_t nexthop;			 // 下一跳节点
	vector<uint16_t> Relaynodes; // 中继节点地址
};

struct NetworkTopologyMsg
{
	// 路由模块传给网络视图的网络拓扑消息
	vector<nodeNetNeighList *> *netNetghPtr;
};

struct msgID
{
	unsigned int msgid;	 // 消息标识
	unsigned int msgsub; // 消息子标识
};

struct NetFlightStateMsg
{
	// 链路模块传给网络视图的节点位置信息
	unsigned int nodeNum;
	// 更改后按值传递，此msg后跟nodeNum个节点飞行状态
	// NodePosAndSpeed *nodePosPtr;
};

struct NetRT_EntryMsg
{
	// 路由模块传给管理节点的全网路由转发
	vector<vector<nodeNetForwardingTable *>> *netRTtablePtr;
};

// 服务层传来的任务传输指令
struct TaskAssignment
{
	// 应用层传来的任务传输指令
	unsigned short begin_node;	  // 开始节点ID
	unsigned short end_node;	  // 结束节点ID
	unsigned short type;		  // 业务类型
	unsigned short priority;	  // 链路优先级（0-MAX）
	float size;					  // 业务量大小（单位：Byte）
	unsigned short QOS;			  // QOS需求（在几帧内传输）
	unsigned short begin_time[3]; // 业务开始时间（第一位为时,第二位为分,第三位为秒）
	unsigned short end_time[3];	  // 业务结束时间（第一位为时,第二位为分,第三位为秒）
	unsigned short frequency;	  // 业务频率
};

struct LinkAssignmentHeader
{
	unsigned short linkNum;
};

struct LinkAssignment
{
	unsigned short listNumber;						// 指示下面的数组里面前多少个有效
	unsigned short begin_node;						// 链路发送节点ID
	unsigned short end_node;						// 链路接收节点ID
	unsigned short type[BUSSINE_TYPE_NUM];			// 业务类型, 取值范围[1-16]
	unsigned short priority[BUSSINE_TYPE_NUM];		// 业务优化级，取值范围为[1-16]
	float size[BUSSINE_TYPE_NUM];					// 业务量大小（单位：Byte）；物理量范围为[0-1MB]，取值范围为[0-1048576]
	unsigned short QOS[BUSSINE_TYPE_NUM];			// QOS需求；物理量范围为[0-5s],取值范围为[0-2000],时延=取值*2.5ms,
	unsigned short begin_time[BUSSINE_TYPE_NUM][3]; // 业务开始时间（每行第一位为时,取值范围为[1-24]，第二位为分,取值范围为[0-60]，第三位为秒,取值范围为[0-60]）；
	unsigned short end_time[BUSSINE_TYPE_NUM][3];	// 业务结束时间（每行第一位为时,取值范围为[1-24]，第二位为分,取值范围为[0-60]，第三位为秒,取值范围为[0-60]）
	unsigned short frequency[BUSSINE_TYPE_NUM];		// 业务频率，物理量范围为[2.5ms/次-5s/次],取值范围为[1-2000],业务频率=取值*2.5ms/次

	// 一个默认初始化函数，全为0
	LinkAssignment()
	{
		memset(this, 0, sizeof(LinkAssignment));
	}
};

struct FlightStatus
{
	// 本地飞行状态信息
	unsigned short nodeId; // 节点ID
	float timeStemp;
	unsigned short int length;
	float positionX;
	float positionY;
	float positionZ;
	float speedX;
	float speedY;
	float speedZ;
	float accelerateX;
	float accelerateY;
	float accelerateZ;
	unsigned short int pitchAngle;
	unsigned short int yawAngle;
	unsigned short int rollAngle;
};

struct msgFromControl
{
	unsigned int priority : 3;	  // 优先级，3bit
	unsigned int backup : 1;	  // 0:上层|1:本层，1bit
	unsigned int msgType : 4;	  // 消息类型，4bit
	unsigned short packetLength;  // 整个数据包长度，16bit
	unsigned int srcAddr : 10;	  // 始发地址，10bit
	unsigned int destAddr : 10;	  // 目的地址，10bit
	unsigned int seq : 10;		  // 序列号，10bit
	unsigned int repeat : 2;	  // 重传次数，2bit
	unsigned int fragmentNum : 4; // 分片数，4bit
	unsigned int fragmentSeq : 4; // 分片序号，4bit
	char *data;					  // 报文内容
	unsigned short CRC;			  // CRC校验码，16bit
};

struct MessageDiversionHeader
{
	// 用于网管消息中，加到所有负载前面
	unsigned short moduleName; // 模块名，16bit
	unsigned short eventType;  // 事件名，16bit
};

//================================================================================================================
// 节点管控模块结构 author:zhangyue

// 链路状态包
struct ReceiveLocalLinkState_Msg
{
	vector<nodeLocalNeighList *> *neighborList;
};

struct NodeNotification
{
	unsigned int nodeIIndex;
	unsigned int groupID;
	unsigned int nodeResponsibility;
	unsigned int mastercontrolNodeId;
	unsigned int reservemastercontrolNodeId;
	unsigned int intragroupcontrolNodeId;	// 所属群控制节点ID
	unsigned int reserveintracontrolNodeId; // 备用控制
	unsigned int timereferenceNodeId;
	unsigned int reservetimereferenceNodeId;
	unsigned int intergroupgatewayNodeId;		 // 所属群网关节点ID
	unsigned int reserveintergroupgatewayNodeId; // 备用网关
	unsigned int satellitebasedgatewayNodeId;
	unsigned int reservesatellitebasedgatewayNodeId;
	NodeNotification()
	{
		nodeResponsibility = 0;
		mastercontrolNodeId = 0;
		reservemastercontrolNodeId = 0;
		intragroupcontrolNodeId = 0;
		reserveintracontrolNodeId = 0;
		timereferenceNodeId = 0;
		reservetimereferenceNodeId = 0;
		intergroupgatewayNodeId = 0;
		reserveintergroupgatewayNodeId = 0;
		satellitebasedgatewayNodeId = 0;
		reservesatellitebasedgatewayNodeId = 0;
	}

	NodeNotification(unsigned int index, unsigned int id, unsigned int responsibility)
	{
		nodeIIndex = index;
		groupID = id;
		nodeResponsibility = responsibility;
	}

}; // 本模块需要向其他模块传送的身份信息结构

// 关于毁伤汇聚消息
struct DamagedHeader
{
	unsigned short damagedNum;
};
// struct DamagedNode
// {
// 	NodeAddress damagedNode;
// };

struct GroupResponsibility
{
	list<NodeNotification *> *groupResponse;
	// list<NodeNotification*> groupResponse;
};

/*Mac层主要结构体*/
typedef struct NetNeighList
{
	// unsigned int nodeAddr;
	vector<vector<uint16_t>> *receive_topology; // 拓扑结构
} NetNeighList;

struct Layer2_ForwardingTable_Msg
{
	vector<vector<uint16_t>> *Layer2_FT_Ptr; // vector指针，指向发给层2的转发表
};

//*****************************网管模块与链路层新增接口*********************
struct LinkStatus
{
	unsigned int linkStatus; // 链路运行状态
};
struct IdentityStatus
{
	bool isBroadcast; // 标志是否成功广播
	unsigned int seq; // 原身份广播包的序列号
};

// 身份通知改为广播新增
struct NodeBroadcast
{
	unsigned short groupID;							   // 所属群群号
	unsigned short intragroupcontrolNodeId;			   // 所属群控制节点ID
	unsigned short reserveintracontrolNodeId;		   // 所属群备用控制
	unsigned short intergroupgatewayNodeId;			   // 所属群网关节点ID
	unsigned short reserveintergroupgatewayNodeId;	   // 所属群备用网关
	unsigned short satellitebasedgatewayNodeId;		   // 所属群天基网关
	unsigned short reservesatellitebasedgatewayNodeId; // 所属群备用天基网关
	NodeBroadcast()
	{
		groupID = 0;
		intragroupcontrolNodeId = 0;
		reserveintracontrolNodeId = 0;
		intergroupgatewayNodeId = 0;
		reserveintergroupgatewayNodeId = 0;
		satellitebasedgatewayNodeId = 0;
		reservesatellitebasedgatewayNodeId = 0;
	}
};

/****************************网络层->mac层fifo队列定义*******************************/
#define MAX_BUFFER_NUM 256 // 接受缓冲区的最大个数(大小待定)
#define MAX_DATA_LEN 65536 // 一条数据的最大长度（大小待定）

// 循环缓冲区定义(暂定)
struct ringBuffer
{
	uint8_t recvFrmData[MAX_BUFFER_NUM][MAX_DATA_LEN]; // 用于存放数据的缓存
	// 下列数据类型根据MAX_BUFFER_NUM给出
	uint8_t recvFrmHead; // 添加数据的位置
	uint8_t recvFrmTail; // 读取数据的位置
	uint8_t recvFrmNum;	 // 缓冲区中已经存放的数据个数
	mutex lock;			 // 写时互斥锁，防止同时对缓冲区进行写操作

	// 写缓冲区数据成员函数
	void ringBuffer_put(const uint8_t wBuffer[], uint32_t write_len)
	{ // uint32_t是根据MAX_DATA_LEN给出
		if (recvFrmNum < MAX_BUFFER_NUM)
		{
			lock.lock();
			// 将要写入的缓冲区置0
			memset(recvFrmData[recvFrmHead], 0, MAX_DATA_LEN);
			// 将数据写入缓冲区
			memcpy(recvFrmData[recvFrmHead], wBuffer, write_len);
			// 更新缓冲区编号
			recvFrmHead = (recvFrmHead + 1) % MAX_BUFFER_NUM;
			++recvFrmNum;
			lock.unlock();
		}
	}

	// 读缓冲区数据成员函数
	void ringBuffer_get(uint8_t rBuffer[])
	{
		if (recvFrmNum > 0 && (sizeof(recvFrmData[recvFrmTail]) > 0))
		{
			// 读取缓冲区数据
			memcpy(rBuffer, recvFrmData[recvFrmTail], sizeof(recvFrmData[recvFrmTail]));

			// 更新缓冲区编号
			recvFrmTail = (recvFrmTail + 1) % MAX_BUFFER_NUM;
			--recvFrmNum;
		}
	}
};

// void MockTestPrintNetViewFlightState(Node *node, FlightStatus *flightPtr);

#endif