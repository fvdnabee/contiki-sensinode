/*
 * Copyright (c) 2011, Institute for Pervasive Computing, ETH Zurich
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

#include <stdio.h>
#include <string.h>

#include "er-coap-13-observing.h"

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif


MEMB(observers_memb, coap_observer_t, COAP_MAX_OBSERVERS);
LIST(observers_list);

/*-----------------------------------------------------------------------------------*/
coap_observer_t *
coap_add_observer(uip_ipaddr_t *addr, uint16_t port, const uint8_t *token, size_t token_len, const char *url, coap_condition_t condition) // Conditional observe
{
  /* Remove existing observe relationship, if any. */
  coap_remove_observer_by_url(addr, port, url);

  coap_observer_t *o = memb_alloc(&observers_memb);

  if (o)
  {
    o->url = url;
    uip_ipaddr_copy(&o->addr, addr);
    o->port = port;
    o->token_len = token_len;
    memcpy(o->token, token, token_len);
    o->last_mid = 0;

    stimer_set(&o->refresh_timer, COAP_OBSERVING_REFRESH_INTERVAL);

    /* Conditional observe */
    o->condition.cond_type = condition.cond_type;
    o->condition.reliability_flag = condition.reliability_flag;
    o->condition.value_type = condition.value_type;
    o->condition.value = condition.value;

    PRINTF("Adding observer for /%s [0x%02X%02X] cond=%d\n", o->url, o->token[0], o->token[1], o->condition.value);
    list_add(observers_list, o);
  }

  return o;
}
/*-----------------------------------------------------------------------------------*/
void
coap_remove_observer(coap_observer_t *o)
{
  PRINTF("Removing observer for /%s [0x%02X%02X]\n", o->url, o->token[0], o->token[1]);

  memb_free(&observers_memb, o);
  list_remove(observers_list, o);
}

int
coap_remove_observer_by_client(uip_ipaddr_t *addr, uint16_t port)
{
  int removed = 0;
  coap_observer_t* obs = NULL;

  for (obs = (coap_observer_t*)list_head(observers_list); obs; obs = obs->next)
  {
    PRINTF("Remove check client ");
    PRINT6ADDR(addr);
    PRINTF(":%u\n", port);
    if (uip_ipaddr_cmp(&obs->addr, addr) && obs->port==port)
    {
      coap_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}

int
coap_remove_observer_by_token(uip_ipaddr_t *addr, uint16_t port, uint8_t *token, size_t token_len)
{
  int removed = 0;
  coap_observer_t* obs = NULL;

  for (obs = (coap_observer_t*)list_head(observers_list); obs; obs = obs->next)
  {
    PRINTF("Remove check Token 0x%02X%02X\n", token[0], token[1]);
    if (uip_ipaddr_cmp(&obs->addr, addr) && obs->port==port && obs->token_len==token_len && memcmp(obs->token, token, token_len)==0)
    {
      coap_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}

int
coap_remove_observer_by_url(uip_ipaddr_t *addr, uint16_t port, const char *url)
{
  int removed = 0;
  coap_observer_t* obs = NULL;

  for (obs = (coap_observer_t*)list_head(observers_list); obs; obs = obs->next)
  {
    PRINTF("Remove check URL %p\n", url);
    if ((addr==NULL || (uip_ipaddr_cmp(&obs->addr, addr) && obs->port==port)) && (obs->url==url || memcmp(obs->url, url, strlen(obs->url))==0))
    {
      coap_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}

int
coap_remove_observer_by_mid(uip_ipaddr_t *addr, uint16_t port, uint16_t mid)
{
  int removed = 0;
  coap_observer_t* obs = NULL;

  for (obs = (coap_observer_t*)list_head(observers_list); obs; obs = obs->next)
  {
    PRINTF("Remove check MID %u\n", mid);
    if (uip_ipaddr_cmp(&obs->addr, addr) && obs->port==port && obs->last_mid==mid)
    {
      coap_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}
/*-----------------------------------------------------------------------------------*/
void
coap_notify_observers(resource_t *resource, uint16_t obs_counter, void *notification, uint8_t cond_value) // Conditional observe
{
  coap_packet_t *const coap_res = (coap_packet_t *) notification;
  coap_observer_t* obs = NULL;
  uint8_t preferred_type = coap_res->type;

  static uint16_t last_value = 0; // Conditional observe

  PRINTF("Observing: Notification from %s\n", resource->url);

  /* Iterate over observers. */
  for (obs = (coap_observer_t*)list_head(observers_list); obs; obs = obs->next)
  {
    if (obs->url==resource->url) /* using RESOURCE url pointer as handle */
    {
      coap_transaction_t *transaction = NULL;

      /* Conditional observe */
      if (!satisfies_condition(obs, cond_value)) {
        last_value = cond_value;
        PRINTF("Conditional observe: new value doesn't satisfy condition, skipping this observer");
        continue;
      }
      PRINTF("Obs Cond =%u serv val = %u\n", obs->condition.value, cond_value);

      /*TODO implement special transaction for CON, sharing the same buffer to allow for more observers. */

      if ( (transaction = coap_new_transaction(coap_get_mid(), &obs->addr, obs->port)) )
      {
        PRINTF("           Observer ");
        PRINT6ADDR(&obs->addr);
        PRINTF(":%u\n", obs->port);

        /* Update last MID for RST matching. */
        obs->last_mid = transaction->mid;

        /* Prepare response */
        coap_res->mid = transaction->mid;
        if (obs_counter>=0) coap_set_header_observe(coap_res, obs_counter);
	/* Conditional observe: if observer has a condition set, then add it to the coap header */
	if (obs->condition.value != 0) { // FIXME: if Condition option isn't set, &obs->condition should be NULL?
	  coap_set_header_condition(coap_res, cond_value);
	}
        coap_set_header_token(coap_res, obs->token, obs->token_len);

        /* Use CON to check whether client is still there/interested after COAP_OBSERVING_REFRESH_INTERVAL. */
        if (stimer_expired(&obs->refresh_timer))
        {
          PRINTF("           Refreshing with CON\n");
          coap_res->type = COAP_TYPE_CON;
          stimer_restart(&obs->refresh_timer);
        }
        else
        {
          coap_res->type = preferred_type;
        }

        transaction->packet_len = coap_serialize_message(coap_res, transaction->packet);

        coap_send_transaction(transaction);
      }
    }
  }
}
/*-----------------------------------------------------------------------------------*/
void
coap_observe_handler(resource_t *resource, void *request, void *response)
{
  coap_packet_t *const coap_req = (coap_packet_t *) request;
  coap_packet_t *const coap_res = (coap_packet_t *) response;

  static char content[16];
  coap_condition_t cond; // Conditional Observe

  if (coap_req->code==COAP_GET && coap_res->code<128) /* GET request and response without error code */
  {
    if (IS_OPTION(coap_req, COAP_OPTION_OBSERVE))
    {
      if (coap_decode_condition(&cond, coap_req->condition)) // Conditional observe
      {
        if (coap_add_observer(&UIP_IP_BUF->srcipaddr, UIP_UDP_BUF->srcport, coap_req->token, coap_req->token_len, resource->url, cond))
        {
          coap_set_header_observe(coap_res, 0);
          /*
          * For demonstration purposes only. A subscription should return the same representation as a normal GET.
          * TODO: Comment the following line for any real application.
          */
          coap_set_payload(coap_res, content, snprintf(content, sizeof(content), "Added %u/%u", list_length(observers_list), COAP_MAX_OBSERVERS));
        }
        else
        {
          coap_res->code = SERVICE_UNAVAILABLE_5_03;
          coap_set_payload(coap_res, "TooManyObservers", 16);
        } /* if (added observer) */
      }
    }
    else /* if (observe) */
    {
      /* Remove client if it is currently observing. */
      coap_remove_observer_by_url(&UIP_IP_BUF->srcipaddr, UIP_UDP_BUF->srcport, resource->url);
    } /* if (observe) */
  }
}

/*------------------------------------------------------------------------------------*/
/* Conditional Observe functions */
int
coap_encode_condition(coap_condition_t *cond, uint16_t *encoded_cond)
{
  uint16_t temp = 0;

  temp = (uint16_t) (cond->cond_type) << 11;
  temp = temp | ((uint32_t)(cond->reliability_flag)) << 10;
  temp = temp | ((uint32_t)(cond->value_type)) << 8;
  temp = temp | ((uint32_t)(cond->value));

  *encoded_cond = temp;

  return 1;
}

int
coap_decode_condition(coap_condition_t *cond, uint16_t encoded_cond)
{
  uint16_t temp = encoded_cond;

  cond->cond_type = temp >> 11;
  temp = temp << 5;
  cond->reliability_flag = temp >> 15;
  temp = temp << 1;
  cond->value_type = temp >> 14;
  temp = temp << 2;
  cond->value = temp>>8;

  PRINTF("Encoded %u ++++ Cond Type: %u Reli: %u Val_T: %u, Val: %u\n", encoded_cond, cond->cond_type, cond->reliability_flag, cond->value_type, cond->value);

  return 1;
}
/*-----------------------------------------------------------------------------------*/
int
satisfies_condition(coap_observer_t *obs, uint8_t cond_value)
{
  if (obs->condition.cond_type == CONDITION_TIME_SERIES) {
  }
  else if (obs->condition.cond_type == CONDITION_MIN_RESP_TIME) {
  }
  else if (obs->condition.cond_type == CONDITION_MAX_RESP_TIME) {
  }
  else if (obs->condition.cond_type == CONDITION_STEP) {
  }
  else if (obs->condition.cond_type == CONDITION_ALLVALUES_LESS) {
    if (obs->condition.value > cond_value) return 1;
    return 0;
  }
  else if (obs->condition.cond_type == CONDITION_ALLVALUES_GREATER) {
  //PRINTF("here: obs =%u val = %u\n", obs->condition.value, cond_value);
    if (obs->condition.value < cond_value) return 1;
    return 0;
  }
  else if (obs->condition.cond_type == CONDITION_VALUE_EQUAL) {
    if (obs->condition.value == cond_value) return 1;
    return 0;
  }
  else if (obs->condition.cond_type == CONDITION_VALUE_NOT_EQUAL) {
    if (obs->condition.value != cond_value) return 1;
    return 0;
  }
  else if (obs->condition.cond_type == CONDITION_PERIODIC) {
  }
}
//---------------------------------------------------------------------------------------------------------------
int
coap_set_header_condition(void *packet, uint16_t condition)
{
  coap_packet_t *const coap_pkt = (coap_packet_t *) packet;

  coap_pkt->condition = condition;
  SET_OPTION(coap_pkt, COAP_OPTION_CONDITION);
  return 1;
}

int
coap_get_header_condition(void *packet, uint16_t *condition)
{
  coap_packet_t *const coap_pkt = (coap_packet_t *) packet;

  if (!IS_OPTION(coap_pkt, COAP_OPTION_CONDITION)) return 0;

  *condition = coap_pkt->condition;
  return 1;
}
//---------------------------------------------------------------------------------------------------------------
