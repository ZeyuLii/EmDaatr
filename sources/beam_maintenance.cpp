/********************************************波束维护模块************************************************************* */
#include "beam_maintenance.h"
#include <cmath>

static A_ang Coordinate_System_Transaction(float lat_1, float lng_1, float h_1, float pitch_1, float roll_1, int index, float yaw_1, float lat_2, float lng_2, float h_2)
{
    lat_1 = PI * lat_1 / 180.0;
    lng_1 = PI * lng_1 / 180.0;
    pitch_1 = PI * pitch_1 / 180.0;
    roll_1 = PI * roll_1 / 180.0;
    yaw_1 = PI * yaw_1 / 180.0;
    lat_2 = PI * lat_2 / 180.0;
    lng_2 = PI * lng_2 / 180.0;
    float e2 = (pow(RADIS_A, 2) - pow(RADIS_B, 2)) / pow(RADIS_A, 2); // 椭球偏心率
    float n_1 = RADIS_A / sqrt(1 - e2 * pow(sin(lat_1), 2));
    float n_2 = RADIS_A / sqrt(1 - e2 * pow(sin(lat_2), 2));
    float Pdx = 0, Pdy = 0, Pdz = 0; // 地心地固坐标系下的相对位置
    Pdx = (n_2 + h_2) * cos(lat_2) * cos(lng_2) - (n_1 + h_1) * cos(lat_1) * cos(lng_1);
    Pdy = (n_2 + h_2) * cos(lat_2) * sin(lng_2) - (n_1 + h_1) * cos(lat_1) * sin(lng_1);
    Pdz = (n_2 * (1 - e2) + h_2) * sin(lat_2) - (n_1 * (1 - e2) + h_1) * sin(lat_1);
    float dx = 0, dy = 0, dz = 0; // 地理坐标系下的相对位置
    dx = -sin(lat_1) * cos(lng_1) * Pdx - sin(lat_1) * sin(lng_1) * Pdy + cos(lat_1) * Pdz;
    dy = cos(lat_1) * cos(lng_1) * Pdx + cos(lat_1) * sin(lng_1) * Pdy + sin(lat_1) * Pdz;
    dz = -sin(lng_1) * Pdx + cos(lng_1) * Pdy;
    float dx1 = 0, dy1 = 0, dz1 = 0; // DT坐标系下的相对位置
    dx1 = cos(yaw_1) * cos(pitch_1) * dx + sin(pitch_1) * dy - sin(yaw_1) * cos(pitch_1) * dz;
    dy1 = (sin(yaw_1) * sin(roll_1) - cos(yaw_1) * sin(pitch_1) * cos(roll_1)) * dx + cos(pitch_1) * cos(roll_1) * dy + (cos(yaw_1) * sin(roll_1) + sin(yaw_1) * sin(pitch_1) * cos(roll_1)) * dz;
    dz1 = (sin(yaw_1) * cos(roll_1) + cos(yaw_1) * sin(pitch_1) * sin(roll_1)) * dx - cos(pitch_1) * sin(roll_1) * dy + (cos(yaw_1) * cos(roll_1) - sin(yaw_1) * sin(pitch_1) * sin(roll_1)) * dz;
    float dr = 0; // 节点距离
    dr = sqrt(pow(dx, 2) + pow(dy, 2) + pow(dz, 2));
    float ang_x = 0, ang_z = 0; // ang_x为侧面三个天线板的坐标系旋转角度,
    // ang_z为前后两个天线板的坐标系旋转角度
    float dxs = 0, dys = 0, dzs = 0; // 天线系下指向
    // 选择天线板, 并计算该天线系下指向
    switch (index)
    {
    case 1:
        ang_x = PI / 3.0;
        dxs = dx1;
        dys = cos(ang_x) * dy1 - sin(ang_x) * dz1;
        dzs = sin(ang_x) * dy1 + cos(ang_x) * dz1;
        break;
    case 2:
        ang_x = PI;
        dxs = dx1;
        dys = cos(ang_x) * dy1 - sin(ang_x) * dz1;
        dzs = sin(ang_x) * dy1 + cos(ang_x) * dz1;
        break;
    case 3:
        ang_x = -PI / 3.0;
        dxs = dx1;
        dys = cos(ang_x) * dy1 - sin(ang_x) * dz1;
        dzs = sin(ang_x) * dy1 + cos(ang_x) * dz1;
        break;
    case 4:
        ang_z = -PI / 2.0;
        dxs = cos(ang_z) * dx1 - sin(ang_z) * dy1;
        dys = sin(ang_z) * dx1 + cos(ang_z) * dy1;
        dzs = dz1;
        break;
    case 5:
        ang_z = PI / 2.0;
        dxs = cos(ang_z) * dx1 - sin(ang_z) * dy1;
        dys = sin(ang_z) * dx1 + cos(ang_z) * dy1;
        dzs = dz1;
        break;
    default:
        printf("ERROR!\n");
        break;
    }
    A_ang a_ang;
    a_ang.el = asin(dxs / dr);
    a_ang.az = atan(dzs / dys);
    a_ang.b_ang = acos(fabs(cos(a_ang.el) * cos(a_ang.az)));
    a_ang.dys = dys;
    return a_ang;
}

// 返回俯仰角 方位角 等天线指向信息
A_ang Get_Angle_Info(MacPacket_Daatr *macpacket_daatr, MacDaatr *macdata_daatr)
{

    float lat_1 = macdata_daatr->local_node_position_info.positionX;
    float lng_1 = macdata_daatr->local_node_position_info.positionY;
    float h_1 = macdata_daatr->local_node_position_info.positionZ;
    short int pitch_1 = macdata_daatr->local_node_position_info.pitchAngle;
    short int roll_1 = macdata_daatr->local_node_position_info.rollAngle;
    short int yaw_1 = macdata_daatr->local_node_position_info.yawAngle;
    float lat_2 = macpacket_daatr->node_position.positionX;
    float lng_2 = macpacket_daatr->node_position.positionY;
    float h_2 = macpacket_daatr->node_position.positionZ;
    A_ang a_ang[5];
    // 计算5个天线对应的俯仰角和方位角等信息
    a_ang[0] = Coordinate_System_Transaction(lat_1, lng_1, h_1, pitch_1, roll_1, 1, yaw_1, lat_2, lng_2, h_2);
    a_ang[1] = Coordinate_System_Transaction(lat_1, lng_1, h_1, pitch_1, roll_1, 2, yaw_1, lat_2, lng_2, h_2);
    a_ang[2] = Coordinate_System_Transaction(lat_1, lng_1, h_1, pitch_1, roll_1, 3, yaw_1, lat_2, lng_2, h_2);
    a_ang[3] = Coordinate_System_Transaction(lat_1, lng_1, h_1, pitch_1, roll_1, 4, yaw_1, lat_2, lng_2, h_2);
    a_ang[4] = Coordinate_System_Transaction(lat_1, lng_1, h_1, pitch_1, roll_1, 5, yaw_1, lat_2, lng_2, h_2);
    float flag[5] = {0}; // 俯仰角和方位角绝对值之和
    // 是否在波束范围内的判断条件: (1)位于天线板上方(dys>0);
    // (2)波束指向向量与天线坐标系的y轴正向的夹角b_ang小于等于60°
    if (a_ang[0].dys > 0 && a_ang[0].b_ang <= PI / 3.0)
    {
        flag[0] = fabs(a_ang[0].el) + fabs(a_ang[0].az);
    }
    if (a_ang[1].dys > 0 && a_ang[1].b_ang <= PI / 3.0)
    {
        flag[1] = fabs(a_ang[1].el) + fabs(a_ang[1].az);
    }
    if (a_ang[2].dys > 0 && a_ang[2].b_ang <= PI / 3.0)
    {
        flag[2] = fabs(a_ang[2].el) + fabs(a_ang[2].az);
    }
    if (a_ang[3].dys > 0 && a_ang[3].b_ang <= PI / 3.0)
    {
        flag[3] = fabs(a_ang[3].el) + fabs(a_ang[3].az);
    }
    if (a_ang[4].dys > 0 && a_ang[4].b_ang <= PI / 3.0)
    {
        flag[4] = fabs(a_ang[4].el) + fabs(a_ang[4].az);
    }
    float min = 0;
    int min_index = 0;
    for (int i = 0; i < 5; i++)
    {
        if (flag[i] != 0)
        {
            min = flag[i];
            min_index = i;
            break;
        }
    }
    for (int j = 0; j < 5; j++)
    {
        if (flag[j] != 0)
        {
            if (flag[j] < min)
            {
                min = flag[j];
                min_index = j;
            }
        }
    }

    a_ang[min_index].el = a_ang[min_index].el * 180.0 / PI;
    a_ang[min_index].az = a_ang[min_index].az * 180.0 / PI;
    a_ang[min_index].index = min_index + 1;
    // printf("最佳天线编号: %d, 俯仰角: %lf, 方位角: %lf", min_index + 1,
    // a_ang[min_index].el, a_ang[min_index].az);
    writeInfo("最佳天线编号: %d, 俯仰角: %lf, 方位角: %lf", min_index + 1, a_ang[min_index].el, a_ang[min_index].az);
    return a_ang[min_index];
}

/* ------ 波束维护模块 ------ */
// 完成从 short int 到 -pi/2, pi/2 的转换
static float Angle_Transaction_short_to_pi(short int ang)
{
    ang = ((ang + 32768.0) / 65536.0) * 180.0 - 90.0;
    return ang;
}

// 完成从 -pi/2, pi/2 到  short int 的转换
// 量化精度可能会出现问题?
static short int Angle_Transaction_pi_to_short(float ang)
{
    ang = ((ang + 90.0) / 180.0) * 65536.0 - 32768.0;
    return (short int)ang;
}

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
    macdata_daatr->if_receive_mana_flight_frame[node_temp - 1] = true;
    // macdata_daatr->businessQueue[node_temp - 1].distance =
    //     calculateLinkDistance(macdata_daatr->local_node_position_info.positionX,
    //                           macdata_daatr->local_node_position_info.positionY,
    //                           macdata_daatr->local_node_position_info.positionZ,
    //                           macpacket_daatr->node_position.positionX,
    //                           macpacket_daatr->node_position.positionY,
    //                           macpacket_daatr->node_position.positionZ);
    macdata_daatr->businessQueue[node_temp - 1].distance = 1;
    // cout << "Node " << node_temp << " distance: " << macdata_daatr->businessQueue[node_temp - 1].distance << " km " << endl;
}