//
// Created by jantonio on 4/9/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/param.h>

#define IMALIVE_CL_STATE_C
#include "client_state.h"
#include "debug.h"
#include "tools.h"

cl_status * initData(cl_status *st){ /* if null create; else fill */
    if(!st) {
        st=calloc(NUM_CLIENTS,sizeof(cl_status) );
    }
    if (!st) {
        debug(DBG_ERROR,"calloc(cl_status)");
        return NULL;
    }
    time_t t=time(NULL);
    for (int n=0;n<NUM_CLIENTS;n++) {
        cl_status *pt=st+n;
        pt->timestamp=t;
        if (!pt->state) free(pt->state);
        pt->state=NULL;
    }
    return st;
}

int freeData(cl_status *st){
    int count=0;
    if (!st) return 0;
    for (int n=0;n<NUM_CLIENTS;n++) {
        cl_status *pt=st+n;
        pt->timestamp=0;
        if (!pt->state) {
            if (strstr(pt->state,":???:")>=0) count++;
            free(pt->state);
        }
        pt->state=NULL;
    }
    return count;
}

int clst_putData(cl_status *st,char *data){
    st->timestamp=time(NULL);
    if (!data) {
        if (st->state) free(st->state);
        st->state=NULL;
    } else {
        if (st->state && strcmp(st->state,data)==0) return 0; // no change
        free(st->state);
        st->state=strdup(data);
    }
    return 1;
}

char *clst_getData(cl_status *st,int format) {

    char *csv_template ="%s:%s:%s:%s\n"; // notice end of line
    char *json_template = "{\"name\":\"%s\",\"state\":\"%s\",\"server\":\"%s\",\"users\":\"%s\"}";
    char *xml_template = "<client name=\"%s\"><state>%s</state><server>%s</server><users>%s</users></client>";
    int nelem=0;

    char *result=calloc(BUFFER_LENGTH,sizeof(char));
    if (!result) return NULL;
    char **tokens=tokenize(st->state,':',&nelem);
    char *templ;
    switch (format%3) {
        case 0: /* csv */ templ=csv_template; break;
        case 1: /* json */ templ=json_template; break;
        case 2: /* xml; do not add header */ templ=xml_template; break;
    }
    snprintf(result,BUFFER_LENGTH,templ,tokens[0],tokens[1],tokens[2],tokens[3]);
    free_tokens(tokens);
    return result;
}

char *clst_getDataByName(cl_status *st,char *client,int format){  /* 0:csv 1:json 2:xml */
    if (!st) return NULL;
    for (cl_status *pt=st; pt<st+NUM_CLIENTS;pt++) {
        if (!pt->state) continue;
        if (strpos(client,pt->state)!=0) continue;
        // entry found. handle it
        return clst_getData(pt,format);
    }
    // arriving here means client not found
    return NULL;
}

char *clst_getList(cl_status *st, int cl_from,int cl_to, int format) {
    char *result=NULL;
    cl_from=MAX(0,cl_from);
    cl_to=MIN(cl_to,NUM_CLIENTS-1);
    for (cl_status *pt=st+cl_from; pt<st+cl_to;pt++) {
        if (!pt) continue; // should not occurs
        if (!pt->state) continue;
        char *entry=clst_getData(pt,format);
        if (!entry) continue;
        str_concat(result,entry);
        free(entry);
    }
    return result;
}

int clst_expireData(cl_status *st){
    int count=0;
    time_t current=time(NULL);
    for (cl_status *pt=st; pt<st+NUM_CLIENTS; pt++) {
        if ( (current - pt->timestamp) < EXPIRE_TIME ) continue;
        if (!pt->state) continue; // null
        if (strpos (pt->state,":-:-:-")>0) continue; // already expired
        // expired. set state to "unknown". Notice reuse of current buffer
        snprintf(strchr(st->state,':'),BUFFER_LENGTH,":-:-:-");
        count++;
    }
    return count; // number of entries expired
}

int clst_loadFile(cl_status *st, char *filename, int format){
    return 0;
}

int clst_saveFile(cl_status *st, char *filename, int format){
    return 0;
}

#undef IMALIVE_CL_STATE_C