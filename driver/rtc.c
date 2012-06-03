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
#pragma codeseg  APP_BANK

//#define PRINT_A
//#define PRINT_B

#include "system.h"
#include "psock.h"
#include "swtimers.h"
#include "iet_debug.h"
#include "flash.h"
#include "rtc_i2c.h"
#include "rtc.h"

#include <stdio.h>
#include <string.h>

extern unsigned long g_time;

/* This gives us a nice time base to work with */
#define TIME_BASE   (65536 - 50000)

#define RTC_DAY_THE_FIRST    1
#define RTC_MONTH_JANUARY    1
#define RTC_MONTH_FEBRUARY   2
#define RTC_MONTH_DECEMBER  12

/* To save .code storage these could be made into functions */
#define bcd2bin(val) ((val)&15) + ((val)>>4)*10
#define bin2bcd(val) (((val)/10)<<4) + (val)%10

struct time_client tc;

struct time_state {
  struct psock psock;
  u8_t connected;
  u8_t inputbuffer[4];
};

__bit RTC_GET_TIME_EVENT;
__bit RTC_GET_FAILED;
struct time_param *RTC_SET_HW_RTC;

static const u8_t days_in_month[] =
  {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static char *weekdays[] =
  {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

void binary_to_dat(struct time_param *tp) __reentrant __banked;

struct time_state s;

void start_rtc(void) __banked
{
  TR3 = 1;
}

void stop_rtc(void) __banked
{
  TR3 = 0;
}

void init_rtc(void) __banked
{
#if BUILD_TARGET == IET912X
  SFRPAGE = TMR3_PAGE;                  // Set the correct SFR page
  RCAP3L = TIME_BASE & 0xff;	          // Timer 3 Reload Register Low Byte
  RCAP3H = TIME_BASE >> 8;              // Timer 3 Reload Register High Byte
#elif BUILD_TARGET == IET902X
  TMR3RLL = 0x00;	                      // Timer 3 Reload Register Low Byte
  TMR3RLH = 0x00;	                      // Timer 3 Reload Register High Byte
#endif
  TMR3L = TIME_BASE & 0xff;
  TMR3H = TIME_BASE >> 8;

  TMR3CN = 0x00;	                      // Timer 3 Control Register
  TMR3CF = 0x18;                        // Sysclk / 2
  EIE2 |= 1;                            // Enable timer 3 interrupts
  TR3 = 1;                              // Start timer 3
#if BUILD_TARGET == IET912X
  SFRPAGE = LEGACY_PAGE;                // Reset to legacy SFR page
#endif

  /* We want to obtain a fresh time right from the start */
  RTC_GET_TIME_EVENT = 0;
  RTC_GET_FAILED = 0;
  RTC_SET_HW_RTC = 0;

  memset (&tc, 0, sizeof tc);
  PT_INIT(&tc.pt);

}

/*************************************************************************************
 *
 * Getter for getting the current binary time value.
 *
 ************************************************************************************/
unsigned long get_g_time(void) __banked
{
  unsigned long t;
  /* Disable timer 3 interrupts momentarily */
  EIE2 &= ~1;
  /* transfer new time value */
  t = g_time;
  /* Enable timer 3 interrupts again */
  EIE2 |= 1;

  return t;
}

/*************************************************************************************
 *
 * Setter for setting the current binary time value.
 *
 ************************************************************************************/
void set_g_time(struct time_param *tp) __reentrant __banked
{
  /* Disable timer 3 interrupts momentarily */
  EIE2 &= ~1;
  /* transfer new time value */
  g_time = tp->b_time;
  /* Enable timer 3 interrupts again */
  EIE2 |= 1;
}
/*************************************************************************************
 *
 * Function for printing the current date to the specified buffer.
 *
 ************************************************************************************/
void print_date_formated(char *buf) __reentrant __banked
{
  struct time_param tp;

  /* Get the binary time value */
  tp.b_time = get_g_time();
  /* Convert to readable format */
  binary_to_dat(&tp);

  sprintf(buf, "%04d-%02d-%02d",
          tp.time.year,
          tp.time.month,
          tp.time.day);
}
/*************************************************************************************
 *
 * Function for printing the current time to the specified buffer.
 *
 ************************************************************************************/
void print_time_formated(char *buf) __reentrant __banked
{
  struct time_param tp;

  /* Get the binary time value */
  tp.b_time = get_g_time();
  /* Convert to readable format */
  binary_to_dat(&tp);

  sprintf(buf, "%02d:%02d",
          tp.time.hrs,
          tp.time.min);
}
/*************************************************************************************
 *
 * Function for printing the current time to the specified buffer.
 *
 ************************************************************************************/
void print_datetime_formated(char *buf) __reentrant __banked
{
  struct time_param tp;
  u8_t dow;

  /* Get the binary time value */
  tp.b_time = get_g_time();
  /* Convert to readable format */
  binary_to_dat(&tp);

  /* Get day of week */
  dow = day_of_week(tp.time.year, tp.time.month, tp.time.day);

  /* As we can't use more than 4 parameters for sprintf (Due to stack overflows)
   * we need to split the print sequence in two parts here. Tack för det televerket.
   */
  sprintf(buf, "%04d-%02d-%02d ",
          tp.time.year,
          tp.time.month,
          tp.time.day);
  B_(printf (__AT__ " dow %d (%s)\n", dow, day_of_week_str(dow));)
  sprintf((char*)(buf+strlen(buf)), " (%s) ",
          day_of_week_str(day_of_week(tp.time.year, tp.time.month, tp.time.day)));
  sprintf((char*)(buf+strlen(buf)), "%02d:%02d:%02d",
          tp.time.hrs,
          tp.time.min,
          tp.time.sec);
}

/*************************************************************************************
 *
 * Check if the given year is a leap year or not and return true or false accordingly.
 *
 ************************************************************************************/
static u8_t is_leap_year(u16_t year) __reentrant
{
  if (year % 4 != 0)
    return FALSE;
  if (year % 100 != 0)
    return TRUE;
  if (year % 400 != 0)
    return FALSE;
  return TRUE;
}

/*************************************************************************************
 *
 * Day of week: Sakamoto's Method
 *  tabular alternative to Mike Keith's method is embodied in the following ANSI C
 *  function, which is adaptable to other high level programming languages with minor
 *  changes (a 6502 assembly language version of Sakamoto's algorithm exists as well).
 *  Devised by Tomohiko Sakamoto[2] in 1993, it is accurate for any date in the range
 *  October 15, 1582–December 31, 9999:
 *
 *  0 = Sunday, 1 = Monday, etc.
 *
 ************************************************************************************/
u8_t day_of_week(int y, int m, int d) __reentrant
{
  static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= m < 3;
  return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

char *day_of_week_str (u8_t day) __reentrant
{
  return weekdays[day];
}

static __xdata unsigned long second;
static __xdata unsigned long minute;
static __xdata unsigned long hour;
static __xdata int day;
static __xdata int month;
static __xdata int year;
static __xdata long binval;

static __xdata unsigned long whole_minutes;
static __xdata unsigned long whole_hours;
static __xdata long whole_days;

/*************************************************************************************
 *
 * This function converts a 32-__bit binary time value to something understandable.
 *
 * To call this function, create an instance of a time_param struct, fill the b_time
 * (binary time) with the time that you want to convert. Then call this function.
 * example:
 *
 *  {
 *    unsigned long time_to_convert = <some_32_bit_value>;
 *    struct time_param tp;
 *
 *    tp.b_time = time_to_convert;
 *    binary_to_dat(&tp);
 *
 ************************************************************************************/
void binary_to_dat(struct time_param *tp) __reentrant __banked
{
  /*
   * Calculate the time first
   */
  whole_minutes = (tp->b_time + sys_cfg.time_zone * 3600) / 60;
  second = (tp->b_time + sys_cfg.time_zone * 3600) - (60 * whole_minutes);
  whole_hours  = whole_minutes / 60;
  minute = whole_minutes - (60 * whole_hours);
  whole_days   = whole_hours / 24;
  hour         = whole_hours - (24 * whole_days);

  /* Calculate year */
  year = 1899;
  while (whole_days >= 0) {
    year++;
    if (is_leap_year(year)) {
      whole_days -= 366;
    } else {
      whole_days -= 365;
    }
  }
  /* Adjust year */
  if (whole_days < 0) {
    if (is_leap_year(year))
      whole_days += 366;
    else
      whole_days += 365;
  }
  /* Calculate month
   * Month ranges from 0-11 */
  month = -1;
  while (whole_days >= 0) {
    month++;
    if (is_leap_year(year) && month == 1)
      whole_days -= 29;
    else
      whole_days -= days_in_month[month];
  }
  /* Adjust month */
  if (whole_days < 0) {
    if (is_leap_year(year) && month == 1)
      whole_days += 29;
    else
      whole_days += days_in_month[month];
  }
  day = whole_days;

  tp->time.sec   = (u8_t)second;             /* seconds after the minute - [0,59]    */
  tp->time.min   = (u8_t)minute;             /* minutes after the hour - [0,59]      */
  tp->time.hrs   = (u8_t)hour;               /* hours since midnight - [0,23]        */
  tp->time.day   = (u8_t)day+1;              /* day of the month - [0,30]            */
  tp->time.month = (u8_t)month+1;            /* months since January - [0,11]        */
  tp->time.year  = (u16_t)year;              /* years since 1900                     */
}
/*************************************************************************************
 *
 * This function converts the human readable part of a date to a 32-__bit number.
 *
 * To call this function, create an instance of a time_param struct, fill the pmdtime
 * structure with the date and time that you want to convert. Then call this function.
 * example:
 *
 *  {
 *    unsigned long binary_time;
 *    struct time_param tp;
 *
 *    tp.time.year = 2004;
 *    tp.time.month = 03;           // March
 *    tp.time.day = 04;             // The 4th of march
 *    tp.time.hrs = 20;
 *    tp.time.min = 43;
 *    tp.time.sec = 0;              // Sets the time to 20:43
 *    dat_to_binary(&tp);
 *    binary_time = tp.b_time;      // Retrieve the binary time value
 *
 ************************************************************************************/
void dat_to_binary(struct time_param *tp) __reentrant __banked
{
  /* Set up before calculations start */
  year = tp->time.year-1;
  month = (int)tp->time.month-2;
  tp->b_time = 0;

  /* Caclulate seconds per year down to 1900 */
  while(year >= 1900) {
    if (is_leap_year(year))
      tp->b_time += 31622400;
    else
      tp->b_time += 31536000;
    year--;
  }

  /* Seconds for already passed months this year */
  while(month >= 0) {
    if (is_leap_year(tp->time.year) && (month == 1))
      tp->b_time += 29 * 86400;
    else
      tp->b_time += days_in_month[month] * 86400;
    month--;
  }

  /* Seconds for passed days this month*/
  tp->b_time += (tp->time.day-1) * 86400;

  /* And the passed hours, minutes and seconds today */
  tp->b_time += ((unsigned long)tp->time.hrs) * 3600 +
                ((unsigned long)tp->time.min) * 60 +
                ((unsigned long)tp->time.sec);

  /* Compensate for time zone */
  tp->b_time -= sys_cfg.time_zone * 3600;
}
/*************************************************************************************
 *
 * Check if it is time to do another time update
 *
 ************************************************************************************/
static u8_t time_for_update(struct time_client *tc) __reentrant
{
  EIE2 &= ~1;
  if (g_time >= tc->update_time) {
    tc->update_time = g_time + sys_cfg.update_interval * 3600;
    A_(printf (__AT__ " global time %lu, new update time %lu, "
               "update interval %d\n", g_time, tc->update_time,
               sys_cfg.update_interval);)
    EIE2 |= 1;
    return 1;
  }
  EIE2 |= 1;
  return 0;
}

/*************************************************************************************
 *
 * The data received from the hw_rtc (PCF8563) can be dirty, meaning that bits that
 * are declared as "not relevant" can have a value of 1. This function will clean
 * those bits out, leaving the time structure clean as a babys bum. You should
 * call this directly after reading the data from the RTC.
 *
 ************************************************************************************/
static void cleanup_hw_rtc_data (rtc_data_t *rtc) __reentrant
{
  /* First separate status bits out from time/date data */
  rtc->century = rtc->century_months & HWRTC_CENTURY_BIT ? 1 : 0;
  rtc->low_voltage = rtc->vl_seconds & HWRTC_VOLTAGE_LOW ? 1 : 0;
  /* Clean up time/date data */
  rtc->vl_seconds     &= 0x7f;
  rtc->minutes        &= 0x7f;
  rtc->hours          &= 0x3f;
  rtc->days           &= 0x3f;
  rtc->weekdays       &= 0x07;
  rtc->century_months &= 0x1f;
  rtc->hour_alarm     &= 0xbf;
  rtc->day_alarm      &= 0xbf;
  rtc->weekday_alarm  &= 0x87;
  rtc->clkout_control &= 0x83;
  rtc->timer_control  &= 0x83;
}

/*************************************************************************************
 *
 * Translate from hw_rtc data to the system RTC data format.
 *
 ************************************************************************************/
static void translate_hw_rtc (rtc_data_t *hw_rtc, struct time_param *tp) __reentrant
{
  if (hw_rtc->century)
    tp->time.year = 2100 + bcd2bin(hw_rtc->years);
  else
    tp->time.year = 2000 + bcd2bin(hw_rtc->years);

  tp->time.month = bcd2bin(hw_rtc->century_months);
  tp->time.day = bcd2bin(hw_rtc->days);
  tp->time.hrs = bcd2bin(hw_rtc->hours);
  tp->time.min = bcd2bin(hw_rtc->minutes);
  tp->time.sec = bcd2bin(hw_rtc->vl_seconds);
  dat_to_binary(tp);
}

/*************************************************************************************
 *
 * We als need to translate from system time to hw RTC time.
 *
 ************************************************************************************/
static void translate_system_rtc (struct time_param *tp, rtc_data_t *hw_rtc) __reentrant
{
  if (tp->time.year >= 2100) {
    hw_rtc->century_months = HWRTC_CENTURY_BIT + bin2bcd(tp->time.month);
    hw_rtc->years = bin2bcd(tp->time.year - 2100);
  } else {
    hw_rtc->century_months = bin2bcd(tp->time.month);
    hw_rtc->years = bin2bcd(tp->time.year - 2000);
  }
  hw_rtc->days = bin2bcd(tp->time.day);
  hw_rtc->hours = bin2bcd(tp->time.hrs);
  hw_rtc->minutes = bin2bcd(tp->time.min);
  /* We always reset the VL flag when writing new time data */
  hw_rtc->vl_seconds = bin2bcd(tp->time.sec);
}

/*************************************************************************************
 *
 * This is the basic time client code.
 *
 * This thread is responsible for responding to RTC_GET_TIME_EVENT's.
 * When the event is received the client will, after a initial 4 second delay, connect
 * to a RFC-868 compliant TIME server __using a TCP connect.
 *
 ************************************************************************************/
static char str[10];
PT_THREAD(handle_time_client(struct time_client *tc) __reentrant __banked)
{
  PT_BEGIN(&tc->pt);

  A_(printf (__AT__ " Entering time_client !\n");)

  /* First of all, get the current time from the hw RTC. This may or may not be
   * the correct time, but this can be corrected later on. */
  tc->rtc_i2c.device  = I2C_RTC;
  /* This is the register were we will start to read data */
  tc->rtc_i2c.address = 0x02;
  /* The PCF9563 only wants one address byte */
  tc->rtc_i2c.address_size = 1;
  tc->rtc_i2c.buffer  = &tc->hw_rtc.vl_seconds;
  /* Transfer 7 bytes */
  tc->rtc_i2c.len     = 7;
  /* Read out data from hw RTC */
  PT_SPAWN(&tc->pt, &tc->rtc_i2c.pt, SM_Receive(&tc->rtc_i2c));
  cleanup_hw_rtc_data (&tc->hw_rtc);
  A_(printf (__AT__ " Time read %x:%x:%x\n",
             (int)tc->hw_rtc.hours,
             (int)tc->hw_rtc.minutes,
             (int)tc->hw_rtc.vl_seconds);)
  if (tc->hw_rtc.low_voltage) {
    A_(printf (__AT__ " Battery voltage is low, RTC data may be compromised !\n");)
  }

  /* Convert the hw rtc data to tp format */
  translate_hw_rtc (&tc->hw_rtc, &tc->tp);
  /* Set system rtc value */
  set_g_time(&tc->tp);

  while (1) {
    /* Wait for someone to tell us to get the time */
    PT_WAIT_UNTIL(&tc->pt, RTC_GET_TIME_EVENT ||
                           RTC_GET_FAILED ||
                           RTC_SET_HW_RTC ||
                           time_for_update(tc));
    print_time_formated(str);
    /* Default is not to start a new time update */
    tc->do_update = 0;
    if (RTC_GET_TIME_EVENT) {
      A_(printf(__AT__ " An application requested a time update at %s\r\n", str);)
      RTC_GET_TIME_EVENT = 0;
      tc->do_update = 1;
      tc->retries = 0;
    } else if (RTC_GET_FAILED) {
      RTC_GET_FAILED = 0;
      if (tc->retries < 4) {
        tc->retries++;
        tc->do_update=1;
        A_(printf(__AT__ " Previous attempt to get a time value failed at %s, "
                  "so we try again. Attempt %d\r\n", str, tc->retries+1);)
      } else {
        A_(printf(__AT__ " Maximum number of retries reached, Error connecting to TIME server !\n");)
      }
    } else if (RTC_SET_HW_RTC) {
      /* We received a signal to set a new hw rtc time */
      translate_system_rtc (RTC_SET_HW_RTC, &tc->hw_rtc);
      A_(printf (__AT__ " Setting new hw RTC tim %02x:%02x:%02x\n",
        (int)tc->hw_rtc.hours,
        (int)tc->hw_rtc.minutes,
        (int)tc->hw_rtc.vl_seconds);)
      /* Prepare to talk to the RTC */
      tc->rtc_i2c.device  = I2C_RTC;
      /* This is the register were we will start writing */
      tc->rtc_i2c.address = 0x02;
      /* The PCF9563 only wants one address byte */
      tc->rtc_i2c.address_size = 1;
      tc->rtc_i2c.buffer  = &tc->hw_rtc.vl_seconds;
      /* Transfer 7 bytes */
      tc->rtc_i2c.len     = 7;
      /* Write data to hw RTC */
      PT_SPAWN(&tc->pt, &tc->rtc_i2c.pt, SM_Send(&tc->rtc_i2c));
      /* Transfer done */
      A_(printf(__AT__ " Done writing new data to RTC %s\r\n", str);)
      RTC_SET_HW_RTC = 0;
    } else {
      if (sys_cfg.enable_time) {
        A_(printf(__AT__ " Recurring update of the time !\r\n", str);)
        tc->do_update = 1;
        tc->retries = 0;
      }
    }

    /* Only try to connect the server if the functionality is enabled */
    if (sys_cfg.enable_time && tc->do_update) {
      /* Wait for 4 seconds before processing request */
      tc->timer = alloc_timer();
      set_timer(tc->timer, 400, NULL, NULL);
      PT_WAIT_UNTIL(&tc->pt, get_timer(tc->timer) == 0);
      free_timer(tc->timer);

      if (uip_connect(&sys_cfg.time_server[0], htons(sys_cfg.time_port)) != NULL) {
        A_(printf (__AT__ " Connecting to the time server !\n");)
        /* Connection was successful, proceed with the socket */
        s.connected = 1;
        PSOCK_INIT(&s.psock, s.inputbuffer, sizeof(s.inputbuffer));
      } else {
        A_(printf (__AT__ " Immeditely failed to connect to the TIME server !\n");)
        /* Retry to connect to time server */
        RTC_GET_FAILED = 1;
      }
    }
  }

  PT_END(&tc->pt);
}

/*************************************************************************************
 *
 * Takes care of the TIME server response.
 *
 * This thread is called when the TIME server responds. It will take care of the
 * 32-__bit binary value, convert it to a value suitable for our system and load the
 * rtc with the new value.
 *
 ************************************************************************************/
static PT_THREAD(time_thread(void))
{
  PSOCK_BEGIN(&s.psock);

  /* Read the reply from the TIME server */
  PSOCK_READBUF(&s.psock);

  if (PSOCK_DATALEN(&s.psock) == 4) {
    /* To work on the global instance structure is really ugly but will have to do
     * for now. Needs to be rewritten*/
    tc.tp.b_time = ((unsigned long)s.inputbuffer[0] << 24) |
                   ((unsigned long)s.inputbuffer[1] << 16) |
                   ((unsigned long)s.inputbuffer[2] << 8) |
                    (unsigned long)s.inputbuffer[3];

    set_g_time(&tc.tp);
    /* Calculate next time we should do an update */
    tc.update_time = g_time + sys_cfg.update_interval * 3600;

    /* Update the hw rtc as well */
    binary_to_dat(&tc.tp);
    RTC_SET_HW_RTC = &tc.tp;

    print_time_formated(str);
    A_(printf(__AT__ " Updated real time clock at %s\r\n", str);)
  }

  PSOCK_END(&s.psock);
}

/*************************************************************************************
 *
 * Take care of TIME server client calls
 *
 * This function is called by the packet demultiplexer in response to TCP events
 * coming from uip.
 *
 ************************************************************************************/
void time_appcall(void) __banked
{
  A_(printf(__AT__ " time_appcall\n");)

  if (uip_closed()) {
    s.connected = 0;
    return;
  }
  if (uip_aborted() || uip_timedout()) {
    s.connected = 0;
    /* Let the client know that we failed */
    RTC_GET_FAILED = 1;
    return;
  }
  time_thread();
}

/* End of file */
