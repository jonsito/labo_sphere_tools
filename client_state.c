//
// Created by jantonio on 4/9/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/param.h>

#define IMALIVE_CL_STATE_C
#include "client_state.h"
#include "debug.h"
#include "tools.h"
#include "wsserver.h"

static cl_status status[NUM_CLIENTS];

int clst_initData(void){
    time_t t=time(NULL);
    for (int n=0;n<NUM_CLIENTS;n++) {
        cl_status *pt=&status[n];
        // host:state:server:users:load:meminfo
        // host => -1:indeterminate 0:off 1:on >1:uptime
        // users => name[[,name]...]
        // load => 1min/5min/15min
        // meminfo => total/free
        if (n==0) // special case for l000
            snprintf(pt->state,BUFFER_LENGTH-1,"l%03d:0/0/0/0/0:/0/0/0/0/0:/0/0:0.0 / 0.0 / 0.0:0 / 0:-",n);
        else snprintf(pt->state,BUFFER_LENGTH-1,"l%03d:-1:-:-:0.0 / 0.0 / 0.0:0 / 0:-",n);
        pt->state[BUFFER_LENGTH-1]='\0';
        pt->timestamp=t;
    }
    return 0;
}

int clst_freeData(){
    int count=0;
    for (int n=0;n<NUM_CLIENTS;n++) {
        cl_status *pt=&status[n];
        pt->timestamp=0;
        // check only (uptime/server/users) part
        if (strstr(pt->state,":-1:-:-:")>=0) count++; // not yet initialized
        if (strstr(pt->state,":0:-:-:")>=0) count++; // already clean
        snprintf(pt->state,BUFFER_LENGTH,"l%03d:0:-:-:0.0 / 0.0 / 0.0:0 / 0:-",n);
    }
    return count;
}

/* return -1:error 0:nochange 1:change */
int clst_setData(cl_status *st,char *data){
    int res=0;
    st->timestamp=time(NULL);
    // just compare on/off state, as every else may change
    if (strncmp(data,st->state,7)!=0) {
        debug(DBG_TRACE,"old state: '%s' => new state: '%s'",st->state,data);
        res= 1;
    } else {
        debug(DBG_TRACE,"unchanged '%s'",st->state);
    }
    snprintf(st->state,BUFFER_LENGTH-1,"%s",data);
    st->state[BUFFER_LENGTH-1]='\0';
    return res;
}

/* return -1:error 0:nochange 1:change */
int clst_setDataByIndex(int index, char *data) {
    if ((index<0) || (index>=NUM_CLIENTS)) {
        debug(DBG_ERROR,"index %d out of range",index);
        return -1;
    }
    return clst_setData(&status[index],data);
}

/* return -1:error 0:nochange 1:change */
int clst_setDataByName(char *client,char *data) {
    for (int n=0;n<NUM_CLIENTS;n++) {
        cl_status *pt=&status[n];
        if (strpos(pt->state,client)!=0) continue;
        // entry found. handle it
        return clst_setData(pt,data);
    }
    // arriving here means client not found
    debug(DBG_ERROR,"client '%s' not found",client);
    return -1;
}

char *clst_getData(cl_status *st,int format) {
    // host:state:server:users:load:meminfo:model
    char *csv_template ="%s:%s:%s:%s:%s:%s:%s"; // notice no end of line
    char *json_template = "{\"name\":\"%s\",\"state\":\"%s\",\"server\":\"%s\",\"users\":\"%s\",\"load\":\"%s\",\"meminfo\":\"%s\",\"model\":\"%s\"}";
    char *xml_template = "<client name=\"%s\"><state>%s</state><server>%s</server><users>%s</users><load>%s</load><meminfo>%s</meminfo><model>%s</model></client>";
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
    // compatibility with old clients:
    switch(nelem) {
        case 3:
            snprintf(result,BUFFER_LENGTH,templ,tokens[0],"1",tokens[1],tokens[2],"0.0 / 0.0 / 0.0","0 / 0","-");
            break;
        case 4:
            snprintf(result,BUFFER_LENGTH,templ,tokens[0],tokens[1],tokens[2],tokens[3],"0.0 / 0.0 / 0.0","0 / 0","-");
            break;
        case 6:
            snprintf(result,BUFFER_LENGTH,templ,tokens[0],tokens[1],tokens[2],tokens[3],tokens[4],tokens[5],"-");
            break;
        case 7:
            snprintf(result,BUFFER_LENGTH,templ,tokens[0],tokens[1],tokens[2],tokens[3],tokens[4],tokens[5],tokens[6]);
            break;
        default:
            debug(DBG_ERROR,"Invalid number of tokens (%d) in stored data '%s'",nelem,st->state);
    }
    free_tokens(tokens);
    return result;
}

char *clst_getDataByName(char *client,int format){  /* 0:csv 1:json 2:xml */
    for (int n=0;n<NUM_CLIENTS;n++) { // iterate entries
        cl_status *pt=&status[n];
        if (strpos(pt->state,client)!=0) continue;
        // entry found. handle it
        return clst_getData(pt,format); // found
    }
    debug(DBG_ERROR,"clst_getDataByName():: client '%s' not found",client);
    return NULL; // not found
}

char *clst_getList(int cl_from,int cl_to, int format) {
    char *result=calloc(500,sizeof(char));
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
        free(entry);
        if (n<(cl_to)) { // do not add field separator on last item
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
    }
    // handle footer
    switch(format%3) {
        case 0: /* csv: add nothing */
            str="";  break;
        case 1: /* json: add '\n]' end mark */
            str="\n]"; break;
        case 2: /* xml: add newline and close tag */
            str="\n</statelist>\n";  break;
    }
    result=str_concat(result,str);
    // debug(DBG_TRACE,"getList from:%d to:%d format:%d returns:\n%s",cl_from,cl_to,0,result);
    return result;
}

static int clst_accountData() {
    int state[5]={0,0,0,0,0}; // on off busy unknown total
    int servers[5]={0,0,0,0,0}; // binario1..4 mac
    int users[2]={0,0}; // users percent
    int toklen=0;
    for (int n=50;n<NUM_CLIENTS;n++) { //l000 has global data l001-l049 unused
        cl_status *pt=&status[n];
        debug(DBG_TRACE,"accounting entry '%s'",pt->state);
        // host:state:server:users:load:meminfo:model
        char **tokens=tokenize(pt->state,':',&toklen);
        state[4]++; // increase total host count
        switch(signature(atoi(tokens[1]))) { // eval on/of/unknown
            case -1: state[3]++; break; //unknown
            case 0: state[1]++; break; // off
            case 1: // on
                state[0]++;
                // eval number of users
                if (strcmp(tokens[3],"-")!=0){
                    state[2]++; // add busy count
                    users[0]++; // at least 1 user
                    for (char *c=&tokens[3][0];*c;c++) if (*c==',') users[0]++; // add same users as comma separators
                }
                // on active host, evaluate servers
                switch(tokens[2][strlen(tokens[2])-1]) {
                    case '1': servers[0]++; break; // binario 1
                    case '2': servers[1]++; break; // binario 2
                    case '3': servers[2]++; break; // binario 3
                    case '4': servers[3]++; break; // binario 4
                    case '-': servers[4]++; break; // mac
                }
                break;
        }
        free_tokens(tokens);
    }
    // eval users/hosts percentaje
    users[1]=(100*users[0])/state[4];
    // store evaluated data into 'l000' host slot
    // host:state:server:users:load:meminfo:model
    snprintf(status[0].state,BUFFER_LENGTH,"l000:%d/%d/%d/%d/%d:%d/%d/%d/%d/%d:%d/%d:-:-:-",
             state[0],state[1],state[2],state[3],state[4],
             servers[0],servers[1],servers[2],servers[3],servers[4],
             users[0],users[1]
             // load, meminfo, model goes to defaults
             );
    // notify websockets to get available data
    ws_dataAvailable();
    return state[4]; // number of entries parsed
}

int clst_expireData(){
    int count=0;
    time_t current=time(NULL);
    // l000 is used for global data. state field cannot be parsed by expire
    // l001-l059 are used for virtual clients running in acceso.lab
    for (int n=50;n<NUM_CLIENTS;n++) {
        cl_status *pt=&status[n];
        if ( (current - pt->timestamp) < EXPIRE_TIME ) continue;
        if (strpos (pt->state,":0:-:-")>0) continue; // already expired ( only check state,server,users )
        debug(DBG_TRACE,"Expiring entry '%s'",pt->state);
        // expired. set state to "off". Notice reuse of current buffer.
        char *c=strchr(pt->state,':');
        snprintf(c,BUFFER_LENGTH - ( c - pt->state),":0:-:-:0.0 / 0.0 /0.0:0 / 0:-");
        count++;
    }
    // on change notify websockets
    if (count>0) ws_dataAvailable();
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

void init_expireThread() {
    extern configuration *myConfig;
    // enter loop
    while( myConfig->loop )	{
        clst_expireData();
        sleep(EXPIRE_TIME/2);
        clst_accountData();
        sleep(EXPIRE_TIME/2);
    }
}

#undef IMALIVE_CL_STATE_C