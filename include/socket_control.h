#ifndef __SOCKET_CONTROL_H
#define __SOCKET_CONTROL_H
#include <cstdint>
#include <thread>
#include <mutex>
#include <string.h>

void mac_daatr_low_freq_socket_send(uint8_t* data, uint32_t len);
void mac_daatr_high_freq_socket_send(uint8_t* data, uint32_t len, uint16_t nodeId);
void mac_daatr_socket_low_freq_thread(bool IF_NOT_BLOCKED = false);
void mac_daatr_socket_high_freq_thread(bool IF_NOT_BLOCKED = false);
#endif