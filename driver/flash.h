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

#ifndef _FLASH_H_
#define _FLASH_H_

#include "system.h"
#include "time_event.h"
#include "event_switch.h"

void flash_write_config(void);
/* write flash */
void write_config_to_flash(void);

struct sys_config
{
  struct uip_eth_addr mac_addr;   /* MAC address of the node */
  char device_id[9];              /* Device ID, allocate 9 bytes for this */
  u8_t ip_addr[4];                /* IP address of the node */
  u8_t netmask[4];                /* network mask of the node */
  u8_t gw_addr[4];                /* the default gateway of the node */
  u16_t http_port;                /* Port for the webserver */
  u8_t enable_time;               /* 0 = Do not use time server, 1 = Use time server */
  u8_t time_server[4];            /* The systems time server address */
  u16_t time_port;                /* The port of the time server */
  u16_t update_interval;          /* number of hours between rtc updates */
  char time_zone;                 /* The offset from GMT for our system time */
  char username[9];               /* Authentication user name */
  char password[9];               /* Authentication password */
  /* Moist sensors */
  moist_data_t moist_data[4];     /* Control data for the moist sensors */
  /* PIR Sensor */
  u8_t pir_enabled;               /* Indicates if the PIR sensor is enabled */
  u8_t pir_sensitivity;           /* Indicates the sensitivty of the PIR sensor */
  u8_t pir_lockout;               /* Minimum time between triggered events */
  /* Digital input settings */
  u8_t in1_mode;                  /* Select the operation mode of the input pin */
  u8_t in1_inverted;              /* Selects if the pin is inverted or not */
  u8_t in2_mode;
  u8_t in2_inverted;
  /* Time Events */
  u8_t nmbr_time_events;          /* The number of time events in the table */
  time_spec_t time_events[16];    /* Array of time events */
  /* Event switch */
  u8_t nmbr_of_rules;             /* Hold the number of registered rules */
  rule_t rules[MAX_NR_RULES];     /* Array of rules */
};

#define CONFIG_MEM_SIZE ((u16_t)sizeof(struct sys_config))

#define LAST_PAGE_ADDRESS 0xFC00

extern struct sys_config sys_cfg;
extern const struct sys_config default_cfg;

/* load the system config from the flash when the system is powered on. */
void load_sys_config(void);
void load_default_config(void);
void load_network_params(void);
u8_t validate_config_flash(void);
void sys_getethaddr(struct uip_eth_addr *addr);
void erase_config_area(u8_t *erase_ptr, u8_t bank) __reentrant;
void write_to_flash(u8_t *flash_ptr, u8_t *src_ptr,
                    u16_t len, u8_t bank) __reentrant;

#endif
