#ifndef __BEAM_MAINTENANCE_H__
#define __BEAM_MAINTENANCE_H__

#include "macdaatr.h"
#include "timer.h"

void UpdatePosition(MacPacket_Daatr *macpacket_daatr, MacDaatr *macdata_daatr);
A_ang Get_Angle_Info(MacPacket_Daatr *macpacket_daatr, MacDaatr *macdata_daatr);

#endif