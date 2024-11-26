#ifndef __LOW_FREQ_CHANNEL_H
#define __LOW_FREQ_CHANNEL_H

// std::mutex lowthreadmutex;
// std::condition_variable lowthreadcondition_variable;

void lowFreqSendThread();
bool MacDaatNetworkHasLowFreqChannelPacketToSend(msgFromControl *busin, MacDaatr *macdata_daatr);
void lowFreqChannelRecvHandle(uint8_t *bit_seq, uint64_t len);

#endif