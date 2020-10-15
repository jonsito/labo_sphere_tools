//
// Created by jantonio on 27/8/20.
//

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/param.h>

#include "im_alive.h"
#include "getopt.h"
#include "debug.h"
#include "tools.h"
#include "threads.h"
#include "client_state.h"
#include "wsserver.h"

configuration *myConfig;

static void sig_handler(int sig) {
    switch (sig) {
        case SIGTERM: myConfig->loop=0; break;
        case SIGUSR1: myConfig->period= MAX(0,MIN(myConfig->period+30,300));  break;
        case SIGUSR2: myConfig->period= MAX(0,MIN(myConfig->period-30,300));  break;
        default: debug(DBG_ERROR,"unexpected signal '%d' received\n",sig); break;
    }
}

static int usage(char *progname) {
    fprintf(stderr,"%s command line options:\n",progname);
    fprintf(stderr,"\t -p port      UDP Server port [%d] \n",SERVER_UDPPORT);
    fprintf(stderr,"\t -w port      WSS Server port [%d] \n",SERVER_WSSPORT);
    fprintf(stderr,"\t -c cert      SSL certificate file [%s] \n",SSL_CERT_FILE_PATH);
    fprintf(stderr,"\t -k key       SSL private key file [%s] \n",SSL_PRIVATE_KEY_PATH);
    fprintf(stderr,"\t -l log_level Set debug/logging level 0:none thru 8:all. [3:error]\n");
    fprintf(stderr,"\t -f log_file  Set log file [%s]\n",LOG_FILE);
    fprintf(stderr,"\t -t period    Set expire loop period [%d] (secs)\n",EXPIRE_TIME);
    fprintf(stderr,"\t -d           Run in backgroud (daemon). Implies no verbose [0]\n");
    fprintf(stderr,"\t -v           Send debug to stderr (verbose). Implies no daemon [0]\n");
    fprintf(stderr,"\t -? | -h      Show this help\n");
    return 0;
}

static int parse_cmdline(configuration *config,int argc, char *argv[]) {
    int option;
    while ((option = getopt(argc, argv,"p:w:l:f:t:c:k:hdv")) != -1) {
        switch (option) {
            case 'p' : config->server_udpport = (int) strtol(optarg, NULL, 10); break;
            case 'w' : config->server_wssport = (int) strtol(optarg, NULL, 10); break;
            case 'l' : config->log_level = (int) strtol(optarg, NULL, 10) % 9; break;
            case 't' : config->expire = MAX(0, MIN((int) strtol(optarg, NULL, 10), 300)); break;
            case 'f' : config->log_file = strdup(optarg); break;
            case 'c' : config->ssl_cert_file_path = strdup(optarg); break;
            case 'k' : config->ssl_key_file_path = strdup(optarg); break;
            case 'v' : config->verbose = 1; config->daemon = 0; break;
            case 'd' : config->verbose = 0; config->daemon = 1; break;
            case 'h' :
            case '?' : usage(argv[0]); exit(0);
            default: return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    char *expire_th_name="expirer";
    char *websock_th_name="websockets";
    myConfig=calloc(1,sizeof(configuration));
    // default configuration
    myConfig->server_udpport=SERVER_UDPPORT;
    myConfig->server_wssport=SERVER_WSSPORT;
    myConfig->log_file=strdup(LOG_FILE);
    myConfig->log_level=3;
    myConfig->verbose=0;    // also send logging to stderr 0:no 1:yes
    myConfig->daemon=0;     // run in background
    myConfig->loop=1;
    myConfig->period=DELAY_LOOP;    // 1 minute loop
    myConfig->expire=EXPIRE_TIME;    // 2 minute loop
    myConfig->ssl_cert_file_path=SSL_CERT_FILE_PATH;
    myConfig->ssl_key_file_path=SSL_PRIVATE_KEY_PATH;
    set_debug_level(myConfig->log_level);

    if ( parse_cmdline(myConfig,argc,argv)<0) {
        fprintf(stderr,"error parsing cmd line options");
        usage(argv[0]);
        return 1;
    }
    debug_init(myConfig);

    // socket address used for the server
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;

    // htons: host to network short: transforms a value in host byte
    // ordering format to a short value in network byte ordering format
    server_address.sin_port = htons(SERVER_UDPPORT);

    // htons: host to network long: same as htons but to long
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    // create a UDP socket, creation returns -1 on failure
    int sock;
    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("could not create socket\n");
        return 1;
    }

    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0; /*  wait for response 10 seg. otherwise ignore */
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        debug(DBG_ERROR,"setsockopt");
        return 1;
    }

    // bind it to listen to the incoming connections on the created server
    // address, will return -1 on error
    if ((bind(sock, (struct sockaddr *)&server_address,
              sizeof(server_address))) < 0) {
        printf("could not bind socket\n");
        return 1;
    }

    // socket address used to store client address
    struct sockaddr_in client_address;
    int client_address_len = 0;

    // take care on term signal to end loop
    signal(SIGTERM, sig_handler);

    // become daemon if requested to do so
    if (myConfig->daemon) {
        pid_t pid=fork();
        switch (pid) {
            case -1: // error
                debug(DBG_ERROR,"fork");
                return 1;
            case 0: // child
                fclose(stdin);
                fclose(stdout);
                fclose(stderr);
                setsid();
                break;
            default: // parent
                debug(DBG_INFO,"fork pid:%d",pid);
                return 0;
        }
    }

    // create pid file
    pid_t pid=getpid();
    FILE *fp=fopen("/var/run/ImAlive_server.pid","w");
    if (!fp) debug(DBG_ALERT,"Cannot create pid file %d",pid);
    else { fprintf(fp,"%d",pid); fclose(fp); }

    // initialize status table data
    clst_initData();

    // fireup expire thread
    sc_thread_slot *exp_slot=sc_thread_create(expire_th_name,myConfig,init_expireThread);
    if (!exp_slot) {
        debug(DBG_ERROR,"Cannot create alive expire thread");
        exit(1);
    }
    // fireup websocket thread
    sc_thread_slot *wss_slot=sc_thread_create(websock_th_name,myConfig,init_wsService);
    if (!wss_slot) {
        debug(DBG_ERROR,"Cannot create WSS server thread");
        exit(1);
    }
    // run until sigterm received
    time_t last_expire=time(NULL);
    while (myConfig->loop) {
        char buffer[500];
        char name[32];

        // read content into buffer from an incoming client
        size_t len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_address,&client_address_len);
        // evaluate timestamp of received data
        time_t tstamp=time(NULL);
        if (len<0) {
            debug(DBG_TRACE,"recvfrom timeout/error");
        } else {
            // inet_ntoa prints user friendly representation of the ip address
            buffer[len] = '\0';
            // debug( DBG_TRACE,"Time:%lu Received:'%s' from client:'%s'", tstamp,buffer, inet_ntoa(client_address.sin_addr));
            // take care on old ( no uptime provided ) client protocol
            int nelem=0;
            char **tokens=tokenize(buffer,':',&nelem);
            switch(nelem) {
                case 3: // initial protocol was lxxx:binarioX:users
                    snprintf(buffer,500,"%s:1:%s:%s:0.0 / 0.0 / 0.0:0 / 0:-",tokens[0],tokens[1],tokens[2]);
                    break;
                case 4: // prev protocol is lxxx:uptime:binarioX:users
                    snprintf(buffer,500,"%s:%s:%s:%s:0.0 / 0.0 / 0.0:0 / 0:-",tokens[0],tokens[1],tokens[2],tokens[3]);
                    break;
                case 6: // new protocol is lxxx:uptime:binarioX:users:loadavg:meminfo
                    snprintf(buffer,500,"%s:%s:%s:%s:%s:%s:-",tokens[0],tokens[1],tokens[2],tokens[3],tokens[4],tokens[5]);
                    break;
                case 7: // extended lxxx:uptime:binarioX:users:loadavg:meminfo:computermodel
                    snprintf(buffer,500,"%s:%s:%s:%s:%s:%s:%s",tokens[0],tokens[1],tokens[2],tokens[3],tokens[4],tokens[5],tokens[6]);
                    break;
                default:
                    debug(DBG_ERROR,"invalid data format received:'%s'",buffer);
                    break;
            }
            free_tokens(tokens);
            // extract host name. A bit dirty, as asume fixed name format lxxx, but works
            memset(name,0,sizeof(name));
            memcpy(name,buffer,4);
            // insert into state table
            int res=clst_setDataByName(name,buffer);
            if (res>=0) ws_dataAvailable();
            // send same content back to the client ("echo")
            sendto(sock, buffer, len, 0, (struct sockaddr *)&client_address, sizeof(client_address));
        }
    }
    // clean data
    clst_freeData();
    // remove pid file and exit
    unlink("/var/run/ImAlive_client.pid");
    return 0;
}
