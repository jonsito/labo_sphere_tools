//
// Created by jantonio on 29/8/20.
//

#ifndef IMALIVE_THREADS_H
#define IMALIVE_THREADS_H

#include <pthread.h>
#include "im_alive.h"

typedef struct {
    int id; // thread ID
    char * tname; // thread name
    configuration *config; // pointer to configuration options
    int shouldFinish; // flag to mark thread must die after processing
    pthread_t thread; // where to store pthread_create() info
    void *(*handler)(void *config); // entry point
} sc_thread_slot;

#ifdef IMALIVE_THREADS_C
#define EXTERN extern
#else
#define EXTERN
#endif

EXTERN int sc_thread_create(int index,char *name, configuration *config,void *(*handler)(void *config));
EXTERN int launchAndWait (char *cmd, char *args);
EXTERN void waitForThreads(int numthreads);

EXTERN sc_thread_slot *sc_threads;

#undef EXTERN

#endif //IMALIVE_THREADS_H
