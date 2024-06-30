
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
                                    {R1, STACK_SIZE_R1, "R1"},
                                    {R2, STACK_SIZE_R2, "R2"},
                                    {R3, STACK_SIZE_R3, "R3"},
                                    {W1, STACK_SIZE_W1, "W1"},
                                    {W2, STACK_SIZE_W2, "W2"}};

PUBLIC irq_handler irq_table[NR_IRQ];

PUBLIC system_call sys_call_table[NR_SYS_CALL] = {sys_get_ticks,
                                                  sys_sleep,
                                                  sys_print_str,
                                                  sys_P,
                                                  sys_V};
