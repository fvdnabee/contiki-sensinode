/*
 * Copyright (c) 2012, Institute for Pervasive Computing, ETH Zurich
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
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      CoAP module for observing resources
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#ifndef COAP_OBSERVING_H_
#define COAP_OBSERVING_H_

#include "er-coap-13.h"
#include "er-coap-13-transactions.h"

#ifndef COAP_MAX_OBSERVERS
#define COAP_MAX_OBSERVERS    COAP_MAX_OPEN_TRANSACTIONS-1
#endif /* COAP_MAX_OBSERVERS */

/* Interval in seconds in which NON notifies are changed to CON notifies to check client. */
#define COAP_OBSERVING_REFRESH_INTERVAL  60

#if COAP_MAX_OPEN_TRANSACTIONS<COAP_MAX_OBSERVERS
#warning "COAP_MAX_OPEN_TRANSACTIONS smaller than COAP_MAX_OBSERVERS: cannot handle CON notifications"
#endif

/* Conditional observe */
#define MAX_CONDITIONS 2
typedef enum {
  CONDITION_CANCELLATION = 0,
  CONDITION_TIME_SERIES = 1,
  CONDITION_MIN_RESP_TIME = 2,
  CONDITION_MAX_RESP_TIME = 3,
  CONDITION_STEP = 4,
  CONDITION_ALLVALUES_LESS = 5,
  CONDITION_ALLVALUES_GREATER = 6,
  CONDITION_VALUE_EQUAL = 7,
  CONDITION_VALUE_NOT_EQUAL = 8,
  CONDITION_PERIODIC = 9
} condition_type_t;

typedef enum {
  INTEGER = 0,
  DURATION_S = 1,
  FLOAT = 2
} value_type_t;

typedef enum {
  CON = 0,
  NON = 1
} cond_reliability;

typedef struct {
  uint8_t cond_type;
  uint8_t reliability_flag;
  uint8_t value_type;
  uint8_t value;
} coap_condition_t;

typedef struct coap_observer {
  struct coap_observer *next; /* for LIST */

  const char *url;
  uip_ipaddr_t addr;
  uint16_t port;
  uint8_t token_len;
  uint8_t token[COAP_TOKEN_LEN];
  uint16_t last_mid;
  struct stimer refresh_timer;

  /* Conditional observe */
  coap_condition_t condition; //[MAX_CONDITIONS]; /*Girum: will be used for Multiple observations*/
  uint8_t condition_index;  //Girum: Will be used for Multiple Observation*/
} coap_observer_t;

list_t coap_get_observers(void);

coap_observer_t *coap_add_observer(uip_ipaddr_t *addr, uint16_t port, const uint8_t *token, size_t token_len, const char *url, coap_condition_t condition); // Conditional observe

void coap_remove_observer(coap_observer_t *o);
int coap_remove_observer_by_client(uip_ipaddr_t *addr, uint16_t port);
int coap_remove_observer_by_token(uip_ipaddr_t *addr, uint16_t port, uint8_t *token, size_t token_len);
int coap_remove_observer_by_url(uip_ipaddr_t *addr, uint16_t port, const char *url);
int coap_remove_observer_by_mid(uip_ipaddr_t *addr, uint16_t port, uint16_t mid);

void coap_notify_observers(resource_t *resource, uint16_t obs_counter, void *notification, uint8_t cond_value); // Conditional observe

void coap_observe_handler(resource_t *resource, void *request, void *response);

/* Conditional observe */
int coap_decode_condition(coap_condition_t *cond, uint16_t encoded_cond);
int coap_encode_condition(coap_condition_t *cond, uint16_t *encoded_cond);
int satisfies_condition(coap_observer_t *obs, uint8_t cond_value);
int coap_set_header_condition(void *packet, uint16_t condition);
int coap_get_header_condition(void *packet, uint16_t *condition);

#endif /* COAP_OBSERVING_H_ */
