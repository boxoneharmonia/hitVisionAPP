#include "C_DDSPub.h"
#include "DDS_type.h"
#include <stdio.h>
#include <unistd.h>

int main(int argc, char const *argv[])
{

  DDSPub_t* pub = DDSPub_create();

  DDSPub_set_domain_id(pub,2);
  DDSPub_init(pub);
  for(int i=0; i<10; ++i)
{
  if (pub != NULL) 
  {
    //const char* jsonStr = "{\"sync_head\":1,\"pid\":1234,\"cmd\":\"on\"}";
    const char* jsonStr = "{\"sync_head\":\"eb90\",\"data_length\":1234,\"send_payload_main_id\":12,\"send_payload_sub_id\":34,\"receive_payload_main_id\":56,\"receive_payload_sub_id\":78,\"data_type\":12,\"send_frame_count\":34,\"time_of_send\":56,\"data_message\":78,\"sum_of_check\":12}";

    if (DDSPub_sendMsg(pub, TELEMETRY_DATA_INDEX, jsonStr)) 
    {
      printf("Message sent successfully.\n");
    } 
    else 
    {
      printf("Failed to send message.\n");
    }
/*    const char* jsonStr2 = "{\"topicType\":6,\"pid\":2234,\"cmd\":\"off\"}";
    if (DDSPub_sendMsg(pub, APP_DATA_INDEX, jsonStr2)) 
    {
      printf("Message2 sent successfully.\n");
    } 
    else 
    {
      printf("Failed to send message2.\n");
    }*/
  } 
  else
  {
    printf("Failed to create DDSPub_t instance.\n");
  }
  sleep(1);
}

    DDSPub_destroy(pub);
return 0;
}
