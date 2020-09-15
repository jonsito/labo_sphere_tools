//
// Created by jantonio on 29/8/20.
//

#define _GNU_SOURCE
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include "threads.h"
#include "debug.h"

sc_thread_slot *sc_thread_create(char *name,configuration *config,void (*handler)(configuration *config)) {
    sc_thread_slot *slot=calloc(1,sizeof(sc_thread_slot));
    if (!slot) {
        debug(DBG_ERROR,"Cannot create thread structure");
        return NULL;
    }
    // need to make sure that thread name is no longer than 16 chars... but mingw has no strndup()
    char buff[32];
    memset(buff,0,32);
    slot->tname=strdup(name);
    if (strlen(name)>16)slot->tname[15]='\0'; // stupid mingw has no strndup(name,len)
    slot->config=config;
    slot->handler=handler;
    int res=pthread_create( &slot->thread, NULL, (void * (*)(void *)) &handler, &slot->id);
    if (res<0) {
        debug(DBG_ERROR,"Cannot create and start posix thread");
        return NULL;
    }
#ifndef __APPLE__
    res=pthread_setname_np(slot->thread,slot->tname);
    if (res!=0) debug(DBG_NOTICE,"Cannot set thread name: %s",slot->tname);
#else
    sleep(1); // let thread set itself their name
#endif
    res=pthread_getname_np(slot->thread,buff,32);
    debug(DBG_TRACE,"getname returns %d thread name:%s",res,buff);
    return 0;
}