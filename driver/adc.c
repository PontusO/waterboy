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

#include <stdio.h>
#include <system.h>
#include <string.h>

#include "adc.h"
#include "iet_debug.h"

#ifdef IET_VDVT_ADVANCED
#define USED_ADC_CHANNELS     5
#else
#define USED_ADC_CHANNELS     4
#endif

#define ADC_INT_ON()          EIE2 |= 0x02
#define ADC_INT_OFF()         EIE2 &= ~0x02

#define USE_ADC_TIMER         2

static struct adc adc[USED_ADC_CHANNELS];
static __data u8_t adc_chan;

/* Signal definition */
char SIG_NEW_ADC_VALUE_RECEIVED = -1;

/*
 * A/D converter ISR.
 */
void ADC_ISR (void) __interrupt AD0INT_VECTOR __using 2
{
  __data u8_t w_ptr = adc[adc_chan].w_ptr;
  __data u8_t r_ptr;
  __data u16_t sample = ADC0L | (ADC0H << 8);
  __data u8_t i;
  __data long sum = 0;

  /* I really don't know why this has to be here, but it
   * just wont work otherwise */
  if (adc_chan > USED_ADC_CHANNELS) {
    adc_chan = 0;
    printf ("ERROR: adc_chan=%d\r\n", adc_chan);
    goto exit;
  }

  adc[adc_chan].values[w_ptr] = sample;
  adc[adc_chan].last_sample = sample;

  /* Increment write ptr and wrap if necessery */
  if (++adc[adc_chan].w_ptr == MAX_SAMPLES)
    adc[adc_chan].w_ptr = 0;

  /* Make sure the sample buffer is full */
  if (adc[adc_chan].n == MAX_SAMPLES) {
    /* Increment and adjust the read pointer */
    if (++adc[adc_chan].r_ptr == MAX_SAMPLES)
      adc[adc_chan].r_ptr = 0;
    /* Start from here */
    r_ptr = adc[adc_chan].r_ptr;
    /* calculate an average */
    for (i=0;i<MAX_SAMPLES;i++) {
      sum += adc[adc_chan].values[r_ptr];
      if (++r_ptr == MAX_SAMPLES)
        r_ptr = 0;
    }
    /* This is the equivilent of dividing by 32 */
    adc[adc_chan].latest_average = (u16_t)(sum >> 5);
  } else {
    /* Just increment n of samples */
    adc[adc_chan].n++;
    /* If the averaging has not started yet we simply store the
     * last value here */
    adc[adc_chan].latest_average = sample;
  }

  /* Notify user space that a new sample has arrived */
  SIG_NEW_ADC_VALUE_RECEIVED = adc_chan;

  /* Now prepare for the next channel */
  if (++adc_chan == USED_ADC_CHANNELS)
    adc_chan = 0;

  /* Set up channel for next measurement */
  AMX0SL = adc[adc_chan].channel;
exit:
#if BUILD_TARGET == IET912X
  AD0INT = 0;
#else
  ADCINT = 0;
#endif
}

/*
 * adc_get_average
 *
 * This function will run an average algorithm on the samples in the specified
 * buffer and return the averaged value.
 */
u16_t adc_get_average(u8_t channel) __reentrant __banked
{
  u16_t tmp;

  ADC_INT_OFF();
  if (adc[channel].n != MAX_SAMPLES)
    tmp = 0;
  else
    tmp = adc[channel].latest_average;
  ADC_INT_ON();
  return tmp;
}

/*
 * adc_get_last_sample
 *
 * This function returns the last value that was collected from the A/D converter
 */
u16_t adc_get_last_sample(u8_t channel) __reentrant __banked
{
  u16_t tmp;
  ADC_INT_OFF();
  tmp = adc[channel].last_sample;
  ADC_INT_ON();
  return tmp;
}

/*
 * Get temperature value from averaged value
 */
int get_temperature(u8_t channel) __reentrant __banked
{
  int adc = adc_get_average(channel);
  int sample = adc_get_last_sample(channel);
  int normalized = adc - 1864;
  int t1 = (normalized * 25) / 17;

  return sample;
}


#if USE_ADC_TIMER == 3
/*
 * adc_timer_init Used to control the ADC0 sample rate.
 *
 * Timer 3 will be set to __using sysclk / 12 (= 1.7MHz)
 *
 */
void adc_timer_init (int counts) __reentrant __banked
{
#if BUILD_TARGET == IET912X
  unsigned char store = SFRPAGE;    /* Save the SFR register */
  SFRPAGE = TMR3_PAGE;
#endif
  /* Set timer 3 control register, disabled, use sysclk / 12 */
  TMR3CN = 0x00;

#if BUILD_TARGET == IET912X
  RCAP3L  = (-(counts) & 0x00ff);	      // Timer 3 Reload Register Low Byte
  RCAP3H  = (-(counts) >> 8);	          // Timer 3 Reload Register High Byte
#else
  TMR3RLL = (-(counts) & 0x00ff);	      // Timer 3 Reload Register Low Byte
  TMR3RLH = (-(counts) >> 8);	          // Timer 3 Reload Register High Byte
#endif
  /* Make sure timer is reloaded immediatly */
  TMR3L = 0xff;
  TMR3H = 0xff;

  SFRPAGE = store;
  /* return without enabling the timer, let the adc setup do that */
}

void adc_timer_enable (void)
{
  SFRPAGE = TMR3_PAGE;
  TMR3CN |= 4;
}
#elif USE_ADC_TIMER == 2
/*
 * adc_timer_init Used to control the ADC0 sample rate.
 *
 * Timer 2 will be set to __using sysclk / 12 (= 1.7MHz)
 *
 */
void adc_timer_init (int counts) __reentrant __banked
{
#if BUILD_TARGET == IET912X
  unsigned char store = SFRPAGE;    /* Save the SFR register */
  SFRPAGE = TMR2_PAGE;
#endif
  /* Set timer 3 control register, disabled, use sysclk / 12 */
  TMR2CN = 0x00;

  RCAP2  = -counts;
  /* Make sure timer is reloaded immediatly */
  TMR2 = 0xffff;

#if BUILD_TARGET == IET912X
  SFRPAGE = store;
#endif
}

void adc_timer_enable (void)
{
  SFRPAGE = TMR2_PAGE;
  TMR2CN |= 4;
}
#endif

void adc_init(void)
{
  adc_chan = 0;
  /* Clear all adc channel operating data */
  memset(&adc[0], 0, sizeof(struct adc) * USED_ADC_CHANNELS);

  /* Set up the a/d channels we want to measure */
  adc[0].channel = 0;  /* POT 1 */
  adc[1].channel = 1;  /* POT 2 */
  adc[2].channel = 2;  /* POT 3 */
  adc[3].channel = 3;  /* POT 4 */
#ifdef IET_VDVT_ADVANCED
  adc[4].channel = 8;  /* Chip temp sensor */
#endif

  /* Initialize the 12 __bit A/D Converter */
  SFRPAGE = ADC0_PAGE;
#if USE_ADC_TIMER == 3
  ADC0CN = 0x80 | 0x04;
#else
  ADC0CN = 0x80 | 0x0c;
#endif
  ADC0CF = 0xF8 | 0x00;
  REF0CN = 0x03;

  /* Initilize sample rate counter 1500 Hz sample rate*/
  adc_timer_init(1111);

  /* Enable ADC interrupts */
  ADC_INT_ON();

  /* Start the timer */
  adc_timer_enable ();
}

/* EOF */
