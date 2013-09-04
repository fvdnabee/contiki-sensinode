#include "erbium.h"

// Function to toggle LED lights on a certain pin of PORT 1!
void
led_light_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset, uint8_t pin)
{
 char message[REST_MAX_CHUNK_SIZE];
 int length;
 int on;
 uint8_t* payload=NULL;
 rest_resource_flags_t method;

 // Note: the following two rules should actually only be executed once...
 P1DIR |= pin;
 P1SEL &= ~pin;

 // get_method_type & payload

 method = REST.get_method_type(request);

 if(method & METHOD_POST){
     P1OUT ^= pin;
 }else if(method & METHOD_PUT){
     REST.get_request_payload(request,&payload);
     if(payload[0]=='O' && payload[1]=='N'){
      P1OUT |= pin;
     }else{
      P1OUT &= ~pin;
     }
 }


 on = P1OUT & pin;

 if(on){
   length = sprintf(message, "Led On");
 }else{
   length = sprintf(message, "Led Off");
 }

 if(length>=0) {
   memcpy(buffer, message, length);
 }

 REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
 REST.set_header_etag(response, (uint8_t *)&length, 1);
 REST.set_response_payload(response, buffer, length);
}
