/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @Date: 2024-10-27 18:20:34
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 * @LastEditTime: 2024-10-30 01:51:14
 * @FilePath: /EmDaatr/include/timer.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef __TIMER_H__
#define __TIMER_H__

#include "cstdint"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include "daatr_control.h"
#include "socket_control.h"
#include "macdaatr.h"
#include "main.h"
#include <pthread.h>
#include "vector"
#include <stdarg.h>
#include <sys/time.h>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <sstream>

#define writeInfo(fmt, ...) _writeInfo(1, fmt, ##__VA_ARGS__)

using namespace std;

typedef void (*event_function)(void); // 回调函数指针

typedef struct myevent
{
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