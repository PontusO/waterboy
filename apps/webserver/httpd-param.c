/*
 * Copyright (c) 2008, Invector Embedded Technologies.
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * Author: Pontus Oldberg <pontus@invector.se>
 *
 */
#pragma codeseg UIP_BANK

//#define PRINT_A

#include "system.h"
#include "uip.h"
#include "httpd-param.h"
#include "httpd-cgi.h"
#include "flash.h"
#include "rtc.h"
#include "iet_debug.h"
#include "uiplib.h"
#include "cgi_utils.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define PARAM_FUNC(x) \
        static void x(struct httpd_state *s, char *buffer) __reentrant
#define PARAM_ENTRY(x,y) \
        { x , y }
#define NEOP(x) (x != 0)
/*---------------------- Local data types -----------------------------------*/
struct parameter_table {
  const char *param;
  void (*function)(struct httpd_state *s, char *buffer) __reentrant;
};

/*---------------------- Local data ----------------------------------------*/
/* Form parameters that needs to be parsed */
static const struct parameter_table parmtab[] = {
  PARAM_ENTRY("usr", set_username),
  PARAM_ENTRY("pwd", set_password),
  PARAM_ENTRY("node", set_device_id),
  PARAM_ENTRY("hostip", set_ip),
  PARAM_ENTRY("netmask", set_netmask),
  PARAM_ENTRY("gateway", set_gateway),
  PARAM_ENTRY("webport", set_webport),
  PARAM_ENTRY("resettime", reset_time),
  PARAM_ENTRY("entim", enable_time),
  PARAM_ENTRY("timip", set_time),
  PARAM_ENTRY("timprt", set_timeport),
  PARAM_ENTRY("intrvl", set_interval),
  PARAM_ENTRY("timval", set_timevalue),
  PARAM_ENTRY("datval", set_datevalue),
  PARAM_ENTRY("tz", set_timezone),
  /* Entry list parameters */
  PARAM_ENTRY("cb0", set_tslist),
  PARAM_ENTRY("cb1", set_tslist),
  PARAM_ENTRY("cb2", set_tslist),
  PARAM_ENTRY("cb3", set_tslist),
  PARAM_ENTRY("cb4", set_tslist),
  PARAM_ENTRY("cb5", set_tslist),
  PARAM_ENTRY("cb6", set_tslist),
  PARAM_ENTRY("cb7", set_tslist),
  PARAM_ENTRY("cb8", set_tslist),
  PARAM_ENTRY("cb9", set_tslist),
  PARAM_ENTRY("cb10", set_tslist),
  PARAM_ENTRY("cb11", set_tslist),
  PARAM_ENTRY("cb12", set_tslist),
  PARAM_ENTRY("cb13", set_tslist),
  PARAM_ENTRY("cb14", set_tslist),
  PARAM_ENTRY("cb15", set_tslist),
  PARAM_ENTRY("cb16", set_tslist),
  PARAM_ENTRY("cb17", set_tslist),
  PARAM_ENTRY("cb18", set_tslist),
  PARAM_ENTRY("cb19", set_tslist),
  PARAM_ENTRY("cb20", set_tslist),
  PARAM_ENTRY("cb21", set_tslist),
  PARAM_ENTRY("cb22", set_tslist),
  PARAM_ENTRY("cb23", set_tslist),
  PARAM_ENTRY("cb24", set_tslist),
  PARAM_ENTRY("cb25", set_tslist),
  PARAM_ENTRY("cb26", set_tslist),
  PARAM_ENTRY("cb27", set_tslist),
  PARAM_ENTRY("cb28", set_tslist),
  PARAM_ENTRY("cb29", set_tslist),
  PARAM_ENTRY("cb30", set_tslist),
  PARAM_ENTRY("cb31", set_tslist),
  PARAM_ENTRY("tscmd", set_tscmd),
  /* Create time event parameters */
  PARAM_ENTRY("tsx", set_tsx),
  PARAM_ENTRY("tsname", set_tsname),
  PARAM_ENTRY("tsena", set_tsenable),
  PARAM_ENTRY("tstim", set_tstime),
  PARAM_ENTRY("tsd1", set_tsday),
  PARAM_ENTRY("tsd2", set_tsday),
  PARAM_ENTRY("tsd3", set_tsday),
  PARAM_ENTRY("tsd4", set_tsday),
  PARAM_ENTRY("tsd5", set_tsday),
  PARAM_ENTRY("tsd6", set_tsday),
  PARAM_ENTRY("tsd7", set_tsday),
  /* map.shtml */
  PARAM_ENTRY("mapcmd", set_mapcmd),
  /* Create route parameters cmap.shtml */
  PARAM_ENTRY("mapx", set_mapx),
  PARAM_ENTRY("mapenabled", set_mapenabled),
  PARAM_ENTRY("evt", set_evt),
  PARAM_ENTRY("act", set_act),
  PARAM_ENTRY("rmode", set_rmode),
  PARAM_ENTRY("wcmd", set_wcmd),  /* Write command */
  /* PIR parameters */
  PARAM_ENTRY("pirclr", set_pirclr),
  PARAM_ENTRY("pirena", set_pirena),
  PARAM_ENTRY("pirlevel", set_pirlevel),
  PARAM_ENTRY("pirlock", set_pirlock),
  /* Digital input parameters */
  PARAM_ENTRY("digclr", set_digclr),
  PARAM_ENTRY("in1m", set_in1m),
  PARAM_ENTRY("in1s", set_in1s),
  PARAM_ENTRY("in2m", set_in2m),
  PARAM_ENTRY("in2s", set_in2s),
	/* Parameters used in xcgi commands */
  PARAM_ENTRY("channel", cgi_set_channel),
  PARAM_ENTRY("achannel", cgi_set_achannel),
  PARAM_ENTRY("level", cgi_set_level),
  PARAM_ENTRY("rate", cgi_set_rate),
  PARAM_ENTRY("step", cgi_set_step),
  PARAM_ENTRY("timeon", cgi_set_timeon),
  PARAM_ENTRY("onoff", cgi_set_onoff),
    /* This is the last parameter in the list of html parameters.
     * It's used to trigger the real flash save function
     */
  PARAM_ENTRY("submit", set_save),
  /* This entry terminates the table */
  PARAM_ENTRY("*", 0)
};

static u16_t year;
static u8_t month;
static u8_t day;
static u8_t hrs;
static u8_t min;
static __bit need_time_update = FALSE;
static __bit need_reset = FALSE;
static __bit ts_update = FALSE;
static __bit rp_update = FALSE;

/*---------------------------------------------------------------------------*/
static char *skip_to_char(char *buf, char chr) __reentrant
{
  while (*buf != chr)
    buf++;
  buf++;

  return buf;
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_digclr)
{
  IDENTIFIER_NOT_USED(s);
  IDENTIFIER_NOT_USED(buffer);

  /* Simply prepare these flag in case it was not set to true (Stupid html) */
  sys_cfg.in1_inverted = FALSE;
  sys_cfg.in2_inverted = FALSE;
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_in1m)
{
  IDENTIFIER_NOT_USED(s);
  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    sys_cfg.in1_mode = atoi(buffer);
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_in2m)
{
  IDENTIFIER_NOT_USED(s);
  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    sys_cfg.in2_mode = atoi(buffer);
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_in1s)
{
  IDENTIFIER_NOT_USED(s);
  IDENTIFIER_NOT_USED(buffer);

  sys_cfg.in1_inverted = TRUE;
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_in2s)
{
  IDENTIFIER_NOT_USED(s);
  IDENTIFIER_NOT_USED(buffer);

  sys_cfg.in2_inverted = TRUE;
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_pirclr)
{
  IDENTIFIER_NOT_USED(s);
  IDENTIFIER_NOT_USED(buffer);

  /* Simply prepare this flag in case it was not set to true (Stupid html) */
  sys_cfg.pir_enabled = FALSE;
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_pirlevel)
{
  IDENTIFIER_NOT_USED(s);

  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    sys_cfg.pir_sensitivity = atoi(buffer);
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_pirlock)
{
  IDENTIFIER_NOT_USED(s);

  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    sys_cfg.pir_lockout = atoi(buffer);
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_pirena)
{
  IDENTIFIER_NOT_USED(s);
  IDENTIFIER_NOT_USED(buffer);

  sys_cfg.pir_enabled = TRUE;
}

/*---------------------------------------------------------------------------*/
/*
 * See the tsx parameter for more detailed information */
PARAM_FUNC (set_mapx)
{
  rule_t *rp;

  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    rp = (rule_t __xdata *)atoi(buffer);

    /* We'we got to be careful that the read value is appropriate, so check
     * that it falls within the array of rules */
    if (rp == NULL ||
        rp < &sys_cfg.rules[0] ||
        rp > &sys_cfg.rules[0] + sizeof sys_cfg.rules) {
      /* Something's wrong, reset the modify flag */
      s->parms.modify = 0;
      A_(printf (__AT__ " Error, invalid rp value !\n");)
      return;
    }
    A_(printf (__AT__ " Setting rp pointer to %p\n", rp);)
    s->parms.rp = rp;
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_mapcmd)
{
  util_param_t param = {s, buffer};

  x_set_mapcmd (&param);
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_mapenabled)
{
  IDENTIFIER_NOT_USED(buffer);

  if (s->parms.rp) {
    s->parms.rp->status = RULE_STATUS_ENABLED;
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_evt)
{
  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    s->parms.evt = atol(buffer);
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_act)
{
  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    s->parms.act = atol(buffer);
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_wcmd)
{
  util_param_t param = {s, buffer};

  x_set_wcmd(&param);
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_tslist)
{
  u8_t tsentry;

  buffer = skip_to_char(buffer, '=');
  /* Skip back to the first character before the = sign */
  buffer -= 2;
  /* Loop back to first non numeric character */
  while (isdigit(*buffer))
    buffer--;
  /* Go back to the first numeric character in this parameter */
  buffer++;
  tsentry = (u8_t)atoi(buffer);
  s->parms.marklist |= (1 << tsentry);
}

/*---------------------------------------------------------------------------*/
/*
 * The tsx parameter is a hidden parameter that indicates whether the user
 * pressed the modify button on the tevents.shtml page. It is an absolute
 * pointer to the time_spec entry that is being edited. */
PARAM_FUNC (set_tsx)
{
  time_spec_t *ts;

  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    ts = (time_spec_t __xdata *)atoi(buffer);

    /* We'we got to be careful that the read value is appropriate, so check
     * that it falls within the array of time_spec_t's */
    if (ts < &sys_cfg.time_events[0] ||
        ts > &sys_cfg.time_events[0] + sizeof sys_cfg.time_events) {
      s->parms.modify = FALSE;
      A_(printf (__AT__ " Error, invalid ts value !\n");)
      return;
    }
    /* Need to clear the weekday entry */
    ts->weekday  = 0;
    s->parms.ts = ts;
    s->parms.modify = TRUE;
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_tsname)
{
  char i = 8;

  if (s->parms.ts) {
    char *ptr = ((time_spec_t*)(s->parms.ts))->name;

    buffer = skip_to_char(buffer, '=');

    while (NEOP(*buffer) && (i >= 0)) {
      *ptr++ = *buffer++;
      i--;
    }
    *buffer = 0x00;
    ts_update = TRUE;
  } else {
    A_(printf (__AT__ " Serious error occured !\n");)
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_tsenable)
{
  IDENTIFIER_NOT_USED(buffer);

  if (s->parms.ts) {
    s->parms.ts->status |= TIME_EVENT_ENABLED;
    ts_update = TRUE;
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_tstime)
{
  /* Update sys_cfg time_specs time values directly. We rely on the web
   * front end to ensure that the values are correct before being sent
   * here, nasty !? you bet */
  if (s->parms.ts) {
    buffer = skip_to_char(buffer, '=');
    if (NEOP(*buffer)) {
      s->parms.ts->hrs = atoi(buffer);
      buffer = skip_to_char(buffer, '%');
      buffer += 2;
      s->parms.ts->min = atoi(buffer);
    }
    ts_update = TRUE;
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_tsday)
{
  u8_t day;

  if (s->parms.ts) {
    buffer = skip_to_char(buffer, '=');
    buffer -= 2;
    day = *buffer-0x31;
    s->parms.ts->weekday |= (1 << day);
    A_(printf (__AT__ " Day: %d\n", day);)
    ts_update = TRUE;
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_tscmd)
{
  util_param_t param = {s, buffer};

  x_set_tscmd(&param);
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_device_id)
{
  char *ptr = sys_cfg.device_id;
  s->i = 8;

  buffer = skip_to_char(buffer, '=');
  while (NEOP(*buffer) && (s->i-- >= 0)) {
    *ptr++ = *buffer++;
  }
  *ptr = 0x00;
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_ip)
{
  static uip_ipaddr_t ip;
  IDENTIFIER_NOT_USED (s);
  buffer = skip_to_char(buffer, '=');
  uiplib_ipaddrconv(buffer, (u8_t*)&sys_cfg.ip_addr);
  need_reset = TRUE;
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_netmask)
{
  static uip_ipaddr_t ip;
  IDENTIFIER_NOT_USED (s);
  buffer = skip_to_char(buffer, '=');
  uiplib_ipaddrconv(buffer, (u8_t*)&sys_cfg.netmask);
  need_reset = TRUE;
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_gateway)
{
  static uip_ipaddr_t ip;
  IDENTIFIER_NOT_USED (s);
  buffer = skip_to_char(buffer, '=');
  uiplib_ipaddrconv(buffer, (u8_t*)&sys_cfg.gw_addr);
  need_reset = TRUE;
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_webport)
{
  IDENTIFIER_NOT_USED (s);

  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    sys_cfg.http_port = atoi(buffer);
    /* Block non http ports below 80 */
    if (sys_cfg.http_port < 80) {
      sys_cfg.http_port = 80;
    }
    need_reset = TRUE;
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (reset_time)
{
  IDENTIFIER_NOT_USED (buffer);
  IDENTIFIER_NOT_USED (s);

  sys_cfg.enable_time = 0;
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (enable_time)
{
  IDENTIFIER_NOT_USED (s);
  IDENTIFIER_NOT_USED(buffer);

  RTC_GET_TIME_EVENT = 1;
  sys_cfg.enable_time = 1;
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_time)
{
  static uip_ipaddr_t ip;
  IDENTIFIER_NOT_USED (s);
  buffer = skip_to_char(buffer, '=');
  uiplib_ipaddrconv(buffer, (u8_t*)&sys_cfg.time_server);
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_timeport)
{
  IDENTIFIER_NOT_USED (s);

  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer))
    sys_cfg.time_port = atoi(buffer);
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_interval)
{
  IDENTIFIER_NOT_USED (s);

  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    sys_cfg.update_interval = atoi(buffer);
    /* Update Interval can never be 0, so adjust */
    if (!sys_cfg.update_interval)
      sys_cfg.update_interval = 1;
  }
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_timevalue)
{
  IDENTIFIER_NOT_USED (s);
  /* Here we need to parse the time value and store it before creating a 32 __bit
    * binary time value
    * Note that if the option to use a time server we do not parse these values
    */
  if (!sys_cfg.enable_time) {
    buffer = skip_to_char(buffer, '=');
    if (NEOP(*buffer)) {
      hrs = atoi(buffer);
      /* We should be checking for a colon here but since it has been converted
       * to web characters it is now '%3A'. So we check for a percent and
       * skip ahead two characters. Crude, but it works =)
       */
      buffer = skip_to_char(buffer, '%');
      buffer += 2;
      min = atoi(buffer);
      need_time_update = TRUE;
    }
  }
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_datevalue)
{
  IDENTIFIER_NOT_USED (s);
  /* Here we need to parse the date value and store it before creating a 32 bit
    * binary time value
    * Note that if the option to use a time server we do not parse these values
    */
  if (!sys_cfg.enable_time) {
    buffer = skip_to_char(buffer, '=');
    if (NEOP(*buffer)) {
      year=atoi(buffer);
      buffer = skip_to_char(buffer, '-');
      month = atoi(buffer);
      buffer = skip_to_char(buffer, '-');
      day = atoi(buffer);
      need_time_update = TRUE;
    }
  }
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_timezone)
{
  static u8_t tz;

  IDENTIFIER_NOT_USED (s);

  buffer = skip_to_char(buffer, '=');
  tz = atoi(buffer);

  sys_cfg.time_zone = tz;
}
/*---------------------------------------------------------------------------*/
struct time_param tp;
PARAM_FUNC (set_save)
{
  IDENTIFIER_NOT_USED (s);

  buffer = skip_to_char(buffer, '=');
  if (strncmp("save", buffer, 4) == 0) {
    /* Actions needed for the time event set page */
    if (ts_update && !s->parms.modify) {
      s->parms.ts->status |= TIME_EVENT_ENTRY_USED;
      add_time_event (s->parms.ts, s->parms.ts_index);
    }
    /* Write new configuration to flash */
    write_config_to_flash();
    /* In case we are setting the time manually */
    if (need_time_update) {
      need_time_update = 0;
      tp.time.year = year;
      tp.time.month = month;
      tp.time.day = day;
      tp.time.hrs = hrs;
      tp.time.min = min;
      tp.time.sec = 0;
      dat_to_binary(&tp);
      set_g_time(&tp);
      /* Signal the time client to update the hw rtc */
      RTC_SET_HW_RTC = &tp;
    }
    if (need_reset) {
      need_reset = FALSE;
      A_(printf (__AT__ " Performing a planned software reset !\n");)
      RSTSRC |= 0x10; // Force a software reset
    }
  }
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_username)
{
  char *ptr = sys_cfg.username;
  s->i = 8;

  buffer = skip_to_char(buffer, '=');
  while (NEOP(*buffer) && (s->i-- >= 0)) {
    *ptr++ = *buffer++;
  }
  *ptr = 0x00;
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_password)
{
  char *ptr = sys_cfg.password;
  s->i = 8;

  buffer = skip_to_char(buffer, '=');
  while (NEOP(*buffer) && (s->i-- >= 0)) {
    *ptr++ = *buffer++;
  }
  *ptr = 0x00;
}

/*---------------------------------------------------------------------------*/
PARAM_FUNC (cgi_set_achannel)
{
  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    s->parms.achannel = atoi(buffer);
    s->parms.achannel_updated = 1;
    s->parms.num_parms++;
  }
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (cgi_set_channel)
{
  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    s->parms.channel = atoi(buffer);
    s->parms.channel_updated = 1;
    s->parms.num_parms++;
  }
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (cgi_set_level)
{
  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    s->parms.level = atoi(buffer);
    s->parms.level_updated = 1;
    s->parms.num_parms++;
  }
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (cgi_set_rate)
{
  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    s->parms.rate = atoi(buffer);
    s->parms.rate_updated = 1;
    s->parms.num_parms++;
  }
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (cgi_set_step)
{
  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    s->parms.step = atoi(buffer);
    s->parms.step_updated = 1;
    s->parms.num_parms++;
  }
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (set_rmode)
{
  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer)) {
    s->parms.ramp_mode = atoi(buffer);
    s->parms.ramp_mode_updated = 1;
    s->parms.num_parms++;
  }
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (cgi_set_timeon)
{
  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer))
    s->parms.timeon = atoi(buffer);
}
/*---------------------------------------------------------------------------*/
PARAM_FUNC (cgi_set_onoff)
{
  buffer = skip_to_char(buffer, '=');
  if (NEOP(*buffer))
    s->parms.onoff = atoi(buffer);
}

/*---------------------------------------------------------------------------*/
static u8_t parse_expr(struct httpd_state *s, char *buf)
{
  struct parameter_table *tptr = (struct parameter_table *)parmtab;

  while (*tptr->param != '*')
  {
    if ((buf[strlen(tptr->param)] == '=') &&
       (strncmp(buf, tptr->param, strlen(tptr->param)) == 0))
    {
      /* Call the parameter set function */
      tptr->function(s, buf);
      return 1;
    }
    tptr++;
  }
  return 0;
}/*---------------------------------------------------------------------------*/
void parse_input(struct httpd_state *s, char *buf) __banked
{
  static char token[128];
  char *tok;

  /* Search for query or end of line */
  while (*buf != ISO_query &&
         *buf != ISO_nl &&
         *buf != ISO_space)
    buf++;

  /* Clear the cgi control parameter structure */

  /* Return if no query is present */
  if (*buf == ISO_nl || *buf == ISO_space)
    return;
  memset (&s->parms, 0, sizeof s->parms);

  /* In case these parameters belong to tentry.shtml */
  s->parms.ts = get_first_free_time_event_entry (&s->parms.ts_index);
  if (s->parms.ts) {
    memset (s->parms.ts, 0, sizeof *(s->parms.ts));
    ts_update = FALSE;
  }

  s->parms.rp = rule_find_free_entry();
  A_(printf (__AT__ "Rule pointer is %p\n", s->parms.rp);)
  s->parms.modify = FALSE;

  while (*buf != ISO_space)
  {
    buf++;
    tok = token;
    while (*buf != ISO_space && *buf != ISO_and)
      *tok++ = *buf++;
    *tok = 0;
    /* Here we simply try to parse the tokenized parameter
     * If the parameter does not exist, we simply discard it silently */
    A_(printf(__AT__ " Parsing token %s\n", token);)
    parse_expr(s, token);
  }
}

/* End of File */
