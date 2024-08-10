#ifndef __SOCKET_CONTROL_H
#define __SOCKET_CONTROL_H

#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <fstream>
#include <cstdint>
#include <thread>
#include <mutex>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "socket_control.h"
#include <arpa/inet.h>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <cstdlib>
#include <stdio.h>

int macDaatrCreateUDPSocket(std::string ip, int port, bool if_not_block);
void macDaatrSocketLowFreq_Send(uint8_t *data, uint32_t len, int id);

#endif