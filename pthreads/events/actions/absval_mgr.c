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

/* Function prototypes */
void absval_trigger (struct rule *rule);

/* The action manager handle */
static action_mgr_t  absvalmgr;
static const char *absval_name = "Styr vattenventil";

/*
 * Initialize the absval_mgr
 */
void init_absval_mgr(void) __reentrant __banked
{
  absvalmgr.base.type = EVENT_ACTION_MANAGER;
  absvalmgr.base.name = action_base_name_onoff;
  absvalmgr.type = ATYPE_ABSOLUTE_ACTION;
  absvalmgr.action_name = (char*)absval_name;
  absvalmgr.vt.trigger_action = absval_trigger;

  /* register this non threaded action manager */
  evnt_register_handle(&absvalmgr);
}

/* Set new data */
void absval_trigger (struct rule *rule) __reentrant
{
  act_absolute_data_t *absdata = (act_absolute_data_t *)rule->action_data;
  rule_data_t *rdata = rule->r_data;

  set_onoff (absdata->channel-1, absdata->value);

  A_(printf(__AT__ "Setting: Channel %d, Value %d\n", absdata->channel-1,
          absdata->value);)
}

/* EOF */
