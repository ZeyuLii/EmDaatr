#ifndef __TIMER_H__
#define __TIMER_H__
#include "cstdint"

typedef void (*event_function)(void); // 回调函数指针

typedef struct myevent
{
    uint64_t time_delay;      // 时延（单位：us）
    uint64_t time_has_passed; // 已经经过的时间
    event_function func;      // 回调函数指针
    bool if_used;             // 是否是空的 false表示空，true表示非空
} myevent;

bool insertEventTimer_us(uint64_t time_del, event_function event_func);
bool insertEventTimer_ms(double time_del, event_function event_func);
void eventTimerUpdate();
#endif