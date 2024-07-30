#ifndef __HIGHFREQTOOLFUNC_H
#define __HIGHFREQTOOLFUNC_H

#include <vector>
#include <math.h>

#include "macdaatr.h"
#include "common_struct.h"

double Calculate_Transmission_Rate_by_Distance(double distance);

bool Judge_If_Include_ReTrans(const msgFromControl &MFC_temp,
                              const vector<msgFromControl> &MFC_list_temp);

void Business_Lead_Forward(MacDaatr *macdata_daatr, int dest_node, int prio, int location);

void MFC_Priority_Adjustment(MacDaatr *macdata_daatr);

int getAvailableLocationOfQueue(MacDaatr *macdata_daatr, int dest_node, int pri);

int searchNextHopAddr(MacDaatr *macdata_daatr, int destAddr);

void Insert_MFC_to_Queue(MacDaatr *macdata_daatr, msgFromControl *busin,
                         int pri, int loc, int temp, int my_pkt_flag);

bool Compare_by_Priority(const LinkAssignment_single LA1, const LinkAssignment_single LA2);

void ReAllocate_Traffic_slottable(MacDaatr *macdata_daatr,
                                  vector<Alloc_slot_traffic> &Alloc_slottable_traffic,
                                  vector<LinkAssignment_single> &LinkAssignment_Storage);
#endif