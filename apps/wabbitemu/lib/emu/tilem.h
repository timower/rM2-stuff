/*
 * libtilemcore - Graphing calculator emulation library
 *
 * Copyright (C) 2009-2012 Benjamin Moody
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _TILEM_H
#define _TILEM_H

#include "tilemint.h"
#include <stddef.h>
#include <stdio.h>

#define restrict __restrict

#ifdef __cplusplus
extern "C" {
#endif

/* Basic integer types */
typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef uint64_t qword;

/* Structure types */
typedef struct _TilemHardware TilemHardware;
typedef struct _TilemCalc TilemCalc;

/* Useful macros */
#if __GNUC__ >= 3
#define TILEM_ATTR_PURE __attribute__((__pure__))
#define TILEM_ATTR_UNUSED __attribute__((__unused__))
#define TILEM_ATTR_MALLOC __attribute__((__malloc__))
#define TILEM_ATTR_PRINTF(x, y) __attribute__((__format__(__printf__, x, y)))
#define TILEM_LIKELY(xxx) (__builtin_expect((xxx), 1))
#define TILEM_UNLIKELY(xxx) (__builtin_expect((xxx), 0))
#else
#define TILEM_ATTR_PURE
#define TILEM_ATTR_UNUSED
#define TILEM_ATTR_MALLOC
#define TILEM_ATTR_PRINTF(x, y)
#define TILEM_LIKELY(xxx) (xxx)
#define TILEM_UNLIKELY(xxx) (xxx)
#endif

#define TILEM_DWORD_TO_PTR(xxx) ((void*)(uintptr_t)(xxx))
#define TILEM_PTR_TO_DWORD(xxx) ((dword)(uintptr_t)(xxx))

/* Memory allocation */
void*
tilem_malloc(size_t size) TILEM_ATTR_MALLOC;
void*
tilem_malloc0(size_t size) TILEM_ATTR_MALLOC;
void*
tilem_malloc_atomic(size_t size) TILEM_ATTR_MALLOC;
void*
tilem_try_malloc(size_t size) TILEM_ATTR_MALLOC;
void*
tilem_try_malloc0(size_t size) TILEM_ATTR_MALLOC;
void*
tilem_try_malloc_atomic(size_t size) TILEM_ATTR_MALLOC;
void*
tilem_realloc(void* ptr, size_t size) TILEM_ATTR_MALLOC;
void
tilem_free(void* ptr);
#define tilem_new(ttt, nnn) ((ttt*)tilem_malloc((nnn) * sizeof(ttt)));
#define tilem_new0(ttt, nnn) ((ttt*)tilem_malloc0((nnn) * sizeof(ttt)));
#define tilem_new_atomic(ttt, nnn)                                             \
  ((ttt*)tilem_malloc_atomic((nnn) * sizeof(ttt)));

#define tilem_try_new(ttt, nnn) ((ttt*)tilem_try_malloc((nnn) * sizeof(ttt)));
#define tilem_try_new0(ttt, nnn) ((ttt*)tilem_try_malloc0((nnn) * sizeof(ttt)));
#define tilem_try_new_atomic(ttt, nnn)                                         \
  ((ttt*)tilem_try_malloc_atomic((nnn) * sizeof(ttt)));
#define tilem_renew(ttt, ppp, nnn)                                             \
  ((ttt*)tilem_realloc((ppp), (nnn) * sizeof(ttt)))

/* Message/error logging */

/* Write an informative message.  This can be used to notify the user
   of major occurences, such as changes in the Flash protection.
   These messages can occur regularly in normal operation and are
   provided chiefly to aid in debugging. */
void
tilem_message(TilemCalc* calc, const char* msg, ...) TILEM_ATTR_PRINTF(2, 3);

/* Write a warning message.  These messages occur when the calculator
   software (either the OS or a user program) performs an invalid
   operation; these messages often indicate a bug in the calculator
   software, but are otherwise harmless. */
void
tilem_warning(TilemCalc* calc, const char* msg, ...) TILEM_ATTR_PRINTF(2, 3);

/* Write a warning about an internal error.  These messages should
   never occur and indicate a bug in TilEm. */
void
tilem_internal(TilemCalc* calc, const char* msg, ...) TILEM_ATTR_PRINTF(2, 3);

/* Z80 CPU */

/* This union allows us to manipulate register pairs as either byte or
   word values.  It may need to be modified for really unusual host
   CPUs. */
typedef union _TilemZ80Reg {
#ifdef WORDS_BIGENDIAN
  struct {
    byte h3, h2, h, l;
  } b;

  struct {
    word h, l;
  } w;

  dword d;
#else
  struct {
    byte l, h, h2, h3;
  } b;

  struct {
    word l, h;
  } w;

  dword d;
#endif
} TilemZ80Reg;

typedef struct _TilemZ80Regs {
  TilemZ80Reg af, bc, de, hl;
  TilemZ80Reg ix, iy, pc, sp;
  TilemZ80Reg ir, wz, wz2;
  TilemZ80Reg af2, bc2, de2, hl2;
  int iff1, iff2, im;
  byte r7;
} TilemZ80Regs;

/* Breakpoint types */
enum {
  TILEM_BREAK_MEM_READ = 1, /* Break after reading from memory */
  TILEM_BREAK_MEM_EXEC,     /* Break prior to executing from memory */
  TILEM_BREAK_MEM_WRITE,    /* Break after writing to memory */
  TILEM_BREAK_PORT_READ,    /* Break after reading from port */
  TILEM_BREAK_PORT_WRITE,   /* Break after writing to port */
  TILEM_BREAK_EXECUTE,      /* Break after executing opcode */

  TILEM_BREAK_TYPE_MASK = 0xffff,

  TILEM_BREAK_PHYSICAL = 0x10000, /* Use physical addresses */
  TILEM_BREAK_DISABLED = 0x20000  /* Disabled breakpoint */
};

/* Emulation flags */
enum {
  TILEM_Z80_BREAK_INVALID = 1,      /* Break on invalid
                                       instructions */
  TILEM_Z80_BREAK_UNDOCUMENTED = 2, /* Break on undocumented
                                       instructions */
  TILEM_Z80_SKIP_UNDOCUMENTED = 4,  /* Ignore undocumented
                                       instructions entirely
                                       (act as two NOPs) */
  TILEM_Z80_RESET_UNDOCUMENTED = 8, /* Reset CPU following
                                       undocumented instructions */
  TILEM_Z80_BREAK_EXCEPTIONS = 16,  /* Break on hardware exceptions */
  TILEM_Z80_IGNORE_EXCEPTIONS = 32  /* Ignore hardware exceptions */
};

/* Reasons for stopping emulation */
enum {
  TILEM_STOP_TIMEOUT = 0,           /* stopped due to timeout */
  TILEM_STOP_BREAKPOINT = 1,        /* stopped due to breakpoint */
  TILEM_STOP_INVALID_INST = 2,      /* invalid instruction */
  TILEM_STOP_UNDOCUMENTED_INST = 4, /* undocumented instruction */
  TILEM_STOP_EXCEPTION = 8,         /* hardware exception */
  TILEM_STOP_LINK_STATE = 16,       /* blacklink state change */
  TILEM_STOP_LINK_READ_BYTE = 32,   /* graylink finished reading byte */
  TILEM_STOP_LINK_WRITE_BYTE = 64,  /* graylink finished writing byte */
  TILEM_STOP_LINK_ERROR = 128       /* graylink encountered error */
};

/* Types of interrupt */
enum {
  TILEM_INTERRUPT_ON_KEY = 1,      /* ON key pressed */
  TILEM_INTERRUPT_TIMER1 = 2,      /* Main interrupt timer */
  TILEM_INTERRUPT_TIMER2 = 4,      /* Alt. interrupt timer (83/83+) */
  TILEM_INTERRUPT_USER_TIMER1 = 8, /* Programmable timers (83+SE) */
  TILEM_INTERRUPT_USER_TIMER2 = 16,
  TILEM_INTERRUPT_USER_TIMER3 = 32,
  TILEM_INTERRUPT_LINK_ACTIVE = 512, /* Link port state changed */
  TILEM_INTERRUPT_LINK_READ = 1024,  /* Link assist read a byte */
  TILEM_INTERRUPT_LINK_IDLE = 2048,  /* Link assist is idle */
  TILEM_INTERRUPT_LINK_ERROR = 4096  /* Link assist failed */
};

/* Types of hardware exception */
enum {
  TILEM_EXC_RAM_EXEC = 1,    /* Executing at invalid RAM address */
  TILEM_EXC_FLASH_EXEC = 2,  /* Executing at invalid Flash address */
  TILEM_EXC_FLASH_WRITE = 4, /* Writing to invalid Flash address */
  TILEM_EXC_INSTRUCTION = 8  /* Invalid instruction */
};

/* Constant hardware timer IDs */
enum {
  TILEM_TIMER_NONE = 0,
  TILEM_TIMER_LCD_DELAY,
  TILEM_TIMER_FLASH_DELAY,
  TILEM_TIMER_LINK_ASSIST,
  TILEM_TIMER_USER1,
  TILEM_TIMER_USER2,
  TILEM_TIMER_USER3,
  TILEM_TIMER_HW
};

#define TILEM_NUM_SYS_TIMERS (TILEM_TIMER_HW - 1)

/* Type of a timer callback function.  Second arg is the callback data
   passed to tilem_z80_add_timer(). */
typedef void (*TilemZ80TimerFunc)(TilemCalc*, void*);

/* Type of a breakpoint test function.  Second arg is the memory
   address (or opcode in the case of TILEM_BREAK_EXECUTE breakpoints.)
   Third arg is the callback data passed to
   tilem_z80_add_breakpoint(). */
typedef int (*TilemZ80BreakpointFunc)(TilemCalc*, dword, void*);

typedef struct _TilemZ80Timer TilemZ80Timer;
typedef struct _TilemZ80Breakpoint TilemZ80Breakpoint;

typedef struct _TilemZ80 {
  TilemZ80Regs r;
  unsigned int interrupts; /* Currently active interrupts */
  int clockspeed;          /* Current CPU speed (kHz) */
  int halted;
  unsigned int exception;
  dword clock;
  dword lastwrite;
  dword lastlcdwrite;

  unsigned int emuflags;

  int ntimers;
  TilemZ80Timer* timers;
  int timer_cpu;  /* Sorted list of timers (CPU-based) */
  int timer_rt;   /* Sorted list of timers (realtime) */
  int timer_free; /* List of free timer structs */

  int nbreakpoints;
  TilemZ80Breakpoint* breakpoints;
  int breakpoint_mr;       /* Memory read breakpoints */
  int breakpoint_mx;       /* Memory exec breakpoints */
  int breakpoint_mw;       /* Memory write breakpoints */
  int breakpoint_pr;       /* Port read breakpoints */
  int breakpoint_pw;       /* Port write breakpoints */
  int breakpoint_op;       /* Opcode breakpoints */
  int breakpoint_mpr;      /* Physical mem read breakpoints */
  int breakpoint_mpx;      /* Physical mem exec breakpoints */
  int breakpoint_mpw;      /* Physical mem write breakpoints */
  int breakpoint_disabled; /* Disabled breakpoints */
  int breakpoint_free;     /* List of free bp structs */

  int stopping;
  dword stop_reason;
  dword stop_mask;
  int stop_breakpoint;
} TilemZ80;

/* Reset CPU */
void
tilem_z80_reset(TilemCalc* calc);

/* Halt simulation */
void
tilem_z80_stop(TilemCalc* calc, dword reason);

/* Set CPU speed (kHz) */
void
tilem_z80_set_speed(TilemCalc* calc, int speed);

/* Raise a hardware exception */
void
tilem_z80_exception(TilemCalc* calc, unsigned type);

/* Add a timer with the given callback function and data.  The
   callback function will be called after 'count' time units, and
   every 'period' time units thereafter.  If rt = 0, the time units
   are CPU clock cycles; if rt = 1, time units are microseconds.

   Note that if a timer is set in response to a memory read or write,
   the length of the delay may be off by as much as 19 clock cycles;
   the precise timings for these are not (yet) properly emulated.
   Timers set in response to port I/O events will be accurate to the
   nearest CPU clock cycle.

   The timer is considered to have "fired" as soon as the specified
   amount of time has elapsed.  The callback function, however, may
   not be called until after the instruction finishes.  If multiple
   timers fire during the same instruction, the order in which the
   callback functions will be called is undefined.

   If you create a timer using this function (either repeating or
   non-repeating), you must call tilem_z80_remove_timer() when the
   timer is no longer needed.
*/
int
tilem_z80_add_timer(TilemCalc* calc,
                    dword count,
                    dword period,
                    int rt,
                    TilemZ80TimerFunc func,
                    void* data);

/* Change settings for an existing timer.  Arguments are the same as
   above.  If count = 0, the timer is disabled. */
void
tilem_z80_set_timer(TilemCalc* calc, int id, dword count, dword period, int rt);

/* Change period for an existing timer without affecting the current
   interval. */
void
tilem_z80_set_timer_period(TilemCalc* calc, int id, dword period);

/* Delete a timer. */
void
tilem_z80_remove_timer(TilemCalc* calc, int id);

/* Check whether a timer is currently running. */
int
tilem_z80_timer_running(TilemCalc* calc, int id) TILEM_ATTR_PURE;

/* Get the number of clock ticks from now until the next time the
   given timer fires.  (This may be negative, if the timer has already
   fired during this instruction.)  NOTE: If the timer is disabled,
   the return value is undefined. */
int
tilem_z80_get_timer_clocks(TilemCalc* calc, int id) TILEM_ATTR_PURE;

/* Get the number of microseconds from now until the next time the
   given timer fires. */
int
tilem_z80_get_timer_microseconds(TilemCalc* calc, int id) TILEM_ATTR_PURE;

/* Add a breakpoint.  The breakpoint will be triggered if the address,
   ANDed with the given mask, falls between the given start and end
   inclusive.  If a callback function is specified it acts as an
   additional filter, to determine whether the simulation should be
   halted. */
int
tilem_z80_add_breakpoint(TilemCalc* calc,
                         int type,
                         dword start,
                         dword end,
                         dword mask,
                         TilemZ80BreakpointFunc func,
                         void* data);

/* Remove the given breakpoint. */
void
tilem_z80_remove_breakpoint(TilemCalc* calc, int id);

/* Enable the given breakpoint. */
void
tilem_z80_enable_breakpoint(TilemCalc* calc, int id);

/* Disable the given breakpoint. */
void
tilem_z80_disable_breakpoint(TilemCalc* calc, int id);

/* Check whether the given breakpoint is currently enabled. */
int
tilem_z80_breakpoint_enabled(TilemCalc* calc, int id);

/* Get the type of the given breakpoint. */
int
tilem_z80_get_breakpoint_type(TilemCalc* calc, int id);

/* Get the start address of the given breakpoint. */
dword
tilem_z80_get_breakpoint_address_start(TilemCalc* calc, int id);

/* Get the start address of the given breakpoint. */
dword
tilem_z80_get_breakpoint_address_end(TilemCalc* calc, int id);

/* Get the start address of the given breakpoint. */
dword
tilem_z80_get_breakpoint_address_mask(TilemCalc* calc, int id);

/* Get the callback/filter function associated to the given breakpoint. */
TilemZ80BreakpointFunc
tilem_z80_get_breakpoint_callback(TilemCalc* calc, int id);

/* Get the data associated to the given breakpoint. */
void*
tilem_z80_get_breakpoint_data(TilemCalc* calc, int id);

/* Set the type of the given breakpoint. */
void
tilem_z80_set_breakpoint_type(TilemCalc* calc, int id, int type);

/* Set the start address of the given breakpoint. */
void
tilem_z80_set_breakpoint_address_start(TilemCalc* calc, int id, dword start);

/* Set the start address of the given breakpoint. */
void
tilem_z80_set_breakpoint_address_end(TilemCalc* calc, int id, dword end);

/* Set the start address of the given breakpoint. */
void
tilem_z80_set_breakpoint_address_mask(TilemCalc* calc, int id, dword mask);

/* Set the callback/filter function associated to the given breakpoint. */
void
tilem_z80_set_breakpoint_callback(TilemCalc* calc,
                                  int id,
                                  TilemZ80BreakpointFunc func);

/* Set the data associated to the given breakpoint. */
void
tilem_z80_set_breakpoint_data(TilemCalc* calc, int id, void* data);

/* Run the simulated CPU for the given number of clock
   ticks/microseconds, or until a breakpoint is hit or
   tilem_z80_stop() is called. */
dword
tilem_z80_run(TilemCalc* calc, int clocks, int* remaining);
dword
tilem_z80_run_time(TilemCalc* calc, int microseconds, int* remaining);

/* LCD driver */

/* Emulation flags */
enum {
  TILEM_LCD_REQUIRE_DELAY = 1,     /* Emulate required delay
                                      between commands */
  TILEM_LCD_REQUIRE_LONG_DELAY = 2 /* Require extra-long delay */
};

typedef struct _TilemLCD {
  /* Common settings */
  byte active;   /* LCD driver active */
  byte contrast; /* Contrast value (0-63) */
  int rowstride; /* Number of bytes per row */
  unsigned int emuflags;

  /* T6A43 internal driver */
  word addr; /* Memory address */

  /* T6A04 external driver */
  byte mode;     /* I/O mode (0 = 6bit, 1 = 8bit) */
  byte inc;      /* Increment mode (4-7) */
  byte nextbyte; /* Output register */
  int x, y;      /* Current position */
  int rowshift;  /* Starting row for display */
  byte busy;
} TilemLCD;

/* Reset LCD driver */
void
tilem_lcd_reset(TilemCalc* calc);

/* Get LCD driver status (port 10 input) */
byte
tilem_lcd_t6a04_status(TilemCalc* calc);

/* Send command to LCD driver (port 10 output) */
void
tilem_lcd_t6a04_control(TilemCalc* calc, byte val);

/* Read data from LCD driver (port 11 input) */
byte
tilem_lcd_t6a04_read(TilemCalc* calc);

/* Write data to LCD driver (port 11 output) */
void
tilem_lcd_t6a04_write(TilemCalc* calc, byte val);

/* Get screen image (T6A04 style) */
void
tilem_lcd_t6a04_get_data(TilemCalc* calc, byte* data);

/* Get screen image (T6A43 style) */
void
tilem_lcd_t6a43_get_data(TilemCalc* calc, byte* data);

/* Callback for TILEM_TIMER_LCD_DELAY */
void
tilem_lcd_delay_timer(TilemCalc* calc, void* data);

/* DBUS link port driver */

/* Link port / assist mode flags */
enum {
  TILEM_LINK_MODE_ASSIST = 1,        /* Enable link assist */
  TILEM_LINK_MODE_NO_TIMEOUT = 2,    /* Assist doesn't time out (xp) */
  TILEM_LINK_MODE_INT_ON_ACTIVE = 4, /* Interrupt on state change */
  TILEM_LINK_MODE_INT_ON_READ = 8,   /* Interrupt on asst. read */
  TILEM_LINK_MODE_INT_ON_IDLE = 16,  /* Interrupt when asst. idle */
  TILEM_LINK_MODE_INT_ON_ERROR = 32  /* Interrupt on asst. error */
};

/* Link port state flags */
enum {
  TILEM_LINK_ASSIST_READ_BYTE = 1,   /* Assisted read finished */
  TILEM_LINK_ASSIST_READ_BUSY = 2,   /* Assisted read in progress */
  TILEM_LINK_ASSIST_READ_ERROR = 4,  /* Assisted read failed */
  TILEM_LINK_ASSIST_WRITE_BUSY = 8,  /* Assisted write in progress */
  TILEM_LINK_ASSIST_WRITE_ERROR = 16 /* Assisted write failed */
};

/* Link emulation mode */
enum {
  TILEM_LINK_EMULATOR_NONE = 0,  /* Link port disconnected */
  TILEM_LINK_EMULATOR_BLACK = 1, /* Connected to virtual BlackLink
                                    (exit emulation on state change) */
  TILEM_LINK_EMULATOR_GRAY = 2   /* Connected to virtual GrayLink
                                    (auto send/receive bytes) */
};

typedef struct _TilemLinkport {
  byte lines, extlines; /* Link line state for TI/PC
                           0 = both lines high
                           1 = red wire low
                           2 = white wire low
                           3 = both wires low */

  unsigned int mode; /* Mode flags */

  /* Internal link assist */
  unsigned int assistflags; /* Assist state */
  byte assistin;            /* Input buffer (recv from PC) */
  byte assistinbits;        /* Input bit count */
  byte assistout;           /* Output buffer (send to PC) */
  byte assistoutbits;       /* Output bit count */
  byte assistlastbyte;      /* Last byte received */

  /* External link emulator */
  byte linkemu;
  byte graylinkin;      /* Input buffer (recv from TI) */
  byte graylinkinbits;  /* Input bit count */
  byte graylinkout;     /* Output buffer (send to TI) */
  byte graylinkoutbits; /* Output bit count */
} TilemLinkport;

/* Reset link port */
void
tilem_linkport_reset(TilemCalc* calc);

/* Read link port lines */
byte
tilem_linkport_get_lines(TilemCalc* calc);

/* Set link port lines */
void
tilem_linkport_set_lines(TilemCalc* calc, byte lines);

/* Read from, and clear, link assist input buffer */
byte
tilem_linkport_read_byte(TilemCalc* calc);

/* Write to link assist output buffer */
void
tilem_linkport_write_byte(TilemCalc* calc, byte data);

/* Get assist state */
unsigned int
tilem_linkport_get_assist_flags(TilemCalc* calc);

/* Set link port mode */
void
tilem_linkport_set_mode(TilemCalc* calc, unsigned int mode);

/* Set line states for virtual BlackLink */
void
tilem_linkport_blacklink_set_lines(TilemCalc* calc, byte lines);

/* Get line states from virtual BlackLink */
byte
tilem_linkport_blacklink_get_lines(TilemCalc* calc);

/* Reset GrayLink */
void
tilem_linkport_graylink_reset(TilemCalc* calc);

/* Check if GrayLink is ready to send data */
int
tilem_linkport_graylink_ready(TilemCalc* calc);

/* Send a byte via virtual GrayLink */
int
tilem_linkport_graylink_send_byte(TilemCalc* calc, byte value);

/* Get byte received by virtual GrayLink (-1 = none available) */
int
tilem_linkport_graylink_get_byte(TilemCalc* calc);

/* Callback for TILEM_TIMER_LINK_ASSIST */
void
tilem_linkport_assist_timer(TilemCalc* calc, void* data);

/* Keypad */

typedef struct _TilemKeypad {
  byte group;
  byte onkeydown;
  byte onkeyint;
  byte keysdown[8];
} TilemKeypad;

/* Reset keypad */
void
tilem_keypad_reset(TilemCalc* calc);

/* Set current group (port 1 output) */
void
tilem_keypad_set_group(TilemCalc* calc, byte group);

/* Read keys from current group (port 1 input) */
byte
tilem_keypad_read_keys(TilemCalc* calc);

/* Press a key */
void
tilem_keypad_press_key(TilemCalc* calc, int scancode);

/* Release a key */
void
tilem_keypad_release_key(TilemCalc* calc, int scancode);

/* Flash */

/* Emulation flags */
enum {
  TILEM_FLASH_REQUIRE_DELAY = 1 /* Require delay after
                                   program/erase */
};

typedef struct _TilemFlashSector {
  dword start;
  dword size;
  byte protectgroup;
} TilemFlashSector;

typedef struct _TilemFlash {
  byte unlock;
  byte state;
  unsigned int emuflags;
  byte busy;
  dword progaddr;
  byte progbyte;
  byte toggles;
  byte overridegroup;
} TilemFlash;

/* Reset Flash */
void
tilem_flash_reset(TilemCalc* calc);

/* Read a byte from the Flash chip */
byte
tilem_flash_read_byte(TilemCalc* calc, dword pa);

/* Erase a Flash sector */
void
tilem_flash_erase_address(TilemCalc* calc, dword pa);

/* Write a byte to the Flash chip */
void
tilem_flash_write_byte(TilemCalc* calc, dword pa, byte v);

/* Callback for TILEM_TIMER_FLASH_DELAY */
void
tilem_flash_delay_timer(TilemCalc* calc, void* data);

/* MD5 assist */

enum {
  TILEM_MD5_REG_A = 0, /* initial 'a' value */
  TILEM_MD5_REG_B = 1, /* 'b' value */
  TILEM_MD5_REG_C = 2, /* 'c' value */
  TILEM_MD5_REG_D = 3, /* 'd' value */
  TILEM_MD5_REG_X = 4, /* 'X' (or 'T') value */
  TILEM_MD5_REG_T = 5  /* 'T' (or 'X') value */
};

enum {
  TILEM_MD5_FUNC_FF = 0,
  TILEM_MD5_FUNC_GG = 1,
  TILEM_MD5_FUNC_HH = 2,
  TILEM_MD5_FUNC_II = 3
};

typedef struct _TilemMD5Assist {
  dword regs[6];
  byte shift;
  byte mode;
} TilemMD5Assist;

/* Reset MD5 assist */
void
tilem_md5_assist_reset(TilemCalc* calc);

/* Get output value */
dword
tilem_md5_assist_get_value(TilemCalc* calc);

/* Programmable timers */

#define TILEM_MAX_USER_TIMERS 3

enum {
  TILEM_USER_TIMER_LOOP = 1,         /* loop when counter
                                        reaches 0 */
  TILEM_USER_TIMER_INTERRUPT = 2,    /* generate interrupt when
                                        finished */
  TILEM_USER_TIMER_OVERFLOW = 4,     /* timer has expired at
                                        least twice since last
                                        mode setting */
  TILEM_USER_TIMER_FINISHED = 256,   /* timer has expired at
                                        least once since last
                                        mode setting (port 4
                                        status bit) */
  TILEM_USER_TIMER_NO_HALT_INT = 512 /* suppress interrupt if
                                        CPU is halted */
};

typedef struct _TilemUserTimer {
  byte frequency;
  byte loopvalue;
  unsigned int status;
} TilemUserTimer;

/* Reset timers */
void
tilem_user_timers_reset(TilemCalc* calc);

/* Set frequency control register */
void
tilem_user_timer_set_frequency(TilemCalc* calc, int n, byte value);

/* Set status flags */
void
tilem_user_timer_set_mode(TilemCalc* calc, int n, byte mode);

/* Start timer */
void
tilem_user_timer_start(TilemCalc* calc, int n, byte value);

/* Get timer value */
byte
tilem_user_timer_get_value(TilemCalc* calc, int n);

/* Callback function */
void
tilem_user_timer_expired(TilemCalc* calc, void* data);

/* Calculators */

/* Model IDs */
enum {
  TILEM_CALC_TI73 = '7',         /* TI-73 / TI-73 Explorer */
  TILEM_CALC_TI76 = 'f',         /* TI-76.fr */
  TILEM_CALC_TI81 = '1',         /* TI-81 */
  TILEM_CALC_TI82 = '2',         /* TI-82 */
  TILEM_CALC_TI83 = '3',         /* TI-83 / TI-82 STATS [.fr] */
  TILEM_CALC_TI83P = 'p',        /* TI-83 Plus */
  TILEM_CALC_TI83P_SE = 's',     /* TI-83 Plus Silver Edition */
  TILEM_CALC_TI84P = '4',        /* TI-84 Plus */
  TILEM_CALC_TI84P_SE = 'z',     /* TI-84 Plus Silver Edition */
  TILEM_CALC_TI84P_NSPIRE = 'n', /* TI-Nspire 84 Plus emulator */
  TILEM_CALC_TI85 = '5',         /* TI-85 */
  TILEM_CALC_TI86 = '6'          /* TI-86 */
};

/* Calculator flags */
enum {
  TILEM_CALC_HAS_LINK = 1,        /* Has link port */
  TILEM_CALC_HAS_LINK_ASSIST = 2, /* Has hardware link assist */
  TILEM_CALC_HAS_USB = 4,         /* Has USB controller */
  TILEM_CALC_HAS_FLASH = 8,       /* Has (writable) Flash */
  TILEM_CALC_HAS_T6A04 = 16,      /* Has separate LCD driver */
  TILEM_CALC_HAS_MD5_ASSIST = 32  /* Has hardware MD5 assist */
};

/* Calculator hardware description */
struct _TilemHardware {
  char model_id;    /* Single character identifying model */
  const char* name; /* Short name (e.g. ti83p) */
  const char* desc; /* Full name (e.g. TI-83 Plus) */

  unsigned int flags;

  int lcdwidth, lcdheight; /* Size of LCD */
  dword romsize, ramsize;  /* Size of ROM and RAM */
  dword lcdmemsize;        /* Size of external LCD memory */
  byte rampagemask;        /* Bit mask used for RAM page */

  int nflashsectors;
  const TilemFlashSector* flashsectors;

  int nusertimers;

  int nhwregs;             /* Number of hardware registers */
  const char** hwregnames; /* Harware register names */

  int nhwtimers;             /* Number of hardware timers */
  const char** hwtimernames; /* Hardware timer names*/

  const char** keynames;

  /* Reset calculator */
  void (*reset)(TilemCalc*);

  /* Reinitialize after loading state */
  void (*stateloaded)(TilemCalc*, int);

  /* Z80 ports and memory */
  byte (*z80_in)(TilemCalc*, dword);
  void (*z80_out)(TilemCalc*, dword, byte);
  void (*z80_wrmem)(TilemCalc*, dword, byte);
  byte (*z80_rdmem)(TilemCalc*, dword);
  byte (*z80_rdmem_m1)(TilemCalc*, dword);

  /* Evaluate a non-standard instruction */
  void (*z80_instr)(TilemCalc*, dword);

  /* Persistent timer callback */
  void (*z80_ptimer)(TilemCalc*, int);

  /* Retrieve LCD contents */
  void (*get_lcd)(TilemCalc*, byte*);

  /* Convert physical <-> logical addresses */
  dword (*mem_ltop)(TilemCalc*, dword);
  dword (*mem_ptol)(TilemCalc*, dword);
};

/* Current state of the calculator */
struct _TilemCalc {
  TilemHardware hw;

  TilemZ80 z80;
  byte* mem;
  byte* ram;
  byte* lcdmem;
  byte mempagemap[4];

  TilemLCD lcd;
  TilemLinkport linkport;
  TilemKeypad keypad;
  TilemFlash flash;
  TilemMD5Assist md5assist;
  TilemUserTimer usertimers[TILEM_MAX_USER_TIMERS];

  byte poweronhalt; /* System power control.  If this is
                       zero, turn off LCD, timers,
                       etc. when CPU halts */

  byte battery; /* Battery level (units of 0.1 V) */

  dword* hwregs;
};

/* Get a list of supported hardware models */
void
tilem_get_supported_hardware(const TilemHardware*** models, int* nmodels);

/* Create a new calculator.  This function returns NULL if
   insufficient memory is available. */
TilemCalc*
tilem_calc_new(char id);

/* Make an exact copy of an existing calculator (including both
   internal and external state.)  Be careful when using this in
   conjunction with custom timer/breakpoint callback functions.  This
   function returns NULL if insufficient memory is available. */
TilemCalc*
tilem_calc_copy(TilemCalc* calc);

/* Free a calculator that was previously created by tilem_calc_new()
   or tilem_calc_copy(). */
void
tilem_calc_free(TilemCalc* calc);

/* Reset calculator (essentially, remove and replace batteries.) */
void
tilem_calc_reset(TilemCalc* calc);

/* Load calculator state from ROM and/or save files. */
int
tilem_calc_load_state(TilemCalc* calc, FILE* romfile, FILE* savfile);

/* Save calculator state to ROM and/or save files. */
int
tilem_calc_save_state(TilemCalc* calc, FILE* romfile, FILE* savfile);

/* LCD image conversion/scaling */

/* Scaling algorithms */
enum {
  TILEM_SCALE_FAST = 0, /* Fast scaling (nearest neighbor) -
                           looks lousy unless the scaling
                           factor is fairly large */
  TILEM_SCALE_SMOOTH    /* Smooth scaling - slower and looks
                           better at small sizes; note that
                           this falls back to using the "fast"
                           algorithm if we are scaling up by
                           an integer factor */
};

/* Buffer representing a snapshot of the LCD state */
typedef struct _TilemLCDBuffer {
  byte width;       /* Width of LCD */
  byte height;      /* Height of LCD */
  byte rowstride;   /* Offset between rows in buffer */
  byte contrast;    /* Contrast value (0-63) */
  dword stamp;      /* Timestamp */
  dword tmpbufsize; /* Size of temporary buffer */
  byte* data;       /* Image data (rowstride*height bytes) */
  void* tmpbuf;     /* Temporary buffer used for scaling */
} TilemLCDBuffer;

/* Create new TilemLCDBuffer. */
TilemLCDBuffer*
tilem_lcd_buffer_new(void) TILEM_ATTR_MALLOC;

/* Free  a TilemLCDBuffer. */
void
tilem_lcd_buffer_free(TilemLCDBuffer* buf);

/* Convert current LCD memory contents to a TilemLCDBuffer (i.e., a
   monochrome snapshot.) */
void
tilem_lcd_get_frame(TilemCalc* restrict calc, TilemLCDBuffer* restrict buf);

/* Convert current LCD memory contents to a TilemLCDBuffer (i.e., a
   monochrome snapshot.)
   Output is only 0 and 1 */
void
tilem_lcd_get_frame1(TilemCalc* restrict calc, TilemLCDBuffer* restrict buf);

/* Convert and scale image to an 8-bit indexed image buffer.  IMGWIDTH
   and IMGHEIGHT are the width and height of the output image,
   ROWSTRIDE the number of bytes from the start of one row to the next
   (often equal to IMGWIDTH), and SCALETYPE the scaling algorithm to
   use. */
void
tilem_draw_lcd_image_indexed(TilemLCDBuffer* restrict frm,
                             byte* restrict buffer,
                             int imgwidth,
                             int imgheight,
                             int rowstride,
                             int scaletype);

/* Convert and scale image to a 24-bit RGB or 32-bit RGBA image
   buffer.  IMGWIDTH and IMGHEIGHT are the width and height of the
   output image, ROWSTRIDE the number of bytes from the start of one
   row to the next (often equal to 3 * IMGWIDTH), PIXBYTES the number
   of bytes per pixel (3 or 4), and SCALETYPE the scaling algorithm to
   use.  PALETTE is an array of 256 color values. */
void
tilem_draw_lcd_image_rgb(TilemLCDBuffer* restrict frm,
                         byte* restrict buffer,
                         int imgwidth,
                         int imgheight,
                         int rowstride,
                         int pixbytes,
                         const dword* restrict palette,
                         int scaletype);

/* Calculate a color palette for use with the above functions.
   RLIGHT, GLIGHT, BLIGHT are the RGB components (0 to 255) of the
   lightest possible color; RDARK, GDARK, BDARK are the RGB components
   of the darkest possible color.  GAMMA is the gamma value for the
   output device (2.2 for most current computer displays and image
   file formats.) */
dword*
tilem_color_palette_new(int rlight,
                        int glight,
                        int blight,
                        int rdark,
                        int gdark,
                        int bdark,
                        double gamma);

/* Calculate a color palette, as above, and convert it to a packed
   array of bytes (R, G, B) */
byte*
tilem_color_palette_new_packed(int rlight,
                               int glight,
                               int blight,
                               int rdark,
                               int gdark,
                               int bdark,
                               double gamma);

/* Grayscale LCD simulation */

typedef struct _TilemGrayLCD TilemGrayLCD;

/* Create a new LCD and attach to a calculator.  Sampling for
   grayscale is done across WINDOWSIZE frames, with samples taken
   every SAMPLEINT microseconds. */
TilemGrayLCD*
tilem_gray_lcd_new(TilemCalc* calc, int windowsize, int sampleint);

/* Detach and free an LCD. */
void
tilem_gray_lcd_free(TilemGrayLCD* glcd);

/* Generate a grayscale image for the next frame, based on current
   calculator state.  This function also updates the frame counter and
   internal state; for proper grayscale behavior, this function needs
   to be called at regular intervals. */
void
tilem_gray_lcd_get_frame(TilemGrayLCD* restrict glcd,
                         TilemLCDBuffer* restrict frm);

/* Miscellaneous functions */

/* Guess calculator type for a ROM file */
char
tilem_guess_rom_type(FILE* romfile);

/* Get calculator type for a SAV file */
char
tilem_get_sav_type(FILE* savfile);

/* Check validity of calculator certificate; repair if necessary */
void
tilem_calc_fix_certificate(TilemCalc* calc,
                           byte* cert,
                           int app_start,
                           int app_end,
                           unsigned exptab_offset);

#ifdef __cplusplus
}
#endif

#endif
