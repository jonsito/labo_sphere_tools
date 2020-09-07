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

EXTERN int initData(void); /* if null create; else fill */
EXTERN int freeData(void);

EXTERN int clst_setData(cl_status *st,char *data);
EXTERN int clst_setDataByIndex(int index,char *data);
EXTERN int clst_setDataByName(char *name,char *data);

EXTERN char *clst_getData(cl_status *st,int format); /* 0:csv 1:json 2:xml */
EXTERN char *clst_getDataByIndex(int index,int format); /* 0:csv 1:json 2:xml */
EXTERN char *clst_getDataByName(char *client,int format); /* 0:csv 1:json 2:xml */

EXTERN char *clst_getList(int cl_from,int cl_to, int format);
EXTERN int clst_expireData();

EXTERN int clst_loadFile(char *filename, int format);
EXTERN int clst_saveFile(char *filename, int format);

#undef EXTERN
#endif //IMALIVE_CL_STATE_H
