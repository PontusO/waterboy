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

#include <stdlib.h>

#include "system.h"
#include "flash.h"
#include "iet_debug.h"
#include "event_switch.h"
#include "pir_event.h"
#include "swtimers.h"

#include "comparator.h"
#include "dac.h"

/* Event handle */
static event_prv_t pirevent;
static const char *base_name = "Analog Sensor";
static const char *pir_name = "PIR Sensor";

/*
 * Initialize the pir_event pthread
 */
void init_pir_event(pir_event_t *pir_event) __reentrant __banked
{
  PT_INIT(&pir_event->pt);

  /* Initialize the event data */
  pirevent.base.type = EVENT_EVENT_PROVIDER;
  pirevent.base.name = base_name;
  pirevent.type = ETYPE_PIR_SENSOR_INPUT_EVENT;
  pirevent.event_name = (char*)pir_name;
  pirevent.vt.init_event = NULL;
}

/*
 * Deallocate comparator and dac
 */
static void deallocate_hw (void)
{
  free_comparator (0);
  write_dac (0,0);
  free_dac (0);
}

/*
 * Allocate and setup dac and comparator hardware for use with a PIR sensor.
 */
static void allocate_hw (pir_event_t *pir_event)
{
  long tmp;
  /* Setup dac operation */
  if (allocate_dac(0) != DAC_ERR_OK) {
    add_error_to_log (1);
    A_(printf(__AT__ " Could not allocate dac !\n");)
  }
  tmp = DAC_MAX_SCALE * (long)sys_cfg.pir_sensitivity / 100;
  write_dac (0, (u16_t)tmp);
  /* And set the comparator up */
  if (allocate_comparator(0) != COMP_ERR_OK) {
    add_error_to_log (1);
    A_(printf(__AT__ " Could not allocate comparator !\n");)
  }
  /* Set up a timer to wait for the dac and comparator to settle */
  pir_event->tmr = alloc_timer();
  set_timer(pir_event->tmr, 200, NULL, NULL);
}

/*
 * The pir main thread
 */
PT_THREAD(handle_pir_event(pir_event_t *pir_event) __reentrant __banked)
{
  PT_BEGIN(&pir_event->pt);

  /* Setup a timer that will lock out the event being triggered again to soon */
  pir_event->ltmr = alloc_timer();

  /* Register the event provider */
  if (evnt_register_handle(&pirevent) < 0) {
    add_error_to_log (2);
    A_(printf (__AT__ " Could not register pir event !\n");)
  }

  A_(printf (__AT__ " Starting pir_event pthread, handle ptr %p !\n", &pir_event);)

  /* Make sure the hardware is initialized if necessary */
  if (sys_cfg.pir_enabled) {
    allocate_hw (pir_event);
    PT_WAIT_UNTIL (&pir_event->pt, !get_timer(pir_event->tmr));
    free_timer (pir_event->tmr);
  }

  while (1)
  {
again:
    if (!sys_cfg.pir_enabled) {
      /* If the sensor input is disabled, wait here for it to be enabled */
      PT_WAIT_UNTIL (&pir_event->pt, sys_cfg.pir_enabled);
      A_(printf(__AT__ " PIR sensor has been enabled !\n");)
      allocate_hw (pir_event);
      PT_WAIT_UNTIL (&pir_event->pt, !get_timer(pir_event->tmr));
      free_timer (pir_event->tmr);
    }

    /* Wait for the comparator to go low, or PIR sensor being turned off */
    PT_WAIT_UNTIL (&pir_event->pt, (!get_comparator_state (0) &&
                                     get_timer (pir_event->ltmr) == 0) ||
                                    !sys_cfg.pir_enabled);
    if (!sys_cfg.pir_enabled) {
      A_(printf(__AT__ " PIR Sensor turned off !\n");)
      goto again;
    }
    A_(printf(__AT__ " PIR sensor triggered !\n");)

    /* Set and restart timer */
    set_timer (pir_event->ltmr, sys_cfg.pir_lockout * 100, NULL, NULL);

    /* Send the signal */
    rule_send_event_signal (&pirevent);

    /* And wait for it to go high again */
    PT_WAIT_UNTIL (&pir_event->pt, get_comparator_state(0) || !sys_cfg.pir_enabled);

    A_(printf(__AT__ " PIR sensor returned to normal again !\n");)
  } /* while (1) */

  PT_END(&pir_event->pt);
}

/* EOF */
