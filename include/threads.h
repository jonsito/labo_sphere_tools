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
    pthread_t thread; // where to store pthread_create() info
    void (*handler)(configuration *config); // entry point
} sc_thread_slot;

#ifdef IMALIVE_THREADS_C
#define EXTERN extern
#else
#define EXTERN
#endif

EXTERN sc_thread_slot *sc_thread_create(char *name, configuration *config,void (*handler)(configuration *config));

#undef EXTERN

#endif //IMALIVE_THREADS_H
