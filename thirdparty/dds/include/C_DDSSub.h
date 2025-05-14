#ifndef C_DDSSUB_H
#define C_DDSSUB_H
#ifdef __cplusplus
// #include "GlobalVariables.h"
#include <pthread.h>
extern "C" {
#endif

typedef void (*CDataCallback)(int, const void*);
typedef struct DDSSub_t DDSSub_t;
struct ThreadArgs {
    DDSSub_t* wrapper;
    int* arry_topic;
    int arry_size;
};
DDSSub_t* DDSSub_creat();
void DDSSub_destroy(DDSSub_t* wrapper);
// void DDSSub_init(DDSSub_t* wrapper);
void DDSSub_init(DDSSub_t* wrapper,int arry_topic[],int arry_size);
void DDSSub_registerCallback(CDataCallback cb);
void DDSSub_set_domain_id(DDSSub_t* wrapper,int domain_id);
int DDSSub_get_domain_id(DDSSub_t* wrapper);
void* thread_function(void* arg);

#ifdef __cplusplus
}
#endif
#endif