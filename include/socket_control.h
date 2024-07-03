#ifndef __SOCKET_CONTROL_H
#define __SOCKET_CONTROL_H
#include <cstdint>
#include <thread>
#include <mutex>
#include <string.h>

void macDaatrLowFreqSocketSend(uint8_t *data, uint32_t len);
void macDaatrHighFreqSocketSend(uint8_t *data, uint32_t len, uint16_t nodeId);
void macDaatrSocketLowFreqThread(bool IF_NOT_BLOCKED = false);
void macDaatrSocketHighFreqThread(bool IF_NOT_BLOCKED = false);
#endif