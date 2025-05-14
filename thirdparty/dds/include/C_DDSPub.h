#ifndef C_DDSPUB_H
#define C_DDSPUB_H
#ifdef __cplusplus
extern "C" {
#endif


typedef struct DDSPub_t DDSPub_t;

DDSPub_t* DDSPub_create();
void DDSPub_destroy(DDSPub_t* wrapper);
void DDSPub_init(DDSPub_t* wrapper);
int DDSPub_sendMsg(DDSPub_t* wrapper, int topicType, const char* jsonStr);
void DDSPub_set_domain_id(DDSPub_t* wrapper,int domain_id);
int DDSPub_get_domain_id(DDSPub_t* wrapper);

#ifdef __cplusplus
}
#endif
#endif