
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            main.c
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

#define LEN_WAIT_LIST 10
#define TIME_SLICE 10

#define Capacity 3 // 仓库的容量

PRIVATE int count_producer1 = 0;                        // for counting the product of product1 into capacity
PRIVATE int count_producer2 = 0;                        // for counting the product of product2 into capacity
PRIVATE int count_consumer1 = 0;                        // for counting the product1 that consumer1 consumed
PRIVATE int count_consumer2 = 0;                        // for counting the product2 that consumer2 consumed
PRIVATE int count_consumer3 = 0;                        // for counting the product3 that consumer3 consumed
SEMAPHORE mutex_produce = {.value = 1, .wait_list = 0}; // for controlling request of producer
SEMAPHORE mutex_get = {.value = 1, .wait_list = 0};
SEMAPHORE mutex_product1 = {.value = 0, .wait_list = 0};          // for counting product of producer1
SEMAPHORE mutex_product2 = {.value = 0, .wait_list = 0};          // for counting product of producer2
SEMAPHORE mutex_rest_space = {.value = Capacity, .wait_list = 0}; // for counting the rest space of capacity

/*======================================================================*
                            kernel_main
 *======================================================================*/
PUBLIC int kernel_main()
{
    disp_str("-----\"kernel_main\" begins-----\n");

    TASK *p_task = task_table;
    PROCESS *p_proc = proc_table;
    char *p_task_stack = task_stack + STACK_SIZE_TOTAL;
    u16 selector_ldt = SELECTOR_LDT_FIRST;
    int i;
    for (i = 0; i < NR_TASKS; i++)
    {
        strcpy(p_proc->p_name, p_task->name); // name of the process
        p_proc->pid = i;                      // pid

        p_proc->ldt_sel = selector_ldt;

        memcpy(&p_proc->ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],
               sizeof(DESCRIPTOR));
        p_proc->ldts[0].attr1 = DA_C | PRIVILEGE_TASK << 5;
        memcpy(&p_proc->ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],
               sizeof(DESCRIPTOR));
        p_proc->ldts[1].attr1 = DA_DRW | PRIVILEGE_TASK << 5;
        p_proc->regs.cs = ((8 * 0) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | RPL_TASK;
        p_proc->regs.ds = ((8 * 1) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | RPL_TASK;
        p_proc->regs.es = ((8 * 1) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | RPL_TASK;
        p_proc->regs.fs = ((8 * 1) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | RPL_TASK;
        p_proc->regs.ss = ((8 * 1) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | RPL_TASK;
        p_proc->regs.gs = (SELECTOR_KERNEL_GS & SA_RPL_MASK) | RPL_TASK;

        p_proc->regs.eip = (u32)p_task->initial_eip;
        p_proc->regs.esp = (u32)p_task_stack;
        p_proc->regs.eflags = 0x1202; /* IF=1, IOPL=1 */

        p_proc->sleep_until = 0;
        p_proc->blocked = 0;

        p_task_stack -= p_task->stacksize;
        p_proc++;
        p_task++;
        selector_ldt += 1 << 3;
    }

    k_reenter = 0;
    ticks = 0;

    p_proc_ready = proc_table;

    /* 初始化 8253 PIT */
    out_byte(TIMER_MODE, RATE_GENERATOR);
    out_byte(TIMER0, (u8)(TIMER_FREQ / HZ));
    out_byte(TIMER0, (u8)((TIMER_FREQ / HZ) >> 8));

    put_irq_handler(CLOCK_IRQ, clock_handler); /* 设定时钟中断处理程序 */
    enable_irq(CLOCK_IRQ);                     /* 让8259A可以接收时钟中断 */

    restart();

    while (1)
    {
    }
}

PRIVATE void get_str_round(char *p_str_round, int p_nr_round)
{
    if (p_nr_round < 10)
    {
        p_str_round[0] = p_nr_round % 10 + '0';
        return;
    }
    else
    {
        p_str_round[0] = p_nr_round / 10 + '0';
        p_str_round[1] = p_nr_round % 10 + '0';
    }
}

/*======================================================================*
                            init
 *======================================================================*/
void init_sem(SEMAPHORE *sem, int value)
{
    sem->value = value;
    for (int i = 0; i < LEN_WAIT_LIST; ++i)
    {
        sem->wait_list[i] = 0;
    }
}

void init()
{
    // initialize semaphores and other global variable
    init_sem(&mutex_produce, 1);
    init_sem(&mutex_product1, 0);
    init_sem(&mutex_product2, 0);
    init_sem(&mutex_rest_space, Capacity);
    disp_pos = 0;
    for (int i = 0; i < 80 * 17; ++i)
    {
        disp_str(" ");
    }
    disp_pos = 0;
}

/*======================================================================*
                            A
 *======================================================================*/
void A()
{
    disable_irq(CLOCK_IRQ); // for higher performance of initialization
    init();
    enable_irq(CLOCK_IRQ);
    int nr_round = 0; // print 20 times
    while (1)
    {
        if (nr_round <= 200)
        {
            if (nr_round == 0)
            {
                sleep(10);
            }
            else
            {
                disable_irq(CLOCK_IRQ);
                char str_round[3] = {0};
                get_str_round(str_round, nr_round);
                if (nr_round < 10)
                {
                    print_str(" ");
                }
                print_str(str_round);
                print_str(":");

                char count_producer1_str[3] = {0};
                get_str_round(count_producer1_str, count_producer1);
                print_str(count_producer1_str);
                print_str(" ");

                char count_producer2_str[3] = {0};
                get_str_round(count_producer2_str, count_producer2);
                print_str(count_producer2_str);
                print_str(" ");

                char count_consumer1_str[3] = {0};
                get_str_round(count_consumer1_str, count_consumer1);
                print_str(count_consumer1_str);
                print_str(" ");

                char count_consumer2_str[3] = {0};
                get_str_round(count_consumer2_str, count_consumer2);
                print_str(count_consumer2_str);
                print_str(" ");

                char count_consumer3_str[3] = {0};
                get_str_round(count_consumer3_str, count_consumer3);
                print_str(count_consumer3_str);
                print_str("\n");
                enable_irq(CLOCK_IRQ);
                sleep(1 * TIME_SLICE);
            }
        }
        ++nr_round;
    }
}

void Producer1()
{
    while (1)
    {
        P(&mutex_rest_space); // 申请一个仓库空间
        P(&mutex_produce);    // 申请访问仓库的权限
        count_producer1++;    // 仓库的产品1数量增加
        sleep(TIME_SLICE);    // 一个时间片的时间生产一个产品1
        V(&mutex_produce);    // 返回访问仓库的权限
        V(&mutex_product1);
    }
}

void Producer2()
{
    while (1)
    {
        P(&mutex_rest_space); // 申请一个仓库空间
        P(&mutex_produce);    // 申请访问仓库的权限
        count_producer2++;    // 仓库的产品2数量增加
        sleep(TIME_SLICE);    // 一个时间片的时间生产一个产品2
        V(&mutex_produce);    // 返回访问仓库的权限
        V(&mutex_product2);
    }
}

void Consumer1()
{
    while (1)
    {
        P(&mutex_product1);
        P(&mutex_get);
        count_consumer1++;
        sleep(TIME_SLICE);
        V(&mutex_get);
        V(&mutex_rest_space);
    }
}

void Consumer2()
{
    while (1)
    {
        P(&mutex_product2);
        P(&mutex_get);
        count_consumer2++;
        sleep(TIME_SLICE);
        V(&mutex_get);
        V(&mutex_rest_space);
    }
}

void Consumer3()
{
    while (1)
    {
        P(&mutex_product2);
        P(&mutex_get);
        count_consumer3++;
        sleep(TIME_SLICE);
        V(&mutex_get);
        V(&mutex_rest_space);
    }
}