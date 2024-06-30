
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

#define READING 0
#define WAITING_READ 1
#define WRITING 2
#define WAITING_WRITE 3
#define REST 4
#define NR_RW 5
#define LEN_WAIT_LIST 10
#define TIME_SLICE 10

#define Max_Reader 2
#define Time_Rest 0

#define Working_R1 2
#define Working_R2 3
#define Working_R3 3
#define Working_W1 3
#define Working_W2 4

PRIVATE int state_rw[NR_RW] = {0};                                   // 读者和写者的状态
PRIVATE int count_reader = 0;                                        // 当前的读者数量
PRIVATE int count_writer = 0;                                        // 当前写者的数量
SEMAPHORE mutex_read = {.value = 1, .wait_list = 0};                 // 限制访问count_reader 变量的权限
SEMAPHORE mutex_w = {.value = 1, .wait_list = 0};                    // 保证写优先
SEMAPHORE mutex_write = {.value = 1, .wait_list = 0};                // 限制访问count_writer 变量的权限
SEMAPHORE mutex_rw = {.value = 1, .wait_list = 0};                   // 用于实现对共享资源的互斥访问
SEMAPHORE mutex_max_reading = {.value = Max_Reader, .wait_list = 0}; // 限制最大读者数量
SEMAPHORE mutex_S = {.value = 1, .wait_list = 0};
PRIVATE int strategy_rw = 2; // 0: reader first; 1: writer first; 2: fair

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
    // initialize state_rw[] to WAITING
    for (int i = 0; i < NR_RW; ++i)
    {
        state_rw[i] = i < 3 ? WAITING_READ : WAITING_WRITE;
    }
    // clear screen
    disp_pos = 0;
    for (int i = 0; i < 80 * 17; ++i)
    {
        disp_str(" ");
    }
    disp_pos = 0;
    // print strategy
    switch (strategy_rw)
    {
    case 0:
        print_str("Reader first\n\n");
        break;
    case 1:
        print_str("Writer first\n\n");
        break;
    case 2:
        print_str("Fair\n\n");
        break;
    default:
        print_str("Unrecognizable strategy\n\n");
        while (1)
            ;
        break;
    }
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
                for (int i = 0; i < NR_RW; ++i)
                {
                    print_str(" ");
                    if (state_rw[i] == READING || state_rw[i] == WRITING)
                    {
                        print_str("O");
                    }
                    else if (state_rw[i] == WAITING_READ || state_rw[i] == WAITING_WRITE)
                    {
                        print_str("X");
                    }
                    else if (state_rw[i] == REST)
                    {
                        print_str("Z");
                    }
                }
                print_str("\n");
                enable_irq(CLOCK_IRQ);
                sleep(1 * TIME_SLICE);
            }
        }
        nr_round++;
    }
}

/*======================================================================*
                            reader first
 *======================================================================*/
void read_reader_first(int id, int time_read)
{
    state_rw[id] = WAITING_READ;

    P(&mutex_read);
    if (++count_reader == 1)
    {
        P(&mutex_rw);
    }
    V(&mutex_read);

    P(&mutex_max_reading);
    state_rw[id] = READING;
    sleep(time_read * TIME_SLICE); // reading
    V(&mutex_max_reading);
    P(&mutex_read);
    if (--count_reader == 0)
    {
        V(&mutex_rw);
    }
    V(&mutex_read);

    state_rw[id] = REST;
    sleep(Time_Rest * TIME_SLICE);
}

void write_reader_first(int id, int time_write)
{
    state_rw[id] = WAITING_WRITE;

    P(&mutex_rw);
    state_rw[id] = WRITING;

    sleep(time_write * TIME_SLICE);
    V(&mutex_rw);

    state_rw[id] = REST;
    sleep(Time_Rest * TIME_SLICE);
}

/*======================================================================*
                            writer first
 *======================================================================*/
void read_writer_first(int id, int time_read)
{
    state_rw[id] = WAITING_READ;
    P(&mutex_max_reading);
    P(&mutex_w);
    P(&mutex_read);
    if (++count_reader == 1)
    {
        P(&mutex_rw);
    }
    V(&mutex_read);
    V(&mutex_w);

    state_rw[id] = READING;
    sleep(time_read * TIME_SLICE);

    P(&mutex_read);
    if (--count_reader == 0)
    {
        V(&mutex_rw);
    }
    V(&mutex_read);
    V(&mutex_max_reading);
    state_rw[id] = REST;
    sleep(Time_Rest * TIME_SLICE);
}

void write_writer_first(int id, int time_write)
{
    state_rw[id] = WAITING_WRITE;
    P(&mutex_write);
    if (++count_writer == 1)
    {
        P(&mutex_w);
    }
    V(&mutex_write);

    // P(&mutex_w);
    P(&mutex_rw);
    state_rw[id] = WRITING;
    sleep(time_write * TIME_SLICE);
    V(&mutex_rw);
    // V(&mutex_w);

    P(&mutex_write);
    if (--count_writer == 0)
    {
        V(&mutex_w);
    }
    V(&mutex_write);

    state_rw[id] = REST;
    sleep(Time_Rest * TIME_SLICE);
}

/*======================================================================*
                            r-w fair
 *======================================================================*/
void read_fair(int id, int time_read)
{
    state_rw[id] = WAITING_READ;
    P(&mutex_S);
    P(&mutex_max_reading);
    P(&mutex_read);
    if (++count_reader == 1)
    {
        P(&mutex_rw);
    }
    V(&mutex_read);
    V(&mutex_S);

    state_rw[id] = READING;
    sleep(time_read * TIME_SLICE); // reading

    P(&mutex_read);
    if (--count_reader == 0)
    {
        V(&mutex_rw);
    }
    V(&mutex_read);
    V(&mutex_max_reading);
    state_rw[id] = REST;
    sleep(Time_Rest * TIME_SLICE);
}

void write_fair(int id, int time_write)
{
    state_rw[id] = WAITING_WRITE;
    P(&mutex_S);
    P(&mutex_rw);
    state_rw[id] = WRITING;
    sleep(time_write * TIME_SLICE);
    V(&mutex_rw);
    V(&mutex_S);
    state_rw[id] = REST;
    sleep(Time_Rest * TIME_SLICE);
}

/*======================================================================*
                            base r-w
 *======================================================================*/
void read(int id, int time_read)
{
    if (strategy_rw == 0)
    {
        read_reader_first(id, time_read);
    }
    else if (strategy_rw == 1)
    {
        read_writer_first(id, time_read);
    }
    else if (strategy_rw == 2)
    {
        read_fair(id, time_read);
    }
    else
    {
        print_str("wrong reading");
    }
}

void write(int id, int time_write)
{
    if (strategy_rw == 0)
    {
        write_reader_first(id, time_write);
    }
    else if (strategy_rw == 1)
    {
        write_writer_first(id, time_write);
    }
    else if (strategy_rw == 2)
    {
        write_fair(id, time_write);
    }
    else
    {
        print_str("wrong writing");
    }
}

void R1()
{
    while (1)
    {
        read(0, Working_R1);
    }
}

void R2()
{
    while (1)
    {
        read(1, Working_R2);
    }
}

void R3()
{
    while (1)
    {
        read(2, Working_R3);
    }
}

void W1()
{
    while (1)
    {
        write(3, Working_W1);
    }
}

void W2()
{
    while (1)
    {
        write(4, Working_W2);
    }
}
