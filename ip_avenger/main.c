/*
 * Copyright (c) 2006, Invector Embedded Technologies
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
 * This file is part of the uIP TCP/IP stack.
 *
 * Author:    Tony Pemberton, Pontus Oldberg
 *
 */

/*
 * Notes on debug usage in this module.
 *
 *  The include file iet_debug.h defines a number of (A-C) inclusive macros
 *  that can be used to enter debug printouts into the code. When no PRINT_X
 *  macros are defined during compile time no debug prints are present in the
 *  resulting binary.
 *
 *  In this module I have classified the three levels of debug prints in the
 *  following classes:
 *    - A = System level printouts, important stuff that is always needed
 *          when debugging the system.
 *    - B = Informative prints, used to trace program flow etc during execution.
 *    - C = Informative prints. Should be used on repetetive code that spams
 *          the output but is sometimes useful.
 */

#include "system.h"       // System description
#include "main.h"         // Locals
#include "iet_debug.h"    // Debug macros
#include "swtimers.h"     // Software timer system
#include "flash.h"
#include "i2c.h"
#include "rtc.h"
#include "httpd-cgi.h"
#include "product.h"

#define SAVE_PSBANK(n)      n = PSBANK
#define SET_PSBANK(bank)    PSBANK = (PSBANK & 0xcf) | (bank << 4)
#define RESTORE_PSBANK(n)   PSBANK = n

//-----------------------------------------------------------------------------
// Function PROTOTYPES
//-----------------------------------------------------------------------------

void Timer0_ISR (void) __interrupt TF0_VECTOR;
extern void SMBus_ISR (void) __interrupt SMB0_VECTOR;
extern void rtc_isr(void) __interrupt TF3_VECTOR;
void ADC_ISR (void) __interrupt AD0INT_VECTOR __using 2;

extern timer_cb timer_cbs[NUMBER_OF_SWTIMERS];

//-----------------------------------------------------------------------------
// Static data definitions
//-----------------------------------------------------------------------------
u16_t half_Sec;
u16_t ten_Secs;
static __near u8_t tmcnt;

__bit TX_EventPending;	        // the DM9000 hardware receive event
__bit ARP_EventPending;         // trigger the arp timer event
__bit callback_kicker;

//-----------------------------------------------------------------------------
// Call the main application
//
//-----------------------------------------------------------------------------
void main(void)
{
  pmd();
}
//-----------------------------------------------------------------------------
// Received a datagram
//   Function template for receiving datagram
//-----------------------------------------------------------------------------
void datagram(void)
{
  C_(printf("Received a datagram\n");)
}

//-----------------------------------------------------------------------------
// Routine to shut down system in an orderly fashion
//-----------------------------------------------------------------------------
void cleanup(void)
{
	EA = FALSE;
	RSTSRC |= 0x10; // Force a software reset
}

// ---------------------------------------------------------------------------
// Timer 0 __interrupt handler. Every 10ms
//   For this ISR to work on a IET912X module SFRGCN must be set to 0x01.
// ---------------------------------------------------------------------------
void Timer0_ISR (void) __interrupt TF0_VECTOR __using 0
{
  TL0 = (-((SYSCLK/12)/100) & 0x00ff);  // Load timer 0 to give
  TH0 = (-((SYSCLK/12)/100) >> 8);      // 20MHz/12/100 = approx 10mS

  half_Sec--;                           // Decrement half sec counter
  if (half_Sec == 0)                    // Count 10ms intervals for half a second
  {
    half_Sec = UIP_TX_TIMER;            // and then set the receive poll flag
    TX_EventPending = TRUE;             // the DM9000 hardware receive.
  }

  ten_Secs--;
  if (ten_Secs == 0)                    // Count 10ms intervals for ten seconds
  {
    ten_Secs = UIP_ARP_TIMER;           // and then set the event required to
    ARP_EventPending = TRUE;            // trigger the arp timer if necessary
  }

  /* Count all used sw timers down by one */
  for(tmcnt=0 ; tmcnt<NUMBER_OF_SWTIMERS ; tmcnt++)
  {
    if (timer_table[tmcnt] == TMR_RUNNING)
    {
      swtimer[tmcnt]--;
      if (!swtimer[tmcnt])
      {
        if (timer_cbs[tmcnt])
        {
          timer_table[tmcnt] = TMR_KICK;
          callback_kicker = 1;
        }
        else
        {
          timer_table[tmcnt] = TMR_ENDED;
        }
      }
    }
  }
  ET0 = TRUE;                           // enable Timer0 interrupts, again
}

/* End of file */






