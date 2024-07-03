/********************************************波束维护模块************************************************************* */
#include "beam_maintenance.h"
#include <cmath>
// 返回任意两节点间距离(单位: km)
double calculateLinkDistance(float lat_1, float lng_1, float h_1, float lat_2, float lng_2, float h_2)
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

// 根据网管信道数据包更新子网内各节点的位置信息
// 并将其更新业务信道队列的 distance
void UpdatePosition(MacPacket_Daatr *macpacket_daatr, MacDaatr *macdata_daatr)
{
    for (int i = 0; i < SUBNET_NODE_NUMBER_MAX - 1; i++)
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