#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <fstream>
#include <sys/time.h>
#include <sys/syscall.h>

#include "routing_mmanet.h"
#include "common_struct.h"
#include "timer.h"
int ikl = 0;
extern MMANETData *mmanet; // routing_mmanet模块本节点所需要的数据，在主函数中初始化，通过全局变量引用
// routing_mmanet模块所需的循环缓冲区
extern ringBuffer macToRouting_buffer;
extern ringBuffer netToRouting_buffer;
extern ringBuffer svcToRouting_buffer;
// network模块所需的循环缓冲区
extern ringBuffer svcToNet_buffer;
extern ringBuffer macToNet_buffer;
extern ringBuffer routingToNet_buffer;
extern bool end_simulation;
extern ringBuffer RoutingTomac_Buffer;

using namespace std;

void *RoutingReceiveFromSvc(void *arg)
{

	cout << "线程receiveFromSvc创建成功!" << endl;
	uint8_t buffer_svc[MAX_DATA_LEN]; // 将从循环缓冲区读取出来的数据(含包头)存放于此
	// sleep(8);
	vector<uint8_t> msg_svctomac;
	while (!end_simulation)
	{

		// 接收数据
		memset(buffer_svc, 0, MAX_DATA_LEN);
		svcToRouting_buffer.ringBuffer_get(buffer_svc);
		uint8_t type = buffer_svc[0];
		uint16_t len = (buffer_svc[1] << 8) | buffer_svc[2];
		vector<uint8_t> dataPacket_svc(&buffer_svc[3], &buffer_svc[3] + len);

		switch (type)
		{
		case 0: // 无数据接收
			break;

		case 0x08: // 接收格式化消息

			cout << "Routing接收到了来自服务层的格式化消息，准备进行处理" << endl;
			// 调用处理函数
			HandleFormatMsgFromSvc(dataPacket_svc);
			break;

		default:
			cout << "不是ReceiveFromSvc应该接收的数据类型:" << (int)type << endl;
			break;
		}

		// 每间隔100us读取一次数据
		usleep(100);
	}

	return NULL;
}

void *RoutingReceiveFromNet(void *arg)
{
	cout << "线程receiveFromNet创建成功!" << endl;
	uint8_t buffer_net[MAX_DATA_LEN]; // 将从循环缓冲区读取出来的数据(含包头)存放于此

	while (!end_simulation)
	{
		// 接收数据
		memset(buffer_net, 0, MAX_DATA_LEN);
		netToRouting_buffer.ringBuffer_get(buffer_net);
		// 解析数据
		uint8_t type = buffer_net[0];
		uint16_t len = (buffer_net[1] << 8) | buffer_net[2];
		vector<uint8_t> dataPacket_net(&buffer_net[3], &buffer_net[3] + len);

		switch (type)
		{
		case 0: // 无数据接收
			break;

		case 0x19: // 网络传输任务
			break;

		case 0x1A: // 身份配置信息
			break;

		case 0x1E: // 管控开销消息
			uint8_t wBufferToMac[MAX_DATA_LEN];
			// 封装层间接口
			cout << "Routing收到管控开销消息" << endl;
			PackRingBuffer(wBufferToMac, &dataPacket_net, 0x10);
			RoutingTomac_Buffer.ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
			break;

		default:
			break;
		}

		// 每间隔100us读取一次数据
		usleep(100);
	}

	return NULL;
}

void *RoutingReceiveFromMac(void *arg)
{
	cout << "线程receiveFromMac创建成功!" << endl;
	uint8_t buffer_mac[MAX_DATA_LEN]; // 将从循环缓冲区读取出来的数据(含包头)存放于此

	// 注册SIGUSR1的信号处理函数
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO; // 即使在信号处理函数执行期间，其他同类型的信号也会继续被递送。
	sa.sa_sigaction = timer_callback;
	sigemptyset(&sa.sa_mask); // 清空信号屏蔽字
	sigaction(SIGUSR1, &sa, NULL);

	while (!end_simulation)
	{
		// 接收数据
		memset(buffer_mac, 0, MAX_DATA_LEN);
		macToRouting_buffer.ringBuffer_get(buffer_mac);
		// 解析数据
		uint8_t type = buffer_mac[0];
		uint16_t len = (buffer_mac[1] << 8) | buffer_mac[2];
		vector<uint8_t> dataPacket_mac(&buffer_mac[3], &buffer_mac[3] + len);
		switch (type)
		{
		case 0: // 无数据接收
			break;

		case 0x0D: // 接收本地链路状态信息
			cout << "Routing本地链路状态信息" << endl;
			MSG_ROUTING_ReceiveLocalLinkMsg(dataPacket_mac);
			break;

		case 0x10: // 接收网络数据包(业务+信令)
			cout << "Routing接收网络数据包(业务+信令)" << endl;
			HandleMfcFromMac(dataPacket_mac);
			break;

		default:
			cout << "不是ReceiveFromMac应该接收的数据类型:" << (int)type << endl;
			break;
		}

		// 每间隔100us读取一次数据
		usleep(100);
	}

	return NULL;
}

// 对routing_mmanet模块数据结构MMANETData进行初始化
void MMANETInit(MMANETData *mmanet)
{

	mmanet->sn = 0;
	mmanet->disu_sn = 0;
	mmanet->seq_Num = 0;		// 业务包序列号从0开始
	mmanet->signalOverhead = 0; // 初始化时开销为0

	mmanet->ReceiveLocalLinkMsg_Flg = false; // 重置发送本地链路状态信息开始flg为false
	mmanet->BC_Process_Init_Flg = false;	 // 重置广播流程开启flg为false
	mmanet->Hello_Init_Flg = false;			 // 重置判断hello包是否重复flg为false
	mmanet->Hello_Ack_Flg = false;			 // 用于判断此时是否是hello包探测时期，在实现非同步的时候要用
	mmanet->List_Flg = false;				 // 用于判读邻接表是否改变
	mmanet->Msg_Flg = false;				 // 用于判断接收到的其它邻接表是否改变
	mmanet->NetConFlag = false;				 // 初始化时网络还未构建
											 // mmanet->LastHelloBroadCastTime = node->getNodeTime(); // 首先记录当前时间（初始化时还未开始进行实际上的Hello包发送流程）
}

// 从上层（服务层）接收格式化消息，并对其进行相应处理
void HandleFormatMsgFromSvc(vector<uint8_t> curDataPacket)
{
	vector<msgFromControl *> *msgFromControl_Array = GetMsgFromControl(&curDataPacket); // 对接收的格式化消息进行分片，生成多个业务数据包,将其存放在指针指向的数组中

	for (size_t i = 0; i < msgFromControl_Array->size(); i++)
	{
		vector<uint8_t> *curData = (vector<uint8_t> *)((*msgFromControl_Array)[i]->data);

		// 对包头部分成员进行赋值
		(*msgFromControl_Array)[i]->destAddr = (curDataPacket[0] >> 3);
		(*msgFromControl_Array)[i]->srcAddr = (curDataPacket[1] >> 1) & 0x1F;
		(*msgFromControl_Array)[i]->msgType = 3;
		(*msgFromControl_Array)[i]->packetLength = curData->size() + 10;
		(*msgFromControl_Array)[i]->repeat = 3;
		(*msgFromControl_Array)[i]->seq = mmanet->seq_Num;
		(*msgFromControl_Array)[i]->priority = 1;
	}
	// cout << "从上层（服务层）接收格式化消息  destAddr " << (int)(curDataPacket[0] >> 3) << " srcAddr " << (int)((curDataPacket[1] >> 1) & 0x1F) << endl;
	(mmanet->seq_Num)++; // 保证同一个源节点的每个业务消息序列号都不同

	// cout << "源地址为：" << (*msgFromControl_Array)[0]->srcAddr << endl;
	// cout << "目的地址为：" << (*msgFromControl_Array)[0]->destAddr << endl;
	// // cout << "该网络数据包的序列号为：" << (*msgFromControl_Array)[0]->seq << endl;
	// cout << "mmanet->seq_Num发送序列号为: " << mmanet->seq_Num << endl;

	unsigned char reTrans = ((curDataPacket[2] >> 1) & 0x01);	  // 读取重传标识
	unsigned char transEnable = ((curDataPacket[5] >> 6) & 0x01); // 读取网络层可靠传输使能标识

	// 对业务数据包进行封装并发给层2
	for (size_t i = 0; i < msgFromControl_Array->size(); i++)
	{
		if ((transEnable == 1) && (reTrans == 0))
		{ // 重传
			for (int j = 0; j < 3; j++)
			{
				// 修改包头重传次数数值, 并进行3次重传
				(*msgFromControl_Array)[i]->repeat = j;

				vector<uint8_t> *repeat_packetedDataPkt = PackMsgFromControl((*msgFromControl_Array)[i]);

				// 将封装后的业务数据包封装层间接口数据帧成发给层2
				uint8_t wBufferToMac[MAX_DATA_LEN]; // 将要写入循环缓冲区的数据(含包头)存放于此
				PackRingBuffer(wBufferToMac, repeat_packetedDataPkt, 0x10);
				RoutingTomac_Buffer.ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
				// MAC_Daatr_Receive->ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
				delete repeat_packetedDataPkt;
			}
		}
		else if (transEnable == 0)
		{ // 非重传
			// 表示非重传包
			(*msgFromControl_Array)[i]->repeat = 3;

			// 封装成vector<uint8_t>数组
			vector<uint8_t> *packetedDataPkt = PackMsgFromControl((*msgFromControl_Array)[i]);

			// 将封装后的业务数据包封装成层间接口数据帧发给层2
			uint8_t wBufferToMac[MAX_DATA_LEN]; // 将要写入循环缓冲区的数据(含包头)存放于此
			PackRingBuffer(wBufferToMac, packetedDataPkt, 0x10);
			RoutingTomac_Buffer.ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
			// MAC_Daatr_Receive->ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
			delete packetedDataPkt;
		}
	}

	// 此处应该将数组内保存的指针也进行释放
	// delete msgFromControl_Array;
}

// 根据格式化消息得到分片数并生成对应个数业务数据包,以数组的形式存储
vector<msgFromControl *> *GetMsgFromControl(vector<uint8_t> *dataPacket)
{
	// 求分片数
	size_t count = 0; // 分片数
	if (dataPacket->size() % MMANET_MFC_DATASIZE != 0)
	{
		count = dataPacket->size() / MMANET_MFC_DATASIZE + 1;
	}
	else
	{
		count = dataPacket->size() / MMANET_MFC_DATASIZE;
	}

	vector<msgFromControl *> *arrayOfmsgFromControl = new vector<msgFromControl *>; // 存放业务数据包指针的数组
	for (size_t i = 1; i <= count; i++)
	{
		msgFromControl *temp_packet = new msgFromControl();
		vector<uint8_t> *cur_data = new vector<uint8_t>;

		temp_packet->fragmentNum = (unsigned int)count;
		temp_packet->fragmentSeq = (unsigned int)i;

		for (size_t j = 0; j < MMANET_MFC_DATASIZE; j++)
		{ // 分片存入数据
			if ((j + (i - 1) * MMANET_MFC_DATASIZE) <= (dataPacket->size() - 1))
			{ // 若为最后一片，且数据没有存满，空着
				cur_data->push_back((*dataPacket)[j + (i - 1) * MMANET_MFC_DATASIZE]);
			}
		}
		temp_packet->data = (char *)(cur_data);
		arrayOfmsgFromControl->push_back(temp_packet);
	}

	return arrayOfmsgFromControl;
}

// 封装业务数据包并添加CRC校验位，形成字节流
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

	vector<uint8_t> *curData = (vector<uint8_t> *)(packet->data);

	// 封装报文内容
	for (size_t i = 0; i < curData->size(); i++)
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

// 将数据包封装成层间接口数据帧，以便进行发送
void PackRingBuffer(uint8_t buffer[], vector<uint8_t> *vec, uint8_t type)
{
	// 封装数据类型字段
	buffer[0] = type;

	// 封装数据长度字段
	buffer[1] = (vec->size() >> 8) & 0xFF;
	buffer[2] = vec->size() & 0xFF;

	// 封装数据区
	for (size_t i = 0; i < vec->size(); i++)
	{
		buffer[i + 3] = (*vec)[i];
	}

	return;
}

// 路由模块收到链路层发来的网络数据包，并对其进行相应处理
void HandleMfcFromMac(vector<uint8_t> &curPktPayload)
{
	msgID *curmsgID = new msgID();
	curmsgID->msgid = (curPktPayload[8] >> 3) & 0x1F; // 消息标识
	curmsgID->msgsub = (curPktPayload[8] & 0x07);	  // 取消息子标识

	msgFromControl *cur_msgFromControl = AnalysisLayer2MsgFromControl(&curPktPayload); // 解析数组，还原为包结构体类型
	uint16_t cur_srcAddr = cur_msgFromControl->srcAddr;
	uint16_t cur_packet_type = cur_msgFromControl->msgType;
	// 删除原本校验位，重新计算CRC校验码，以便检查数据包是否出错
	curPktPayload.pop_back();
	curPktPayload.pop_back();
	uint16_t temp_CRC = CRC16(&curPktPayload);

	if (cur_packet_type == 2)
	{ // 消息为路由管理消息
		switch (curmsgID->msgsub)
		{
		case 3:
		{
			if (temp_CRC == cur_msgFromControl->CRC)
			{ // 数据包未出错，接收
				// 收到一个Hello包

				NodeAddress lastSrcAddr = cur_srcAddr; // 记录源节点地址

				clocktype cur_localTimer = ((uint64_t)(curPktPayload)[13] << 56) | ((uint64_t)(curPktPayload)[14] << 48) | ((uint64_t)(curPktPayload)[15] << 40) | ((uint64_t)(curPktPayload)[16] << 32) | ((uint64_t)(curPktPayload)[17] << 24) | ((uint64_t)(curPktPayload)[18] << 16) | ((uint64_t)(curPktPayload)[19] << 8) | ((uint64_t)(curPktPayload)[20]);

				// 计算单项时延
				clocktype curLinkDelay = getCurrentTime() - cur_localTimer;

				// 测试用，将映射关系存入映射表
				NodeAddress cur_Node_ID = ((curPktPayload)[9] << 24) | ((curPktPayload)[10] << 16) | ((curPktPayload)[11] << 8) | ((curPktPayload)[12]);
				tst_mapping_table *newtable = new tst_mapping_table();
				newtable->nodeAddr = lastSrcAddr;
				newtable->Node_ID = cur_Node_ID;
				mmanet->mapping_table.push_back(newtable);

				// cout << "收到节点" << cur_srcAddr << "传来的HELLO包" << endl;

				// 启动ACK流程
				msgFromControl *back_ack_pkt = new msgFromControl();
				back_ack_pkt->msgType = 2;
				back_ack_pkt->srcAddr = mmanet->nodeAddr;
				back_ack_pkt->destAddr = lastSrcAddr;
				back_ack_pkt->repeat = 0;
				back_ack_pkt->fragmentNum = 1;
				back_ack_pkt->fragmentSeq = 1;

				vector<uint8_t> *ACK_Array = new vector<uint8_t>; // 存放ACK内容的数组
				// 测试用，存储节点号
				ACK_Array->push_back((mmanet->nodeAddr >> 24) & 0xFF); // 10
				ACK_Array->push_back((mmanet->nodeAddr >> 16) & 0xFF); // 11
				ACK_Array->push_back((mmanet->nodeAddr >> 8) & 0xFF);  // 12
				ACK_Array->push_back((mmanet->nodeAddr) & 0xFF);	   // 13
				// 记录源节点发送时间
				ACK_Array->push_back((curLinkDelay >> 56) & 0xFF); // 14
				ACK_Array->push_back((curLinkDelay >> 48) & 0xFF); // 15
				ACK_Array->push_back((curLinkDelay >> 40) & 0xFF); // 16
				ACK_Array->push_back((curLinkDelay >> 32) & 0xFF); // 17
				ACK_Array->push_back((curLinkDelay >> 24) & 0xFF); // 18
				ACK_Array->push_back((curLinkDelay >> 16) & 0xFF); // 19
				ACK_Array->push_back((curLinkDelay >> 8) & 0xFF);  // 20
				ACK_Array->push_back((curLinkDelay) & 0xFF);	   // 21

				vector<uint8_t> *curACK_data = new vector<uint8_t>; // 填充data
				msgID *curmsgID = new msgID;
				curmsgID->msgid = 29;
				curmsgID->msgsub = 4;
				curACK_data->push_back((curmsgID->msgid << 3) | (curmsgID->msgsub));
				for (int i = 0; i < 12; i++)
				{
					curACK_data->push_back((*ACK_Array)[i]);
				}
				back_ack_pkt->packetLength = curACK_data->size() + 10;
				back_ack_pkt->data = (char *)curACK_data;
				vector<uint8_t> *ACK_packetedDataPkt = new vector<uint8_t>;
				ACK_packetedDataPkt = PackMsgFromControl(back_ack_pkt);

				// MESSAGE_PacketAlloc(node, back_ack_msg, ACK_packetedDataPkt->size(), TRACE_MMANET);

				// memcpy(MESSAGE_ReturnPacket(back_ack_msg), ACK_packetedDataPkt->data(), ACK_packetedDataPkt->size());

				// MESSAGE_Send(node, back_ack_msg, 0);
				uint8_t wBufferToMac[MAX_DATA_LEN];
				// 封装层间接口
				PackRingBuffer(wBufferToMac, ACK_packetedDataPkt, 0x10);
				RoutingTomac_Buffer.ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
				mmanet->signalOverhead += back_ack_pkt->packetLength;
				// delete packetedDataPkt;
				delete ACK_Array;
				delete curACK_data;
				delete ACK_packetedDataPkt;
			}
			break;
		}
		case 4:
		{
			if (temp_CRC == cur_msgFromControl->CRC)
			{ // 数据包未出错，接收
				// 收到一个ACK包
				// 内容记录到邻居表内
				// 1.记录源节点
				NodeAddress NeighAddress = cur_srcAddr;

				// cout << "收到节点" << cur_srcAddr << "的ACK包" << endl;

				// 检查此节点在邻居表中是否已经存在
				for (int i = 0; i < mmanet->temp_neighborList.size(); i++)
				{
					if (mmanet->temp_neighborList[i]->nodeAddr == cur_srcAddr)
					{
						mmanet->temp_neighborList.erase(mmanet->temp_neighborList.begin() + i); // 在则删除
						break;
					}
				}
				// 2.记录单项时延
				clocktype curLinkDelay;

				clocktype cur_localTimer = ((uint64_t)(curPktPayload)[13] << 56) | ((uint64_t)(curPktPayload)[14] << 48) | ((uint64_t)(curPktPayload)[15] << 40) | ((uint64_t)(curPktPayload)[16] << 32) | ((uint64_t)(curPktPayload)[17] << 24) | ((uint64_t)(curPktPayload)[18] << 16) | ((uint64_t)(curPktPayload)[19] << 8) | ((uint64_t)(curPktPayload)[20]);
				curLinkDelay = cur_localTimer;

				// 3.加入到本地邻居表中
				nodeLocalNeighList *curLocalNeighbor = new nodeLocalNeighList();
				curLocalNeighbor->nodeAddr = NeighAddress;
				curLocalNeighbor->Delay = curLinkDelay;

				mmanet->temp_neighborList.push_back(curLocalNeighbor);
				sort(mmanet->temp_neighborList.begin(), mmanet->temp_neighborList.end(), CMP_neighborList);
				// delete curLocalNeighbor;
			}
			break;
		}
		case 5:
		{
			/*// cout << "crc比较    " << temp_CRC << "   " << cur_msgFromControl->CRC << endl;*/
			NodeAddress ListNodeAddr = cur_srcAddr;
			// // cout << "源节点" << cur_srcAddr << endl;
			// cout << "收到源节点" << cur_srcAddr << "传来msgsub=5的包,拓扑表" << endl;
			// // cout << "第几片" << cur_msgFromControl->fragmentSeq << endl;
			// // cout << "总数" << cur_msgFromControl->fragmentNum << endl;
			if (temp_CRC == cur_msgFromControl->CRC)
			{
				bool id_flg = false;
				bool msgFromControl_flg = false;
				bool fragmentSeq_flg = false;
				if (mmanet->nodeAddr != cur_srcAddr)
				{
					for (int i = 0; i < mmanet->sn_NetNeighList.size(); i++)
					{ // 遍历此时的拓扑表
						if (mmanet->sn_NetNeighList[i]->nodeAddr == cur_srcAddr)
						{
							id_flg = true;
							/*// cout << "cur_msgFromControl->seq  " << cur_msgFromControl->seq << endl;
							// cout << "mmanet->sn_NetNeighList[i]->SN " << mmanet->sn_NetNeighList[i]->SN << endl;
							// cout << "mmanet->sn_NetNeighList[i]->SN - 100" << mmanet->sn_NetNeighList[i]->SN - 100 << endl;*/
							if (cur_msgFromControl->seq > mmanet->sn_NetNeighList[i]->SN || cur_msgFromControl->seq <= mmanet->sn_NetNeighList[i]->SN - 100 && mmanet->sn_NetNeighList[i]->SN > 100)
							{ // 若条件成立，代表此片为新
								for (int j = 0; j < mmanet->BC_msgFromControl.size(); j++)
								{ // 在存放分片的数组中搜寻是否已有
									if (mmanet->BC_msgFromControl[j][0]->srcAddr == cur_srcAddr)
									{
										msgFromControl_flg = true;
										for (int k = 0; k < mmanet->BC_msgFromControl[j].size(); k++)
										{
											if (cur_msgFromControl->fragmentSeq == mmanet->BC_msgFromControl[j][k]->fragmentSeq)
											{
												if (cur_msgFromControl->seq == mmanet->BC_msgFromControl[j][k]->seq)
												{ // 已找到此片
													fragmentSeq_flg = true;
												}
											}
										}
										// 没有此片
										if (fragmentSeq_flg == false)
										{
											mmanet->BC_msgFromControl[j].push_back(cur_msgFromControl);
											MMANETSendPacket(&curPktPayload, ListNodeAddr, ANY_DEST, 2);
										}
									}
								}
								if (msgFromControl_flg == false)
								{
									mmanet->one_msgFromComtrol.push_back(cur_msgFromControl);
									mmanet->BC_msgFromControl.push_back(mmanet->one_msgFromComtrol);
									mmanet->one_msgFromComtrol.clear();
									MMANETSendPacket(&curPktPayload, ListNodeAddr, ANY_DEST, 2);
								}
							}
						}
					}
					if (id_flg == false)
					{
						for (int j = 0; j < mmanet->BC_msgFromControl.size(); j++)
						{ // 在存放分片的数组中搜寻是否已有
							if (mmanet->BC_msgFromControl[j][0]->srcAddr == cur_srcAddr)
							{
								msgFromControl_flg = true;
								for (int k = 0; k < mmanet->BC_msgFromControl[j].size(); k++)
								{
									if (cur_msgFromControl->fragmentSeq == mmanet->BC_msgFromControl[j][k]->fragmentSeq)
									{
										if (cur_msgFromControl->seq == mmanet->BC_msgFromControl[j][k]->seq)
										{ // 已找到此片
											fragmentSeq_flg = true;
										}
									}
								}
								if (fragmentSeq_flg == false)
								{
									mmanet->BC_msgFromControl[j].push_back(cur_msgFromControl);
									MMANETSendPacket(&curPktPayload, ListNodeAddr, ANY_DEST, 2);
								}
							}
						}
						if (msgFromControl_flg == false)
						{
							mmanet->one_msgFromComtrol.push_back(cur_msgFromControl);
							mmanet->BC_msgFromControl.push_back(mmanet->one_msgFromComtrol);
							mmanet->one_msgFromComtrol.clear();
							MMANETSendPacket(&curPktPayload, ListNodeAddr, ANY_DEST, 2);
						}
					}
					unsigned int num = 0;
					vector<msgFromControl *> temp_vec;
					for (int j = 0; j < mmanet->BC_msgFromControl.size(); j++)
					{
						if (mmanet->BC_msgFromControl[j][0]->srcAddr == cur_srcAddr)
						{
							// // cout << "源节点此时片数" << mmanet->BC_msgFromControl[j].size() << endl;
							for (int k = 0; k < mmanet->BC_msgFromControl[j].size(); k++)
							{
								/*// cout << mmanet->BC_msgFromControl[j][k]->seq << endl;
								// cout << cur_msgFromControl->seq << endl;*/
								if (mmanet->BC_msgFromControl[j][k]->seq == cur_msgFromControl->seq)
								{
									temp_vec.push_back(mmanet->BC_msgFromControl[j][k]);
									num++;
									// // cout << num << endl;
								}
							}
							if (num == cur_msgFromControl->fragmentNum)
							{ // 若已收完此数据的所有片
								// mmanet->Msg_Flg = true;
								sort(temp_vec.begin(), temp_vec.end(), CMP);
								vector<uint8_t> *BC_DataPkt = new vector<uint8_t>; // 存储data的数组
								for (int k = 0; k < temp_vec.size(); k++)
								{
									vector<uint8_t> *cur_data = new vector<uint8_t>;
									cur_data = (vector<uint8_t> *)temp_vec[k]->data; // 取出vector<uint8_t>类型数组指针
									for (int m = 1; m < cur_data->size(); m++)
									{
										BC_DataPkt->push_back((*cur_data)[m]);
									}
								}

								vector<nodeLocalNeighList *> *temp_NeighborList = new vector<nodeLocalNeighList *>();
								for (int n = 0; n < BC_DataPkt->size() / 14; n++)
								{
									nodeLocalNeighList *onetemp_NeighborList = new nodeLocalNeighList();
									onetemp_NeighborList->nodeAddr = ((*BC_DataPkt)[0 + 14 * n] << 8) | ((*BC_DataPkt)[1 + 14 * n]);
									onetemp_NeighborList->Delay = ((uint64_t)(*BC_DataPkt)[2 + 14 * n] << 56) | ((uint64_t)(*BC_DataPkt)[3 + 14 * n] << 48) | ((uint64_t)(*BC_DataPkt)[4 + 14 * n] << 40) | ((uint64_t)(*BC_DataPkt)[5 + 14 * n] << 32) | ((uint64_t)(*BC_DataPkt)[6 + 14 * n] << 24) | ((uint64_t)(*BC_DataPkt)[7 + 14 * n] << 16) | ((uint64_t)(*BC_DataPkt)[8 + 14 * n] << 8) | ((uint64_t)(*BC_DataPkt)[9 + 14 * n]);
									onetemp_NeighborList->Capacity = ((*BC_DataPkt)[10 + 14 * n] << 8) | ((*BC_DataPkt)[11 + 14 * n]);
									onetemp_NeighborList->Queuelen = ((*BC_DataPkt)[12 + 14 * n] << 8) | ((*BC_DataPkt)[13 + 14 * n]);
									temp_NeighborList->push_back(onetemp_NeighborList);
								}

								nodeNetNeighList *newNetNeighListMember = new nodeNetNeighList(); // msg有可能被释放，因此必须new新的对象
								newNetNeighListMember->nodeAddr = ListNodeAddr;
								newNetNeighListMember->localNeighList = (*temp_NeighborList); // 取邻居表

								UpdateNetNeighList(newNetNeighListMember);
								for (int i = 0; i < mmanet->NetNeighList.size(); i++)
								{
									if (mmanet->NetNeighList[i]->nodeAddr == ListNodeAddr)
									{
										mmanet->NetNeighList.erase(mmanet->NetNeighList.begin() + i);
										break;
									}
								}
								mmanet->NetNeighList.push_back(newNetNeighListMember);
								sort(mmanet->NetNeighList.begin(), mmanet->NetNeighList.end(), CMP_NetNeighList);

								// cout << "mmanet->NetNeighList.size()  " << mmanet->NetNeighList.size() << endl;
								for (int ik = 0; ik < mmanet->NetNeighList.size(); ik++)
								{
									// cout << mmanet->NetNeighList[ik]->nodeAddr << "   " << mmanet->NetNeighList[ik]->localNeighList.size() << endl;
								}

								// // cout<<mmanet->NetNeighList[2]->localNeighList[1]<<endl;
								for (int i = 0; i < mmanet->BC_msgFromControl[j].size(); i++)
								{ // 发送完后清空小于等于该序列号的分片,为了应对边界，增加一个范围
									if (mmanet->BC_msgFromControl[j][i]->seq <= cur_msgFromControl->seq || cur_msgFromControl->seq <= mmanet->BC_msgFromControl[j][i]->seq - 100 && mmanet->BC_msgFromControl[j][i]->seq > 100)
									{
										mmanet->BC_msgFromControl[j].erase(mmanet->BC_msgFromControl[j].begin() + i);
										i--;
									}
								}
								bool Seq_Flg = false;
								for (int i = 0; i < mmanet->sn_NetNeighList.size(); i++)
								{ // 一个新的结构体去存储sn号，
									if (mmanet->sn_NetNeighList[i]->nodeAddr == cur_srcAddr)
									{
										Seq_Flg = true;
										mmanet->sn_NetNeighList[i]->SN = cur_msgFromControl->seq;
									}
								}
								if (Seq_Flg == false)
								{
									sn_nodeNetNeighList *temp_sn = new sn_nodeNetNeighList();
									temp_sn->nodeAddr = cur_srcAddr;
									temp_sn->SN = cur_msgFromControl->seq;
									mmanet->sn_NetNeighList.push_back(temp_sn);
								}
								// 以及标志位的更新
								// 本地邻接表发生改变或者接收到其它新的数据包发送表项
								// 注意表项的清空时机
								// // cout << "高速包变化" << endl;
								Count_List();
							}
						}
					}
				}
			}
			break;
		}
		case 6:
		{
			/*// cout << "crc比较    " << temp_CRC << "   " << cur_msgFromControl->CRC << endl;*/
			NodeAddress ListNodeAddr = cur_srcAddr;
			/*// cout << "源节点" << cur_srcAddr << endl;
			// cout << "第几片" << cur_msgFromControl->fragmentSeq << endl;
			// cout << "总数" << cur_msgFromControl->fragmentNum << endl;*/
			if (temp_CRC == cur_msgFromControl->CRC)
			{
				bool id_flg = false;
				bool msgFromControl_flg = false;
				bool fragmentSeq_flg = false;
				if (mmanet->nodeAddr != cur_srcAddr)
				{
					for (int i = 0; i < mmanet->disu_sn_NetNeighList.size(); i++)
					{ // 遍历此时的拓扑表
						if (mmanet->disu_sn_NetNeighList[i]->nodeAddr == cur_srcAddr)
						{
							id_flg = true;
							/*// cout << "cur_msgFromControl->seq  " << cur_msgFromControl->seq << endl;
							// cout << "mmanet->disu_sn_NetNeighList[i]->SN " << mmanet->disu_sn_NetNeighList[i]->SN << endl;
							// cout << "mmanet->disu_sn_NetNeighList[i]->SN - 100" << mmanet->disu_sn_NetNeighList[i]->SN - 100 << endl;*/
							if (cur_msgFromControl->seq > mmanet->disu_sn_NetNeighList[i]->SN || cur_msgFromControl->seq <= mmanet->disu_sn_NetNeighList[i]->SN - 100 && mmanet->disu_sn_NetNeighList[i]->SN > 100)
							{ // 若条件成立，代表此片为新
								for (int j = 0; j < mmanet->BC_msgFromControl2.size(); j++)
								{ // 在存放分片的数组中搜寻是否已有
									if (mmanet->BC_msgFromControl2[j][0]->srcAddr == cur_srcAddr)
									{
										msgFromControl_flg = true;
										for (int k = 0; k < mmanet->BC_msgFromControl2[j].size(); k++)
										{
											if (cur_msgFromControl->fragmentSeq == mmanet->BC_msgFromControl2[j][k]->fragmentSeq)
											{
												if (cur_msgFromControl->seq == mmanet->BC_msgFromControl2[j][k]->seq)
												{ // 已找到此片
													fragmentSeq_flg = true;
												}
											}
										}
										if (fragmentSeq_flg == false)
										{
											mmanet->BC_msgFromControl2[j].push_back(cur_msgFromControl);
											MMANETSendPacket(&curPktPayload, ListNodeAddr, ANY_DEST, 2);
										}
									}
								}
								if (msgFromControl_flg == false)
								{
									mmanet->one_msgFromComtrol2.push_back(cur_msgFromControl);
									mmanet->BC_msgFromControl2.push_back(mmanet->one_msgFromComtrol2);
									mmanet->one_msgFromComtrol2.clear();
									MMANETSendPacket(&curPktPayload, ListNodeAddr, ANY_DEST, 2);
								}
							}
						}
					}
					if (id_flg == false)
					{
						for (int j = 0; j < mmanet->BC_msgFromControl2.size(); j++)
						{ // 在存放分片的数组中搜寻是否已有
							if (mmanet->BC_msgFromControl2[j][0]->srcAddr == cur_srcAddr)
							{
								msgFromControl_flg = true;
								for (int k = 0; k < mmanet->BC_msgFromControl2[j].size(); k++)
								{
									if (cur_msgFromControl->fragmentSeq == mmanet->BC_msgFromControl2[j][k]->fragmentSeq)
									{
										if (cur_msgFromControl->seq == mmanet->BC_msgFromControl2[j][k]->seq)
										{ // 已找到此片
											fragmentSeq_flg = true;
										}
									}
								}
								if (fragmentSeq_flg == false)
								{
									mmanet->BC_msgFromControl2[j].push_back(cur_msgFromControl);
									MMANETSendPacket(&curPktPayload, ListNodeAddr, ANY_DEST, 2);
								}
							}
						}
						if (msgFromControl_flg == false)
						{
							mmanet->one_msgFromComtrol2.push_back(cur_msgFromControl);
							mmanet->BC_msgFromControl2.push_back(mmanet->one_msgFromComtrol2);
							mmanet->one_msgFromComtrol2.clear();
							MMANETSendPacket(&curPktPayload, ListNodeAddr, ANY_DEST, 2);
						}
					}
					unsigned int num = 0;
					vector<msgFromControl *> temp_vec;
					for (int j = 0; j < mmanet->BC_msgFromControl2.size(); j++)
					{
						if (mmanet->BC_msgFromControl2[j][0]->srcAddr == cur_srcAddr)
						{
							// // cout << "源节点此时片数" << mmanet->BC_msgFromControl2[j].size() << endl;
							for (int k = 0; k < mmanet->BC_msgFromControl2[j].size(); k++)
							{
								/*// cout << mmanet->BC_msgFromControl2[j][k]->seq << endl;
								// cout << cur_msgFromControl->seq << endl;*/
								if (mmanet->BC_msgFromControl2[j][k]->seq == cur_msgFromControl->seq)
								{
									temp_vec.push_back(mmanet->BC_msgFromControl2[j][k]);
									num++;
									// // cout << num << endl;
								}
							}
							if (num == cur_msgFromControl->fragmentNum)
							{ // 若已收完此数据的所有片
								// mmanet->Msg_Flg = true;
								sort(temp_vec.begin(), temp_vec.end(), CMP);
								vector<uint8_t> *BC_DataPkt = new vector<uint8_t>; // 存储data的数组
								for (int k = 0; k < temp_vec.size(); k++)
								{
									vector<uint8_t> *cur_data = new vector<uint8_t>;
									cur_data = (vector<uint8_t> *)temp_vec[k]->data; // 取出vector<uint8_t>类型数组指针
									for (int m = 1; m < cur_data->size(); m++)
									{
										BC_DataPkt->push_back((*cur_data)[m]);
									}
								}
								vector<nodeLocalNeighList *> *temp_NeighborList = new vector<nodeLocalNeighList *>();
								for (int n = 0; n < BC_DataPkt->size() / 14; n++)
								{
									nodeLocalNeighList *onetemp_NeighborList = new nodeLocalNeighList();
									onetemp_NeighborList->nodeAddr = ((*BC_DataPkt)[0 + 14 * n] << 8) | ((*BC_DataPkt)[1 + 14 * n]);
									onetemp_NeighborList->Delay = ((uint64_t)(*BC_DataPkt)[2 + 14 * n] << 56) | ((uint64_t)(*BC_DataPkt)[3 + 14 * n] << 48) | ((uint64_t)(*BC_DataPkt)[4 + 14 * n] << 40) | ((uint64_t)(*BC_DataPkt)[5 + 14 * n] << 32) | ((uint64_t)(*BC_DataPkt)[6 + 14 * n] << 24) | ((uint64_t)(*BC_DataPkt)[7 + 14 * n] << 16) | ((uint64_t)(*BC_DataPkt)[8 + 14 * n] << 8) | ((uint64_t)(*BC_DataPkt)[9 + 14 * n]);
									onetemp_NeighborList->Capacity = ((*BC_DataPkt)[10 + 14 * n] << 8) | ((*BC_DataPkt)[11 + 14 * n]);
									onetemp_NeighborList->Queuelen = ((*BC_DataPkt)[12 + 14 * n] << 8) | ((*BC_DataPkt)[13 + 14 * n]);
									temp_NeighborList->push_back(onetemp_NeighborList);
								}
								nodeNetNeighList *newNetNeighListMember = new nodeNetNeighList(); // msg有可能被释放，因此必须new新的对象
								newNetNeighListMember->nodeAddr = ListNodeAddr;
								newNetNeighListMember->localNeighList = (*temp_NeighborList); // 取邻居表

								for (int i = 0; i < mmanet->disu_NetNeighList.size(); i++)
								{
									if (mmanet->disu_NetNeighList[i]->nodeAddr == ListNodeAddr)
									{
										mmanet->disu_NetNeighList.erase(mmanet->disu_NetNeighList.begin() + i);
										break;
									}
								}
								mmanet->disu_NetNeighList.push_back(newNetNeighListMember);
								sort(mmanet->disu_NetNeighList.begin(), mmanet->disu_NetNeighList.end(), CMP_NetNeighList);
								for (int i = 0; i < mmanet->BC_msgFromControl2[j].size(); i++)
								{ // 发送完后清空小于等于该序列号的分片,为了应对边界，增加一个范围
									if (mmanet->BC_msgFromControl2[j][i]->seq <= cur_msgFromControl->seq || cur_msgFromControl->seq <= mmanet->BC_msgFromControl2[j][i]->seq - 100 && mmanet->BC_msgFromControl2[j][i]->seq > 100)
									{
										mmanet->BC_msgFromControl2[j].erase(mmanet->BC_msgFromControl2[j].begin() + i);
										i--;
									}
								}
								bool Seq_Flg = false;
								for (int i = 0; i < mmanet->disu_sn_NetNeighList.size(); i++)
								{ // 一个新的结构体去存储sn号，
									if (mmanet->disu_sn_NetNeighList[i]->nodeAddr == cur_srcAddr)
									{
										Seq_Flg = true;
										mmanet->disu_sn_NetNeighList[i]->SN = cur_msgFromControl->seq;
									}
								}
								if (Seq_Flg == false)
								{
									sn_nodeNetNeighList *temp_sn = new sn_nodeNetNeighList();
									temp_sn->nodeAddr = cur_srcAddr;
									temp_sn->SN = cur_msgFromControl->seq;
									mmanet->disu_sn_NetNeighList.push_back(temp_sn);
								}
								// 以及标志位的更新
								// 本地邻接表发生改变或者接收到其它新的数据包发送表项
								// 注意表项的清空时机
								// // cout << "低速包变化" << endl;
								Count_List();
							}
						}
					}
				}
			}
			break;
		}
		default:
		{
			// cout << "This is an unknown packet type for this event, ERROR ?" << endl;
			// ERROR_Assert(FALSE, "This is an unknown packet type for this event, ERROR ?\n");
			break;
		}
		}
		// break;
	}
	else if (cur_packet_type == 3)
	{ // 消息为业务消息
		if (temp_CRC == cur_msgFromControl->CRC)
		{ // 数据包未出错，接收
			if (!mmanet->ToNetwork_msgFromControl.empty())
			{ // 之前已经接收过消息分片
				// 遍历其源地址和序列号来寻找属于哪一个格式化消息的片段
				for (size_t i = 0; i < mmanet->ToNetwork_msgFromControl.size(); i++)
				{
					if ((mmanet->ToNetwork_msgFromControl[i][0]->srcAddr == cur_msgFromControl->srcAddr) && (mmanet->ToNetwork_msgFromControl[i][0]->seq == cur_msgFromControl->seq))
					{
						// 源地址和序列号都相同，找到该分片是属于哪一个格式化消息的片段，再遍历判断该分片是否已经被接收
						for (size_t j = 0; j < mmanet->ToNetwork_msgFromControl[i].size(); j++)
						{
							// 若该分片已经被接收，则丢弃
							if (cur_msgFromControl->fragmentSeq == mmanet->ToNetwork_msgFromControl[i][j]->fragmentSeq)
								break;

							if (j == mmanet->ToNetwork_msgFromControl[i].size() - 1)
							{ // 遍历完了，仍然没有。即该分片是新分片，接收
								mmanet->ToNetwork_msgFromControl[i].push_back(cur_msgFromControl);

								// 更新接收时间
								mmanet->FormatMessageControl[i].lastMsgFromControlTimer = getCurrentTime();

								if ((mmanet->ToNetwork_msgFromControl[i].size() == mmanet->ToNetwork_msgFromControl[i][0]->fragmentNum) && mmanet->FormatMessageControl[i].reorganizationCompleted == false)
								{
									// 所有分片已经接收，调用格式化消息重组发送函数进行重组并发送给上层
									SendFromatMessageToUpperLayer(i);
								}
								break;
							}
						}
						break;
					}

					if (i == mmanet->ToNetwork_msgFromControl.size() - 1)
					{ // 遍历完了，表明新分片是属于一条新的格式化消息，接收
						// 创建一条新的消息队列
						vector<msgFromControl *> oneMore_msgFromControl;
						oneMore_msgFromControl.push_back(cur_msgFromControl);
						mmanet->ToNetwork_msgFromControl.push_back(oneMore_msgFromControl);

						// 记录加入时间和发送标志位
						struct formatMessageControl oneMore_fM_Control;
						oneMore_fM_Control.lastMsgFromControlTimer = getCurrentTime();
						oneMore_fM_Control.reorganizationCompleted = false;
						mmanet->FormatMessageControl.push_back(oneMore_fM_Control);

						if (mmanet->ToNetwork_msgFromControl[i + 1][0]->fragmentNum == 1)
						{
							// 表明该业务数据包只有一个分片，调用格式化消息重组发送函数直接发送给上层
							SendFromatMessageToUpperLayer(i + 1);
						}
						break;
					}
				}

				// 遍历检查是否有消息长时间未更新，超过了生存周期。若有，直接删除
				for (size_t m = 0; m < mmanet->FormatMessageControl.size(); m++)
				{
					if (((getCurrentTime() - mmanet->FormatMessageControl[m].lastMsgFromControlTimer) > MMANET_TOUPPERMSG_LIFECYCLE) && mmanet->FormatMessageControl[m].reorganizationCompleted == true)
					{
						// 业务数据包分片的更新间隔超过了生存周期，删除
						mmanet->ToNetwork_msgFromControl.erase(mmanet->ToNetwork_msgFromControl.begin() + m);
						mmanet->FormatMessageControl.erase(mmanet->FormatMessageControl.begin() + m);
						// 应该加上delete删除保存的msgfromcontrol指针，和msgfromcontrol内的data指针指向的内存
					}
				}
			}
			else
			{
				// 之前暂未接收过消息分片，创建新的消息队列
				vector<msgFromControl *> msgFromControl_new;
				msgFromControl_new.push_back(cur_msgFromControl);
				mmanet->ToNetwork_msgFromControl.push_back(msgFromControl_new);

				// 记录加入时间和发送标志位
				struct formatMessageControl fM_Control;
				fM_Control.lastMsgFromControlTimer = getCurrentTime();
				fM_Control.reorganizationCompleted = false;
				mmanet->FormatMessageControl.push_back(fM_Control);

				if (mmanet->ToNetwork_msgFromControl[0][0]->fragmentNum == 1)
				{
					// 表明该业务数据包只有一个分片，调用格式化消息重组发送函数直接发送给上层
					SendFromatMessageToUpperLayer(0);
				}
			}
		}
	}
	else if (cur_packet_type == 1 || cur_packet_type == 4)
	{ // 消息为链路层传给网管模块的消息，直接透传给网管模块
		curPktPayload.push_back((temp_CRC >> 8) & 0xFF);
		curPktPayload.push_back(temp_CRC & 0xFF);
		uint8_t wBufferToMac[MAX_DATA_LEN];
		PackRingBuffer(wBufferToMac, &curPktPayload, 0x1E);
		RoutingTomac_Buffer.ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
		// XXXXXX->ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
	}
}

// 将uint8_t数组内对应的内容存入msgFromControl结构体
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

	uint16_t dataLen = temp_msgFromControl->packetLength - 10;
	for (size_t i = 1; i <= dataLen; i++)
	{
		cur_data->push_back((*dataPacket)[7 + i]);
	}
	temp_msgFromControl->data = (char *)cur_data;
	temp_msgFromControl->CRC = ((*dataPacket)[8 + dataLen] << 8) | ((*dataPacket)[9 + dataLen]);

	return temp_msgFromControl;
}

// 使用CRC-16/MODBUS计算CRC校验码
uint16_t CRC16(vector<uint8_t> *buffer)
{
	uint16_t crc = 0xFFFF; // 初始化计算值
	for (size_t i = 0; i < buffer->size(); i++)
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

// 通过gettimeofday获取当前时间，并转换为clocktype类型保存，单位为微秒
clocktype getCurrentTime()
{
	timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return static_cast<clocktype>(currentTime.tv_sec) * 1000000 + currentTime.tv_usec;
}

// 将分片已经完全接收到的格式化消息进行重组并发送给服务层
void SendFromatMessageToUpperLayer(int k)
{
	sort(mmanet->ToNetwork_msgFromControl[k].begin(), mmanet->ToNetwork_msgFromControl[k].end(), CMP); // 排序

	vector<uint8_t> *toUpperLayerDataPkt = new vector<uint8_t>; // 要发送的vector业务消息数组
	for (size_t m = 0; m < mmanet->ToNetwork_msgFromControl[k].size(); m++)
	{
		// 将msgFromControl中的data数据组装成0/1序列发送
		vector<uint8_t> *curData = (vector<uint8_t> *)(mmanet->ToNetwork_msgFromControl[k][m]->data);
		for (size_t n = 0; n < curData->size(); n++)
		{
			toUpperLayerDataPkt->push_back((*curData)[n]);
		}
	}

	// 将重组后的业务消息封装成层间接口数据帧发给服务层
	uint8_t wBufferToSvc[MAX_DATA_LEN]; // 将要写入循环缓冲区的数据(含包头)存放于此
	PackRingBuffer(wBufferToSvc, toUpperLayerDataPkt, 0x08);
	// 写入服务层循环缓冲区，待定
	//..............

	mmanet->FormatMessageControl[k].reorganizationCompleted = true; // 发送后将标志位改为true，代表已发送，但是不清除，仍然保存一段时间
	delete toUpperLayerDataPkt;
}

// 按照分片序号将业务数据包进行升序排列
bool CMP(msgFromControl *a, msgFromControl *b)
{
	return a->fragmentSeq < b->fragmentSeq;
}

// 设置一个定时器
void SetTimer(long sc, long ms, int number)
{
	// 创建定时器
	timer_t timerid;
	struct sigevent sevp;
	sevp.sigev_notify = SIGEV_SIGNAL | SIGEV_THREAD_ID;
	sevp._sigev_un._tid = syscall(SYS_gettid); // 获取LWP，标识线程身份，交给cpu使用
	sevp.sigev_signo = SIGUSR1;
	sevp.sigev_value.sival_int = number; // 将定时器标识符传递给信号处理函数
	timer_create(CLOCK_REALTIME, &sevp, &timerid);

	// 设置定时器的超时时间和间隔
	struct itimerspec its;
	its.it_value.tv_sec = sc;			 // 初次超时时间秒数
	its.it_value.tv_nsec = ms * 1000000; // 初次超时时间纳秒数
	its.it_interval.tv_sec = 0;			 // 间隔时间：0秒
	its.it_interval.tv_nsec = 0;
	timer_settime(timerid, 0, &its, NULL);
}

// T3计时器被触发，判断是广播流程开始还是结束
// SIGUSR1信号的回调函数
void timer_callback(int signum, siginfo_t *si, void *uc)
{
	// writeInfo("定時器1触发");
	//  si->si_value.sival_int为设置定时器时设置的定时器编号，再次用于区分不同定时器的功能
	if (si->si_value.sival_int == 1)
	{
		// // cout << "T3定时器执行,开始广播流程，置标志位，进行广播" << endl;
		// cout << "HELLO探测定时器执行" << endl;

		// char clockStr[MAX_STRING_LENGTH];
		// TIME_PrintClockInSecond(node->getNodeTime(), clockStr);
		// // cout << "-----------广播 Current Time : " << clockStr << " Sec ------------" << endl;
		mmanet->Hello_Ack_Flg = false;
		mmanet->t2 = getCurrentTime() / 1000000000.0; // 记录此时的时间更新
		// // cout << mmanet->t2 << endl;
		// vector<nodeLocalNeighList *> *temp_gaosu = new vector<nodeLocalNeighList *>(); // 获取一个临时高速表去判断相较于上次探测生成的表项是否发生变化
		// for (int i = 0; i < mmanet->temp_neighborList.size(); i++)
		// {
		// 	temp_gaosu->push_back(mmanet->temp_neighborList[i]);
		// } // 将表项更新
		// for (int i = 0; i < temp_gaosu->size(); i++)
		// {
		// 	if ((*temp_gaosu)[i]->Capacity < 0)
		// 	{
		// 		temp_gaosu->erase(temp_gaosu->begin() + i);
		// 		i--;
		// 	}
		// }
		vector<int> temp_flag(21, 0);
		mmanet->new_flag = temp_flag;
		if (/*node->nodeId == 1*/ 1)
		{
			// cout << "临时存储本地节点邻居表节点: " << endl;
			for (int i = 0; i < mmanet->temp_neighborList.size(); i++)
			{
				// cout << mmanet->temp_neighborList[i]->nodeAddr << " ";
			}
			// cout << endl;
			// if (mmanet->time_Flg == false)
			//{
			//  time_Flg == false不是第一次记录时间  // 人为改变场景，并不是真实场景
			for (int i = 0; i < mmanet->temp_neighborList.size(); i++)
			{
				if (mmanet->temp_neighborList[i]->Capacity < 0)
				{
					mmanet->new_flag[mmanet->temp_neighborList[i]->nodeAddr] = 2; // 低速标记为2
				}
				else
				{
					mmanet->new_flag[mmanet->temp_neighborList[i]->nodeAddr] = 1; // 高速标记为1
				}
			}
			//}
			// else{
			// 	mmanet->new_flag = mmanet->old_flag;
			// 	if(mmanet->t2 > mmanet->data[mmanet->data_th] && mmanet->data_th < 50){
			// 		if(mmanet->new_flag[4] == 1){
			// 			mmanet->new_flag[4] = 0;
			// 		}
			// 		else{
			// 			mmanet->new_flag[4] = 1;
			// 		}
			// 		mmanet->data_th++;
			// 	}
			// }
			// for (int i = 1; i < mmanet->old_flag.size(); i++)
			// {
			// 	// cout << mmanet->old_flag[i] << " ";
			// }
			// // cout << endl;
			// // cout << "mmanet->new_flag" << endl;
			// for (int i = 1; i < mmanet->new_flag.size(); i++)
			// {
			// 	// cout << mmanet->new_flag[i] << " ";
			// }
			// cout << endl;
			if (mmanet->time_Flg == false)
			{
				for (int i = 1; i < mmanet->t1.size(); i++)
				{
					mmanet->t1[i] = getCurrentTime() / 1000000000.0; // 记录时间的单位是s
				}
				mmanet->time_Flg = true;
				mmanet->List_Flg = true;
			}
			else
			{
				for (int i = 0; i < mmanet->new_flag.size(); i++)
				{
					if (mmanet->new_flag[i] != mmanet->old_flag[i])
					{
						mmanet->List_Flg = true;
						if (i == 4)
						{
							mmanet->nolstm.push_back(mmanet->t2);
						}
						int k = 0;
						for (int j = 0; j < mmanet->time_lag.size(); j++)
						{
							if (mmanet->time_lag[j]->nodeAddr == i)
							{
								mmanet->time_lag[j]->timelag.push_back((mmanet->t2 - mmanet->t1[i]) / 10);
								k++;
							}
						}
						if (k == 0)
						{
							NodeTime_lag *temp_tl = new NodeTime_lag();
							temp_tl->nodeAddr = i;
							temp_tl->timelag.push_back((mmanet->t2 - mmanet->t1[i]) / 10);
							mmanet->time_lag.push_back(temp_tl);
						}
						mmanet->t1[i] = mmanet->t2;
					}
				}
			}
			for (int j = 0; j < mmanet->time_lag.size(); j++)
			{
				if (mmanet->time_lag[j]->timelag.size() == 5 && mmanet->Lstm_Flg == true)
				{
					for (int i = 0; i < mmanet->time_lag[j]->timelag.size(); i++)
					{
						// cout << (double)mmanet->time_lag[j]->timelag[i] << " ";
					}
					// cout << endl;

					// 准备测试集数据
					vector<double *> input;
					/*vector<double> output_real;
					vector<double> output;*/
					double *testinput = new double[INPUT_N]; // INPUT_N个输入值
					// double testoutput;//1个输出值
					for (int k = 0; k < INPUT_N; k++)
					{
						testinput[k] = mmanet->time_lag[j]->timelag[k]; // 形成具体输入值
					}
					// testoutput = data[INPUT_N + i];//1个输出值（样本集真实数据）
					input.push_back(testinput); // 输入值序列
					// output_real.push_back(testoutput);//输出值（样本集真实数据）序列，用于比对

					/////////////////////////////////////////
					// 开始执行预测过程
					// INPUT_N个输入值形成1个输出。
					double *in = input[0];
					double out;
					out = lstm_predict(&mmanet->state[mmanet->time_lag[j]->nodeAddr], &mmanet->lstm_module[mmanet->time_lag[j]->nodeAddr], in); // 预测值，INPUT_N个输入值形成1个输出
					// cout << out << endl;
					mmanet->lstm1.push_back(out * 10);
					mmanet->lstm2.push_back(mmanet->t2 + out * 10);
					mmanet->time_lag[j]->timelag.erase(mmanet->time_lag[j]->timelag.begin());

					// Message* timerMsg_Broadcast = MESSAGE_Alloc(node, NETWORK_LAYER, ROUTING_PROTOCOL_MMANET, MSG_ROUTING_HelloProcessTimer);
					clocktype delay_hello = out * 10000;
					// clocktype delay_hello = out * 10000 * MILLI_SECOND;
					// MESSAGE_Send(node, timerMsg_Broadcast, delay_hello);
					SetTimer2(0, delay_hello, 1);
				}
			}
			mmanet->old_flag = mmanet->new_flag;
			/*vector<int> temp_flag(21, 0);
			mmanet->new_flag = temp_flag;*/
			/*
			if(mmanet->time_Flg == false){
				mmanet->t1 = node->getNodeTime();
				mmanet->time_Flg = true;
				mmanet->List_Flg = true;
			}
			else{
				if(mmanet->neighborList.size() == mmanet->temp_neighborList.size()){
					for(int j = 0; j < mmanet->neighborList.size(); j++){
						if(mmanet->neighborList[j]->nodeAddr != mmanet->temp_neighborList[j]->nodeAddr){
							mmanet->List_Flg = true;
							mmanet->time_lag.push_back((mmanet->t2 - mmanet->t1)/10000000000.0);
							mmanet->nolstm.push_back(mmanet->t2 / 1000000000.0);
							mmanet->t1 = node->getNodeTime();
							return;
						}
					}
					if(temp_gaosu->size() == mmanet->gaosu_neighborList.size()){
						for(int j = 0; j < mmanet->gaosu_neighborList.size(); j++){
							if(mmanet->gaosu_neighborList[j]->nodeAddr != (*temp_gaosu)[j]->nodeAddr){
								mmanet->List_Flg = true;
								mmanet->time_lag.push_back((mmanet->t2 - mmanet->t1)/10000000000.0);
								mmanet->nolstm.push_back(mmanet->t2 / 1000000000.0);
								mmanet->t1 = node->getNodeTime();
								return;
								}
							}
						}
					else{
						mmanet->List_Flg = true;
						mmanet->time_lag.push_back((mmanet->t2 - mmanet->t1)/10000000000.0);
						mmanet->nolstm.push_back(mmanet->t2 / 1000000000.0);
						mmanet->t1 = node->getNodeTime();
					}

				}
				else{
					mmanet->List_Flg = true;
					mmanet->time_lag.push_back((mmanet->t2 - mmanet->t1)/10000000000.0);
					mmanet->nolstm.push_back(mmanet->t2 / 1000000000.0);
					mmanet->t1 = node->getNodeTime();

				}
				for(int i = 0; i < mmanet->time_lag.size(); i++){
					// cout << (double)mmanet->time_lag[i] << " ";
				}
				// cout << endl;
			}


			if(mmanet->time_lag.size() == 5 && mmanet->Lstm_Flg == true){

				*/
			if (mmanet->t2 > 360.0 && mmanet->lstm_flag == false && mmanet->Lstm_Flg == true)
			{ // 最后一次时间为321.3s
				for (int i = 0; i < mmanet->nolstm.size(); i++)
				{
					// cout << mmanet->nolstm[i] << " ";
				}
				// cout << endl;
				// cout << mmanet->nolstm.size() << endl;
				for (int i = 0; i < mmanet->lstm2.size(); i++)
				{
					// cout << mmanet->lstm2[i] << " ";
				}
				// cout << endl;
				// cout << mmanet->lstm2.size() << endl;
				for (int i = 0; i < mmanet->lstm1.size(); i++)
				{
					// cout << mmanet->lstm1[i] << " ";
				}
				// cout << endl;
				// cout << mmanet->lstm1.size() << endl;
				saveRESULTfile(mmanet->nolstm, mmanet->lstm2, mmanet->lstm1, "result.txt", mmanet);
				mmanet->lstm_flag = true;
			}
		}
		mmanet->gaosu_neighborList.clear(); // 更新处理得到高速表
		for (int i = 0; i < mmanet->temp_neighborList.size(); i++)
		{
			mmanet->gaosu_neighborList.push_back(mmanet->temp_neighborList[i]);
		} // 将表项更新
		for (int i = 0; i < mmanet->gaosu_neighborList.size(); i++)
		{
			if ((mmanet->gaosu_neighborList)[i]->Capacity < 0)
			{
				mmanet->gaosu_neighborList.erase(mmanet->gaosu_neighborList.begin() + i);
				i--;
			}
		}
		mmanet->disu_neighborList.clear(); // 更新处理得到低速表
		for (int i = 0; i < mmanet->temp_neighborList.size(); i++)
		{
			mmanet->disu_neighborList.push_back(mmanet->temp_neighborList[i]);
		} // 将表项更新
		for (int i = 0; i < mmanet->disu_neighborList.size(); i++)
		{
			if ((mmanet->disu_neighborList)[i]->Capacity >= 0)
			{
				mmanet->disu_neighborList.erase(mmanet->disu_neighborList.begin() + i);
				i--;
			}
		}
		mmanet->neighborList.clear(); // 更新处理得到高速+低速表，为了支持邻居表广播
		for (int i = 0; i < mmanet->temp_neighborList.size(); i++)
		{
			mmanet->neighborList.push_back(mmanet->temp_neighborList[i]);
		} // 将表项更新
		mmanet->temp_neighborList.clear();
		// if(mmanet->neighborList.empty()){             //若此时邻接表为空，则判定此节点脱网
		//	mmanet->NetNeighList.clear();
		//	mmanet->disu_NetNeighList.clear();
		// }
		if (!mmanet->gaosu_neighborList.empty())
		{
			nodeNetNeighList *cur_nodeNetNeighList = new nodeNetNeighList();
			cur_nodeNetNeighList->nodeAddr = mmanet->nodeAddr;
			cur_nodeNetNeighList->localNeighList = mmanet->gaosu_neighborList;

			UpdateNetNeighList(cur_nodeNetNeighList);
			for (int i = 0; i < mmanet->NetNeighList.size(); i++)
			{
				if (mmanet->NetNeighList[i]->nodeAddr == mmanet->nodeAddr)
				{
					mmanet->NetNeighList.erase(mmanet->NetNeighList.begin() + i);
					break;
				}
			}

			mmanet->NetNeighList.emplace_back(cur_nodeNetNeighList);
			sort(mmanet->NetNeighList.begin(), mmanet->NetNeighList.end(), CMP_NetNeighList);
		}
		if (!mmanet->disu_neighborList.empty())
		{
			for (int i = 0; i < mmanet->disu_NetNeighList.size(); i++)
			{
				if (mmanet->disu_NetNeighList[i]->nodeAddr == mmanet->nodeAddr)
				{
					mmanet->disu_NetNeighList.erase(mmanet->disu_NetNeighList.begin() + i);
					break;
				}
			}
			nodeNetNeighList *cur1_nodeNetNeighList = new nodeNetNeighList();
			cur1_nodeNetNeighList->nodeAddr = mmanet->nodeAddr;
			cur1_nodeNetNeighList->localNeighList = mmanet->disu_neighborList;
			mmanet->disu_NetNeighList.emplace_back(cur1_nodeNetNeighList);
			sort(mmanet->disu_NetNeighList.begin(), mmanet->disu_NetNeighList.end(), CMP_NetNeighList);
		}

		// cout << "存储高速全网邻居表:  " << mmanet->NetNeighList.size() << endl;
		for (int ik = 0; ik < mmanet->NetNeighList.size(); ik++)
		{
			// cout << mmanet->NetNeighList[ik]->nodeAddr << "   " << mmanet->NetNeighList[ik]->localNeighList.size() << endl;
		}

		if (mmanet->List_Flg == true)
		{
			vector<nodeLocalNeighList *> *neighborListPtr = &(mmanet->gaosu_neighborList); // 指针指向高速邻居表
			MMANETBroadcastMessage((char *)neighborListPtr, 1);
			vector<nodeLocalNeighList *> *neighborListPtr1 = &(mmanet->disu_neighborList); // 指针指向低速邻居表
			MMANETBroadcastMessage((char *)neighborListPtr1, 2);
			Count_List();
		}
		mmanet->List_Flg = false;
		// 将T3计时器释放
		// MESSAGE_Free(node, msg);
		/*}*/
		// 触发邻居表广播函数
		// 由于只需要广播一次，之后直接跳出，一旦收到新的ACK从而使得邻居表表项更新，那么就会在ACK中触发广播函数
		// 注：触发的是广播的函数，不是此事件

		// 测试，发送底层传输需求
		ikl++;
		if (ikl == 3 && mmanet->nodeAddr == 1)
		{
			TaskAssignment *task_Msg = new TaskAssignment();
			task_Msg->begin_node = 1;
			task_Msg->end_node = 2;
			task_Msg->type = 3;
			task_Msg->priority = 1;
			task_Msg->size = 10.0;
			task_Msg->QOS = 10;
			task_Msg->begin_time[0] = 10;
			task_Msg->begin_time[1] = 20;
			task_Msg->begin_time[2] = 30;
			task_Msg->end_time[0] = 40;
			task_Msg->end_time[1] = 50;
			task_Msg->end_time[2] = 60;
			task_Msg->frequency = 1;
			uint8_t arr1[MAX_DATA_LEN];
			arr1[0] = 0x06;
			// 封装数据长度字段
			arr1[1] = (sizeof(TaskAssignment) >> 8) & 0xFF;
			arr1[2] = sizeof(TaskAssignment) & 0xFF;

			memcpy(&arr1[3], task_Msg, sizeof(TaskAssignment));
			svcToNet_buffer.ringBuffer_put(arr1, sizeof(arr1));
			// cout << "底层传输需求已发送给net层" << endl;
			delete task_Msg;
		}
	}
	else if (si->si_value.sival_int == 2)
	{
		// 此处编写定时器的具体功能
	}
}

// 设置T2定时器
void SetTimer2(long sc, long ms, int number)
{
	// 创建定时器
	timer_t timerid;
	struct sigevent sevp;
	sevp.sigev_notify = SIGEV_SIGNAL | SIGEV_THREAD_ID;
	sevp._sigev_un._tid = syscall(SYS_gettid); // 获取LWP，标识线程身份，交给cpu使用
	sevp.sigev_signo = SIGUSR2;
	sevp.sigev_value.sival_int = number; // 将定时器标识符传递给信号处理函数
	timer_create(CLOCK_REALTIME, &sevp, &timerid);

	// 设置定时器的超时时间和间隔
	struct itimerspec its;
	its.it_value.tv_sec = sc;			 // 初次超时时间秒数
	its.it_value.tv_nsec = ms * 1000000; // 初次超时时间纳秒数
	its.it_interval.tv_sec = 0;			 // 间隔时间：0秒
	its.it_interval.tv_nsec = 0;
	timer_settime(timerid, 0, &its, NULL);
}

// SIGUSR2信号的回调函数
void timer_callback2(int signum, siginfo_t *si, void *uc)
{

	// si->si_value.sival_int为设置定时器时设置的定时器编号，再次用于区分不同定时器的功能
	if (si->si_value.sival_int == 1)
	{
		if (mmanet->Hello_Ack_Flg == false)
		{
			// char clockStr[MAX_STRING_LENGTH];
			// // cout<<"getCurrentTime : "<< getCurrentTime()<<endl;

			// TIME_PrintClockInSecond(getCurrentTime(), clockStr);
			// // cout << "-----------Current Time : " << clockStr << " Sec ------------" << endl;

			MMANETBroadcastHello(); // 进行本次Hello包发送
			mmanet->LastHelloBroadCastTime = getCurrentTime();
		}
		// 释放计时器
	}
}

// 根据全网邻居表计算路由表，并将其他模块所需信息发送给对应模块
void Count_List()
{
	// 通过全网邻居表构建映射表
	mmanet->NodeAddrToIndex.clear();
	FillNodeAddrToIndexTable();

	// 通过全网邻居表构建邻接矩阵
	mmanet->neighMatrix.clear();
	FillNeighMatrix();

	// 计算路由表
	CleanRoutingTable();
	DijkstraAlgorithm();

	// 是否需要重新生成路由表的标志
	bool isRegenerate = false;
	// 通过本地路由表对全网拓扑表进行修订,然后重新生成路由表并发送
	for (size_t i = 0; i < mmanet->LFT_Entry.size(); i++)
	{
		if (mmanet->LFT_Entry[i]->hop > 100)
		{						 // 100是随便给的一个数，表示跳数大于100跳，不可达
			isRegenerate = true; // 需要重新生成路由表
			for (size_t j = 0; j < mmanet->NetNeighList.size(); j++)
			{
				if (mmanet->LFT_Entry[i]->destAddr == mmanet->NetNeighList[j]->nodeAddr)
				{
					mmanet->NetNeighList.erase(mmanet->NetNeighList.begin() + j);
					// 应该把该行存储的指针也delete掉，释放内存
				}
			}
		}
	}

	// 通过修订的全网邻居表重新生成路由表并发送
	if (isRegenerate)
	{
		mmanet->NodeAddrToIndex.clear();
		FillNodeAddrToIndexTable();
		mmanet->neighMatrix.clear();
		FillNeighMatrix();
		CleanRoutingTable();
		DijkstraAlgorithm();
	}

	mmanet->Layer2_ForwardingTable.clear();
	FillToLayer2_ForwardingTable();

	// 进行相关数据发送，由于是深拷贝，接收方应该释放内存
	SendForwardingTable();
	SendTopologyTable();
	FindDamagedNodes();
	SendDamageReport();
	SendLayer2_ForwardingTable();
}

// 构建邻接矩阵中下标与节点地址的映射表
void FillNodeAddrToIndexTable()
{
	// 将表内所有的节点地址存入映射表中，使数组下标与地址一一对应
	for (size_t i = 0; i < mmanet->NetNeighList.size(); i++)
	{
		vector<uint16_t>::iterator iter1 = find(mmanet->NodeAddrToIndex.begin(), mmanet->NodeAddrToIndex.end(), mmanet->NetNeighList[i]->nodeAddr);
		if (iter1 == mmanet->NodeAddrToIndex.end())
		{
			mmanet->NodeAddrToIndex.push_back(mmanet->NetNeighList[i]->nodeAddr);
		}

		for (size_t j = 0; j < mmanet->NetNeighList[i]->localNeighList.size(); j++)
		{
			vector<uint16_t>::iterator iter2 = find(mmanet->NodeAddrToIndex.begin(), mmanet->NodeAddrToIndex.end(), (mmanet->NetNeighList[i]->localNeighList)[j]->nodeAddr);
			if (iter2 == mmanet->NodeAddrToIndex.end())
			{
				mmanet->NodeAddrToIndex.push_back((mmanet->NetNeighList[i]->localNeighList)[j]->nodeAddr);
			}
		}
	}
}

// 通过存储的全网邻居表构建邻接矩阵
void FillNeighMatrix()
{
	int maxLen = mmanet->NodeAddrToIndex.size();
	// 拷贝构造
	vector<vector<uint16_t>> NeighMatrix = vector<vector<uint16_t>>(maxLen, vector<uint16_t>(maxLen, INF));
	mmanet->neighMatrix = NeighMatrix;

	// 每个节点按照自己存储的全网邻居表顺序填充邻接矩阵
	for (size_t i = 0; i < mmanet->NetNeighList.size(); i++)
	{
		for (size_t j = 0; j < mmanet->NetNeighList[i]->localNeighList.size(); j++)
		{
			// 找到当前节点在自己存储的邻接矩阵中的下标
			int src = ReturnSNinLocalNeighMatrix(mmanet->NetNeighList[i]->nodeAddr);
			int coor = ReturnSNinLocalNeighMatrix(mmanet->NetNeighList[i]->localNeighList[j]->nodeAddr);
			if (src == -1 || coor == -1)
			{
				// cout << "没有在映射表中找到该节点地址" << endl;
			}

			if (src != -1 && coor != -1)
			{
				// 将邻接矩阵对应坐标置1
				mmanet->neighMatrix[src][coor] = 1;
				mmanet->neighMatrix[coor][src] = 1;
			}
		}
	}
	// // cout << "邻接矩阵为:" << endl;
	// for (const auto &innerVector : mmanet->neighMatrix)
	// {
	// 	for (const auto &element : innerVector)
	// 	{
	// 		// cout << element << ' ';
	// 	}
	// 	// cout << endl;
	// }
}

// 根据节点地址确定其在自己存储的邻接矩阵中的下标
int ReturnSNinLocalNeighMatrix(uint16_t nodeAddr)
{
	for (size_t i = 0; i < mmanet->NodeAddrToIndex.size(); i++)
	{
		if (mmanet->NodeAddrToIndex[i] == nodeAddr)
			return i;
	}

	return -1; // 没有找到，返回-1表明数据错误
}

// 根据邻接矩阵中的下标得到该下标对应的节点地址
int ReturnAddrinLocalNeighMatrix(size_t coor)
{
	if (coor >= mmanet->NodeAddrToIndex.size())
		return -1; // 超出范围，返回-1表明数据错误

	return mmanet->NodeAddrToIndex[coor];
}

// 根据源节点的邻接矩阵计算本地路由表和全网路由表
void DijkstraAlgorithm()
{
	size_t i, j, k = 0;
	int min;

	// 初始化vector数组
	vector<int> flag(mmanet->NodeAddrToIndex.size(), 0); // flag[i]表示源节点到"节点i"的最短路径是否成功获取。flag[i] = 1表示成功，flag[i] = 0表示失败
	vector<int> prev(mmanet->NodeAddrToIndex.size(), 0); // prev[i]记录i的最短路径中前驱节点的标号

	routingTableMessage *newRTM = new routingTableMessage();
	newRTM->dist = vector<int>(mmanet->NodeAddrToIndex.size(), 0);
	newRTM->path = vector<int>(mmanet->NodeAddrToIndex.size(), 0);

	// 拷贝构造
	vector<vector<nodeNetForwardingTable *>> NewNetRT_Entry = vector<vector<nodeNetForwardingTable *>>(mmanet->NodeAddrToIndex.size(), vector<nodeNetForwardingTable *>(mmanet->NodeAddrToIndex.size(), 0));
	mmanet->NetRT_Entry = NewNetRT_Entry;

	// 开始路由算法
	for (size_t coor = 0; coor < mmanet->NodeAddrToIndex.size(); coor++)
	{
		/* 1.表格初始化*/
		for (i = 0; i < mmanet->NodeAddrToIndex.size(); i++)
		{
			flag[i] = 0;									// 节点i的最短路径还没获取
			prev[i] = coor;									// 节点i的前驱顶点为源节点
			newRTM->dist[i] = mmanet->neighMatrix[coor][i]; // 节点i到目的节点的最短路径为邻接矩阵中对应的直达距离
		}

		flag[coor] = 1; // 对源节点进行初始化
		newRTM->dist[coor] = 0;

		/* 2.  遍历（节点个数 - 1）次，每次找出源节点到另一个节点的最短路径 */
		for (i = 0; i < mmanet->NodeAddrToIndex.size(); i++)
		{
			/* 2.1 在还未获取最短路径的节点中，找到离源节点最近的节点k */
			min = INF;
			for (j = 0; j < mmanet->NodeAddrToIndex.size(); j++)
			{
				if (flag[j] == 0 && newRTM->dist[j] < min)
				{
					min = newRTM->dist[j]; // 改变最近距离
					k = j;				   // 记录离源节点最近距离节点的标号j
				}
			}

			/* 2.2 对刚刚已找到最短距离的节点k进行标记 */
			flag[k] = 1; // 节点k的最短路径已经获取

			/*  2.3   已知节点k的最短路径后,更新未获取最短路径的节点的最短路径和前驱顶点*/
			for (j = 0; j < mmanet->NodeAddrToIndex.size(); j++)
			{
				int tmp = (mmanet->neighMatrix[k][j] == INF ? INF : (min + mmanet->neighMatrix[k][j]));
				if (flag[j] == 0 && (tmp < newRTM->dist[j]))
				{				 // 若节点j还未获得最短路径且从节点k到节点j得到的距离比表格记录的距离短
					prev[j] = k; // 更新k的前驱和最短距离
					newRTM->dist[j] = tmp;
				}
			}
		}
		// 至此Dijkstra算法完成，开始整理路由表

		size_t pre;
		size_t nowCoor = ReturnSNinLocalNeighMatrix(mmanet->nodeAddr); // 获得当前节点的下标
		for (i = 0; i < mmanet->NodeAddrToIndex.size(); i++)
		{
			int top = -1;
			newRTM->count = 0; // 计数
			newRTM->path[++top] = i;
			pre = prev[i];
			while (pre != coor)
			{
				newRTM->path[++top] = pre;
				pre = prev[pre];
				newRTM->count++;
			}

			if (i != coor)
			{
				if (nowCoor == coor)
					FillLocalForwardingTable(newRTM, i);
			}

			FillNetForwardingTable(newRTM, coor, i);
		}
	}
	delete newRTM;
	// 至此根据高速邻接矩阵生成的高速路由表整理完毕

	// cout << "本地路由表表项：" << endl;
	// cout << "目的节点 下一跳 跳数 时延" << endl;
	int ji;
	for (ji = 0; ji < mmanet->LFT_Entry.size(); ji++)
	{
		// cout << mmanet->LFT_Entry[ji]->destAddr << " " << mmanet->LFT_Entry[ji]->nextHop << " " << mmanet->LFT_Entry[ji]->hop << " " << mmanet->LFT_Entry[ji]->delay << endl;
	}
	// // cout<<"全网路由表表项："<<endl;
	// // cout<<"源节点 目的节点 跳数 时延 下一跳"<<endl;
	// for (const auto& innerVector : mmanet->NetRT_Entry) {
	// 	for (const auto& element : innerVector) {
	//     	if (element) {
	//         	// cout << element->source << " " << element->dest << " " << element->hop << " " << element->delay << " " << element->nexthop << endl;
	// 	 } else {
	//         	// cout << "0 " << "0 "<< "0 "<< "0 "<< "0 "<<endl;
	//     	}
	// 	}
	// 	// cout << endl;
	// }

	// 将低速一跳可达的路径加入路由表中
	if (mmanet->NetRT_Entry.size() == 0)
	{ // 为空则表示所有点都高速不可达，直接将低速一条可达的路径加入路由表中
		for (size_t i = 0; i < mmanet->disu_NetNeighList.size(); i++)
		{
			vector<nodeNetForwardingTable *> cur_NetRT_Entry;
			// 先加入自己
			nodeNetForwardingTable *my_NFT = new nodeNetForwardingTable();

			my_NFT->source = mmanet->disu_NetNeighList[i]->nodeAddr;
			my_NFT->dest = my_NFT->source;
			my_NFT->nexthop = my_NFT->dest;
			my_NFT->hop = 0;
			my_NFT->Relaynodes.push_back(my_NFT->source);
			my_NFT->Relaynodes.push_back(my_NFT->dest);
			cur_NetRT_Entry.push_back(my_NFT);

			// 再增补其低速可达节点
			for (size_t j = 0; j < mmanet->disu_NetNeighList[i]->localNeighList.size(); j++)
			{
				nodeNetForwardingTable *new_NFT = new nodeNetForwardingTable();

				new_NFT->source = mmanet->disu_NetNeighList[i]->nodeAddr;
				new_NFT->dest = (mmanet->disu_NetNeighList[i]->localNeighList)[j]->nodeAddr;
				new_NFT->nexthop = new_NFT->dest;
				new_NFT->hop = 1;
				new_NFT->Relaynodes.push_back(new_NFT->source);
				new_NFT->Relaynodes.push_back(new_NFT->dest);

				cur_NetRT_Entry.push_back(new_NFT);
			}

			mmanet->NetRT_Entry.push_back(cur_NetRT_Entry);
		}
	}
	else
	{ // 将高速不可达且低速一条可达的路径加入路由表中
		if (mmanet->disu_NetNeighList.size() != 0)
		{
			for (size_t i = 0; i < mmanet->disu_NetNeighList.size(); i++)
			{
				for (size_t j = 0; j < mmanet->NetRT_Entry.size(); j++)
				{
					if (mmanet->disu_NetNeighList[i]->nodeAddr == mmanet->NetRT_Entry[j][0]->source)
					{
						for (size_t m = 0; m < (mmanet->disu_NetNeighList[i]->localNeighList).size(); m++)
						{
							for (size_t n = 0; n < mmanet->NetRT_Entry[j].size(); n++)
							{
								if ((mmanet->disu_NetNeighList[i]->localNeighList)[m]->nodeAddr == mmanet->NetRT_Entry[j][n]->dest)
								{
									// 该目的节点有高速可达路由
									break;
								}

								if (n == mmanet->NetRT_Entry[j].size() - 1)
								{
									nodeNetForwardingTable *new_NFT = new nodeNetForwardingTable();

									new_NFT->source = mmanet->disu_NetNeighList[i]->nodeAddr;
									new_NFT->dest = (mmanet->disu_NetNeighList[i]->localNeighList)[m]->nodeAddr;
									new_NFT->nexthop = new_NFT->dest;
									new_NFT->hop = 1;
									new_NFT->Relaynodes.push_back(new_NFT->source);
									new_NFT->Relaynodes.push_back(new_NFT->dest);

									mmanet->NetRT_Entry[j].push_back(new_NFT);
									break;
								}
							}
						}

						break;
					}

					if (j == mmanet->NetRT_Entry.size() - 1)
					{
						// 高速可达路由中没有该源节点出发的路由项
						vector<nodeNetForwardingTable *> curNetRT_Entry;

						// 先加入自己
						nodeNetForwardingTable *my_NFT = new nodeNetForwardingTable();

						my_NFT->source = mmanet->disu_NetNeighList[i]->nodeAddr;
						my_NFT->dest = my_NFT->source;
						my_NFT->nexthop = my_NFT->dest;
						my_NFT->hop = 0;
						my_NFT->Relaynodes.push_back(my_NFT->source);
						my_NFT->Relaynodes.push_back(my_NFT->dest);
						curNetRT_Entry.push_back(my_NFT);

						for (size_t k = 0; k < mmanet->disu_NetNeighList[i]->localNeighList.size(); k++)
						{
							nodeNetForwardingTable *new_NFT = new nodeNetForwardingTable();

							new_NFT->source = mmanet->disu_NetNeighList[i]->nodeAddr;
							new_NFT->dest = (mmanet->disu_NetNeighList[i]->localNeighList)[k]->nodeAddr;
							new_NFT->nexthop = new_NFT->dest;
							new_NFT->hop = 1;
							new_NFT->Relaynodes.push_back(new_NFT->source);
							new_NFT->Relaynodes.push_back(new_NFT->dest);

							curNetRT_Entry.push_back(new_NFT);
						}

						mmanet->NetRT_Entry.push_back(curNetRT_Entry);
						break;
					}
				}
			}

			for (size_t i = 0; i < mmanet->disu_NetNeighList.size(); i++)
			{
				for (size_t j = 0; j < mmanet->disu_NetNeighList[i]->localNeighList.size(); j++)
				{
					for (size_t m = 0; m < mmanet->NetRT_Entry.size(); m++)
					{
						if ((mmanet->disu_NetNeighList[i]->localNeighList)[j]->nodeAddr == mmanet->NetRT_Entry[m][0]->source)
							break;

						if (m == mmanet->NetRT_Entry.size() - 1)
						{
							vector<nodeNetForwardingTable *> curNetRT_Entry;
							nodeNetForwardingTable *me_NFT = new nodeNetForwardingTable();
							me_NFT->source = (mmanet->disu_NetNeighList[i]->localNeighList)[j]->nodeAddr;
							me_NFT->dest = me_NFT->source;
							me_NFT->nexthop = me_NFT->dest;
							me_NFT->hop = 0;
							me_NFT->Relaynodes.push_back(me_NFT->source);
							me_NFT->Relaynodes.push_back(me_NFT->dest);
							curNetRT_Entry.push_back(me_NFT);

							nodeNetForwardingTable *my_NFT = new nodeNetForwardingTable();
							my_NFT->source = (mmanet->disu_NetNeighList[i]->localNeighList)[j]->nodeAddr;
							my_NFT->dest = mmanet->disu_NetNeighList[i]->nodeAddr;
							my_NFT->nexthop = my_NFT->dest;
							my_NFT->hop = 1;
							my_NFT->Relaynodes.push_back(my_NFT->source);
							my_NFT->Relaynodes.push_back(my_NFT->dest);

							curNetRT_Entry.push_back(my_NFT);
							mmanet->NetRT_Entry.push_back(curNetRT_Entry);
							break;
						}
					}
				}
			}
		}
	}

	// 将同一源节点，不同目的节点的路由项升序排列
	for (size_t i = 0; i < mmanet->NetRT_Entry.size(); i++)
	{
		sort(mmanet->NetRT_Entry[i].begin(), mmanet->NetRT_Entry[i].end(), CMP_NetRT_Entry);
	}
}

// 根据Dijkstra算法计算后的信息，填入本地路由表表项
void FillLocalForwardingTable(routingTableMessage *newRTM, int dest_coor)
{
	nodeLocalForwardingTable *newLFT = new nodeLocalForwardingTable();
	newLFT->destAddr = ReturnAddrinLocalNeighMatrix(dest_coor);
	newLFT->nextHop = ReturnAddrinLocalNeighMatrix(newRTM->path[newRTM->count]);
	newLFT->hop = newRTM->dist[dest_coor];
	mmanet->LFT_Entry.push_back(newLFT);

	// // cout << "本地路由表表项：" << endl;
	// int i;
	// for (i = 0; i < mmanet->LFT_Entry.size(); i++)
	// {
	// 	// cout << mmanet->LFT_Entry[i]->destAddr << " " << mmanet->LFT_Entry[i]->nextHop << " " << mmanet->LFT_Entry[i]->hop << " " << mmanet->LFT_Entry[i]->delay << endl;
	// }
}

// 根据Dijkstra算法计算后的信息，填入全网路由表表项
void FillNetForwardingTable(routingTableMessage *newRTM, int src_coor, int dest_coor)
{
	nodeNetForwardingTable *newNFT = new nodeNetForwardingTable();
	newNFT->source = ReturnAddrinLocalNeighMatrix(src_coor);
	newNFT->dest = ReturnAddrinLocalNeighMatrix(dest_coor);
	newNFT->nexthop = ReturnAddrinLocalNeighMatrix(newRTM->path[newRTM->count]);
	newNFT->hop = newRTM->dist[dest_coor];

	newNFT->Relaynodes.push_back(newNFT->source);
	while (newRTM->count >= 0)
	{
		uint16_t curAddr = ReturnAddrinLocalNeighMatrix(newRTM->path[newRTM->count]);
		newNFT->Relaynodes.push_back(curAddr);
		newRTM->count--;
	}
	mmanet->NetRT_Entry[src_coor][dest_coor] = newNFT;

	// // cout << "全网路由表表项：" << endl;
	// for (const auto &innerVector : mmanet->NetRT_Entry)
	// {
	// 	for (const auto &element : innerVector)
	// 	{
	// 		if (element)
	// 		{
	// 			// cout << element->source << " " << element->dest << " " << element->hop << " " << element->delay << " " << element->nexthop << endl;
	// 		}
	// 		else
	// 		{
	// 			// cout << "0 " << "0 " << "0 " << "0 " << "0 " << endl;
	// 		}
	// 	}
	// 	// cout << endl;
	// }
}

// 同一源节点的路由项按照其目的节点地址大小进行升序排列
bool CMP_NetRT_Entry(nodeNetForwardingTable *a, nodeNetForwardingTable *b)
{
	return a->dest < b->dest;
}

// 清空上一次生成的本地路由表和全网路由表
void CleanRoutingTable()
{
	// 清除本地路由表
	if (!mmanet->LFT_Entry.empty())
	{
		vector<nodeLocalForwardingTable *>::iterator it;
		for (it = mmanet->LFT_Entry.begin(); it != mmanet->LFT_Entry.end(); it++)
		{
			delete *it;
			*it = NULL;
		}
		mmanet->LFT_Entry.clear();
	}

	// 清除全网路由表
	if (!mmanet->NetRT_Entry.empty())
	{
		vector<vector<nodeNetForwardingTable *>>::iterator it1;
		vector<nodeNetForwardingTable *>::iterator it2;
		for (it1 = mmanet->NetRT_Entry.begin(); it1 != mmanet->NetRT_Entry.end(); it1++)
		{
			for (it2 = it1->begin(); it2 != it1->begin(); it2++)
			{
				delete *it2;
				*it2 = NULL;
			}
			it1->clear();
		}
		mmanet->NetRT_Entry.clear();
	}
}

// 填充发给层2的转发表
void FillToLayer2_ForwardingTable()
{
	size_t coor = 0; // 记录该源节点在全网路由表中的坐标
	// 找到该源节点在全网路由表中的坐标
	for (size_t i = 0; i < mmanet->NetRT_Entry.size(); i++)
	{
		if (mmanet->nodeAddr == mmanet->NetRT_Entry[i][0]->source)
		{
			coor = i;

			size_t maxLen = mmanet->NetRT_Entry[coor].size();
			mmanet->Layer2_ForwardingTable = vector<vector<uint16_t>>(maxLen, vector<uint16_t>(2, 0));
			for (size_t i = 0; i < mmanet->NetRT_Entry[coor].size(); i++)
			{
				mmanet->Layer2_ForwardingTable[i][0] = mmanet->NetRT_Entry[coor][i]->dest;
				mmanet->Layer2_ForwardingTable[i][1] = mmanet->NetRT_Entry[coor][i]->nexthop;
			}
		}
	}
}

// 发送全网路由表给网管模块
void SendForwardingTable()
{
	uint8_t wBufferToNet[MAX_DATA_LEN]; // 将要写入循环缓冲区的数据(含包头)存放于此

	// 使用深拷贝，将里面存放的指针也new一遍
	NetRT_EntryMsg *Cur_RT_Entry = new NetRT_EntryMsg();
	Cur_RT_Entry->netRTtablePtr = new vector<vector<nodeNetForwardingTable *>>(mmanet->NetRT_Entry.size(), vector<nodeNetForwardingTable *>(mmanet->NetRT_Entry.size(), 0));
	for (size_t i = 0; i < mmanet->NetRT_Entry.size(); i++)
	{
		for (size_t j = 0; j < mmanet->NetRT_Entry[i].size(); j++)
		{
			nodeNetForwardingTable *toNetForwardingTable = new nodeNetForwardingTable();
			toNetForwardingTable->source = mmanet->NetRT_Entry[i][j]->source;
			toNetForwardingTable->dest = mmanet->NetRT_Entry[i][j]->dest;
			toNetForwardingTable->hop = mmanet->NetRT_Entry[i][j]->hop;
			toNetForwardingTable->delay = mmanet->NetRT_Entry[i][j]->delay;
			toNetForwardingTable->nexthop = mmanet->NetRT_Entry[i][j]->nexthop;
			toNetForwardingTable->Relaynodes = mmanet->NetRT_Entry[i][j]->Relaynodes;
			(*Cur_RT_Entry->netRTtablePtr)[i][j] = toNetForwardingTable;
		}
	}

	// 浅拷贝直接传指针
	// NetRT_EntryMsg *Cur_RT_Entry = new NetRT_EntryMsg();
	// Cur_RT_Entry->netRTtablePtr = &(mmanet->NetRT_Entry);

	// 封装数据类型字段：0x1B路由信息
	wBufferToNet[0] = 0x1B;

	// 封装数据长度字段，指针长度为8字节
	wBufferToNet[1] = 0x00;
	wBufferToNet[2] = 0x08;

	// 封装数据，将指针赋值给数组
	memcpy(&wBufferToNet[3], Cur_RT_Entry, sizeof(Cur_RT_Entry));
	// cout << "Cur_RT_Entry" << Cur_RT_Entry << endl;
	routingToNet_buffer.ringBuffer_put(wBufferToNet, sizeof(wBufferToNet));
	// cout << "156636" << endl;
}

// 发送全网邻居表（拓扑表）给网管模块
void SendTopologyTable()
{
	uint8_t wBufferToNet[MAX_DATA_LEN]; // 将要写入循环缓冲区的数据(含包头)存放于此

	// 使用深拷贝时，是否应该将里面存放的指针也new一遍
	NetworkTopologyMsg *Cur_Topo_Entry = new NetworkTopologyMsg();
	Cur_Topo_Entry->netNetghPtr = new vector<nodeNetNeighList *>(mmanet->NetNeighList.size(), 0);
	for (size_t i = 0; i < mmanet->NetNeighList.size(); i++)
	{
		nodeNetNeighList *toNetTopologyTable = new nodeNetNeighList();
		toNetTopologyTable->localNeighList.push_back(0); // 使vector不为空
		toNetTopologyTable->nodeAddr = mmanet->NetNeighList[i]->nodeAddr;
		for (size_t j = 0; j < (mmanet->NetNeighList[i]->localNeighList).size(); j++)
		{
			nodeLocalNeighList *toNetLocalNeighList = new nodeLocalNeighList();
			toNetLocalNeighList->nodeAddr = mmanet->NetNeighList[i]->localNeighList[j]->nodeAddr;
			toNetLocalNeighList->Delay = mmanet->NetNeighList[i]->localNeighList[j]->Delay;
			toNetLocalNeighList->Capacity = mmanet->NetNeighList[i]->localNeighList[j]->Capacity;
			toNetLocalNeighList->Queuelen = mmanet->NetNeighList[i]->localNeighList[j]->Queuelen;
			toNetLocalNeighList->Load = mmanet->NetNeighList[i]->localNeighList[j]->Load;

			toNetTopologyTable->localNeighList[j] = toNetLocalNeighList;
		}
		(*Cur_Topo_Entry->netNetghPtr)[i] = toNetTopologyTable;
	}
	// 浅拷贝直接传指针
	// NetworkTopologyMsg *Cur_Topo_Entry = new NetworkTopologyMsg();
	// Cur_Topo_Entry->netNetghPtr = &(mmanet->NetNeighList);

	// 封装数据类型字段：0x1C拓扑感知信息
	wBufferToNet[0] = 0x1C;

	// 封装数据长度字段，指针长度为8字节
	wBufferToNet[1] = 0x00;
	wBufferToNet[2] = 0x08;

	// 封装数据，将指针赋值给数组
	memcpy(&wBufferToNet[3], Cur_Topo_Entry, sizeof(Cur_Topo_Entry));
	routingToNet_buffer.ringBuffer_put(wBufferToNet, sizeof(wBufferToNet));
}

// 找损伤节点
void FindDamagedNodes()
{
	mmanet->DamagedNodes.clear(); // 清空上一次毁伤表信息
	if (mmanet->old_node_set.empty())
	{
		for (size_t i = 0; i < mmanet->NetNeighList.size(); i++)
		{
			mmanet->old_node_set.emplace_back(mmanet->NetNeighList[i]->nodeAddr);
		}
		return;
	}
	else
	{
		if (!mmanet->NetNeighList.empty())
		{
			int flg = 0;
			for (size_t i = 0; i < mmanet->old_node_set.size(); i++)
			{
				for (size_t j = 0; j < mmanet->NetNeighList.size(); j++)
				{
					if (mmanet->NetNeighList[j]->nodeAddr == mmanet->old_node_set[i])
					{
						flg = 1;
						break;
					}
				}

				if (flg == 0)
				{
					mmanet->DamagedNodes.emplace_back(mmanet->old_node_set[i]);
				}

				flg = 0;
			}
		}

		mmanet->old_node_set.clear();
		for (size_t i = 0; i < mmanet->NetNeighList.size(); i++)
		{
			mmanet->old_node_set.emplace_back(mmanet->NetNeighList[i]->nodeAddr);
		}
	}
}

// 发送毁伤感知信息给网管模块
void SendDamageReport()
{
	uint8_t wBufferToNet[MAX_DATA_LEN]; // 将要写入循环缓冲区的数据(含包头)存放于此

	// 使用深拷贝时，是否应该将里面存放的指针也new一遍
	DamagedNodes_Msg *Cur_DamageReport_Msg = new DamagedNodes_Msg();
	if (mmanet->DamagedNodes.empty())
	{
		Cur_DamageReport_Msg->DamagedNodes = NULL;
	}
	else
	{
		// 深拷贝
		Cur_DamageReport_Msg->DamagedNodes = new vector<NodeAddress>(mmanet->DamagedNodes);
		// 浅拷贝
		// Cur_DamageReport_Msg->DamagedNodes = &(mmanet->DamagedNodes);
	}

	// 封装数据类型字段：0x1D毁伤感知信息
	wBufferToNet[0] = 0x1D;

	// 封装数据长度字段，指针长度为8字节
	wBufferToNet[1] = 0x00;
	wBufferToNet[2] = 0x08;

	// 封装数据，将指针赋值给数组
	memcpy(&wBufferToNet[3], Cur_DamageReport_Msg, sizeof(Cur_DamageReport_Msg));
	routingToNet_buffer.ringBuffer_put(wBufferToNet, sizeof(wBufferToNet));
}

// 发送转发表信息给链路层
void SendLayer2_ForwardingTable()
{
	uint8_t wBufferToMac[MAX_DATA_LEN]; // 将要写入循环缓冲区的数据(含包头)存放于此

	// 深拷贝
	Layer2_ForwardingTable_Msg *Cur_ForwardingTable_Msg = new Layer2_ForwardingTable_Msg();
	Cur_ForwardingTable_Msg->Layer2_FT_Ptr = new vector<vector<uint16_t>>(mmanet->Layer2_ForwardingTable);

	// 浅拷贝直接传指针
	// Layer2_ForwardingTable_Msg *Cur_ForwardingTable_Msg = new Layer2_ForwardingTable_Msg();
	// Cur_ForwardingTable_Msg->Layer2_FT_Ptr = &(mmanet->Layer2_ForwardingTable);

	// 封装数据类型字段：0x0E表示转发表信息
	wBufferToMac[0] = 0x0E;

	// 封装数据长度字段，指针长度为8字节
	wBufferToMac[1] = 0x00;
	wBufferToMac[2] = 0x08;

	// 封装数据，将指针赋值给数组
	memcpy(&wBufferToMac[3], Cur_ForwardingTable_Msg, sizeof(Cur_ForwardingTable_Msg));
	RoutingTomac_Buffer.ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
	// cout << "路由模块发送转发表信息给链路层" << endl;
}

// 更新全网拓扑表，保持一致性
void UpdateNetNeighList(nodeNetNeighList *newNetNeighListMember)
{
	// 拓扑表一致性
	if (mmanet->NetNeighList.empty())
	{
		for (size_t i = 0; i < newNetNeighListMember->localNeighList.size(); i++)
		{
			nodeNetNeighList *new_NetNeighListMember = new nodeNetNeighList();

			new_NetNeighListMember->nodeAddr = newNetNeighListMember->localNeighList[i]->nodeAddr;
			nodeLocalNeighList *newNodeLocalNeighList = new nodeLocalNeighList();
			newNodeLocalNeighList->nodeAddr = newNetNeighListMember->nodeAddr;
			newNodeLocalNeighList->Delay = newNetNeighListMember->localNeighList[i]->Delay;
			newNodeLocalNeighList->Capacity = newNetNeighListMember->localNeighList[i]->Capacity;
			newNodeLocalNeighList->Queuelen = newNetNeighListMember->localNeighList[i]->Queuelen;
			newNodeLocalNeighList->Load = newNetNeighListMember->localNeighList[i]->Load;

			new_NetNeighListMember->localNeighList.push_back(newNodeLocalNeighList);
			mmanet->NetNeighList.push_back(new_NetNeighListMember);
		}
		return;
	}

	for (size_t i = 0; i < mmanet->NetNeighList.size(); i++)
	{
		if (mmanet->NetNeighList[i]->nodeAddr == newNetNeighListMember->nodeAddr)
		{
			for (size_t j = 0; j < mmanet->NetNeighList[i]->localNeighList.size(); j++)
			{
				for (size_t k = 0; k < newNetNeighListMember->localNeighList.size(); k++)
				{
					if (mmanet->NetNeighList[i]->localNeighList[j]->nodeAddr == newNetNeighListMember->localNeighList[k]->nodeAddr)
						break;

					if (k == newNetNeighListMember->localNeighList.size() - 1)
					{
						// 新的邻居表项中没找到旧表中存在的邻居, 在拓扑表中找到对应的条目进行删减
						for (size_t m = 0; m < mmanet->NetNeighList.size(); m++)
						{
							if (mmanet->NetNeighList[m]->nodeAddr == mmanet->NetNeighList[i]->localNeighList[j]->nodeAddr)
							{
								for (size_t n = 0; n < mmanet->NetNeighList[m]->localNeighList.size(); n++)
								{
									if (mmanet->NetNeighList[m]->localNeighList[n]->nodeAddr == mmanet->NetNeighList[i]->nodeAddr)
									{
										mmanet->NetNeighList[m]->localNeighList.erase(mmanet->NetNeighList[m]->localNeighList.begin() + n);

										if (mmanet->NetNeighList[m]->localNeighList.size() == 0)
										{
											mmanet->NetNeighList.erase(mmanet->NetNeighList.begin() + m);
										}
										break;
									}
								}
								break;
							}
						}
						break;
					}
				}
			}

			for (size_t j = 0; j < newNetNeighListMember->localNeighList.size(); j++)
			{
				for (size_t k = 0; k < mmanet->NetNeighList[i]->localNeighList.size(); k++)
				{
					if (newNetNeighListMember->localNeighList[j]->nodeAddr == mmanet->NetNeighList[i]->localNeighList[k]->nodeAddr)
						break;

					if (k == mmanet->NetNeighList[i]->localNeighList.size() - 1)
					{
						// 旧的邻居表项中没找到新表中存在的邻居。在拓扑表对应表项中进行添加，若未找到表项，则添加新表项
						for (size_t m = 0; m < mmanet->NetNeighList.size(); m++)
						{
							if (newNetNeighListMember->localNeighList[j]->nodeAddr == mmanet->NetNeighList[m]->nodeAddr)
							{
								for (size_t n = 0; n < mmanet->NetNeighList[m]->localNeighList.size(); n++)
								{
									if (mmanet->NetNeighList[m]->localNeighList[n]->nodeAddr == newNetNeighListMember->nodeAddr)
										break;

									if (n == mmanet->NetNeighList[m]->localNeighList.size() - 1)
									{
										nodeLocalNeighList *newNodeLocalNeighList = new nodeLocalNeighList();

										newNodeLocalNeighList->nodeAddr = newNetNeighListMember->nodeAddr;
										newNodeLocalNeighList->Delay = newNetNeighListMember->localNeighList[j]->Delay;
										newNodeLocalNeighList->Capacity = newNetNeighListMember->localNeighList[j]->Capacity;
										newNodeLocalNeighList->Queuelen = newNetNeighListMember->localNeighList[j]->Queuelen;
										newNodeLocalNeighList->Load = newNetNeighListMember->localNeighList[j]->Load;
										mmanet->NetNeighList[m]->localNeighList.push_back(newNodeLocalNeighList);

										sort(mmanet->NetNeighList[m]->localNeighList.begin(), mmanet->NetNeighList[m]->localNeighList.end(), CMP_neighborList);
										break;
									}
								}
								break;
							}

							if (m == mmanet->NetNeighList.size() - 1)
							{
								// 未找到表项，添加新表项
								nodeNetNeighList *nei_NetNeighListMember = new nodeNetNeighList();
								nei_NetNeighListMember->nodeAddr = newNetNeighListMember->localNeighList[j]->nodeAddr;
								nodeLocalNeighList *newNodeLocalNeighList = new nodeLocalNeighList();
								newNodeLocalNeighList->nodeAddr = newNetNeighListMember->nodeAddr;
								newNodeLocalNeighList->Delay = newNetNeighListMember->localNeighList[j]->Delay;
								newNodeLocalNeighList->Capacity = newNetNeighListMember->localNeighList[j]->Capacity;
								newNodeLocalNeighList->Queuelen = newNetNeighListMember->localNeighList[j]->Queuelen;
								newNodeLocalNeighList->Load = newNetNeighListMember->localNeighList[j]->Load;

								nei_NetNeighListMember->localNeighList.push_back(newNodeLocalNeighList);
								mmanet->NetNeighList.push_back(nei_NetNeighListMember);
								break;
							}
						}
						break;
					}
				}
			}
			break;
		}
	}
}

// 按照节点序号将本地邻居表进行升序排列
bool CMP_neighborList(nodeLocalNeighList *a, nodeLocalNeighList *b)
{
	return a->nodeAddr < b->nodeAddr;
}

// 从链路层接收链路状态信息
void MSG_ROUTING_ReceiveLocalLinkMsg(vector<uint8_t> &curPktPayload)
{
	// char clockStr[MAX_STRING_LENGTH];
	// TIME_PrintClockInSecond(node->getNodeTime(), clockStr);d
	// // cout << "-----------Current Time : " << clockStr << " Sec ------------" << endl;
	//  // cout << node->nodeId << endl;
	//   Message *timerMsgPtr = & mmanet->timerMsg;
	//  MESSAGE_CancelSelfMsg(node,timerMsgPtr);//取消1s定时器，防止流程冲突
	// cout << "收链路状态信息" << endl;
	mmanet->ReceiveLocalLinkMsg_Flg = true;

	if (mmanet->Hello_Ack_Flg == false)
	{

		ReceiveLocalLinkState_Msg *ReceiveLocalLinkState = (ReceiveLocalLinkState_Msg *)(&curPktPayload[0]);
		// // cout << " ReceiveLocalLinkState->neighborList.size() " << ReceiveLocalLinkState->neighborList.size() << endl;
		// // cout << "ReceiveLocalLinkState->neighborList[0].nodeaddress " << ReceiveLocalLinkState->neighborList[0].nodeAddr << endl;
		// // cout << "ReceiveLocalLinkState->neighborList[1].nodeaddress " << ReceiveLocalLinkState->neighborList[1].nodeAddr << endl;

		vector<nodeLocalNeighList> Cur_neighborList;

		Cur_neighborList = ReceiveLocalLinkState->neighborList;
		for (auto node : mmanet->cur_neighborList)
		{
			delete node;
		}
		mmanet->cur_neighborList.clear();
		mmanet->cur_neighborList = mmanet->old_neighborList;

		// // cout << "节点" << mmanet->nodeAddr << "收到链路层发来的的本地链路状态信息为：";
		// for (int i = 0; i < Cur_neighborList->size(); i++)
		// {
		// 	// cout << " Cur_neighborList->nodeAddr " << (*Cur_neighborList)[i]->nodeAddr << " ";
		// 	// cout << endl;
		// 	for (int j = 0; j < mmanet->cur_neighborList.size(); j++)
		// 	{
		// 		if (mmanet->cur_neighborList[j]->nodeAddr == (*Cur_neighborList)[i]->nodeAddr)
		// 		{
		// 			mmanet->cur_neighborList.erase(mmanet->cur_neighborList.begin() + j);
		// 			j--;
		// 		}
		// 	}
		// 	mmanet->cur_neighborList.push_back((*Cur_neighborList)[i]);
		// }
		// mmanet->old_neighborList = (*Cur_neighborList);
		// delete Cur_neighborList;
		for (int i = 0; i < Cur_neighborList.size(); i++)
		{
			// // cout << " Cur_neighborList->nodeAddr " << (Cur_neighborList)[i].nodeAddr << " ";
			// // cout << endl;
			for (int j = 0; j < mmanet->cur_neighborList.size(); j++)
			{
				if (mmanet->cur_neighborList[j]->nodeAddr == (Cur_neighborList)[i].nodeAddr)
				{
					mmanet->cur_neighborList.erase(mmanet->cur_neighborList.begin() + j);
					j--;
				}
			}
			mmanet->cur_neighborList.push_back(new nodeLocalNeighList(Cur_neighborList[i]));
			// // cout << " Cur_neighborList->nodeAddr " << (Cur_neighborList)[i].nodeAddr << " ";
			// // cout << " mmanet->cur_neighborList " << mmanet->cur_neighborList[i]->nodeAddr << " " << endl;
			// 动态分配,防止Cur_neighborList被销毁后，指针变得无效
			// mmanet->cur_neighborList.push_back(&(Cur_neighborList)[i]);
		}
		// 更新mmanet.old_neighborList内容
		for (auto node : mmanet->old_neighborList)
		{
			delete node;
		}
		mmanet->old_neighborList.clear();
		for (const auto &node : Cur_neighborList)
		{
			mmanet->old_neighborList.push_back(new nodeLocalNeighList(node));
		}

		// // cout << "mmanet->cur_neighborList->nodeAddr " << endl;
		// for (auto node : mmanet->cur_neighborList)
		// {
		// 	if (node != 0)
		// 	{
		// 		// cout << node->nodeAddr << "  ";
		// 	}
		// }
		// // cout << endl;

		// // cout << "mmanet->old_neighborList->nodeAddr " << endl;
		// for (auto node : mmanet->old_neighborList)
		// {
		// 	if (node != 0)
		// 	{
		// 		// cout << node->nodeAddr << "  ";
		// 	}
		// }
		// // cout << endl;
		/*for (int i = 0; i < mmanet->cur_neighborList.size(); i++)
			{
				if((mmanet->cur_neighborList)[i]->Capacity < 0)
				{
					mmanet->cur_neighborList.erase(mmanet->cur_neighborList.begin() + i);
					i--;
				}
			}*/

		/*for (int i = 0; i < mmanet->cur_neighborList.size(); i++)
		{
			// cout << (mmanet->cur_neighborList)[i]->nodeAddr << "  ";
			// cout << (mmanet->cur_neighborList)[i]->Capacity << "  ";

		}
		// cout << endl;
		// cout << "节点数:    " << mmanet->cur_neighborList.size() << endl;*/
		// // cout << "MMANET module has received Link Msg from lower layer" << endl;
		/*if(mmanet->Layer2_ForwardingTable.size() != 0){

			for(int i = 0;i < mmanet->Layer2_ForwardingTable.size();i++)
			{
				for(int j = 0;j < mmanet->cur_neighborList.size();j++)
				{
					if(mmanet->cur_neighborList[j]->nodeAddr == mmanet->Layer2_ForwardingTable[i][0])
					{
						mmanet->Layer2_ForwardingTable[i][1] = mmanet->Layer2_ForwardingTable[i][0];
					}
				}
			}
			SendLayer2_ForwardingTable(node, mmanet);
		}*/
		MMANETBroadcastHello();
	}
}

// 开始Hello流程
void MMANETBroadcastHello()
{
	// cout << "开始Hello流程" << endl;
	// Message *newHelloMsg;
	// newHelloMsg = MESSAGE_Alloc(node, NETWORK_LAYER, ROUTING_PROTOCOL_MMANET, MSG_ROUTING_ReceiveMsgFromLowerLayer);

	// 接下来开始加包头和负载

	// 1.加包头
	msgFromControl *newHelloHeader = new msgFromControl();
	// 给包头成员赋值
	newHelloHeader->srcAddr = mmanet->nodeAddr; // 测试时用节点号代替地址
	newHelloHeader->destAddr = ANY_DEST;		// Hello广播目的地址为全1地址
	// 后面给其他包头成员赋值
	newHelloHeader->msgType = 2;
	newHelloHeader->repeat = 0;
	newHelloHeader->fragmentNum = 1;
	newHelloHeader->fragmentSeq = 1;

	//......
	vector<uint8_t> *hello_neighborArray = new vector<uint8_t>; // 存放hello广播内容的数组
	// 测试用，存储节点号
	hello_neighborArray->push_back((mmanet->nodeAddr >> 24) & 0xFF);
	hello_neighborArray->push_back((mmanet->nodeAddr >> 16) & 0xFF);
	hello_neighborArray->push_back((mmanet->nodeAddr >> 8) & 0xFF);
	hello_neighborArray->push_back((mmanet->nodeAddr) & 0xFF);
	// 记录源节点发送时间
	hello_neighborArray->push_back((getCurrentTime() >> 56) & 0xFF);
	hello_neighborArray->push_back((getCurrentTime() >> 48) & 0xFF);
	hello_neighborArray->push_back((getCurrentTime() >> 40) & 0xFF);
	hello_neighborArray->push_back((getCurrentTime() >> 32) & 0xFF);
	hello_neighborArray->push_back((getCurrentTime() >> 24) & 0xFF);
	hello_neighborArray->push_back((getCurrentTime() >> 16) & 0xFF);
	hello_neighborArray->push_back((getCurrentTime() >> 8) & 0xFF);
	hello_neighborArray->push_back((getCurrentTime()) & 0xFF);

	vector<uint8_t> *cur_data = new vector<uint8_t>; // 填充data
	msgID *curmsgID = new msgID;
	curmsgID->msgid = 29;
	curmsgID->msgsub = 3;

	cur_data->push_back((curmsgID->msgid << 3) | (curmsgID->msgsub));
	for (int i = 0; i < 12; i++)
	{
		cur_data->push_back((*hello_neighborArray)[i]);
	}
	newHelloHeader->packetLength = cur_data->size() + 10;
	newHelloHeader->data = (char *)cur_data;
	vector<uint8_t> *Hello_packetedDataPkt = new vector<uint8_t>;
	Hello_packetedDataPkt = PackMsgFromControl(newHelloHeader);

	uint8_t wBufferToMac[MAX_DATA_LEN];

	mmanet->Hello_Ack_Flg = true;
	// MMANETSendPacket(node, newHelloMsg, node->nodeId, ANY_DEST, 1); // 对于广播，目的地址为全1地址，下一跳暂时也赋值为全1地址
	MMANETSendPacket(Hello_packetedDataPkt, mmanet->nodeAddr, ANY_DEST, 1); // 对于广播，目的地址为全1地址，下一跳暂时也赋值为全1地址
	mmanet->signalOverhead += newHelloHeader->packetLength;
}

// 将网络层产生的封装完毕网管数据包等交给链路层以在网络中发送
void MMANETSendPacket(vector<uint8_t> *curPktPayload, NodeAddress srcAddr, NodeAddress destAddr, int i)
{
	if (destAddr == ANY_DEST)
	{
		// char *curPktPayload1 = MESSAGE_ReturnPacket(msg);
		// vector<uint8_t> curPktPayload(curPktPayload1, curPktPayload1 + MESSAGE_ReturnPacketSize(msg)); // 取数组
		msgFromControl *cur_msgFromControl = new msgFromControl();
		cur_msgFromControl = AnalysisLayer2MsgFromControl(curPktPayload); // 解析数组，还原为包结构体类型
		// // cout << "mmanet->cur_neighborList.size()" << mmanet->cur_neighborList.size() << endl;
		if (i == 1 && mmanet->cur_neighborList.size() != 0)
		{
			for (int i = 0; i < mmanet->cur_neighborList.size(); i++)
			{
				// cout << "发送hello给邻居  " << mmanet->cur_neighborList[i]->nodeAddr << endl;
				cur_msgFromControl->destAddr = mmanet->cur_neighborList[i]->nodeAddr;

				vector<uint8_t> *Hello_packetedDataPkt = new vector<uint8_t>;
				Hello_packetedDataPkt = PackMsgFromControl(cur_msgFromControl);

				uint8_t wBufferToMac[MAX_DATA_LEN];
				// 将封装后的业务数据包封装层间接口数据帧成发给层2
				PackRingBuffer(wBufferToMac, Hello_packetedDataPkt, 0x10);
				RoutingTomac_Buffer.ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
				delete Hello_packetedDataPkt;
				// // cout << "发给由mac层收到的邻居节点" << cur_msgFromControl->destAddr << endl;
			}

			// Message *timerMsg_Broadcast = MESSAGE_Alloc(node, NETWORK_LAYER, ROUTING_PROTOCOL_MMANET, MSG_ROUTING_StartNeighBroadCast);
			// clocktype delay_broadcast = MMANET_NEIGHBOR_BROADCAST_TIMER;
			// cout << "SetTimer Start" << endl;
			// writeInfo("定時器1設置");
			// printf("----------------Hello探测定時器设置--------------\n");
			SetTimer(0, 600, 1); // 用于触发广播流程 //Hello探测的时间0.6s
								 // MESSAGE_Send(node, timerMsg_Broadcast, delay_broadcast); // 用于触发广播流程
		}
		if (i == 2 && mmanet->neighborList.size() != 0)
		{
			// 邻居表广播流程
			// 邻居表根据存储好的邻居表表项进行广播
			for (int i = 0; i < mmanet->neighborList.size(); i++)
			{

				cur_msgFromControl->destAddr = mmanet->neighborList[i]->nodeAddr;
				// cout << "邻居表广播,发给节点" << cur_msgFromControl->destAddr << endl;

				vector<uint8_t> *BC_packetedDataPkt = new vector<uint8_t>;
				BC_packetedDataPkt = PackMsgFromControl(cur_msgFromControl);

				uint8_t wBufferToMac[MAX_DATA_LEN];
				// 将封装后的业务数据包封装层间接口数据帧成发给层2
				PackRingBuffer(wBufferToMac, BC_packetedDataPkt, 0x10);
				RoutingTomac_Buffer.ringBuffer_put(wBufferToMac, sizeof(wBufferToMac));
				delete BC_packetedDataPkt;
			}
			// MESSAGE_Free(node, msg); // 把原来的msg释放了
		}
	}
}

bool CMP_NetNeighList(nodeNetNeighList *a, nodeNetNeighList *b)
{
	return a->nodeAddr < b->nodeAddr;
}

void MMANETBroadcastMessage(char *NeighList, int a)
{
	// 接下来开始加包头和负载
	vector<nodeLocalNeighList *> *neighborListPtr = (vector<nodeLocalNeighList *> *)NeighList; // 指针指向邻居表
	vector<uint8_t> *neighborlistArray = new vector<uint8_t>;								   // 把邻居表内容放到一个数组中

	for (int i = 0; i < neighborListPtr->size(); i++)
	{
		// 邻居节点地址
		neighborlistArray->push_back(((*neighborListPtr)[i]->nodeAddr >> 8) & 0xFF);
		neighborlistArray->push_back(((*neighborListPtr)[i]->nodeAddr) & 0xFF);
		// 时延
		neighborlistArray->push_back(((*neighborListPtr)[i]->Delay >> 56) & 0xFF);
		neighborlistArray->push_back(((*neighborListPtr)[i]->Delay >> 48) & 0xFF);
		neighborlistArray->push_back(((*neighborListPtr)[i]->Delay >> 40) & 0xFF);
		neighborlistArray->push_back(((*neighborListPtr)[i]->Delay >> 32) & 0xFF);
		neighborlistArray->push_back(((*neighborListPtr)[i]->Delay >> 24) & 0xFF);
		neighborlistArray->push_back(((*neighborListPtr)[i]->Delay >> 16) & 0xFF);
		neighborlistArray->push_back(((*neighborListPtr)[i]->Delay >> 8) & 0xFF);
		neighborlistArray->push_back(((*neighborListPtr)[i]->Delay) & 0xFF);
		// 链路容量
		neighborlistArray->push_back(((*neighborListPtr)[i]->Capacity >> 8) & 0xFF);
		neighborlistArray->push_back(((*neighborListPtr)[i]->Capacity) & 0xFF);
		// 队列长度
		neighborlistArray->push_back(((*neighborListPtr)[i]->Queuelen >> 8) & 0xFF);
		neighborlistArray->push_back(((*neighborListPtr)[i]->Queuelen) & 0xFF);
	}
	int BC_datasize = MMANET_MFC_DATASIZE - 1;
	int count = 0; // 分片数
	if (neighborlistArray->size() % BC_datasize != 0)
	{
		count = neighborlistArray->size() / BC_datasize + 1;
	}
	else
	{
		count = neighborlistArray->size() / BC_datasize;
	}

	vector<msgFromControl *> *array_fmsgFromControl = new vector<msgFromControl *>; // 存放业务数据包指针的数组
	if (a == 1)
	{
		mmanet->sn++;
	}
	else
	{
		mmanet->disu_sn++;
	} // 序列号累加
	for (int i = 1; i <= count; i++)
	{
		msgFromControl *curBroadCastMsgHeader = new msgFromControl();
		// 给包头赋值
		curBroadCastMsgHeader->srcAddr = mmanet->nodeAddr; // 测试用地址为节点号
		// curBroadCastMsgHeader->srcAddr = mmanet->nodeAddr;//给定源地址
		curBroadCastMsgHeader->destAddr = ANY_DEST; // 广播，目的地址为全1地址
		// 接下来进行包头其他信息的赋值
		curBroadCastMsgHeader->msgType = 2;
		// curBroadCastMsgHeader->priority = ...
		//......
		vector<uint8_t> *cur_data = new vector<uint8_t>;
		curBroadCastMsgHeader->fragmentNum = (unsigned int)count;
		curBroadCastMsgHeader->fragmentSeq = (unsigned int)i;
		msgID *curmsgID = new msgID;
		curmsgID->msgid = 29;
		if (a == 1)
		{
			curmsgID->msgsub = 5;
			curBroadCastMsgHeader->seq = mmanet->sn;
			if (mmanet->List_Flg == true)
			{
				bool Seq_Flg = false;
				for (int i = 0; i < mmanet->sn_NetNeighList.size(); i++)
				{ // 一个新的结构体去存储sn号，
					if (mmanet->sn_NetNeighList[i]->nodeAddr == mmanet->nodeAddr)
					{
						Seq_Flg = true;
						mmanet->sn_NetNeighList[i]->SN = mmanet->sn;
						break;
					}
				}
				if (Seq_Flg == false)
				{
					sn_nodeNetNeighList *temp_sn = new sn_nodeNetNeighList();
					temp_sn->nodeAddr = mmanet->nodeAddr;
					temp_sn->SN = mmanet->sn;
					mmanet->sn_NetNeighList.push_back(temp_sn);
				}
			}
		}
		else
		{
			curmsgID->msgsub = 6;
			curBroadCastMsgHeader->seq = mmanet->disu_sn;
			if (mmanet->List_Flg == true)
			{
				bool Seq_Flg = false;
				for (int i = 0; i < mmanet->disu_sn_NetNeighList.size(); i++)
				{ // 一个新的结构体去存储sn号，
					if (mmanet->disu_sn_NetNeighList[i]->nodeAddr == mmanet->nodeAddr)
					{
						Seq_Flg = true;
						mmanet->disu_sn_NetNeighList[i]->SN = mmanet->disu_sn;
						break;
					}
				}
				if (Seq_Flg == false)
				{
					sn_nodeNetNeighList *temp_sn = new sn_nodeNetNeighList();
					temp_sn->nodeAddr = mmanet->nodeAddr;
					temp_sn->SN = mmanet->disu_sn;
					mmanet->disu_sn_NetNeighList.push_back(temp_sn);
				}
			}
			/*// cout <<" 序列号"<< mmanet->disu_sn << endl;
			// cout << "        " << node->nodeId << endl;
			for(int i = 0; i < mmanet->disu_neighborList.size(); i++){
				// cout << mmanet->disu_neighborList[i]->nodeAddr << " ";
			}
			// cout << endl;*/
		}
		cur_data->push_back((curmsgID->msgid << 3) | (curmsgID->msgsub));
		for (int j = 0; j < BC_datasize; j++)
		{ // 分片存入数据

			if ((j + (i - 1) * BC_datasize) <= (neighborlistArray->size() - 1)) // 若为最后一片，且数据没有存满，在数据后面补0
			{
				cur_data->push_back((*neighborlistArray)[j + (i - 1) * BC_datasize]);
			}
		}
		curBroadCastMsgHeader->packetLength = cur_data->size() + 10;
		curBroadCastMsgHeader->data = (char *)(cur_data);
		array_fmsgFromControl->push_back(curBroadCastMsgHeader);
	}
	for (int i = 0; i < array_fmsgFromControl->size(); i++)
	{
		// 封装成vector<uint8_t>数组
		vector<uint8_t> *BCpacketedDataPkt = new vector<uint8_t>;
		BCpacketedDataPkt = PackMsgFromControl((*array_fmsgFromControl)[i]); // 调用函数对每个包进行封装

		// 将封装后的业务数据包发给层2
		// Message *curBroadcastMsg = MESSAGE_Alloc(node, NETWORK_LAYER, ROUTING_PROTOCOL_MMANET, MSG_ROUTING_ReceiveMsgFromLowerLayer);

		// MESSAGE_PacketAlloc(node, curBroadcastMsg, BCpacketedDataPkt->size(), TRACE_MMANET);
		// memcpy(MESSAGE_ReturnPacket(curBroadcastMsg), BCpacketedDataPkt->data(), BCpacketedDataPkt->size());

		MMANETSendPacket(BCpacketedDataPkt, mmanet->nodeAddr, ANY_DEST, 2); // 对于广播，目的地址为全1地址，下一跳暂时也赋值为全1地址
		mmanet->signalOverhead += (*array_fmsgFromControl)[i]->packetLength;
	}
}

void saveRESULTfile(vector<double> data1, vector<double> data2, vector<double> data3, string filename, MMANETData *mmanet)
{
	ofstream comp_output(filename);
	if (false == comp_output.is_open())
	{
		return;
	}
	if (mmanet->Lstm_Flg == true)
	{
		comp_output << "real_time     predict_time    predict     " << endl;
	}

	for (unsigned int i = 0; i < data1.size(); i++)
	{
		if (i > 3 && mmanet->Lstm_Flg == true)
		{
			comp_output << data1[i] << "   " << data2[i - 4] << "   " << data3[i - 4] << endl;
		}
		else
		{
			comp_output << data1[i] << endl;
		}
	}

	comp_output.close();
}

ReceiveLocalLinkState_Msg *AnalysisLayer2MsgLocalLinkState(vector<uint8_t> *dataPacket)
{
	// ReceiveLocalLinkState_Msg temp_LocalLinkState;
	// temp_LocalLinkState.neighborList;

	// for (size_t i = 0; i < dataPacket->size(); i += sizeof(nodeLocalNeighList))
	// {
	// 	nodeLocalNeighList Node;
	// 	// Node.nodeAddr = ((*dataPacket)[0] << 8 | (*dataPacket)[1]);
	// 	memcpy(&Node.nodeAddr, &((*dataPacket)[i]), sizeof(uint16_t));
	// 	memcpy(&Node.Delay, &((*dataPacket)[i + sizeof(uint16_t)]), sizeof(clocktype));
	// 	memcpy(&Node.Capacity, &((*dataPacket)[i + sizeof(uint16_t) + sizeof(clocktype)]), sizeof(uint16_t));
	// 	memcpy(&Node.Queuelen, &((*dataPacket)[i + sizeof(uint16_t) + sizeof(clocktype) + sizeof(uint16_t)]), sizeof(uint16_t));
	// 	memcpy(&Node.Load, &((*dataPacket)[i + sizeof(uint16_t) + sizeof(clocktype) + sizeof(uint16_t) + sizeof(uint16_t)]), sizeof(float));
	// 	Node.nodeAddr = (Node.nodeAddr & 0xFF) << 8 | (Node.nodeAddr >> 8) & 0xFF;
	// 	// cout << " AnalysisLayer2MsgLocalLinkState ->nodeAddr " << hex << Node.nodeAddr << dec << endl;
	// 	temp_LocalLinkState.neighborList.push_back(Node);
	// }
	ReceiveLocalLinkState_Msg *temp_LocalLinkState = (ReceiveLocalLinkState_Msg *)dataPacket;
	// cout << " temp_LocalLinkState->neighborList.size() " << temp_LocalLinkState->neighborList.size() << endl;
	// cout << "temp_LocalLinkState->neighborList[0].nodeaddress " << temp_LocalLinkState->neighborList[0].nodeAddr << endl;
	// cout << "temp_LocalLinkState->neighborList[1].nodeaddress " << temp_LocalLinkState->neighborList[1].nodeAddr << endl;

	return temp_LocalLinkState;
}