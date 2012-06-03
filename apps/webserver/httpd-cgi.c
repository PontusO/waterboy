/**
* \addtogroup httpd
* @{
*/

/**
* \file
* Web server script interface
* \author
* Adam Dunkels <adam@sics.se>
*
*/

/*
* Copyright (c) 2001-2006, Adam Dunkels.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* 3. The name of the author may not be used to endorse or promote
* products derived from this software without specific prior
* written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
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
* $Id: httpd-cgi.c,v 1.2 2006/06/11 21:46:37 adam Exp $
*
*/
//#define PRINT_A

#include "uip.h"
#include "psock.h"
#include "httpd.h"
#include "httpd-cgi.h"
#include "cgi_utils.h"
#include "httpd-param.h"
#include "httpd-fs.h"
#include "flash.h"
#include "rtc.h"
#include "i2c.h"
#include "iet_debug.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Time CGI's */
HTTPD_CGI_CALL(L_get_current_time_date, "get-timdat", get_current_time_date);
HTTPD_CGI_CALL(L_get_tz_options, "get-tz-opt", get_tz_options);
/* Generic data type CGI's */
HTTPD_CGI_CALL(f_get_ip_num, "get-ip-num", get_ip_num);
HTTPD_CGI_CALL(f_get_check_box, "get-check", get_check_box);
HTTPD_CGI_CALL(f_get_string, "get-string", get_string);
HTTPD_CGI_CALL(f_get_int, "get-int", get_int);
HTTPD_CGI_CALL(f_get_option, "get-option", get_option);

HTTPD_CGI_CALL(f_get_time_events, "te-get-time-events", get_time_events);
HTTPD_CGI_CALL(f_get_tsday, "get-tsday", get_tsday);

HTTPD_CGI_CALL(f_map_get_events, "map-get-events", map_get_events);

HTTPD_CGI_CALL(f_get_evntfuncs, "get-evntfuncs", get_evntfuncs);

static const struct httpd_cgi_call *calls[] = {
  &f_get_ip_num,
  &f_get_check_box,
  &f_get_string,
  &f_get_int,
  &f_get_option,
  &L_get_current_time_date,
  &L_get_tz_options,
  &f_get_time_events,
  &f_get_tsday,
  &f_map_get_events,
  &f_get_evntfuncs,
  NULL
};

static const char ip_format[] = "%d.%d.%d.%d";
static const char error_string[] = "<ERROR ";
static const char str_selected[] = "selected";

/*---------------------------------------------------------------------------*/
static
PT_THREAD(nullfunction(struct httpd_state *s, char *ptr) __reentrant)
{
  IDENTIFIER_NOT_USED(ptr);

  PSOCK_BEGIN(&s->sout);

  sprintf((char*)uip_appdata, "Invalid CGI encountered !");
  PSOCK_SEND_STR(&s->sout, uip_appdata);

  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
httpd_cgifunction
httpd_cgi(char *name) __reentrant
{
  const struct httpd_cgi_call **f;

  /* Find the matching name in the table, return the function. */
  for (f = calls; *f != NULL; ++f) {
    if (strncmp((*f)->name, name, strlen((*f)->name)) == 0) {
      return (*f)->function;
    }
  }
  return nullfunction;
}

/* ****************************************************************************
* CGI function template
*/
#if 0
/*---------------------------------------------------------------------------*/
static
PT_THREAD(get_rtc(struct httpd_state *s, char *ptr) __reentrant)
{
  PSOCK_BEGIN(&s->sout);
  IDENTIFIER_NOT_USED(ptr);

  /* Do stuff here */

  PSOCK_END(&s->sout);
}
#endif
/* ***************************************************************************/

/*---------------------------------------------------------------------------*/
/*
* Create a list of the currently configured time events in the system
* The output is modeled after this static html code.
* <tr>
* <td><input type="checkbox" name="cb1"></td>
* <td>Evening</td>
* <td>Time 21:40</td>
* <td>Weekdays: Mon-Tue-Wed-Thu-Fri</td>
* </tr>
*
*/
static const char *weekdays[] =
{"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
static u8_t order[] = {0x20,0x10,0x08,0x04,0x02,0x02,0x40};
#pragma save
#pragma nogcse
static
PT_THREAD(get_time_events(struct httpd_state *s, char *ptr) __reentrant)
{
  IDENTIFIER_NOT_USED(ptr);

  PSOCK_BEGIN(&s->sout);

  for(s->i=0; s->i < NMBR_TIME_EVENTS; ++s->i) {
    s->ptr = &sys_cfg.time_events[s->i];
    if (((time_spec_t *)s->ptr)->status & TIME_EVENT_ENTRY_USED) {
      u8_t j;
      s->j = sprintf((char *)uip_appdata,
                     "<tr><td><input type=\"checkbox\" name=\"cb%d\"></td>", s->i);
      s->j += sprintf((char *)uip_appdata+s->j, "<td>%s</td><td>%s</td>",
                      (((time_spec_t *)s->ptr)->status & TIME_EVENT_ENABLED) ?
                      "Yes" : "No", ((time_spec_t *)s->ptr)->name);
      s->j += sprintf((char *)uip_appdata+s->j, "<td>%02d:%02d</td>",
                      ((time_spec_t *)s->ptr)->hrs, ((time_spec_t *)s->ptr)->min);
      s->j += sprintf((char *)uip_appdata+s->j, "<td>Weekdays: ");

      /* Resolve week days */
      for (j=0; j<7; j++) {
        if (((time_spec_t *)s->ptr)->weekday & order[j]) {
          s->j += sprintf((char *)uip_appdata+s->j, "%s ", weekdays[j]);
        }
      }
      PSOCK_SEND_STR(&s->sout, uip_appdata);
    }
  }

  PSOCK_END(&s->sout);
}
#pragma restore

/*---------------------------------------------------------------------------*/
static
PT_THREAD(get_tsday(struct httpd_state *s, char *ptr) __reentrant)
{
  u8_t check_box, day;

  PSOCK_BEGIN(&s->sout);

  if (s->parms.ts && s->parms.modify) {
    while (*ptr != ' ')
      ptr++;
    ptr++;
    check_box = atoi(ptr)-1;
    day = 1 << check_box;

    if (day & s->parms.ts->weekday) {
      sprintf ((char*)uip_appdata, "checked");
      PSOCK_SEND_STR(&s->sout, uip_appdata);
    }
  }

  PSOCK_END(&s->sout);
}

/*---------------------------------------------------------------------------*/
#pragma save
#pragma nogcse
static
PT_THREAD(map_get_events(struct httpd_state *s, char *ptr) __reentrant)
{
  IDENTIFIER_NOT_USED(ptr);

  PSOCK_BEGIN(&s->sout);

  if (rule_iter_create(&s->parms.iter)) {
    A_(printf (__AT__ " Error when creating iterator !\n");)
  } else {
    s->ptr = rule_iter_get_first_entry(&s->parms.iter);
    s->i = 0;
    while (s->ptr) {
      s->ep = ((rule_t *)s->ptr)->event;
      s->am = ((rule_t *)s->ptr)->action;

      s->j = sprintf((char *)uip_appdata,
                      "<tr><td><input type=\"checkbox\" name=\"cb%d\"></td><td>%s</td>",
                      s->i, (((rule_t *)s->ptr)->status == RULE_STATUS_ENABLED) ?
                      "Yes" : "No");
      s->j += sprintf((char *)uip_appdata+s->j, "<td>%s</td>",
                      s->ep->event_name);
      s->j += sprintf((char *)uip_appdata+s->j,
                      "<td>%s</td><td>%d</td><td>None</td></td></tr>",
                      s->am->action_name,
                      ((rule_t *)s->ptr)->action_data.abs_data.channel);
      PSOCK_SEND_STR(&s->sout, uip_appdata);
      s->ptr = rule_iter_get_next_entry(&s->parms.iter);
      s->i++;
    }
  }

  PSOCK_END(&s->sout);
}
#pragma restore

/*---------------------------------------------------------------------------*/
#pragma save
#pragma nogcse
static
PT_THREAD(get_evntfuncs(struct httpd_state *s, char *ptr) __reentrant)
{
  PSOCK_BEGIN(&s->sout);

  while (*ptr != ' ')
    ptr++;
  ptr++;
  s->parms.iter.type = !atoi(ptr) ? EVENT_EVENT_PROVIDER : EVENT_ACTION_MANAGER;

  if (evnt_iter_create(&s->parms.iter)) {
    A_(printf (__AT__ " Error when creating iterator !\n");)
  } else {
    s->ptr = evnt_iter_get_first_entry(&s->parms.iter);
    while (s->ptr) {
      switch (GET_EVENT_BASE(s->ptr).type)
      {
        case EVENT_EVENT_PROVIDER:
          s->num = (int)s->ptr;
          s->num |= (unsigned long)(((event_prv_t *)s->ptr)->type) << 16;
          s->j = sprintf (uip_appdata, "<option value=\"%ld\" ", s->num);
          s->j += sprintf ((char *)uip_appdata+s->j, "%s>",
                           s->parms.modify && s->parms.rp->event == s->ptr ?
                           str_selected : "");
          s->j += sprintf ((char *)uip_appdata+s->j, "(%s) ",
                           GET_EVENT_BASE(s->ptr).name);
          sprintf ((char *)uip_appdata+s->j, "%s</option>",
                   GET_EVENT(s->ptr)->event_name);
          break;
        case EVENT_ACTION_MANAGER:
          s->num = (int)s->ptr;
          s->num |= (unsigned long)(((action_mgr_t*)s->ptr)->type) << 16;
          s->j = sprintf ((char *)uip_appdata, "<option value=\"%ld\" ", s->num);
          s->j += sprintf ((char *)uip_appdata+s->j, "%s>",
                           s->parms.modify && s->parms.rp->action == s->ptr ?
                           str_selected : "");
          s->j += sprintf ((char *)uip_appdata+s->j, "(%s) ",
                           GET_EVENT_BASE(s->ptr).name);
          sprintf ((char *)uip_appdata+s->j, "%s</option>",
                   GET_ACTION(s->ptr)->action_name);
          break;
        default:
          /* TODO: Implement getting rule from number, if necessery */
          A_(printf (__AT__ " You need to implement new features dude !\n");)
          break;
      }
      PSOCK_SEND_STR(&s->sout, uip_appdata);
      s->ptr = (event_prv_t *)evnt_iter_get_next_entry(&s->parms.iter);
    }
  }

  PSOCK_END(&s->sout);
}
#pragma restore

/*---------------------------------------------------------------------------*/
static
PT_THREAD(get_current_time_date(struct httpd_state *s, char *ptr) __reentrant)
{
  PSOCK_BEGIN(&s->sout);

  while (*ptr != ' ')
    ptr++;
  ptr++;
  s->i = atoi(ptr);

  switch (s->i)
  {
      /* Return the time */
    case 1:
      print_time_formated(uip_appdata);
      break;

    case 2:
      print_date_formated(uip_appdata);
      break;

    case 3:
      print_datetime_formated(uip_appdata);
      break;

    case 4:
      if (s->parms.modify) {
        sprintf ((char*)uip_appdata, "%02d:%02d",
                 s->parms.ts->hrs, s->parms.ts->min);
      } else {
        *(char *)uip_appdata = 0;
      }
      break;
  }
  PSOCK_SEND_STR(&s->sout, uip_appdata);

  PSOCK_END(&s->sout);
}

/*---------------------------------------------------------------------------*/
#pragma save
#pragma nogcse
static
PT_THREAD(get_tz_options(struct httpd_state *s, char *ptr) __reentrant)
{
  PSOCK_BEGIN(&s->sout);
  IDENTIFIER_NOT_USED(ptr);

  for (s->i = -11 ; s->i<12 ; s->i++) {
    s->j = sprintf(uip_appdata, "<option value=\"%d\"", s->i);

    /* Mark the selected option */
    if (s->i == sys_cfg.time_zone) {
      s->j += sprintf((char*)uip_appdata+s->j, " selected>GMT");
    } else {
      s->j += sprintf((char*)uip_appdata+s->j, ">GMT");
    }

    /* GMT has no values */
    if (s->i == 0)
      s->j += sprintf((char*)uip_appdata+s->j, "</option>");
    else if (s->i < 0)
      s->j += sprintf((char*)uip_appdata+s->j, " %d hrs</option>", s->i);
    else
      s->j += sprintf((char*)uip_appdata+s->j, " +%d hrs</option>", s->i);

    PSOCK_SEND_STR (&s->sout, uip_appdata);
  }

  PSOCK_END(&s->sout);
}
#pragma restore

/*---------------------------------------------------------------------------*/
static
PT_THREAD(get_ip_num(struct httpd_state *s, char *ptr) __reentrant)
{
  PSOCK_BEGIN(&s->sout);

  while (*ptr != ' ')
    ptr++;
  ptr++;
  s->i = atoi(ptr);

  switch(s->i)
  {
    case 1:
      /* Host IP */
      sprintf((char *)uip_appdata, ip_format,
              sys_cfg.ip_addr[0], sys_cfg.ip_addr[1],
              sys_cfg.ip_addr[2], sys_cfg.ip_addr[3]);
      break;

    case 2:
      /* Netmask */
      sprintf((char *)uip_appdata, ip_format,
              sys_cfg.netmask[0], sys_cfg.netmask[1],
              sys_cfg.netmask[2], sys_cfg.netmask[3]);
      break;

    case 3:
      /* Deafult router */
      sprintf((char *)uip_appdata, ip_format,
              sys_cfg.gw_addr[0], sys_cfg.gw_addr[1],
              sys_cfg.gw_addr[2], sys_cfg.gw_addr[3]);
      break;

    case 4:
      /* Time server */
      sprintf((char *)uip_appdata, ip_format,
              sys_cfg.time_server[0], sys_cfg.time_server[1],
              sys_cfg.time_server[2], sys_cfg.time_server[3]);
      break;

    case 5:
      /* HTTP Port */
      sprintf((char *)uip_appdata, "%u", (u16_t)sys_cfg.http_port);
      break;

    case 6:
      /* Time server Port */
      sprintf((char *)uip_appdata, "%u", (u16_t)sys_cfg.time_port);
      break;
  }

  PSOCK_SEND_STR(&s->sout, uip_appdata);

  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(get_check_box(struct httpd_state *s, char *ptr) __reentrant)
{
  char check_box, state = FALSE;

  PSOCK_BEGIN(&s->sout);

  while (*ptr != ' ')
    ptr++;
  ptr++;
  check_box = atoi(ptr);

  switch(check_box)
  {
    case 0:
      if (s->parms.ts && s->parms.modify)
        state = s->parms.ts->status & TIME_EVENT_ENABLED;
      break;

    case 1:
      if (s->parms.rp && s->parms.modify)
        state = s->parms.rp->status & RULE_STATUS_ENABLED;
      break;

    case 2:
      if (sys_cfg.enable_time)
        state = TRUE;
      break;

    case 3:
      if (sys_cfg.pir_enabled)
        state = TRUE;
      break;

    case 4:
      if (sys_cfg.in1_inverted)
        state = TRUE;
      break;

    case 5:
      if (sys_cfg.in2_inverted)
        state = TRUE;
      break;

    default:
      state = 0;
      break;
  }

  if (state)
    sprintf((char *)uip_appdata, " checked");
  else
    sprintf((char *)uip_appdata, " ");

  PSOCK_SEND_STR(&s->sout, uip_appdata);
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(get_int(struct httpd_state *s, char *ptr) __reentrant)
{
  char intno;
  unsigned long myint = 0;

  PSOCK_BEGIN(&s->sout);

  while (*ptr != ' ')
    ptr++;
  ptr++;
  intno = atoi(ptr);

  switch(intno)
  {
      /* Update interval for the timeset web page */
    case 1:
      myint = (u16_t)sys_cfg.update_interval;
      intno = 1;
      break;

      /* mapx value on the cmap.html page */
    case 2:
      intno = 0;
      if (s->parms.rp && s->parms.modify) {
        myint = (int)s->parms.rp;
        intno = (char)myint;
      }
      break;

      /* tsx value on tentry.shtml page */
    case 3:
      intno = 0;
      if (s->parms.ts && s->parms.modify) {
        myint = (int)s->parms.ts;
        intno = (char)myint;
      }
      break;

      /* Sensitivity value for the PIR sensor */
    case 4:
      myint = sys_cfg.pir_sensitivity;
      intno = 1;
      break;

      /* Trigger lockout time */
    case 5:
      myint = sys_cfg.pir_lockout;
      intno = 1;
      break;

    /* Get onoff value on cmap.shtml page */
    case 6:
      intno = 1;
      if (s->parms.modify)
        myint = s->parms.rp->action_data.abs_data.onoff;
      else
        myint = 0;
      break;

    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
//      myint = (u16_t)ledlib_get_light_percentage(intno - 10);
      intno = 1;
      break;

      /* Retrieve the timeon value on page cmap.shtml */
    case 20:
      intno = 1;
      if (s->parms.modify)
        myint = s->parms.rp->action_data.abs_data.timeon;
      else
        /* Default value */
        myint = 60;
      break;

  }

  /* intno is used to supress output */
  if (intno) {
    sprintf((char *)uip_appdata, "%ld", myint);
    PSOCK_SEND_STR(&s->sout, uip_appdata);
  }

  PSOCK_END(&s->sout);
}

static const char const *inmodes[] = {
  "Tryckknapp", "Vippknapp"
};
static const char const *txt_onoff[] = {
  "Av", "På"
};
/*---------------------------------------------------------------------------*/
static
PT_THREAD(get_option(struct httpd_state *s, char *ptr) __reentrant)
{
  char optno;

  PSOCK_BEGIN(&s->sout);

  while (*ptr != ' ')
    ptr++;
  ptr++;
  optno = atoi(ptr);

  switch(optno)
  {
    /* Generate options for achannel on cmap.shtml */
    case 1:
    {
      s->j = 0;
      for (s->i=1; s->i<7; s->i++) {
        s->j += sprintf((char*)uip_appdata+s->j, "<option value=\"%d\"%s>%d",
                        s->i, (s->parms.modify &&
                        s->parms.rp->action_data.abs_data.channel == s->i) ?
                        " selected" : "", s->i);
      }
    }
    break;

    /* Generate på/av text for option field */
    case 2:
    {
      s->j = 0;
      for (s->i=0; s->i<2; s->i++) {
        s->j += sprintf((char*)uip_appdata+s->j, "<option value=\"%d\"%s>%s",
                        s->i, (s->parms.modify &&
                        s->parms.rp->action_data.abs_data.onoff == s->i) ?
                        " selected" : "", txt_onoff[s->i]);
      }
    }
    break;


      /* Options for input modes on dig.shtml */
    case 3:
    {
      s->j = 0;
      for (s->i=0; s->i<2; s->i++) {
        s->j += sprintf((char*)uip_appdata+s->j, "<option value=\"%d\"%s>%s",
                        s->i, (sys_cfg.in1_mode == s->i) ? " selected" : "",
                        inmodes[s->i]);
      }
    }
    break;

    case 4:
    {
      s->j = 0;
      for (s->i=0; s->i<2; s->i++) {
        s->j += sprintf((char*)uip_appdata+s->j, "<option value=\"%d\"%s>%s",
                        s->i, (sys_cfg.in2_mode == s->i) ? " selected" : "",
                        inmodes[s->i]);
      }
    }
    break;
  }

  PSOCK_SEND_STR(&s->sout, uip_appdata);

  PSOCK_END(&s->sout);
}

/*---------------------------------------------------------------------------*/
static
PT_THREAD(get_string(struct httpd_state *s, char *ptr) __reentrant)
{
  char *string = NULL;
  util_param_t up;

  PSOCK_BEGIN(&s->sout);

  while (*ptr != ' ')
    ptr++;
  ptr++;

  up.s = s;
  up.buffer = ptr;
  string = parse_string (&up);

  if (string) {
    sprintf((char *)uip_appdata, "%s", string);
    PSOCK_SEND_STR(&s->sout, uip_appdata);
  }

  PSOCK_END(&s->sout);
}

/** @} */
