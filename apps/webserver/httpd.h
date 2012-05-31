/*
 * Copyright (c) 2001-2005, Adam Dunkels.
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
 * $Id: httpd.h,v 1.2 2006/06/11 21:46:38 adam Exp $
 *
 */

#ifndef __HTTPD_H__
#define __HTTPD_H__

#include "psock.h"
#include "httpd-fs.h"
#include "time_event.h"

/*
 * A parameter structure for transfering parameters to xcgi commands
 */
struct cgi_parameters {
  u8_t achannel;
  u8_t achannel_updated;
  u8_t ramp_mode;
  u8_t ramp_mode_updated;
  u8_t channel;
  u8_t channel_updated;
  u16_t level;
  u8_t level_updated;
  u8_t rampto_updated;
  u16_t rate;
  u8_t rate_updated;
  u8_t step;
  u8_t step_updated;
  unsigned int timeon;
  u8_t num_parms;
  time_spec_t *ts;        /* Pointer to a time spec structure */
  u8_t ts_index;          /* Index if time spec structure */
  rule_t *rp;             /* Pointer to a rule */
  unsigned long marklist; /* List of Mark check boxes (up to 16) */
  u8_t modify;            /* Modify flag, indicated the user pressed a modify button */
  evnt_iter_t iter;       /* Iterator instance */
  unsigned long evt;
  unsigned long act;
  u8_t aochan;
  u8_t aolevel;
};
extern struct cgi_parameters cgi_parms_ctrl;

struct httpd_state {
  unsigned char timer;
  struct psock sin, sout;
  struct pt outputpt, scriptpt, utilpt;
  char inputbuf[200];
  char filename[35];
  char state;
  struct httpd_fs_file file;
  int len;
  char *scriptptr;
  int scriptlen;
  unsigned short content_length;
  u8_t is_authorized;
  struct cgi_parameters parms;
  int i, j;         /* Free agents, can be used by cgi's to create loops */
  void *ptr;        /* Free agent, can freely be used by cgi's */
  event_prv_t *ep;  /* Usefull pointers, for use by cgi's */
  action_mgr_t *am;
  unsigned long num;
};

void httpd_init(void) __banked;
void httpd_appcall(void) __banked;

/*
void httpd_log(char *msg);
void httpd_log_file(u16_t *requester, char *file);
*/

#endif /* __HTTPD_H__ */
