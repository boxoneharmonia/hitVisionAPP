#include "C_DDSSub.h"
#include "DDS_type.h"
#include <stdio.h>

void myCallbackFunction(int type, const void* data)
{
  printf("type:%d\nmessage:%s\n",type,(char*)data);
}
int main(int argc, char const *argv[])
{
  DDSSub_t *sub = DDSSub_creat();
  int topic_arry[] = {TELEMETRY_DATA_INDEX,APP_DATA_INDEX};
 if (sub != NULL) 
  {
    DDSSub_set_domain_id(sub,2);
    DDSSub_registerCallback(myCallbackFunction);
    //int topic_arry[] = {TELEMETRY_DATA_INDEX,APP_DATA_INDEX};
    DDSSub_init(sub,topic_arry,2);
    DDSSub_destroy(sub);
  } 
  else
  {
    printf("Failed to create DDSSub_t instance.\n");
  }
return 0;
}
