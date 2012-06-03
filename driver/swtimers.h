/*
 * Copyright (c) 2008, Pontus Oldberg.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef SWTIMERS_H_INCLUDED
#define SWTIMERS_H_INCLUDED

#include "system.h"

#define NUMBER_OF_SWTIMERS  16

/*
 * Do not change TMR_RUNNING to any other value that 0.
 * Doing so requires you to rewrite the __interrupt timer logic as well.
 * The other values may be changed and arranged in other ways.
 */
enum timer_stat {
  TMR_RUNNING = 0x00,
  TMR_FREE,
  TMR_ALLOCATED,
  TMR_PAUSED,
  TMR_KICK,
  TMR_ENDED,
  TMR_ERROR,
};

struct kicker {
  struct pt pt;
};

typedef void (*timer_cb)(void* cb_data) __reentrant;
typedef unsigned long timer_time_t;
//typedef unsigned int timer_time_t;

extern struct kicker kicker;
extern timer_time_t swtimer[NUMBER_OF_SWTIMERS];
extern u8_t  timer_table[NUMBER_OF_SWTIMERS];

void init_swtimers(void) ;
void set_timer(u8_t timer, timer_time_t time, timer_cb cb, void *cb_data);
void set_timer_cnt(u8_t timer, timer_time_t time);
timer_time_t get_timer(u8_t timer);
u8_t get_timer_status(u8_t timer);
char alloc_timer(void);
u8_t free_timer(u8_t timer);
void pause_timer(u8_t timer);
void start_timer(u8_t timer);
void init_kicker(void);
PT_THREAD(handle_kicker(struct kicker *kick) );

#endif // SWTIMERS_H_INCLUDED
