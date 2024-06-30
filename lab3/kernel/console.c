
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			      console.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
						    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
	回车键: 把光标移到第一列
	换行键: 把光标前进到下一行
*/


#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

PRIVATE void set_cursor(unsigned int position);

PRIVATE void set_video_start_addr(u32 addr);

PRIVATE void flush(CONSOLE *p_con);

PRIVATE int pos_search_start = -1;

PRIVATE int pos_line_end[SCREEN_SIZE / SCREEN_WIDTH] = {0};

PRIVATE int num_line = 0;

/*======================================================================*
			   init_screen
 *======================================================================*/
PUBLIC void init_screen(TTY *p_tty) {
    int nr_tty = p_tty - tty_table;
    p_tty->p_console = console_table + nr_tty;

    int v_mem_size = V_MEM_SIZE >> 1;    /* 显存总大小 (in WORD) */

    int con_v_mem_size = v_mem_size / NR_CONSOLES;
    p_tty->p_console->original_addr = nr_tty * con_v_mem_size;
    p_tty->p_console->v_mem_limit = con_v_mem_size;
    p_tty->p_console->current_start_addr = p_tty->p_console->original_addr;

    /* 默认光标位置在最开始处 */
    p_tty->p_console->cursor = p_tty->p_console->original_addr;

    if (nr_tty == 0) {
        /* 第一个控制台沿用原来的光标位置 */
        p_tty->p_console->cursor = disp_pos / 2;
        disp_pos = 0;
    } else {
        out_char(p_tty->p_console, nr_tty + '0');
        out_char(p_tty->p_console, '#');
    }

    set_cursor(p_tty->p_console->cursor);
}


/*======================================================================*
			   is_current_console
*======================================================================*/
PUBLIC int is_current_console(CONSOLE *p_con) {
    return (p_con == &console_table[nr_current_console]);
}


/*======================================================================*
			   out_char
 *======================================================================*/
PUBLIC void out_char(CONSOLE *p_con, char ch) {
    u8 *p_vmem = (u8 * )(V_MEM_BASE + p_con->cursor * 2);

    switch (ch) {
        case '\n':
            if (p_con->cursor < p_con->original_addr +
                                p_con->v_mem_limit - SCREEN_WIDTH) {
                pos_line_end[num_line++] = p_con->cursor;
                p_con->cursor = p_con->original_addr + SCREEN_WIDTH *
                                                       ((p_con->cursor - p_con->original_addr) /
                                                        SCREEN_WIDTH + 1);
            }
            break;
        case '\b':
            // 搜索模式下不可退格至开始搜索位置之前
            if (pos_search_start != -1 && p_con->cursor <= pos_search_start) {
                break;
            }
            // 退格
            char charDel = 0;
            if (p_con->cursor > p_con->original_addr) {
                p_con->cursor--;
                charDel = *(p_vmem - 2);
                *(--p_vmem) = DEFAULT_CHAR_COLOR;
                *(--p_vmem) = ' ';
            }
            // 检查tab
            if (charDel == '\t') {
                for (int i = 0; i < 3; ++i) {
                    if (p_con->cursor > p_con->original_addr) {
                        p_con->cursor--;
                        *(--p_vmem) = DEFAULT_CHAR_COLOR;
                        *(--p_vmem) = ' ';
                    } else {
                        break;
                    }
                }
            }
            // 检查是否退到上一行
            int lineNow = (p_con->cursor - p_con->original_addr) / SCREEN_WIDTH;
            if (lineNow < num_line) {
                p_con->cursor = pos_line_end[lineNow];
                pos_line_end[lineNow] = 0;
                num_line = lineNow;
            }
            break;
        case '\t': 
            if (p_con->cursor <
                p_con->original_addr + p_con->v_mem_limit - 8) {
                for (int i = 0; i < 4; ++i) {
                    *p_vmem++ = '\t';
                    *p_vmem++ = TAB_COLOR;
                }
                p_con->cursor += 4;
                // 检查是否到下一行
                int lineNow = (p_con->cursor - p_con->original_addr) / SCREEN_WIDTH;
                if (lineNow > num_line) {
                    pos_line_end[num_line] = p_con->cursor - 4;
                    num_line = lineNow;
                }
            }
            break;
        default:
            if (p_con->cursor <
                p_con->original_addr + p_con->v_mem_limit - 1) {
                *p_vmem++ = ch;
                *p_vmem++ = pos_search_start == -1 ? DEFAULT_CHAR_COLOR : MATCHED_COLOR;
                p_con->cursor++;
                // 检查是否到下一行
                int lineNow = (p_con->cursor - p_con->original_addr) / SCREEN_WIDTH;
                if (lineNow > num_line) {
                    pos_line_end[num_line] = p_con->cursor - 1;
                    num_line = lineNow;
                }
            }
            break;
    }

    while (p_con->cursor >= p_con->current_start_addr + SCREEN_SIZE) {
        scroll_screen(p_con, SCR_DN);
    }

    flush(p_con);
}

/*======================================================================*
                           flush
*======================================================================*/
PRIVATE void flush(CONSOLE *p_con) {
    set_cursor(p_con->cursor);
    set_video_start_addr(p_con->current_start_addr);
}

/*======================================================================*
			    set_cursor
 *======================================================================*/
PRIVATE void set_cursor(unsigned int position) {
    disable_int();
    out_byte(CRTC_ADDR_REG, CURSOR_H);
    out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
    out_byte(CRTC_ADDR_REG, CURSOR_L);
    out_byte(CRTC_DATA_REG, position & 0xFF);
    enable_int();
}

/*======================================================================*
			  set_video_start_addr
 *======================================================================*/
PRIVATE void set_video_start_addr(u32 addr) {
    disable_int();
    out_byte(CRTC_ADDR_REG, START_ADDR_H);
    out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
    out_byte(CRTC_ADDR_REG, START_ADDR_L);
    out_byte(CRTC_DATA_REG, addr & 0xFF);
    enable_int();
}


/*======================================================================*
			   select_console
 *======================================================================*/
PUBLIC void select_console(int nr_console)    /* 0 ~ (NR_CONSOLES - 1) */
{
    if ((nr_console < 0) || (nr_console >= NR_CONSOLES)) {
        return;
    }

    nr_current_console = nr_console;

    set_cursor(console_table[nr_console].cursor);
    set_video_start_addr(console_table[nr_console].current_start_addr);
}

/*======================================================================*
			   scroll_screen
 *----------------------------------------------------------------------*
 滚屏.
 *----------------------------------------------------------------------*
 direction:
	SCR_UP	: 向上滚屏
	SCR_DN	: 向下滚屏
	其它	: 不做处理
 *======================================================================*/
PUBLIC void scroll_screen(CONSOLE *p_con, int direction) {
    if (direction == SCR_UP) {
        if (p_con->current_start_addr > p_con->original_addr) {
            p_con->current_start_addr -= SCREEN_WIDTH;
        }
    } else if (direction == SCR_DN) {
        if (p_con->current_start_addr + SCREEN_SIZE <
            p_con->original_addr + p_con->v_mem_limit) {
            p_con->current_start_addr += SCREEN_WIDTH;
        }
    } else {
    }

    set_video_start_addr(p_con->current_start_addr);
    set_cursor(p_con->cursor);
}

PUBLIC void clear_screen(TTY *tty) {
    disp_pos = 0;
    for (int i = 0; i < SCREEN_SIZE; ++i) {
        disp_str(" ");
    }
    disp_pos = 0;
    for (int i = 0; i < num_line; ++i) {
        pos_line_end[i] = 0;
    }
    num_line = 0;
    init_screen(tty);
}

PUBLIC void begin_search(CONSOLE *p_con) {
    pos_search_start = p_con->cursor;
}

PUBLIC void end_search(CONSOLE *p_con) {
    // 清除搜索串
    u8 *p_vmem = (u8 *)(V_MEM_BASE + pos_search_start * 2);
    for (int i = pos_search_start; i < p_con->cursor; ++i) {
        *(p_vmem++) = ' ';
        *(p_vmem++) = DEFAULT_CHAR_COLOR;
    }
    // 复位光标
    p_con->cursor = pos_search_start;
    // 颜色恢复
    for (int i = 0; i < pos_search_start; ++i) {
        p_vmem = V_MEM_BASE + i * 2;
        if (*p_vmem == '\t') {
            *(p_vmem + 1) = TAB_COLOR;
        } else {
            *(p_vmem + 1) = DEFAULT_CHAR_COLOR;
        }
    }
    pos_search_start = -1;
    set_cursor(p_con->cursor);
}

PUBLIC void match_search(CONSOLE *p_con) {
    int length = p_con->cursor - pos_search_start;
    u8 *p_vmem = (u8 *)(V_MEM_BASE);
    for (int i = 0; i < pos_search_start - length + 1; ++i) {
        int begin = i;
        int end = i;
        int matched = 1;
        for (int j = pos_search_start; j < p_con->cursor; ++j) {
            p_vmem = (u8 *)(V_MEM_BASE + end * 2);
            u8 origin = *p_vmem;
            p_vmem = (u8 *)(V_MEM_BASE + j * 2);
            u8 search = *p_vmem;
            if (origin != search) {
                matched = 0;
                break;
            }
            end++;
        }
        if (matched) {
            // 匹配成功则红色高亮原文
            for (int j = begin; j < end; ++j) {
                p_vmem = (u8 *)(V_MEM_BASE + j * 2);
                if (*p_vmem != '\t') {
                    *(p_vmem + 1) = MATCHED_COLOR;
                }
            }
        }
    }
}
