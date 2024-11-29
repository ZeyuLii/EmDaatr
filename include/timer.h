/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or
 * install git & please set dead value or install git
 * @Date: 2024-10-27 18:20:34
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or
 * install git & please set dead value or install git
 * @LastEditTime: 2024-10-30 01:51:14
 * @FilePath: /EmDaatr/include/timer.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置:
 * https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef __TIMER_H__
#define __TIMER_H__

#include "cstdint"
#include "daatr_control.h"
#include "macdaatr.h"
#include "main.h"
#include "socket_control.h"
#include "vector"
#include <assert.h>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

#define writeInfo(fmt, ...) _writeInfo(1, fmt, ##__VA_ARGS__)

using namespace std;

typedef void (*event_function)(void); // 回调函数指针

typedef struct myevent {
    uint64_t time_delay;      // 时延（单位：us）
    uint64_t time_has_passed; // 已经经过的时间
    event_function func;      // 回调函数指针
    bool if_used;             // 是否是空的 false表示空，true表示非空
} myevent;

void timeInit();
void ppsIRQ();
void microsecondIRQ();
void ppsIRQHandle(int signum, siginfo_t *info, void *context);
int utcGet();
int utcToint(const char *str, int find);
void parseCpuTimes();
double calculateCpuUsage();
void _writeInfo(bool addTimeHeadFlag, const char *fmt, ...);

bool insertEventTimer_us(uint64_t time_del, event_function event_func);
bool insertEventTimer_ms(double time_del, event_function event_func);
void eventTimerUpdate();

extern char simInfo[12000][200];
extern int simInfoPosition;
extern mutex mtx;
extern int PPS_overcount;
extern int fd_irq;
extern int pid;
extern timespec tpstart;

#endif