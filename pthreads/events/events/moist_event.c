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
#include "iet_debug.h"
#include "flash.h"
#include "moist_event.h"
#include "absval_mgr.h"
#include "adc.h"

#define CUT_OUT_SPAN      4
#define KNEE_LEVEL        512
#define ADC_RESOLUTION    10
#define VALUE_SHIFT       (16 - ADC_RESOLUTION)

/* Event handle */
static event_prv_t adcevents[4];
static const char *base_name = "Sensor";
static const char *adc_names[4] =
  { "Fuktsensor 1", "Fuktsensor 2",
    "Fuktsensor 3", "Fuktsensor 4" };

rule_data_t *rptr;

/*
 * Initialize the moist_event pthread
 */
void init_moist_event(moist_event_t *moist_event) __reentrant __banked
{
  char i;

  PT_INIT(&moist_event->pt);

  /* Initialize the event data */
  for (i=0; i<CFG_NUM_POTS; i++) {
    adcevents[i].base.type = EVENT_EVENT_PROVIDER;
    adcevents[i].type = ETYPE_POTENTIOMETER_EVENT;
    adcevents[i].base.name = base_name;
    adcevents[i].event_name = (char*)adc_names[3-i];
    adcevents[i].vt.init_event = NULL;
    /* This will ensure that the light settings will update on start */
    moist_event->prev_pot_val[i] = 100;
  }
}

PT_THREAD(handle_moist_event(moist_event_t *moist_event) __reentrant __banked)
{
  char i;

  PT_BEGIN(&moist_event->pt);

  /* Register the event provider */
  for (i=0; i<CFG_NUM_POTS; i++)
    evnt_register_handle(&adcevents[3-i]);

  A_(printf (__AT__ " Starting moist_event pthread, handle ptr %p !\n",
             &moist_event);)

  while (1)
  {
    long tmp;

    PT_WAIT_UNTIL (&moist_event->pt, SIG_NEW_ADC_VALUE_RECEIVED != -1);
    moist_event->channel = SIG_NEW_ADC_VALUE_RECEIVED;
    SIG_NEW_ADC_VALUE_RECEIVED = -1;
    /* Convert to a percentage */
    tmp = adc_get_average(moist_event->channel);
    tmp = (tmp * 100) / 1024;
    moist_event->pot_val = (int)tmp;

    /* Make sure the moist sensor is enabled and check if we need
     * to trigger the action manager */
    if (sys_cfg.moist_data[moist_event->channel].enabled &&
        moist_event->pot_val < sys_cfg.moist_data[moist_event->channel].activate) {
      /* Send the signal */
      A_(printf (__AT__ "adc event has triggered, channel %d\n",
                 moist_event->channel);)
      rule_send_event_signal (&adcevents[moist_event->channel]);
    }
  }
  PT_END(&moist_event->pt);
}

/* EOF */
