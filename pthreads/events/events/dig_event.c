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
//#define PRINT_A     // Enable A prints
//#define PRINT_B

#include <stdlib.h>

#include "system.h"
#include "flash.h"
#include "iet_debug.h"
#include "event_switch.h"
#include "dig_event.h"
#include "swtimers.h"

#include "comparator.h"
#include "dac.h"

const u8_t button_mask[] = {0x40, 0x20};

/* Event handle */
static event_prv_t digevent[NUMBER_OF_DIG_INPUTS];
static const char *base_name = "Digital Input";
static const char *dig_names[] = { "Button 1", "Button 2" };

/* Prototype */
static void init_event (struct rule *rule) __reentrant;

/*
 * Initialize the dig_event pthread
 */
void init_dig_event(dig_event_t *dig_event) __reentrant __banked
{
  char i;

  PT_INIT(&dig_event->pt);
  /* Setup local data pointer to configuration struct */
  dig_event->dptr = (dig_data_t *)&sys_cfg.in1_mode;

  /* Initialize the event data */
  ITERATE_BUTTONS(i) {
    digevent[i].base.type = EVENT_EVENT_PROVIDER;
    digevent[i].base.name = base_name;
    digevent[i].type = ETYPE_DIG_INPUT_EVENT;
    digevent[i].event_name = (char*)dig_names[i];
    digevent[i].vt.init_event = NULL;
  }
}

/*
 * Toggle the output of the channel connected to this button
 */
static void toggle_light (event_prv_t *ptr) __reentrant
{
  rule_t *rp;

  rp = rule_lookup_from_event (ptr);

  /* If there is no data return without doing anything */
  if (!rp)
    return;

  /* Depending on what action manager this event is routed to we need to do
   * different things */
  switch (rp->action->type) {
    case ATYPE_ABSOLUTE_ACTION:
    {
      u8_t channel = rp->action_data.abs_data.channel-1;

      if (rp->r_data->adata)
        rp->r_data->adata = 0;
      else
        rp->r_data->adata = rp->action_data.abs_data.value;

      B_(printf (__AT__ "Sending toggle event %d, %d !\n",
                 channel, rp->r_data->adata);)
      rp->r_data->command = EVENT_USE_DYNAMIC_DATA;
      rule_send_event_signal (ptr);
      break;
    }

    case ATYPE_CYCLE_ACTION:
      /* If it's a cycle just send a start trigger with our own command */
      rp->r_data->command = EVENT_USE_CONTINUE;
      rule_send_event_signal (ptr);
      break;

    default:
      A_(printf (__AT__ "Incorrect action manager !\n");)
      break;
  }
}

/*
 * Toggle the output of the channel connected to this button
 */
void switch_light (event_prv_t *ptr, u8_t state) __reentrant
{
  rule_t *rp;
  u8_t channel;

  rp = rule_lookup_from_event (ptr);
  channel = rp->action_data.abs_data.channel-1;

  /* If there is no data return without doing anything */
  if (!rp)
    return;

  /* Depending on what action manager this event is routed to we need to do
   * different things */
  switch (rp->action->type) {
    case ATYPE_ABSOLUTE_ACTION:
    {
      if (state)
        rp->r_data->adata = rp->action_data.abs_data.value;
      else
        rp->r_data->adata = 0;

      rp->r_data->command = EVENT_USE_DYNAMIC_DATA;
      B_(printf (__AT__ "Sending switch event %d, %d !\n",
                 channel, rp->r_data->adata);)
      rule_send_event_signal (ptr);
      break;
    }

    default:
      A_(printf (__AT__ "Incorrect action manager !\n");)
      break;
  }
}

/*
 * The button main thread
 */
#pragma save
#pragma nogcse
PT_THREAD(handle_dig_event(dig_event_t *dig_event) __reentrant __banked)
{
  PT_BEGIN(&dig_event->pt);

  A_(printf (__AT__ "Starting digital event provider !\n");)
  /* Set the last button state to a known state */
  dig_event->old_state = BUTTON_PORT & ALL_BUTTONS_MASK;

  /* Hog one timer for button debounce */
  dig_event->tmr = alloc_timer ();

  /* Register the event provider */
  ITERATE_BUTTONS(dig_event->i) {
    evnt_register_handle (&digevent[dig_event->i]);
  }

  while (1)
  {
again:
    /* Wait for a button event */
    A_(printf (__AT__ "Waiting for a button to be pressed !\n");)
    PT_WAIT_UNTIL (&dig_event->pt,
        (dig_event->old_state != (BUTTON_PORT & ALL_BUTTONS_MASK)));
    /* Store the current state */
    dig_event->state = BUTTON_PORT & ALL_BUTTONS_MASK;
    /* key debounce */
    set_timer (dig_event->tmr, DEBOUNCE_TIME, NULL);
    PT_WAIT_UNTIL (&dig_event->pt, get_timer (dig_event->tmr) == 0);
    /* Now check that the state is the same */
    if (dig_event->state != (BUTTON_PORT & ALL_BUTTONS_MASK)) {
      A_(printf (__AT__ "Debounce restart !\n");)
      goto again;
    }

    /* State change found so find out what happened */
    ITERATE_BUTTONS(dig_event->i)
    {
      /* Use this mask for the current button */
      dig_event->mask = button_mask[dig_event->i];
      A_(printf (__AT__ "Checking button %d, 0x%02x, 0x%02x\n", dig_event->i,
          dig_event->state & dig_event->mask,
          dig_event->old_state & dig_event->mask);)

      if ((dig_event->state & dig_event->mask) !=
          (dig_event->old_state & dig_event->mask))
      {
        A_(printf (__AT__ "State has indeed changed !\n");)
        /* Button changed so process change */
        if (dig_event->dptr[dig_event->i].mode == BUTTON_TOGGLE_MODE)
        {
          /* Toggle mode button */
          toggle_light (&digevent[dig_event->i]);
          /* Now wait for the button to be released */
          PT_WAIT_WHILE (&dig_event->pt, ((BUTTON_PORT & dig_event->mask) ==
             (dig_event->state & dig_event->mask)));
          /* key debounce */
          set_timer (dig_event->tmr, DEBOUNCE_TIME, NULL);
          PT_WAIT_UNTIL (&dig_event->pt, get_timer (dig_event->tmr) == 0);
        } else if (dig_event->dptr[dig_event->i].mode == BUTTON_SWITCH_MODE) {
          /* Switch light according to switch state */
          switch_light (&digevent[dig_event->i],
              (dig_event->state & dig_event->mask) ^
              (dig_event->dptr[dig_event->i].inverted ? dig_event->mask : 0));
        }
      }
    }
    dig_event->old_state = (BUTTON_PORT & ALL_BUTTONS_MASK);
  }

  PT_END(&dig_event->pt);
}
#pragma restore

/* EOF */
