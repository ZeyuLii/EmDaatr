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
void writeInfo(const char *fmt, ...);

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