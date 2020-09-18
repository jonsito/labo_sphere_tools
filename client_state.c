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

static cl_status status[NUM_CLIENTS];
static char data[NUM_CLIENTS][BUFFER_LENGTH];

int clst_initData(void){
    time_t t=time(NULL);
    for (int n=0;n<NUM_CLIENTS;n++) {
        cl_status *pt=&status[n];
        pt->timestamp=t;
        snprintf(pt->state,BUFFER_LENGTH,"l%03d:-:-:-",n);
    }
    return 0;
}

int clst_freeData(){
    int count=0;
    for (int n=0;n<NUM_CLIENTS;n++) {
        cl_status *pt=&status[n];
        pt->timestamp=0;
        if (strstr(pt->state,":-:-:-")>=0) count++;
        snprintf(pt->state,BUFFER_LENGTH,"l%03d:-:-:-",n);
    }
    return count;
}

int clst_setData(cl_status *st,char *fmt){
    st->timestamp=time(NULL);
    snprintf(st->state,BUFFER_LENGTH,fmt);
    return 0;
}

int clst_setDataByIndex(int index, char *fmt) {
    if ((index<0) || (index>=NUM_CLIENTS)) return -1;
    return clst_setData(&status[index],fmt);
}

int clst_setDataByName(char *client,char *fmt) {
    for (int n=0;n<NUM_CLIENTS;n++) {
        cl_status *pt=&status[n];
        if (strpos(client,pt->state)!=0) continue;
        // entry found. handle it
        return clst_setData(pt,fmt);
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

char *clst_getDataByName(char *client,int format){  /* 0:csv 1:json 2:xml */
    for (int n=0;n<NUM_CLIENTS;n++) { // iterate entries
        cl_status *pt=&status[n];
        if (strpos(client,pt->state)!=0) continue;
        // entry found. handle it
        return clst_getData(pt,format); // found
    }
    return NULL; // not found
}

char *clst_getList(int cl_from,int cl_to, int format) {
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
    for (int n=cl_from;n<=cl_to;n++) {
        cl_status *pt=&status[n];
        char *entry=clst_getData(pt,format);
        if (!entry) continue;
        result=str_concat(result,entry);
        if (n<(NUM_CLIENTS-1)) { // do not add field separator on last item
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
    for (int n=0;n<NUM_CLIENTS;n++) {
        cl_status *pt=&status[n];
        if ( (current - pt->timestamp) < EXPIRE_TIME ) continue;
        if (strpos (pt->state,":-:-:-")>0) continue; // already expired
        debug(DBG_INFO,"Expiring entry '%s'",pt->state);
        // expired. set state to "unknown". Notice reuse of current buffer.
        char *c=strchr(pt->state,':');
        snprintf(c,BUFFER_LENGTH - ( c - pt->state),":-:-:-");
        count++;
    }
    return count; // number of entries expired
}

int clst_loadFile(char *filename, int format){
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
        clst_setDataByName(host,buffer);
    }
    fclose(f);
    return count; // number of entries loaded
}

int clst_saveFile(char *filename, int format){
    char *formatstr[]={"csv","json","xml"};
    int count=0;
    char *list=clst_getList(0,255,format);
    FILE *f=fopen(filename,"w");
    if (!f) {
        debug(DBG_ERROR,"clst_saveFile::open() failed");
        return -1;
    }
    count=fputs(list,f); // PENDING: ¿what about string size?
    fclose(f);
    debug(DBG_TRACE,"Saving status to file:'%s' in format:'%s'",filename,formatstr[format%3]);
    return count; // number of entries saved
}

void init_expireThread( configuration * config) {
    // enter loop
    while( config->loop )	{
        clst_expireData(status);
    }
}

#undef IMALIVE_CL_STATE_C