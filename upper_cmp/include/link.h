#ifndef __LINK_H
#define __LINK_H

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
#include <sstream>
#include <regex>
bool link_init();
void drop_link_process();

#endif