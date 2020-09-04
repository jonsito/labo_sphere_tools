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
        // if buffer is null realloc=malloc; else reset buffer)
        pt->state=realloc(pt->state,BUFFER_LENGTH*sizeof(char));
        snprintf(pt->state,BUFFER_LENGTH,"l%03d:-:-:-",n);
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

int clst_setData(cl_status *st,char *data){
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

int clst_setDataByName(cl_status *st,char *client,char *data) {
    if (!st) return -1;
    for (cl_status *pt=st; pt<st+NUM_CLIENTS;pt++) {
        if (!pt->state) continue;
        if (strpos(client,pt->state)!=0) continue;
        // entry found. handle it
        return clst_setData(pt,data);
    }
    // arriving here means client not found
    return -1;
}

char *clst_getData(cl_status *st,int format) {

    char *csv_template ="%s:%s:%s:%s"; // notice end of line
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
    char *str=NULL;
    cl_from=MAX(0,cl_from);
    cl_to=MIN(cl_to,NUM_CLIENTS-1);
    // prepare header
    switch(format%3) {
        case 0: /* csv: do nothing */
            str="";  break;
        case 1: /* json: add '[' array mark */
            str="[\n"; break;
        case 2: /* xml: add xml header */
            str="<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<statelist>n\t";  break;
    }
    result=str_concat(result,str);
    for (cl_status *pt=st+cl_from; pt<st+cl_to;pt++) {
        if (!pt) continue; // should not occurs
        if (!pt->state) continue;
        char *entry=clst_getData(pt,format);
        if (!entry) continue;
        result=str_concat(result,entry);
        // handle field separator
        if (pt<st+(cl_to-1)) {
            switch(format%3) {
                case 0: /* csv: add newline */
                    str="\n";  break;
                case 1: /* json: add ',\n ' separator mark */
                    str=",\n "; break;
                case 2: /* xml: add newline-tab as separator */
                str="\n\t";  break;
            }
            result=str_concat(result,str);
        }
        free(entry);
    }
    // handle footer
    switch(format%3) {
        case 0: /* csv: add newline */
            str="\n";  break;
        case 1: /* json: add '\n]' end mark */
            str="\n]"; break;
        case 2: /* xml: add newline and close tag */
            str="\n</statelist>\n";  break;
    }
    result=str_concat(result,str);
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
    char buffer[BUFFER_LENGTH];
    char host[32];
    int count=0;
    if (format!=0) {
        debug(DBG_ERROR,"clst:loadFile() unsupported format");
        return -1;
    }
    FILE *f=fopen(filename,"r");
    if (!f) {
        debug(DBG_ERROR,"clst_loadFile::open() failed");
        return -1;
    }
    while (fgets(buffer, BUFFER_LENGTH, f) != NULL) {
        // compose host name
        memset(host,'\0',32);
        strncpy(host,buffer,31);
        host[strpos(host,":")]='\0';
        clst_setDataByName(st,host,buffer);
    }
    fclose(f);
    return count; // number of entries loaded
}

int clst_saveFile(cl_status *st, char *filename, int format){
    int count=0;
    char *list=clst_getList(st,0,255,format);
    FILE *f=fopen(filename,"w");
    if (!f) {
        debug(DBG_ERROR,"clst_saveFile::open() failed");
        return -1;
    }
    count=fputs(list,f); // PENDING: ¿what about string size?
    fclose(f);
    return count; // number of entries saved
}

#undef IMALIVE_CL_STATE_C