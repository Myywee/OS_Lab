
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               proc.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "proto.h"
#include "string.h"
#include "proc.h"
#include "global.h"

/*======================================================================*
                              schedule
 *======================================================================*/
PUBLIC void schedule()
{
    PROCESS *p = p_proc_ready;
    int current_tick = get_ticks();

    while (1)
    {
        ++p;
        if (p >= proc_table + NR_TASKS)
        {
            p = proc_table;
            current_tick = get_ticks();
        }
        if (p->sleep_until <= current_tick &&
            !(p->blocked))
        {
            p_proc_ready = p;
            break;
        }
    }
}

/*======================================================================*
                           sys_get_ticks
 *======================================================================*/
PUBLIC int sys_get_ticks()
{
    return ticks;
}

/*======================================================================*
                           sys_sleep
 *======================================================================*/
PUBLIC void sys_sleep(int milli_sec)
{
    int time_sleep = 0;
    if (milli_sec % (1000 / HZ) == 0)
    {
        time_sleep = milli_sec;
    }
    else
    {
        time_sleep = milli_sec + 10;
    }
    p_proc_ready->sleep_until = get_ticks() + (time_sleep / (1000 / HZ));
    schedule();
}

/*======================================================================*
                           sys_print_str
 *======================================================================*/
PUBLIC void sys_print_str(char *str)
{
    if (*str == 'O' && *(str + 1) == 0)
    {
        disp_color_str(str, BRIGHT | MAKE_COLOR(BLACK, GREEN));
    }
    else if (*str == 'X' && *(str + 1) == 0)
    {
        disp_color_str(str, BRIGHT | MAKE_COLOR(BLACK, RED));
    }
    else if (*str == 'Z' && *(str + 1) == 0)
    {
        disp_color_str(str, BRIGHT | MAKE_COLOR(BLACK, BLUE));
    }
    else
    {
        disp_str(str);
    }
}

/*======================================================================*
                           block and release
 *======================================================================*/
PUBLIC void block(PROCESS *p_proc)
{
    p_proc->blocked = 1;
}

PUBLIC void release(PROCESS *p_proc)
{
    p_proc->blocked = 0;
}

/*======================================================================*
                           PV
 *======================================================================*/
PUBLIC void sys_P(void *p_sem)
{
    disable_irq(CLOCK_IRQ);
    SEMAPHORE *sem = (SEMAPHORE *)p_sem;
    --(sem->value);
    if (sem->value < 0)
    {
        block(p_proc_ready);
        sem->wait_list[-(sem->value) - 1] = p_proc_ready;
        enable_irq(CLOCK_IRQ);
        schedule();
    }
    enable_irq(CLOCK_IRQ);
}

PUBLIC void sys_V(void *p_sem)
{
    disable_irq(CLOCK_IRQ);
    SEMAPHORE *sem = (SEMAPHORE *)p_sem;
    ++(sem->value);
    if (sem->value <= 0)
    {
        release(sem->wait_list[0]);
        // 释放列表第一个进程后，须将数组内容前移
        for (int i = 0; i < -(sem->value); ++i)
        {
            sem->wait_list[i] = sem->wait_list[i + 1];
        }
        sem->wait_list[-(sem->value)] = 0;
    }
    enable_irq(CLOCK_IRQ);
}