//
// Created by jantonio on 4/9/20.
//
#include <time.h>

#ifndef IMALIVE_CL_STATE_H
#define IMALIVE_CL_STATE_H

#ifndef IMALIVE_CL_STATE_C
#define EXTERN extern
#else
#define EXTERN
#endif

typedef struct cl_status_st {
    time_t timestamp;
    char* state; /* client:state:server:users */
} cl_status;

#define NUM_CLIENTS 255

EXTERN cl_status * initData(cl_status *st); /* if null create; else fill */
EXTERN int freeData(cl_status *st);

EXTERN int clst_putData(cl_status *st,char *data);
EXTERN char *clst_getData(cl_status *st,int format); /* 0:csv 1:json 2:xml */
EXTERN char *clst_getDataByName(cl_status *st,char *client,int format); /* 0:csv 1:json 2:xml */

EXTERN char *clst_getList(cl_status *st, int cl_from,int cl_to, int format);
EXTERN int clst_expireData(cl_status *st);

EXTERN int clst_loadFile(cl_status *st, char *filename, int format);
EXTERN int clst_saveFile(cl_status *st, char *filename, int format);

#undef EXTERN
#endif //IMALIVE_CL_STATE_H
