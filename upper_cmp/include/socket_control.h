/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-10-27 18:20:34
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-11-01 02:30:12
 * @FilePath: /upper_cmp/include/socket_control.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-10-27 18:20:34
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-10-30 01:23:47
 * @FilePath: /upper_cmp/include/socket_control.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
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
#include <iostream>
#include <vector>
#include <chrono>
#include <map>
#include <functional>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>

int macDaatrCreateUDPSocket(std::string ip, int port, bool if_not_block);
void macDaatrSocketLowFreq_Send(uint8_t *data, uint32_t len, int id);
void link_data_print();

#endif