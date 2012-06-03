/*
 * Copyright (c) 2011, Pontus Oldberg.
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
#pragma codeseg APP_BANK
#define PRINT_A     // Enable A prints

#include "system.h"
#include "onoff.h"
#include "iet_debug.h"
#include "absval_mgr.h"
#include "event_switch.h"
#include "swtimers.h"

/* Function prototypes */
void absval_trigger (struct rule *rule);

/* The action manager handle */
static action_mgr_t  absvalmgr;
static const char *absval_name = "Styr vattenventil";

/* Timer table */
static u8_t timetab[NUMBER_OF_RELAYS];

/*
 * Initialize the absval_mgr
 */
void init_absval_mgr(void) __reentrant __banked
{
  int i;

  absvalmgr.base.type = EVENT_ACTION_MANAGER;
  absvalmgr.base.name = action_base_name_onoff;
  absvalmgr.type = ATYPE_ABSOLUTE_ACTION;
  absvalmgr.action_name = (char*)absval_name;
  absvalmgr.vt.trigger_action = absval_trigger;

  /* Initialize timers */
  for (i=0;i<NUMBER_OF_RELAYS;i++) {
    timetab[i] = alloc_timer();
  }
  /* register this action manager */
  evnt_register_handle(&absvalmgr);
}

/* Callback function for the timeout timer */
void absval_callback (void *cb_data)
{
  act_absolute_data_t *absdata = (act_absolute_data_t *)cb_data;

  /* So we should reset the relay at this point.
   * We do this by appying the inverse value of what is was set to.
   */
  set_onoff (absdata->channel-1, absdata->onoff ? 0 : 1);
  A_(printf(__AT__ "Got timer callback on channel %d\n", absdata->channel);)
}

/* Set new data */
void absval_trigger (struct rule *rule) __reentrant
{
  act_absolute_data_t *absdata = (act_absolute_data_t *)rule->action_data;
  u8_t channel = absdata->channel-1;
  u8_t timer = timetab[channel];

  /* Check if timer is already running */
  if (get_timer_status (timer) == TMR_RUNNING ||
      get_timer_status (timer) == TMR_PAUSED) {
    /* If the timer is already running, simply renew the time */
    set_timer_cnt(timer, absdata->timeon * 100);
  } else {
    /* Set the output and start the associated timer */
    set_onoff (channel, absdata->onoff);
    /* Start timer and set callback */
    set_timer (timetab[channel], absdata->timeon * 100, absval_callback, absdata);
    A_(printf(__AT__ "Setting: Channel %d, Value %d\n", channel, absdata->onoff);)
  }
}

/* EOF */
