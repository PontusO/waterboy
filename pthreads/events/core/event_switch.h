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

/**
 *********  General *********
 *
 * This module handles the three defined type of instances in the
 * event system. The types are:
 *  - EVENT_EVENT_PROVIDER, Which is the part of the system that
 *      generates the actual event.
 *  - EVENT_ACTION_MANAGER, Is the manager that performs an action.
 *  - EVENT_RULE, Is the handle that connects events to action managers.
 *
 * Event providers and action managers are static parts of the system
 * and are registered with the event system at system startup. Typically
 * done by the pthread before starting the main loop.
 * Rules are the dynamic part. The table that holds the rules is located
 * in flash memory so that changes to to it is always remembered.
 *
 *********  Event Provider *********
 *
 * Event providers are used to generate a specific event. For instance
 * an event provider could be made to listen to a specific sequence of
 * characters on a serial line. And when detected it can send the event
 * to the event system. The event switch will match the incoming event
 * to all registered rules in the system and if it founds a match it will
 * call the trigger function of the action manager.
 *
 *
 ********* Action Managers *********
 *
 * Action managers reacts to triggers sent from the event switch and
 * then performs a specific action. For instance in a larm system
 * it can turn on a sirene and/or possibly call an alarm number to
 * inform the police that a burglery is ongoing etc.
 *
 *
 *********  Rules (Routes) *********
 *
 * Rules, some times refered to as routes, define what shall happen when
 * an event provider has sent an event to the event switch. It basically
 * holds information on what action manager that should be called when
 * a specified event sends an event. For instance if an instance registers
 * a rule with event provider 0 and action manager 1, every time that
 * event provider 0 sends an event to the event switch it will send a
 * trigger event to the action manager that can proceed to do its
 * specific task
 *
 * In order to parameterize the action managers, every rule also hold
 * a pointer to a data area which holds information on how the action
 * manager should operate. This pointer is passed on to the action
 * manager, to provide it with the data. This pointer could be set to
 * point to predefined data in flash memory or to data defined in
 * ram making it more dynamic. Predefined const data obviuosly does
 * not need to be configure but data in ram does. To do that the event
 * switch can be instructed to call (Callback) the event provider to
 * fill the data structure with the appropriate data.
 *
 */

#ifndef EVENT_SWITCH_H_INCLUDED
#define EVENT_SWITCH_H_INCLUDED

#include "moist_event.h"
#include "absval_mgr.h"

#define MAX_NR_ACTION_MGRS      5
#define MAX_NR_EVENT_PROVIDERS  20
#define MAX_NR_RULES            32
#define RULE_NAME_LENGTH        8

/**
 * Usefull macros for accessing the event system data structures
 */
#define GET_EVENT_BASE(x) ((event_virtual_t *)x)->base
#define GET_ACTION(x)     ((action_mgr_t *)x)
#define GET_EVENT(x)      ((event_prv_t *)x)
#define GET_RULE_DATA(x)  (rule_lookup_from_event(x)->r_data)

/* Structure prototypes */
struct rule;

/* Event switch types */
typedef enum {
  EVENT_EVENT_PROVIDER = 1,
  EVENT_ACTION_MANAGER,
  EVENT_RULE
} event_type_t;

/**
 * This enum represents all available event types in the system.
 * When a new event is introduced this enum need to be updated
 */
typedef enum {
  ETYPE_POTENTIOMETER_EVENT = 0x01,
  ETYPE_TIME_EVENT,
  ETYPE_PIR_SENSOR_INPUT_EVENT,
  ETYPE_DIG_INPUT_EVENT,
  ETYPE_TABLE_END_MARKER_EVENT,
} event_event_t;

/** Event base data structure
 * All event providers, action managers and rules inherit
 * these properties.
 */
typedef struct {
  event_type_t type;    /** Defines the type */
  char *name;           /** Name of the event/action/rule */
  char enabled;         /** Is it enabled ? */
} event_base_t;

/**
 * A virtual type used for accessing the base structure of any
 * of the event type structures.
 */
typedef struct {
  event_base_t base;
} event_virtual_t;

/**
 * This enum represents all available action types in the system.
 * When a new action is introduced this enum need to be updated
 */
typedef enum {
  ATYPE_ABSOLUTE_ACTION = 0x01,
  ATYPE_RAMP_ACTION = 0x02, /* Has been removed from the system */
  ATYPE_CYCLE_ACTION = 0x03,
} event_action_t;

/**
 * Every action manager need to implement these function.
 * A stateless manager can simply return without any action from the
 * stop method.
 */
typedef struct {
  void (*trigger_action)(struct rule *rule) __reentrant;
} action_vt_t;

/**
 * The action manager handle.
 * A manager need to fill out these fields before registering with
 * the event switch.
 */
typedef struct {
  event_base_t base;
  char *action_name;
  event_action_t type;
  action_vt_t vt;
} action_mgr_t;

/**
 * A vector table for event provider call backs.
 */
typedef struct {
  void (*init_event)(struct rule *rule) __reentrant; /** Only taken if !zero */
} event_vt_t;

/**
 * The event provider handle.
 * All event providers need to create its own instance of this data
 * structure and fill out the fields before registering the handle
 * with the event switch.
 */
typedef struct {
  event_base_t base;  /** Base structure */
  char *event_name;   /** The name of the particular event */
  event_event_t type; /** The particular event trype */
  event_vt_t vt;      /** The callback table */
} event_prv_t;

typedef enum {
  RULE_STATUS_FREE = 0x00,
  RULE_STATUS_DISABLED = 0x01,
  RULE_STATUS_ENABLED = 0x02
} rule_status_t;

/**
 * This union holds a collection of all event provider data types
 * available in the system.
 */
union rule_event_data {
  adc_input_data_t adc_data;
} rule_event_data_t;

/**
 * This union holds a collection of all action manager data types
 * available in the system.
 */
union rule_action_data {
  act_absolute_data_t abs_data;
};

/**
 * Control values to determine how action data shall be interpreted
 * This command must be sent in the command parameter in rule_data_t
 */
typedef enum {
  EVENT_USE_FIXED_DATA = 0x00,
  EVENT_USE_DYNAMIC_DATA,
  EVENT_USE_CONTINUE,
} event_data_control_t;

/**
 * The rule structure table is stored in flash so the rule struct
 * must not have any volatile data in it. This data is instead stored
 * in this special data structure.
 */
struct rule_data_s {
  char event_signal;    /** The trigger signal */
  u8_t command;         /** A command that can be forwarded to the mgr */
  u16_t adata;          /** A data parameter that can be forwarded to the mgr */
};
typedef struct rule_data_s  rule_data_t;

/**
 * The rule handle.
 */
struct rule {
  rule_status_t status;
  event_prv_t *event;                 /** Pointer to connected event */
  action_mgr_t *action;               /** Pointer to action manager */
  rule_data_t *r_data;                /** Pointer to the volatile data struct */
  unsigned int scenario;              /** Even/Action combination */
  union rule_event_data event_data;   /** Data instance for event providers*/
  union rule_action_data action_data; /** Data instance for action managers*/
};
typedef struct rule rule_t;

/**
 * Iterator type structure
 */
typedef struct {
  event_type_t type;
  u8_t cur;           /** Current entry pointer */
  u8_t num;           /** Total number of entries */
  char **tptr;        /** Table pointer */
} evnt_iter_t;

/* Protothread structure */
typedef struct {
  struct pt pt;
  u8_t i;
  event_prv_t *current_event;
  action_mgr_t *new_action;
} event_thread_t;

enum error_codes_t {
  EVENT_ERR_OK = 0x00,
  EVENT_ERR_COULD_NOT_REGISTER_HANDLE,
};

/*
 * Action manager base type names
 */
const char const *action_base_name_onoff = "På/Av funktion";

char evnt_register_handle(void *handler) __reentrant;
void init_event_switch(event_thread_t *et);
char evnt_iter_create (evnt_iter_t *iter) __reentrant __banked;
void *evnt_iter_get_first_entry(evnt_iter_t *iter) __reentrant __banked;
void *evnt_iter_get_next_entry(evnt_iter_t *iter) __reentrant __banked;
action_mgr_t *rule_get_action_from_event (event_prv_t *ep);
rule_t *query_events(void);
PT_THREAD(handle_event_switch(event_thread_t *et) __reentrant);

void rule_setup_v_data_pointers (u8_t clear)  __reentrant __banked;
void rule_send_event_signal (event_prv_t *ep)  __reentrant __banked;
union rule_action_data *rule_get_action_dptr (event_prv_t *ep) __reentrant;
rule_t *rule_lookup_from_event (event_prv_t *ep);
rule_t *rule_find_free_entry(void) __reentrant __banked;
char rule_iter_create (evnt_iter_t *iter) __reentrant __banked;
rule_t *rule_iter_get_first_entry(evnt_iter_t *iter) __reentrant __banked;
rule_t *rule_iter_get_next_entry(evnt_iter_t *iter) __reentrant __banked;
void clear_all_rules(void) __reentrant __banked;
#endif // EVENT_SWITCH_H_INCLUDED
