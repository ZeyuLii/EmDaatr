#include "timer.h"
#include "vector"
#include "macdaatr.h"

using namespace std;

#define EVENT_MAX_NUMBER 25 // 定时器最大数量
// 示例
//  extern uint16_t event_number_now;
//  for(int i = 0;i < 10;i++)
//  {
//      insert_event_timer_ms(1,hello);
//      event_timmer_update();
//      cout << "定时器现有数据：" << event_number_now << endl;
//  }
//  for(int i = 0;i < 12;i++)
//  {
//      event_timmer_update();
//  }

myevent event_list[EVENT_MAX_NUMBER];
uint16_t event_number_now = 0;

/**
 * @brief 插入一个微秒级别的事件定时器
 *
 * @param time_del 事件触发前的延迟时间，单位为微秒
 * @param event_func 事件触发时要执行的函数指针
 * @return true 成功插入事件定时器
 * @return false 事件列表已满，无法插入新的定时器
 */
bool insert_event_timer_us(uint64_t time_del, event_function event_func)
{
    if (event_number_now >= EVENT_MAX_NUMBER)
        return false;
    for (int i = 0; i < EVENT_MAX_NUMBER; i++)
    {
        if (event_list[i].if_used == false)
        { // 是否是空闲
            event_list[i].if_used = true;
            event_list[i].time_delay = time_del;
            event_list[i].func = event_func;
            event_list[i].time_has_passed = 0;
            return true;
        }
    }
    return false;
}

/**
 * @brief 插入一个毫秒级别的事件定时器
 *
 * @param time_del 事件触发前的延迟时间，单位为微秒
 * @param event_func 事件触发时要执行的函数指针
 * @return true 成功插入事件定时器
 * @return false 事件列表已满，无法插入新的定时器
 */
bool insert_event_timer_ms(double time_del, event_function event_func)
{
    time_del *= 1000; // 转化成us
    for (int i = 0; i < EVENT_MAX_NUMBER; i++)
    {
        if (event_list[i].if_used == false)
        { // 是否是空闲
            event_list[i].if_used = true;
            event_list[i].time_delay = time_del;
            event_list[i].func = event_func;
            event_list[i].time_has_passed = 0;
            event_number_now += 1;
            return true;
        }
    }
    cout << "事件队列已满！！！" << endl;
    return false;
}

void event_timmer_update()
{
    if (event_number_now == 0)
        return;
    for (int i = 0; i < EVENT_MAX_NUMBER; i++)
    {
        if (event_list[i].if_used == true)
        { // 是否是空闲
            event_list[i].time_has_passed += TIME_PRECISION;
            if (event_list[i].time_has_passed >= event_list[i].time_delay)
            {
                event_list[i].func();
                event_list[i].if_used = false;
                event_number_now -= 1;
            }
        }
    }
}
