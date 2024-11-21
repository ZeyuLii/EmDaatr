#ifndef __HIGHFREQTOOLFUNC_H
#define __HIGHFREQTOOLFUNC_H

#include <math.h>
#include <vector>

#include "common_struct.h"
#include "macdaatr.h"

double Calculate_Transmission_Rate_by_Distance(double distance);

bool judgeReTrans(const msgFromControl &MFC_temp, const vector<msgFromControl> &MFC_list_temp);

void Business_Lead_Forward(MacDaatr *macdata_daatr, int dest_node, int prio, int location);

void MFC_Priority_Adjustment(MacDaatr *macdata_daatr);

int getAvailableLocationOfQueue(MacDaatr *macdata_daatr, int dest_node, int pri);

int searchNextHopAddr(MacDaatr *macdata_daatr, int destAddr);

void Insert_MFC_to_Queue(MacDaatr *macdata_daatr, msgFromControl *busin, int pri, int loc, int temp, int my_pkt_flag);

bool Compare_by_Priority(const LinkAssignment_single LA1, const LinkAssignment_single LA2);

void ReAllocate_Traffic_slottable(MacDaatr *macdata_daatr, vector<Alloc_slot_traffic> &Alloc_slottable_traffic,
                                  vector<LinkAssignment_single> &LinkAssignment_Storage);

void sendLocalLinkStatus(MacDaatr *macdata_daatr);

void sendOtherNodePosition(MacDaatr *macdata_daatr);

vector<LinkAssignment> *Generate_LinkAssignment_Initialization(MacDaatr *macdata_daatr);

void generateSlottableExecution(MacDaatr *macdata_daatr);

unsigned char Generate_M_SequenceValue();

void adjustNarrowBand(unsigned int freqSeq[SUBNET_NODE_NUMBER_MAX][FREQUENCY_COUNT]);

void judgeIfEnterFreqAdjustment(MacDaatr *macdata_daatr);

void MacDaatrInitialize_Frequency_Seqence(MacDaatr *macdata_daatr);

int Generate_LinkAssignment_Stage_1(LinkAssignment link_assign[]);
#endif