/*
 * Copyright (c) 2012, Matthias Kovatsch and other contributors.
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
 *      Erbium (Er) REST Engine example (with CoAP-specific code)
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"

#include "dev/leds.h"
#include "dev/sht11.h"
#include "dev/i2cmaster.h" 
#include "dev/light-ziglet.h"
#include "dev/z1-phidgets.h"
#include "lib/sensors.h"

#include "dev/cc2420.h"

/* Define which resources to include to meet memory constraints. */
#define REST_RES_HELLO 0
#define REST_RES_MIRROR 0 /* causes largest code size */
#define REST_RES_CHUNKS 0
#define REST_RES_SEPARATE 1
#define REST_RES_PUSHING 0
#define REST_RES_EVENT 1
#define REST_RES_SUB 1
#define REST_RES_LEDS 0
#define REST_RES_TOGGLE 1
#define REST_RES_LIGHT 0
#define REST_RES_BATTERY 0
#define REST_RES_RADIO 0

#define REST_RES_SERVERINFO       1
#define REST_RES_ZIG001           0
#define REST_RES_ZIG002           0
#define REST_RES_PH_TOUCH         0
#define REST_RES_PH_SONAR         0
#define REST_RES_PH_FLEXIFORCE    0
#define REST_RES_PH_MOTION        0
#define REST_RES_RFID		  1


#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET) && !defined (CONTIKI_TARGET_NATIVE)
#warning "Compiling with static routing!"
#include "static-routing.h"
#endif

#include "erbium.h"

#include "sys/energest.h" // Conditional observe


#if defined (PLATFORM_HAS_BUTTON)
#include "dev/button-sensor.h"
#endif
#if defined (PLATFORM_HAS_LEDS)
#include "dev/leds.h"
#endif
#if defined (PLATFORM_HAS_LIGHT)
#include "dev/light-sensor.h"
#endif
#if defined (PLATFORM_HAS_BATTERY)
#include "dev/battery-sensor.h"
#endif
#if defined (PLATFORM_HAS_SHT11)
#include "dev/sht11-sensor.h"
#endif
#if defined (PLATFORM_HAS_RADIO)
#include "dev/radio-sensor.h"
#endif
#if REST_REST_RFID
#include "dev/uart0.h"
#endif

/* For CoAP-specific example: not required for normal RESTful Web service. */
#if WITH_COAP == 3
#include "er-coap-03.h"
#elif WITH_COAP == 7
#include "er-coap-07.h"
#elif WITH_COAP == 12
#include "er-coap-12.h"
#elif WITH_COAP == 13
#include "er-coap-13.h"
#else
#warning "Erbium example without CoAP-specifc functionality"
#endif /* CoAP-specific example */

#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif


PROCESS(rest_server_example, "Erbium Example Server");

#if REST_RES_SERVERINFO
/*
 * Resources are defined by the RESOURCE macro.
 * Signature: resource name, the RESTful methods it handles, and its URI path (omitting the leading slash).
 */

RESOURCE(serverinfo, METHOD_GET, ".well-known/serverInfo","title=\"serverInfo\"");
/*
 * A handler function named [resource name]_handler must be implemented for each RESOURCE.
 * A buffer for the response payload is provided through the buffer pointer. Simple resources can ignore
 * preferred_size and offset, but must respect the REST_MAX_CHUNK_SIZE limit for the buffer.
 * If a smaller block size is requested for CoAP, the REST framework automatically splits the data.
 */
void
serverinfo_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const char *len = NULL;
  uip_ds6_addr_t * addr;
  addr = uip_ds6_get_global(-1); //of uip_ds6_get_global ?
  uint16_t nextPos = 0;
  uint16_t i;
  unsigned char serverInfo[63];
  unsigned char *serverInfoPart1="AL|16|A|";
  unsigned char *serverInfoPart2="|NT|S|N|sensor";
  unsigned char charretje[6];


  for ( i = 0; i < 8; i++)
  {
		serverInfo[nextPos++] = serverInfoPart1[i];
  }
  i=0;
  for ( i = 0 ; i < 16 ; i++ )
  {
    itoa(addr->ipaddr.u8[i],charretje,16);

  if(strlen(charretje)==0){ //
    serverInfo[nextPos++] = '0';
    serverInfo[nextPos++] = '0';
  }else if(strlen(charretje)==1){
    serverInfo[nextPos++] = '0';
    serverInfo[nextPos++] = charretje[0];
  }else{
		serverInfo[nextPos++] = charretje[0];
		serverInfo[nextPos++] = charretje[1];		
	} 
    if((i+1)%2 == 0 && i<15){
      serverInfo[nextPos++] = ':';
    }


  }

  for ( i = 0; i < 14; i++)
  {
	serverInfo[nextPos++] = serverInfoPart2[i];
  }
  
	itoa(addr->ipaddr.u8[15],charretje,16);
  if(strlen(charretje)==0){ //
    serverInfo[nextPos++] = '0';
    serverInfo[nextPos++] = '0';
  }else if(strlen(charretje)==1){
    serverInfo[nextPos++] = '0';
    serverInfo[nextPos++] = charretje[0];
  }else{
		serverInfo[nextPos++] = charretje[0];
		serverInfo[nextPos++] = charretje[1];		
	}

  
  serverInfo[nextPos] = '\0';

  /* Some data that has the length up to REST_MAX_CHUNK_SIZE. For more, see the chunk resource. */
  //serverInfo format: AL|AddressLength|A|Address|NT|NameType|N|Name
  //char const * const message = "AL|25|A|aaaa::0212:7402:0002:0202|NT|S|N|2";
  char const * const message = serverInfo;
  int length = nextPos; /*           |<-------->| */

  /* The query string can be retrieved by rest_get_query() or parsed for its key-value pairs. */
  if (REST.get_query_variable(request, "len", &len)) {
    length = atoi(len);
    if (length<0) length = 0;
    if (length>REST_MAX_CHUNK_SIZE) length = REST_MAX_CHUNK_SIZE;
    memcpy(buffer, message, length);
  } else {
    memcpy(buffer, message, length);
  }

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN); /* text/plain is the default, hence this option could be omitted. */
  REST.set_header_etag(response, (uint8_t *) &length, 1);
  REST.set_response_payload(response, buffer, length);

}
#endif 



#if REST_RES_ZIG001
RESOURCE(zig001, METHOD_GET , "digital/zig001_sht11", "Usage=\"..\"");
void zig001_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const unsigned ERROR_VALUE = 65535; // = 0xFFFF, waarde als er iets misloopt
  static unsigned tmp, rh;
  char message[REST_MAX_CHUNK_SIZE];
  int length;

  sht11_init();

  tmp = sht11_temp();
  rh = sht11_humidity();

  if(tmp == ERROR_VALUE || rh == ERROR_VALUE) {
    length = sprintf(message, "Controleer of de sensor (correct) is aangesloten!");
  } else {
    length = sprintf(message, "Temperatuur: %u°C\nRel. Luchtvochtigheid: %u%%",
                       (unsigned) (-39.60 + 0.01 * tmp), (unsigned) (-4 + 0.0405*rh - 2.8e-6*(rh*rh)));
  }

  if(length>=0) {
    memcpy(buffer, message, length);
  }
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_etag(response, (uint8_t *)&length, 1);
  REST.set_response_payload(response, buffer, length);
}
#endif // REST_RES_LEDS_ZIG001

#if REST_RES_ZIG002
RESOURCE(zig002, METHOD_GET , "digital/zig002_light", "Usage=\"..\"");
void zig002_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  char message[REST_MAX_CHUNK_SIZE];
  int length;

  light_ziglet_init();
  length = sprintf(message, "Lichtsterkte: %u lux\n", light_ziglet_read());

  if(length>=0) {
    memcpy(buffer, message, length);
  }
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_etag(response, (uint8_t *)&length, 1);
  REST.set_response_payload(response, buffer, length);
}
#endif // REST_RES_ZIG002

#if REST_RES_PH_TOUCH
RESOURCE(ph_touch, METHOD_GET , "phidget/touch_sensor", "Usage=\"..\"");
void ph_touch_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const int MAX_UNTOUCHED_VALUE = 50;
  const int MIN_TOUCHED_VALUE = 4000;

  char message[REST_MAX_CHUNK_SIZE];
  int length;

  SENSORS_ACTIVATE(phidgets);
  int phidget5V = phidgets.value(PHIDGET5V_1);
  int phidget3V = phidgets.value(PHIDGET3V_2);

  if( phidget5V < MAX_UNTOUCHED_VALUE || phidget3V < MAX_UNTOUCHED_VALUE ) {
    length = sprintf(message, "Sensor aangesloten, niet aangeraakt");
  } else if( phidget5V > MIN_TOUCHED_VALUE || phidget3V > MIN_TOUCHED_VALUE ) {
    length = sprintf(message, "Sensor aangesloten, wel aangeraakt");
  } else {
    length = sprintf(message, "Geen sensor aangesloten");
  }

  if(length>=0) {
    memcpy(buffer, message, length);
  }
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_etag(response, (uint8_t *)&length, 1);
  REST.set_response_payload(response, buffer, length);
}
#endif // REST_RES_PH_TOUCH

#if REST_RES_PH_SONAR
// conversiefuncties
#define round(x)                  ((x)>=0?(int)((x)+0.5):(int)((x)-0.5))
#define convert_to_cm(x)          round(phidgets.value(PHIDGET3V_2) / (4095 / 1296))

RESOURCE(ph_sonar, METHOD_GET , "phidget/sonar_sensor", "Usage=\"..?ph=3|5\"");
void ph_sonar_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const char *ph = NULL;
  char message[REST_MAX_CHUNK_SIZE];
  int length;
  int success = 1;
  int distance;

  if(REST.get_query_variable(request, "ph", &ph)) {
    int type = atoi(ph);
    if(type == 3 || type == 5) {
      SENSORS_ACTIVATE(phidgets);

      int value;
      if(type == 3) {
        value = phidgets.value(PHIDGET3V_2);
      } else {
        value = phidgets.value(PHIDGET5V_1);
      }
      distance = (int)(value / (4095 / 1296));
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if(success) {
    length = sprintf(message, "Voorwerp bevindt zich op (ongeveer) %d cm", distance);

    if(length>=0) {
      memcpy(buffer, message, length);
    }
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    REST.set_header_etag(response, (uint8_t *)&length, 1);
    REST.set_response_payload(response, buffer, length);
  } else {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
  }
}
#endif // REST_RES_PH_SONAR

#if REST_RES_PH_FLEXIFORCE
// conversiefuncties
#define round(x)                  ((x)>=0?(int)((x)+0.5):(int)((x)-0.5))
#define convert_to_cm(x)          round(phidgets.value(PHIDGET3V_2) / (4095 / 1296))

RESOURCE(ph_flexiforce, METHOD_GET , "phidget/flexiforce_sensor", "Usage=\"..?ph=3|5\"");
void ph_flexiforce_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const int MIN_VALUE = 10;

  const char *ph = NULL;
  char message[REST_MAX_CHUNK_SIZE];
  int length;

  SENSORS_ACTIVATE(phidgets);

  int phidget5V = phidgets.value(PHIDGET5V_1);
  int phidget3V = phidgets.value(PHIDGET3V_2);

  int value;

  REST.get_query_variable(request, "ph", &ph);
  int type = atoi(ph);
  if(type == 3) {
      value = phidgets.value(PHIDGET3V_2);
    } else if(type == 5) {
      value = phidgets.value(PHIDGET5V_1);
    } else {
    // kleinste van de 2 zoeken
    value = (phidget5V < phidget3V ? phidget5V : phidget3V);
  }

  if (value < MIN_VALUE) {
    length = sprintf(message, "Sensor niet ingedrukt");
  } else {
    length = sprintf(message, "Sensor ingedrukt (als aangesloten): %d", phidget5V);
  }

  if(length>=0) {
    memcpy(buffer, message, length);
  }
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_etag(response, (uint8_t *)&length, 1);
  REST.set_response_payload(response, buffer, length);
}
#endif // REST_RES_PH_FLEXIFORCE

#if REST_RES_PH_MOTION
// CODE NOG NIET IN ORDE !!!
#define TIMER_INTERVAL            CLOCK_SECOND / 10

RESOURCE(ph_motion, METHOD_GET , "phidget/motion_sensor", "Usage=\"..\"");
void ph_motion_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  // Calibratie voor mote '055' op netstroom
  const int PH_5V_NO_MOTION_MIN_VALUE = 2010;
  const int PH_5V_NO_MOTION_MAX_VALUE = 2120;
  const int PH_3V_NO_MOTION_MIN_VALUE = 3330;
  const int PH_3V_NO_MOTION_MAX_VALUE = 3530;

  char message[REST_MAX_CHUNK_SIZE];
  int length;

  static struct etimer et;
  static int loops = 20; // 2 seconden testen
  static int consecutive_motions = 0;
  const int MAX_MOTIONS = 13; // maximaal aantal opeenvolgende bewegingsdetecties

  SENSORS_ACTIVATE(phidgets);

  while (loops > 0 && consecutive_motions < MAX_MOTIONS) {
    int i=0;
    while(i<100) {
      i++;
    }

    int phidget5V = phidgets.value(PHIDGET5V_1);
    int phidget3V = phidgets.value(PHIDGET3V_2);

    if( (phidget5V > PH_5V_NO_MOTION_MIN_VALUE && phidget5V < PH_5V_NO_MOTION_MAX_VALUE) ||
        (phidget3V > PH_3V_NO_MOTION_MIN_VALUE && phidget3V < PH_3V_NO_MOTION_MAX_VALUE)  ) {
      consecutive_motions = 0; // resetten
    } else {
      consecutive_motions++;
    }
    loops--;
  }

  if(consecutive_motions < MAX_MOTIONS) {
    length = sprintf(message, "Geen beweging waargenomen");
  } else {
    length = sprintf(message, "Beweging waargenomen");
  }

  if(length>=0) {
    memcpy(buffer, message, length);
  }
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_etag(response, (uint8_t *)&length, 1);
  REST.set_response_payload(response, buffer, length);
}
#endif // REST_RES_PH_MOTION

#if REST_RES_RFID
static process_event_t event_rfid_read; //event that is called when a new rfid tag is read.

unsigned long BAUDRATE = 2400; //parallax RFID reader uses a baud rate of 2400, default baudrate is 115200. This has a huge impact on usb communication.
static char startbyte = 0x0A;
static char stopbyte = 0x0D;
char rfidtag[11]="0000000000\0";
char previous_rfidtag[11]="0000000000\0";
uint8_t reading=0;
//uint8_t rfidtag_changed=0;
//static clock_time_t read_time; //the time a tag is read

int uart_rx_callback(unsigned char c) //is called when RFID tag detects card
{

//leds_toggle(LEDS_ALL);
 size_t len;
// uart0_writeb(c);

 if (reading == 0 && c == startbyte){ //start of rfid tag detected
    reading = 1;
    memset(rfidtag, 0, sizeof(rfidtag)); //clear old rfid
 }else if (reading == 1){
    if(c == stopbyte){ //end of rfid tag reached
      reading = 0;
      //read_time = clock_time();
//      PRINTF("Rfid tag: <%s>\n",rfidtag);

      if(strcmp(previous_rfidtag,rfidtag)){ //previous read tag is different from current tag
       // rfidtag_changed=1;
	process_post(&rest_server_example, event_rfid_read, &rfidtag);
     
	memcpy(previous_rfidtag,rfidtag,sizeof(rfidtag));
      }

    }else{ //read next rfid tag byte
      len = strlen(rfidtag);
      rfidtag[len++] = c;
      rfidtag[len] = '\0';
    }
 }
 return 0;
}

EVENT_RESOURCE(rfid, METHOD_GET , "rfid_reader", "Usage=\"..\";obs");
void rfid_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

  const char *len = NULL;
  /* Some data that has the length up to REST_MAX_CHUNK_SIZE. For more, see the chunk resource. */
  int length = sizeof(rfidtag)-1; // -1 because we don't want to send the null character
  
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN); /* text/plain is the default, hence this option could be omitted. */
  REST.set_header_etag(response, (uint8_t *) &length, 1);
  REST.set_response_payload(response, rfidtag, length);
}
void
rfid_event_handler(resource_t *r)
{
  static uint16_t event_counter = 0;

  ++event_counter;
  /* Build notification. */
  coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
  coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
  coap_set_payload(notification, rfidtag, sizeof(rfidtag)-1);

  /* Notify the registered observers with the given message type, observe option, and payload. */
  REST.notify_subscribers(r, event_counter, notification);

}
#endif // REST_RES_RFID


/******************************************************************************/
#if REST_RES_HELLO
/*
 * Resources are defined by the RESOURCE macro.
 * Signature: resource name, the RESTful methods it handles, and its URI path (omitting the leading slash).
 */
RESOURCE(helloworld, METHOD_GET, "hello", "title=\"Hello world: ?len=0..\";rt=\"Text\"");

/*
 * A handler function named [resource name]_handler must be implemented for each RESOURCE.
 * A buffer for the response payload is provided through the buffer pointer. Simple resources can ignore
 * preferred_size and offset, but must respect the REST_MAX_CHUNK_SIZE limit for the buffer.
 * If a smaller block size is requested for CoAP, the REST framework automatically splits the data.
 */
void
helloworld_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const char *len = NULL;
  /* Some data that has the length up to REST_MAX_CHUNK_SIZE. For more, see the chunk resource. */
  char const * const message = "Hello World! ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxy";
  int length = 12; /*           |<-------->| */

  /* The query string can be retrieved by rest_get_query() or parsed for its key-value pairs. */
  if (REST.get_query_variable(request, "len", &len)) {
    length = atoi(len);
    if (length<0) length = 0;
    if (length>REST_MAX_CHUNK_SIZE) length = REST_MAX_CHUNK_SIZE;
    memcpy(buffer, message, length);
  } else {
    memcpy(buffer, message, length);
  }

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN); /* text/plain is the default, hence this option could be omitted. */
  REST.set_header_etag(response, (uint8_t *) &length, 1);
  REST.set_response_payload(response, buffer, length);
}
#endif

/******************************************************************************/
#if REST_RES_MIRROR
/* This resource mirrors the incoming request. It shows how to access the options and how to set them for the response. */
RESOURCE(mirror, METHOD_GET | METHOD_POST | METHOD_PUT | METHOD_DELETE, "debug/mirror", "title=\"Returns your decoded message\";rt=\"Debug\"");

void
mirror_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* The ETag and Token is copied to the header. */
  uint8_t opaque[] = {0x0A, 0xBC, 0xDE};

  /* Strings are not copied, so use static string buffers or strings in .text memory (char *str = "string in .text";). */
  static char location[] = {'/','f','/','a','?','k','&','e', 0};

  /* Getter for the header option Content-Type. If the option is not set, text/plain is returned by default. */
  unsigned int content_type = REST.get_header_content_type(request);

  /* The other getters copy the value (or string/array pointer) to the given pointers and return 1 for success or the length of strings/arrays. */
  uint32_t max_age_and_size = 0;
  const char *str = NULL;
  uint32_t observe = 0;
  const uint8_t *bytes = NULL;
  uint32_t block_num = 0;
  uint8_t block_more = 0;
  uint16_t block_size = 0;
  const char *query = "";
  int len = 0;

  /* Mirror the received header options in the response payload. Unsupported getters (e.g., rest_get_header_observe() with HTTP) will return 0. */

  int strpos = 0;
  /* snprintf() counts the terminating '\0' to the size parameter.
   * The additional byte is taken care of by allocating REST_MAX_CHUNK_SIZE+1 bytes in the REST framework.
   * Add +1 to fill the complete buffer, as the payload does not need a terminating '\0'. */
  if (content_type!=-1)
  {
    strpos += snprintf((char *)buffer, REST_MAX_CHUNK_SIZE+1, "CT %u\n", content_type);
  }
  
  /* Some getters such as for ETag or Location are omitted, as these options should not appear in a request.
   * Max-Age might appear in HTTP requests or used for special purposes in CoAP. */
  if (strpos<=REST_MAX_CHUNK_SIZE && REST.get_header_max_age(request, &max_age_and_size))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "MA %lu\n", max_age_and_size);
  }
  /* For HTTP this is the Length option, for CoAP it is the Size option. */
  if (strpos<=REST_MAX_CHUNK_SIZE && REST.get_header_length(request, &max_age_and_size))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "SZ %lu\n", max_age_and_size);
  }

  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_header_host(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "UH %.*s\n", len, str);
  }

/* CoAP-specific example: actions not required for normal RESTful Web service. */
#if WITH_COAP > 1
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_observe(request, &observe))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Ob %lu\n", observe);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_token(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "To 0x");
    int index = 0;
    for (index = 0; index<len; ++index) {
        strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%02X", bytes[index]);
    }
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "\n");
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_etag(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "ET 0x");
    int index = 0;
    for (index = 0; index<len; ++index) {
        strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%02X", bytes[index]);
    }
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "\n");
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_uri_path(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "UP ");
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%.*s\n", len, str);
  }
#if WITH_COAP == 3
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_location(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Lo %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_block(request, &block_num, &block_more, &block_size, NULL)) /* This getter allows NULL pointers to get only a subset of the block parameters. */
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Bl %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  }
#else
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_location_path(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "LP %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_location_query(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "LQ %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_block2(request, &block_num, &block_more, &block_size, NULL)) /* This getter allows NULL pointers to get only a subset of the block parameters. */
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "B2 %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  }
  /*
   * Critical Block1 option is currently rejected by engine.
   *
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_block1(request, &block_num, &block_more, &block_size, NULL))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "B1 %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  }
  */
#endif /* CoAP > 03 */
#endif /* CoAP-specific example */

  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_query(request, &query)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Qu %.*s\n", len, query);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_request_payload(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%.*s", len, bytes);
  }

  if (strpos >= REST_MAX_CHUNK_SIZE)
  {
      buffer[REST_MAX_CHUNK_SIZE-1] = 0xBB; /* '»' to indicate truncation */
  }

  REST.set_response_payload(response, buffer, strpos);

  PRINTF("/mirror options received: %s\n", buffer);

  /* Set dummy header options for response. Like getters, some setters are not implemented for HTTP and have no effect. */
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_max_age(response, 17); /* For HTTP, browsers will not re-request the page for 17 seconds. */
  REST.set_header_etag(response, opaque, 2);
  REST.set_header_location(response, location); /* Initial slash is omitted by framework */
  REST.set_header_length(response, strpos); /* For HTTP, browsers will not re-request the page for 10 seconds. CoAP action depends on the client. */

/* CoAP-specific example: actions not required for normal RESTful Web service. */
#if WITH_COAP > 1
  coap_set_header_uri_host(response, "tiki");
  coap_set_header_observe(response, 10);
#if WITH_COAP == 3
  coap_set_header_block(response, 42, 0, 64); /* The block option might be overwritten by the framework when blockwise transfer is requested. */
#else
  coap_set_header_proxy_uri(response, "ftp://x");
  coap_set_header_block2(response, 42, 0, 64); /* The block option might be overwritten by the framework when blockwise transfer is requested. */
  coap_set_header_block1(response, 23, 0, 16);
  coap_set_header_accept(response, TEXT_PLAIN);
  coap_set_header_if_none_match(response);
#endif /* CoAP > 03 */
#endif /* CoAP-specific example */
}
#endif /* REST_RES_MIRROR */

/******************************************************************************/
#if REST_RES_CHUNKS
/*
 * For data larger than REST_MAX_CHUNK_SIZE (e.g., stored in flash) resources must be aware of the buffer limitation
 * and split their responses by themselves. To transfer the complete resource through a TCP stream or CoAP's blockwise transfer,
 * the byte offset where to continue is provided to the handler as int32_t pointer.
 * These chunk-wise resources must set the offset value to its new position or -1 of the end is reached.
 * (The offset for CoAP's blockwise transfer can go up to 2'147'481'600 = ~2047 M for block size 2048 (reduced to 1024 in observe-03.)
 */
RESOURCE(chunks, METHOD_GET, "test/chunks", "title=\"Blockwise demo\";rt=\"Data\"");

#define CHUNKS_TOTAL    2050

void
chunks_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  int32_t strpos = 0;

  /* Check the offset for boundaries of the resource data. */
  if (*offset>=CHUNKS_TOTAL)
  {
    REST.set_response_status(response, REST.status.BAD_OPTION);
    /* A block error message should not exceed the minimum block size (16). */

    const char *error_msg = "BlockOutOfScope";
    REST.set_response_payload(response, error_msg, strlen(error_msg));
    return;
  }

  /* Generate data until reaching CHUNKS_TOTAL. */
  while (strpos<preferred_size)
  {
    strpos += snprintf((char *)buffer+strpos, preferred_size-strpos+1, "|%ld|", *offset);
  }

  /* snprintf() does not adjust return value if truncated by size. */
  if (strpos > preferred_size)
  {
    strpos = preferred_size;
  }

  /* Truncate if above CHUNKS_TOTAL bytes. */
  if (*offset+(int32_t)strpos > CHUNKS_TOTAL)
  {
    strpos = CHUNKS_TOTAL - *offset;
  }

  REST.set_response_payload(response, buffer, strpos);

  /* IMPORTANT for chunk-wise resources: Signal chunk awareness to REST engine. */
  *offset += strpos;

  /* Signal end of resource representation. */
  if (*offset>=CHUNKS_TOTAL)
  {
    *offset = -1;
  }
}
#endif

/******************************************************************************/
#if REST_RES_SEPARATE && defined (PLATFORM_HAS_BUTTON) && WITH_COAP > 3
/* Required to manually (=not by the engine) handle the response transaction. */
#if WITH_COAP == 7
#include "er-coap-07-separate.h"
#include "er-coap-07-transactions.h"
#elif WITH_COAP == 12
#include "er-coap-12-separate.h"
#include "er-coap-12-transactions.h"
#elif WITH_COAP == 13
#include "er-coap-13-separate.h"
#include "er-coap-13-transactions.h"
#endif
/*
 * CoAP-specific example for separate responses.
 * Note the call "rest_set_pre_handler(&resource_separate, coap_separate_handler);" in the main process.
 * The pre-handler takes care of the empty ACK and updates the MID and message type for CON requests.
 * The resource handler must store all information that required to finalize the response later.
 */
RESOURCE(separate, METHOD_GET, "test/separate", "title=\"Separate demo\"");

/* A structure to store the required information */
typedef struct application_separate_store {
  /* Provided by Erbium to store generic request information such as remote address and token. */
  coap_separate_t request_metadata;
  /* Add fields for addition information to be stored for finalizing, e.g.: */
  char buffer[16];
} application_separate_store_t;

static uint8_t separate_active = 0;
static application_separate_store_t separate_store[1];

void
separate_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /*
   * Example allows only one open separate response.
   * For multiple, the application must manage the list of stores.
   */
  if (separate_active)
  {
    coap_separate_reject();
  }
  else
  {
    separate_active = 1;

    /* Take over and skip response by engine. */
    coap_separate_accept(request, &separate_store->request_metadata);
    /* Be aware to respect the Block2 option, which is also stored in the coap_separate_t. */

    /*
     * At the moment, only the minimal information is stored in the store (client address, port, token, MID, type, and Block2).
     * Extend the store, if the application requires additional information from this handler.
     * buffer is an example field for custom information.
     */
    snprintf(separate_store->buffer, sizeof(separate_store->buffer), "StoredInfo");
  }
}

void
separate_finalize_handler()
{
  if (separate_active)
  {
    coap_transaction_t *transaction = NULL;
    if ( (transaction = coap_new_transaction(separate_store->request_metadata.mid, &separate_store->request_metadata.addr, separate_store->request_metadata.port)) )
    {
      coap_packet_t response[1]; /* This way the packet can be treated as pointer as usual. */

      /* Restore the request information for the response. */
      coap_separate_resume(response, &separate_store->request_metadata, CONTENT_2_05);

      coap_set_payload(response, separate_store->buffer, strlen(separate_store->buffer));

      /*
       * Be aware to respect the Block2 option, which is also stored in the coap_separate_t.
       * As it is a critical option, this example resource pretends to handle it for compliance.
       */
      coap_set_header_block2(response, separate_store->request_metadata.block2_num, 0, separate_store->request_metadata.block2_size);

      /* Warning: No check for serialization error. */
      transaction->packet_len = coap_serialize_message(response, transaction->packet);
      coap_send_transaction(transaction);
      /* The engine will clear the transaction (right after send for NON, after acked for CON). */

      separate_active = 0;
    }
    else
    {
      /*
       * Set timer for retry, send error message, ...
       * The example simply waits for another button press.
       */
    }
  } /* if (separate_active) */
}
#endif

/******************************************************************************/
#if REST_RES_PUSHING
/*
 * Example for a periodic resource.
 * It takes an additional period parameter, which defines the interval to call [name]_periodic_handler().
 * A default post_handler takes care of subscriptions by managing a list of subscribers to notify.
 */
PERIODIC_RESOURCE(pushing, METHOD_GET, "test/push", "title=\"Periodic demo\";obs", 5*CLOCK_SECOND);

void
pushing_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);

  /* Usually, a CoAP server would response with the resource representation matching the periodic_handler. */
  const char *msg = "It's periodic!";
  REST.set_response_payload(response, msg, strlen(msg));

  /* A post_handler that handles subscriptions will be called for periodic resources by the REST framework. */
}

/*
 * Additionally, a handler function named [resource name]_handler must be implemented for each PERIODIC_RESOURCE.
 * It will be called by the REST manager process with the defined period.
 */
void
pushing_periodic_handler(resource_t *r)
{
  static uint16_t obs_counter = 0;
  static char content[11];

  ++obs_counter;

  PRINTF("TICK %u for /%s\n", obs_counter, r->url);

  /* Build notification. */
  coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
  coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
  coap_set_payload(notification, content, snprintf(content, sizeof(content), "TICK %u", obs_counter));

  /* Notify the registered observers with the given message type, observe option, and payload. */
  REST.notify_subscribers(r, obs_counter, notification, obs_counter); // Conditional observe
}
#endif

/******************************************************************************/
/* Conditional observe example resource */
#if REST_RES_PUSHING
/*
 * Example for a periodic resource.
 * It takes an additional period parameter, which defines the interval to call [name]_periodic_handler().
 * A default post_handler takes care of subscriptions by managing a list of subscribers to notify.
 */
PERIODIC_RESOURCE(temp, METHOD_GET, "sensors/temp", "title=\"Temperature\";obs", 5*CLOCK_SECOND);

/*const static uint8_t data[100] = {
21, 23, 27, 27, 29, 21, 23, 27, 22, 25, 24, 26, 23, 25, 29, 29, 26, 27, 21, 26, 20, 22, 21, 28, 21, 24, 21, 21, 29, 22, 25, 28, 26, 22, 26, 25, 24, 29, 22, 27, 25, 27, 24, 28, 22, 23, 28, 29, 20, 21, 25, 20, 21, 26, 28, 23, 20, 20, 24, 20, 22, 24, 29, 28, 22, 25, 23, 27, 25, 26, 25, 20, 24, 29, 29, 27, 22, 27, 26, 23, 26, 21, 24, 28, 28, 23, 22, 20, 23, 26, 29, 25, 26, 28, 24, 29, 23, 22, 26, 23
};
*/
const static uint8_t data[2008] = {
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 17, 18, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 17, 18, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 
17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 25, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 25, 26, 26, 26, 26, 26, 26, 26, 26, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 
24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 24, 24, 25, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 
24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 21, 22, 21, 22, 22, 22, 22, 22, 22, 22, 22, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 
21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20};

static unsigned max_age = COAP_DEFAULT_MAX_AGE;
unsigned data_iterator = 0;
#ifndef CONDITION
static uint16_t tmp_last = 0;
static uint32_t time_last = 0;
#endif

void
temp_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);

  static unsigned tmp; // state of temperature resource at the moment
  static char msg[11];

#if RES_TEMP_USE_SHT11_DATA // use sht11 sensor for temperature resource
  i2c_disable(); //http://comments.gmane.org/gmane.os.contiki.devel/14819
  sht11_init();
  tmp = (unsigned) (-39.60 + 0.01 * sht11_temp());
#else // use stored values for temperature resource
  tmp = data[data_iterator++ % 2008];
#endif

#ifndef CONDITION
  tmp_last = tmp;
  time_last = clock_seconds();
  coap_set_header_max_age(response, max_age);
#endif
  snprintf(msg, sizeof(msg), "%u", tmp);
  REST.set_response_payload(response, msg, strlen(msg));

  /* A post_handler that handles subscriptions will be called for periodic resources by the REST framework. */
}

/*
 * Additionally, a handler function named [resource name]_handler must be implemented for each PERIODIC_RESOURCE.
 * It will be called by the REST manager process with the defined period.
 */
void
temp_periodic_handler(resource_t *r)
{
  static char content[5];
  static uint16_t obs_counter = 0;
  static unsigned tmp; // state of temperature resource at the moment
  unsigned long cpu, lpm, transmit, listen;
  static uint8_t stop = 0;
#if RES_TEMP_USE_SHT11_DATA // use sht11 sensor for temperature resource
  i2c_disable(); //http://comments.gmane.org/gmane.os.contiki.devel/14819
  sht11_init();
  tmp = (unsigned) (-39.60 + 0.01 * sht11_temp());
  PRINTF("/%s: retreived temperature value from sensor\n", r->url);
#else // use stored values for temperature resource
  tmp = data[data_iterator];

  data_iterator = (data_iterator + 1) ;
  //printf("/%s: retreived temperature value from data array:%u\n", r->url, data[data_iterator-1]);
  if(!stop) printf("==>from data array:val[%u] = %u\n", data_iterator-1, data[data_iterator-1]);

  if(!stop) {
    energest_flush();

    cpu = energest_type_time(ENERGEST_TYPE_CPU)/RTIMER_ARCH_SECOND;
    lpm = energest_type_time(ENERGEST_TYPE_LPM)/RTIMER_ARCH_SECOND;
    transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT)/RTIMER_ARCH_SECOND;
    listen = energest_type_time(ENERGEST_TYPE_LISTEN)/RTIMER_ARCH_SECOND;

    printf("cpu = %lu lpm = %lu tx = %lu rx = %lu\n", cpu, lpm, transmit, listen);
  }
  if (data_iterator >= 2008) {
    if(!stop) {
      energest_flush();

      cpu = energest_type_time(ENERGEST_TYPE_CPU);
      lpm = energest_type_time(ENERGEST_TYPE_LPM);
      transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
      listen = energest_type_time(ENERGEST_TYPE_LISTEN);

      printf(">>>Final: cpu = %lu lpm = %lu tx = %lu rx = %lu\n", cpu, lpm, transmit, listen);
      stop = 1;
      printf("==>End<==\n");

      coap_packet_t last_message[1]; /* This way the packet can be treated as pointer as usual. */
      coap_init_message(last_message, COAP_TYPE_NON, CONTENT_2_05, 0 );
      coap_set_payload(last_message, content, snprintf(content, sizeof(content), "%u", 100));

      REST.notify_subscribers(r, obs_counter, last_message, tmp);
    } /* stop */
    return;
  }
#endif

#ifndef CONDITION /*For Normal Observe, check if value has changed before sending notification */
  if(tmp_last == tmp && ((clock_seconds() - time_last) < max_age)) {
    return;
  } else {
    tmp_last = tmp;
    time_last = clock_seconds();
  }
#endif /*ifndec condition */
  /* Build notification. */
  coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
  coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
  coap_set_header_max_age(notification, max_age);
  coap_set_payload(notification, content, snprintf(content, sizeof(content), "%u", tmp));

  ++obs_counter;
  /* Notify the registered observers with the given message type, observe option, and payload. */
  REST.notify_subscribers(r, obs_counter, notification, tmp);
}
#endif /*if rest pushing*/

/******************************************************************************/
#if REST_RES_EVENT && defined (PLATFORM_HAS_BUTTON)
/*
 * Example for an event resource.
 * Additionally takes a period parameter that defines the interval to call [name]_periodic_handler().
 * A default post_handler takes care of subscriptions and manages a list of subscribers to notify.
 */
EVENT_RESOURCE(event, METHOD_GET, "sensors/button", "title=\"Event demo\";obs");

void
event_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  /* Usually, a CoAP server would response with the current resource representation. */
  const char *msg = "It's eventful!";
  REST.set_response_payload(response, (uint8_t *)msg, strlen(msg));

  /* A post_handler that handles subscriptions/observing will be called for periodic resources by the framework. */
}

/* Additionally, a handler function named [resource name]_event_handler must be implemented for each PERIODIC_RESOURCE defined.
 * It will be called by the REST manager process with the defined period. */
void
event_event_handler(resource_t *r)
{
  static uint16_t event_counter = 0;
  static char content[12];

  ++event_counter;

  PRINTF("TICK %u for /%s\n", event_counter, r->url);

  /* Build notification. */
  coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
  coap_init_message(notification, COAP_TYPE_CON, CONTENT_2_05, 0 );
  coap_set_payload(notification, content, snprintf(content, sizeof(content), "EVENT %u", event_counter));

  /* Notify the registered observers with the given message type, observe option, and payload. */
  REST.notify_subscribers(r, event_counter, notification, event_counter); // Conditional observe
}
#endif /* PLATFORM_HAS_BUTTON */

/******************************************************************************/
#if REST_RES_SUB
/*
 * Example for a resource that also handles all its sub-resources.
 * Use REST.get_url() to multiplex the handling of the request depending on the Uri-Path.
 */
RESOURCE(sub, METHOD_GET | HAS_SUB_RESOURCES, "test/path", "title=\"Sub-resource demo\"");

void
sub_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);

  const char *uri_path = NULL;
  int len = REST.get_url(request, &uri_path);
  int base_len = strlen(resource_sub.url);

  if (len==base_len)
  {
	snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "Request any sub-resource of /%s", resource_sub.url);
  }
  else
  {
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, ".%.*s", len-base_len, uri_path+base_len);
  }

  REST.set_response_payload(response, buffer, strlen((char *)buffer));
}
#endif

/******************************************************************************/
#if defined (PLATFORM_HAS_LEDS)
/******************************************************************************/
#if REST_RES_LEDS
/*A simple actuator example, depending on the color query parameter and post variable mode, corresponding led is activated or deactivated*/
RESOURCE(leds, METHOD_POST | METHOD_PUT , "actuators/leds", "title=\"LEDs: ?color=r|g|b, POST/PUT mode=on|off\";rt=\"Control\"");

void
leds_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  size_t len = 0;
  const char *color = NULL;
  const char *mode = NULL;
  uint8_t led = 0;
  int success = 1;

  if ((len=REST.get_query_variable(request, "color", &color))) {
    PRINTF("color %.*s\n", len, color);

    if (strncmp(color, "r", len)==0) {
      led = LEDS_RED;
    } else if(strncmp(color,"g", len)==0) {
      led = LEDS_GREEN;
    } else if (strncmp(color,"b", len)==0) {
      led = LEDS_BLUE;
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if (success && (len=REST.get_post_variable(request, "mode", &mode))) {
    PRINTF("mode %s\n", mode);

    if (strncmp(mode, "on", len)==0) {
      leds_on(led);
    } else if (strncmp(mode, "off", len)==0) {
      leds_off(led);
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if (!success) {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
  }
}
#endif

/******************************************************************************/
#if REST_RES_TOGGLE
/* A simple actuator example. Toggles the red led */
RESOURCE(toggle, METHOD_GET | METHOD_PUT | METHOD_POST, "actuators/toggle", "title=\"Red LED\";rt=\"Control\"");
void
toggle_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  leds_toggle(LEDS_RED);
}
#endif
#endif /* PLATFORM_HAS_LEDS */

/******************************************************************************/
#if REST_RES_LIGHT && defined (PLATFORM_HAS_LIGHT)
/* A simple getter example. Returns the reading from light sensor with a simple etag */
RESOURCE(light, METHOD_GET, "sensors/light", "title=\"Photosynthetic and solar light (supports JSON)\";rt=\"LightSensor\"");
void
light_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  uint16_t light_photosynthetic = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
  uint16_t light_solar = light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR);

  const uint16_t *accept = NULL;
  int num = REST.get_header_accept(request, &accept);

  if ((num==0) || (num && accept[0]==REST.type.TEXT_PLAIN))
  {
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "%u;%u", light_photosynthetic, light_solar);

    REST.set_response_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
  }
  else if (num && (accept[0]==REST.type.APPLICATION_XML))
  {
    REST.set_header_content_type(response, REST.type.APPLICATION_XML);
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "<light photosynthetic=\"%u\" solar=\"%u\"/>", light_photosynthetic, light_solar);

    REST.set_response_payload(response, buffer, strlen((char *)buffer));
  }
  else if (num && (accept[0]==REST.type.APPLICATION_JSON))
  {
    REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "{'light':{'photosynthetic':%u,'solar':%u}}", light_photosynthetic, light_solar);

    REST.set_response_payload(response, buffer, strlen((char *)buffer));
  }
  else
  {
    REST.set_response_status(response, REST.status.NOT_ACCEPTABLE);
    const char *msg = "Supporting content-types text/plain, application/xml, and application/json";
    REST.set_response_payload(response, msg, strlen(msg));
  }
}
#endif /* PLATFORM_HAS_LIGHT */

/******************************************************************************/
#if REST_RES_BATTERY && defined (PLATFORM_HAS_BATTERY)
/* A simple getter example. Returns the reading from light sensor with a simple etag */
RESOURCE(battery, METHOD_GET, "sensors/battery", "title=\"Battery status\";rt=\"Battery\"");
void
battery_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  int battery = battery_sensor.value(0);

  const uint16_t *accept = NULL;
  int num = REST.get_header_accept(request, &accept);

  if ((num==0) || (num && accept[0]==REST.type.TEXT_PLAIN))
  {
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "%d", battery);

    REST.set_response_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
  }
  else if (num && (accept[0]==REST.type.APPLICATION_JSON))
  {
    REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
    snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "{'battery':%d}", battery);

    REST.set_response_payload(response, buffer, strlen((char *)buffer));
  }
  else
  {
    REST.set_response_status(response, REST.status.NOT_ACCEPTABLE);
    const char *msg = "Supporting content-types text/plain and application/json";
    REST.set_response_payload(response, msg, strlen(msg));
  }
}
#endif /* PLATFORM_HAS_BATTERY */


#if defined (PLATFORM_HAS_RADIO) && REST_RES_RADIO
/* A simple getter example. Returns the reading of the rssi/lqi from radio sensor */
RESOURCE(radio, METHOD_GET, "sensor/radio", "title=\"RADIO: ?p=lqi|rssi\";rt=\"RadioSensor\"");

void
radio_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  size_t len = 0;
  const char *p = NULL;
  uint8_t param = 0;
  int success = 1;

  const uint16_t *accept = NULL;
  int num = REST.get_header_accept(request, &accept);

  if ((len=REST.get_query_variable(request, "p", &p))) {
    PRINTF("p %.*s\n", len, p);
    if (strncmp(p, "lqi", len)==0) {
      param = RADIO_SENSOR_LAST_VALUE;
    } else if(strncmp(p,"rssi", len)==0) {
      param = RADIO_SENSOR_LAST_PACKET;
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if (success) {
    if ((num==0) || (num && accept[0]==REST.type.TEXT_PLAIN))
    {
      REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
      snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "%d", radio_sensor.value(param));

      REST.set_response_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
    }
    else if (num && (accept[0]==REST.type.APPLICATION_JSON))
    {
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);

      if (param == RADIO_SENSOR_LAST_VALUE) {
        snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "{'lqi':%d}", radio_sensor.value(param));
      } else if (param == RADIO_SENSOR_LAST_PACKET) {
        snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "{'rssi':%d}", radio_sensor.value(param));
      }

      REST.set_response_payload(response, buffer, strlen((char *)buffer));
    }
    else
    {
      REST.set_response_status(response, REST.status.NOT_ACCEPTABLE);
      const char *msg = "Supporting content-types text/plain and application/json";
      REST.set_response_payload(response, msg, strlen(msg));
    }
  } else {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
  }
}
#endif



PROCESS(rest_server_example, "Erbium Example Server");
AUTOSTART_PROCESSES(&rest_server_example);

PROCESS_THREAD(rest_server_example, ev, data)
{
  PROCESS_BEGIN();

  PRINTF("Starting Erbium Example Server\n");
  
  //joining multicast server
  if(join_mcast_group() == NULL) {
    PRINTF("Failed to join multicast group\n");
    PROCESS_EXIT();
  }

#ifdef RF_CHANNEL
  PRINTF("RF channel: %u\n", RF_CHANNEL);
#endif
#ifdef IEEE802154_PANID
  PRINTF("PAN ID: 0x%04X\n", IEEE802154_PANID);
#endif

  

/* if static routes are used rather than RPL */
#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET) && !defined (CONTIKI_TARGET_NATIVE)
  set_global_address();
  configure_routing();
#endif

  /* Initialize the REST engine. */
  rest_init_engine();

  /* Activate the application-specific resources. */
#if REST_RES_SERVERINFO
  rest_activate_resource(&resource_serverinfo);
#endif
#if REST_RES_ZIG001
  rest_activate_resource(&resource_zig001);
#endif
#if REST_RES_ZIG002
  rest_activate_resource(&resource_zig002);
#endif
#if REST_RES_PH_TOUCH
  rest_activate_resource(&resource_ph_touch);
#endif
#if REST_RES_PH_SONAR
  rest_activate_resource(&resource_ph_sonar);
#endif
#if REST_RES_PH_FLEXIFORCE
  rest_activate_resource(&resource_ph_flexiforce);
#endif
#if REST_RES_PH_MOTION
  rest_activate_resource(&resource_ph_motion);
#endif
#if REST_RES_RFID
  event_rfid_read = process_alloc_event();
  rest_activate_event_resource(&resource_rfid);
#endif

#if REST_RES_HELLO
  rest_activate_resource(&resource_helloworld);
#endif
#if REST_RES_MIRROR
  rest_activate_resource(&resource_mirror);
#endif
#if REST_RES_CHUNKS
  rest_activate_resource(&resource_chunks);
#endif
#if REST_RES_PUSHING
  //rest_activate_periodic_resource(&periodic_resource_pushing);
  rest_activate_periodic_resource(&periodic_resource_temp); // Use conditional observe resource
#endif
#if defined (PLATFORM_HAS_BUTTON) && REST_RES_EVENT
  rest_activate_event_resource(&resource_event);
#endif
#if defined (PLATFORM_HAS_BUTTON) && REST_RES_SEPARATE && WITH_COAP > 3
  /* No pre-handler anymore, user coap_separate_accept() and coap_separate_reject(). */
  rest_activate_resource(&resource_separate);
#endif
#if defined (PLATFORM_HAS_BUTTON) && (REST_RES_EVENT || (REST_RES_SEPARATE && WITH_COAP > 3))
  SENSORS_ACTIVATE(button_sensor);
#endif
#if REST_RES_SUB
  rest_activate_resource(&resource_sub);
#endif
#if defined (PLATFORM_HAS_LEDS)
#if REST_RES_LEDS
  rest_activate_resource(&resource_leds);
#endif
#if REST_RES_TOGGLE
  rest_activate_resource(&resource_toggle);
#endif
#endif /* PLATFORM_HAS_LEDS */
#if defined (PLATFORM_HAS_LIGHT) && REST_RES_LIGHT
  SENSORS_ACTIVATE(light_sensor);
  rest_activate_resource(&resource_light);
#endif
#if defined (PLATFORM_HAS_BATTERY) && REST_RES_BATTERY
  SENSORS_ACTIVATE(battery_sensor);
  rest_activate_resource(&resource_battery);
#endif
#if defined (PLATFORM_HAS_RADIO) && REST_RES_RADIO
  SENSORS_ACTIVATE(radio_sensor);
  rest_activate_resource(&resource_radio);
#endif
#if REST_RES_RFID
      PRINTF("activating rfid reader, changing baud rate\n");
      uart0_init((MSP430_CPU_SPEED)/(BAUDRATE));
      P4SEL &= ~0x04; // Clear to make it a GPIO (p4.2)
      P4DIR |= 0x04;
      //P4OUT |= 0x04; // disable rfid reader
      P4OUT &= ~0x04; //enable rfid reader
      uart0_set_input(uart_rx_callback); 
#endif 
  cc2420_set_txpower(CC2420_TXPOWER_MAX);
  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
#if defined (PLATFORM_HAS_BUTTON)
    if (ev == sensors_event && data == &button_sensor) {
      PRINTF("BUTTON\n");
#if REST_RES_EVENT
      /* Call the event_handler for this application-specific event. */
      event_event_handler(&resource_event);
#endif
#if REST_RES_SEPARATE && WITH_COAP>3
      /* Also call the separate response example handler. */
      separate_finalize_handler();
#endif
    }
#if REST_RES_RFID
    else if(ev == event_rfid_read){ 
      PRINTF("RFID Event\n");
      /* Call the event_handler for this application-specific event. */
      rfid_event_handler(&resource_rfid);
    }
#endif
    else{
      PRINTF("Unknown Event\n");
    }

#endif /* PLATFORM_HAS_BUTTON */
  } /* while (1) */

  PROCESS_END();
}
