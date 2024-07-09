#ifndef __SOCKET_CONTROL_H
#define __SOCKET_CONTROL_H

#include <cstdint>
#include <thread>
#include <mutex>
#include <string.h>

void macDaatrSocketLowFreq_Send(uint8_t *data, uint32_t len);
void macDaatrSocketHighFreq_Send(uint8_t *data, uint32_t len, uint16_t nodeId);
void macDaatrSocketLowFreq_Recv(bool IF_NOT_BLOCKED);
void macDaatrSocketHighFreq_Recv(bool IF_NOT_BLOCKED);

#endif