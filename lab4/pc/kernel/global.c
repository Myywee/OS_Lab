
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            global.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#define GLOBAL_VARIABLES_HERE

#include "type.h"
#include "const.h"
#include "protect.h"
#include "proto.h"
#include "proc.h"
#include "global.h"

PUBLIC PROCESS proc_table[NR_TASKS];

PUBLIC char task_stack[STACK_SIZE_TOTAL];

PUBLIC TASK task_table[NR_TASKS] = {{A, STACK_SIZE_A, "A"},
                                    {Producer1, STACK_SIZE_P1, "P1"},
                                    {Producer2, STACK_SIZE_P2, "P2"},
                                    {Consumer1, STACK_SIZE_C1, "C1"},
                                    {Consumer2, STACK_SIZE_C2, "C2"},
                                    {Consumer3, STACK_SIZE_C3, "C3"}};

PUBLIC irq_handler irq_table[NR_IRQ];

PUBLIC system_call sys_call_table[NR_SYS_CALL] = {sys_get_ticks,
                                                  sys_sleep,
                                                  sys_print_str,
                                                  sys_P,
                                                  sys_V};
