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

#include <string>
#include <sstream>
#include <chrono>

#include <bitset>

#include "routing_mmanet.h"
#include "common_struct.h"
#include "network.h"

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

// extern timescount; //循环计数
// extern sleeptime;   //最小发送间隔

#define FLIGHTSTATUSFILE "./config/011.txt"
#define TEXTNODENUM 3
int lengthdata = 300; // 发送格式化消息数据长度300byte

using namespace std;

// 定义格式化消息结构体
struct svcToMac
{
    unsigned int dest : 10;
    unsigned int src : 10;
    unsigned int priority : 2;
    unsigned int reTrans : 1;
    unsigned int spare : 2;
    unsigned short packetLength;
    unsigned int transEnable : 1;
    unsigned short fragmentNum;
    unsigned int msgType : 3;
    unsigned int submsgType : 5;
    // char* data;					  // 报文内容
    vector<uint8_t> data;
};

vector<uint8_t> *PackMsgSvc(svcToMac *packet, int length)
{
    std::vector<uint8_t> *buffer = new std::vector<uint8_t>;

    // 封装第一个字节
    buffer->push_back((packet->dest >> 2) & 0xFF);

    // 封装第二个字节
    buffer->push_back(((packet->dest & 0b11) << 6) | ((packet->src >> 4) & 0b111111));

    // 封装第三个字节
    buffer->push_back(((packet->src & 0x03) << 6) | ((packet->priority & 0x03) << 2) | ((packet->reTrans & 0x01) << 1) | (packet->spare << 1));

    // 封装第四个字节
    buffer->push_back(((packet->spare & 0x01) << 7) | ((packet->packetLength & 0x7F00) >> 9) & 0xFF);

    // 封装第五个字节
    //
    buffer->push_back(((packet->packetLength & 0xFFFE) >> 1));

    // 封装第六个字节
    buffer->push_back((((packet->packetLength & 0x01) << 7) | (packet->transEnable & 0x01) << 6 | ((packet->fragmentNum & 0xFC00) >> 10)) & 0xFF);

    // 封装第七个字节  流水号装了一半不装了
    buffer->push_back((packet->fragmentNum << 6) & 0xFF);

    // 封装第八个字节
    // buffer->push_back(((packet->fragmentNum << 4) | packet->fragmentSeq) & 0xFF);

    for (size_t i = 0; i < length; i++)
    {
        buffer->push_back(packet->data[i]);
    }

    return buffer;
}

void readFlightStatusFile(FlightStatus *flightstatusptr, const string filename, int lineNum)
{
    // 创建一个输入文件流对象
    ifstream infile(filename);

    // 检查文件是否成功打开
    if (!infile)
    {
        cerr << "无法打开文件: " << filename << endl;
        return;
    }
    if (infile.peek() == ifstream::traits_type::eof())
    {
        cerr << "文件 " << filename << " 为空！" << endl;
        infile.close();
        return;
    }
    string line; // 用于存储每一行数据
    int currentLine = 0;
    int inlinenum = lineNum % 80;

    // 逐行读取文件
    while (getline(infile, line))
    {
        currentLine++;
        if (currentLine == inlinenum)
        {
            stringstream ss(line);
            int lineIndex;   // 用于存储行索引
            ss >> lineIndex; // 读取第一个数字，表示行号
            ss >> flightstatusptr->positionX >> flightstatusptr->positionY >> flightstatusptr->positionZ >> flightstatusptr->speedX >> flightstatusptr->speedY >> flightstatusptr->speedZ;
            flightstatusptr->nodeId = mmanet->nodeAddr;
            // cout << "readFlightStatusFile mmanet->nodeAddr" << mmanet->nodeAddr << " flightstatusptr->nodeId " << flightstatusptr->nodeId << endl;
            infile.close();
        }
    }
}

void *SvcToAll(void *arg)
{
    extern bool end_simulation;
    cout << "创建Svc线程成功!" << endl;
    sleep(5);
    uint8_t buffer_svc[MAX_DATA_LEN]; // 将从循环缓冲区读取出来的数据(含包头)存放于此
    int currentime = 0;
    cout << "***********************" << endl;

    while (!end_simulation)
    {

        // 测试飞行状态信息，读文件
        // cout << "***********************" << endl;
        uint8_t net_buffer_svc1[MAX_DATA_LEN];
        unsigned int fullpacketSize = sizeof(FlightStatus);
        char *msgToSendPtr = (char *)malloc(fullpacketSize);
        FlightStatus *nodePosPtr = (FlightStatus *)msgToSendPtr;
        int currentlint;
        if (mmanet->nodeAddr == 1)
        {
            currentlint = currentime + mmanet->nodeAddr;
        }
        if (mmanet->nodeAddr == 2)
        {
            currentlint = currentime + mmanet->nodeAddr + 19;
        }
        if (mmanet->nodeAddr == 3)
        {
            currentlint = currentime + mmanet->nodeAddr + 38;
        }
        else
        {
            currentlint = currentime + mmanet->nodeAddr + 57;
        }
        // int currentlint = currentime * TEXTNODENUM + mmanet->nodeAddr;
        // int currentlint = mmanet->nodeAddr;
        readFlightStatusFile(nodePosPtr, FLIGHTSTATUSFILE, currentlint);
        uint8_t arr5[MAX_DATA_LEN];
        PackRingBuffer(arr5, msgToSendPtr, fullpacketSize, 0x01);
        svcToNet_buffer.ringBuffer_put(arr5, sizeof(arr5));
        // cout << "已发送本地飞行状态信息给网络层" << endl;
        // cout << nodePosPtr->nodeId << " positionX " << nodePosPtr->positionX << "  positionY " << nodePosPtr->positionY << " positionZ " << nodePosPtr->positionZ << " speedX " << nodePosPtr->speedX<< " speedY " << nodePosPtr->speedY<< " speedZ " << nodePosPtr->speedZ<< endl;
        // cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;

        // 格式化消息
        if (currentime == 10) // 第11秒发送格式化消息
        {
            uint8_t buffer_svc1[MAX_DATA_LEN];
            svcToMac *svc1 = new svcToMac();
            if (mmanet->nodeAddr == 1)
            {
                svc1->dest = 0b0000100001; // 目的地址
                svc1->src = 0b0011100111;  // 源地址
                svc1->priority = 3;
                svc1->reTrans = 0;
                svc1->spare = 0;
                svc1->packetLength = 0;
                svc1->transEnable = 0;
                svc1->fragmentNum = 3;
                svc1->msgType = 7;
                svc1->submsgType = 10;
            }
            else if (mmanet->nodeAddr == 2)
            {
                svc1->dest = 0b0000100010; // 目的地址
                svc1->src = 0b0011100111;  // 源地址
                svc1->priority = 3;
                svc1->reTrans = 1;
                svc1->spare = 0;
                svc1->packetLength = 0;
                svc1->transEnable = 1;
                svc1->fragmentNum = 3;
                svc1->msgType = 7;
                svc1->submsgType = 10;
            }
            else if (mmanet->nodeAddr == 3)
            {
                svc1->dest = 0b0000100011; // 目的地址
                svc1->src = 0b0011100111;  // 源地址
                svc1->priority = 3;
                svc1->reTrans = 1;
                svc1->spare = 0;
                svc1->packetLength = 0;
                svc1->transEnable = 1;
                svc1->fragmentNum = 3;
                svc1->msgType = 7;
                svc1->submsgType = 10;
            }
            // 分配内存给svc1->data，长度为lengthdata
            svc1->data = vector<uint8_t>();
            for (size_t i = 0; i < lengthdata; ++i)
            {
                svc1->data.push_back(0xAD);
            }
            // vector<uint8_t> *repeat = PackMsgSvc(svc1, lengthdata);
            // PackRingBuffer(buffer_svc1, repeat, 0x08);

            // 调试信息
            //  cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! svc1->data.size() " << svc1->data.size() << " <repeat->size() " << repeat->size() << endl;
            //  svcToRouting_buffer.ringBuffer_put(buffer_svc1, sizeof(buffer_svc1));
            //  for (int i = 0; i < 8; i++)
            //  {
            //      bitset<8> binary(buffer_svc1[3 + i]); // 创建一个8位的bitset，并用buffer_svc1[3+i]初始化
            //      cout << " " << binary.to_string();    // 输出二进制字符串表示
            //  }
            //  cout << endl;
            //  cout << "已显示发送端binary" << endl;
            //  for (int i = 0; i < 8; i++)
            //  {
            //      cout << " " << (int)buffer_svc1[3 + i];
            //  }
            //  cout << "已显示发送端" << endl;
            //  cout << "%%%%%%%%%%%%%%%%%%%%%已发送格式化消息给路由层" << endl;
        }

        if (currentime == 4) // 第10秒发送底层传输需求
        {
            TaskAssignment *task_Msg = new TaskAssignment();
            // 发送底层传输需求
            if (mmanet->nodeAddr == 1)
            {
                task_Msg->begin_node = 1;
                task_Msg->end_node = 2;
                task_Msg->type = 3;
                task_Msg->priority = 1;
                task_Msg->size = 1024; // 即0.1mb
                task_Msg->QOS = 10;
                // task_Msg->begin_time[0] = 10;
                // task_Msg->begin_time[1] = 20;
                // task_Msg->begin_time[2] = 30;
                // task_Msg->end_time[0] = 40;
                // task_Msg->end_time[1] = 50;
                // task_Msg->end_time[2] = 60;
                // task_Msg->frequency = 1;
            }
            else if (mmanet->nodeAddr == 2)
            {

                task_Msg->begin_node = 2;
                task_Msg->end_node = 3;
                task_Msg->type = 3;
                task_Msg->priority = 1;
                task_Msg->size = 1024;
                task_Msg->QOS = 10;
                // task_Msg->begin_time[0] = 10;
                // task_Msg->begin_time[1] = 20;
                // task_Msg->begin_time[2] = 30;
                // task_Msg->end_time[0] = 40;
                // task_Msg->end_time[1] = 50;
                // task_Msg->end_time[2] = 60;
                // task_Msg->frequency = 1;
            }
            else if (mmanet->nodeAddr == 3)
            {
                task_Msg->begin_node = 3;
                task_Msg->end_node = 1;
                task_Msg->type = 3;
                task_Msg->priority = 1;
                task_Msg->size = 1024;
                task_Msg->QOS = 10;
                // task_Msg->begin_time[0] = 10;
                // task_Msg->begin_time[1] = 20;
                // task_Msg->begin_time[2] = 30;
                // task_Msg->end_time[0] = 40;
                // task_Msg->end_time[1] = 50;
                // task_Msg->end_time[2] = 60;
                // task_Msg->frequency = 1;
            }
            else if (mmanet->nodeAddr == 4)
            {
                task_Msg->begin_node = 4;
                task_Msg->end_node = 3;
                task_Msg->type = 3;
                task_Msg->priority = 1;
                task_Msg->size = 1024;
                task_Msg->QOS = 10;
                // task_Msg->begin_time[0] = 10;
                // task_Msg->begin_time[1] = 20;
                // task_Msg->begin_time[2] = 30;
                // task_Msg->end_time[0] = 40;
                // task_Msg->end_time[1] = 50;
                // task_Msg->end_time[2] = 60;
                // task_Msg->frequency = 1;
            }
            uint8_t arr1[MAX_DATA_LEN];
            arr1[0] = 0x06;
            // 封装数据长度字段
            arr1[1] = (sizeof(TaskAssignment) >> 8) & 0xFF;
            arr1[2] = sizeof(TaskAssignment) & 0xFF;

            memcpy(&arr1[3], task_Msg, sizeof(TaskAssignment));
            svcToNet_buffer.ringBuffer_put(arr1, sizeof(arr1));
            cout << "节点 " << mmanet->nodeAddr << " 的底层传输需求已发送给net层" << endl;
            delete task_Msg;
        }
        currentime++;
        // usleep(50000); // 休眠50ms
        sleep(1);
    }

    return NULL;
}