
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            proto.h
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/* kliba.asm */
PUBLIC void out_byte(u16 port, u8 value);
PUBLIC u8 in_byte(u16 port);
PUBLIC void disp_str(char *info);
PUBLIC void disp_color_str(char *info, int color);
PUBLIC void disable_irq(int irq);
PUBLIC void enable_irq(int irq);

/* protect.c */
PUBLIC void init_prot();
PUBLIC u32 seg2phys(u16 seg);

/* klib.c */
PUBLIC void delay(int time);
PUBLIC void disp_int(int input);

/* kernel.asm */
void restart();

/* main.c */
void A();
void Producer1();
void Producer2();
void Consumer1();
void Consumer2();
void Consumer3();

/* i8259.c */
PUBLIC void put_irq_handler(int irq, irq_handler handler);
PUBLIC void spurious_irq(int irq);
PUBLIC void init_8259A();

/* clock.c */
PUBLIC void clock_handler(int irq);
PUBLIC void milli_delay(int milli_sec);

/* 以下是系统调用相关 */

/* proc.c */
PUBLIC void schedule();
PUBLIC int sys_get_ticks(); /* sys_call */
PUBLIC void sys_sleep(int milli_sec);
PUBLIC void sys_print_str(char *str);
PUBLIC void sys_P(void *p_sem);
PUBLIC void sys_V(void *p_sem);

/* syscall.asm */
PUBLIC void sys_call(); /* int_handler */
PUBLIC int get_ticks();
PUBLIC void sleep(int milli_sec);
PUBLIC void print_str(char *str); /* syscall print string */
PUBLIC void P(void *p_sem);
PUBLIC void V(void *p_sem);
