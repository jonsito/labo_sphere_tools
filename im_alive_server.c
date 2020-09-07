//
// Created by jantonio on 27/8/20.
//

#include <arpa/inet.h>
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
#include "client_state.h"

configuration myConfig;

static void sig_handler(int sig) {
    switch (sig) {
        case SIGTERM: myConfig.loop=0; break;
        case SIGUSR1: myConfig.period= MAX(0,MIN(myConfig.period+30,300));  break;
        case SIGUSR2: myConfig.period= MAX(0,MIN(myConfig.period-30,300));  break;
        default: debug(DBG_ERROR,"unexpected signal '%d' received\n",sig); break;
    }
}

static int usage(char *progname) {
    fprintf(stderr,"%s command line options:\n",progname);
    fprintf(stderr,"\t -p port      UDP/WSS Server port [%d] \n",SERVER_PORT);
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
    while ((option = getopt(argc, argv,"p:l:f:t:hdv")) != -1) {
        switch (option) {
            case 'p' : config->server_port = (int) strtol(optarg, NULL, 10); break;
            case 'l' : config->log_level = (int) strtol(optarg, NULL, 10) % 9; break;
            case 't' : config->expire = MAX(0, MIN((int) strtol(optarg, NULL, 10), 300)); break;
            case 'f' : config->log_file = strdup(optarg); break;
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

    // default configuration
    myConfig.server_port=SERVER_PORT;
    myConfig.log_file=strdup(LOG_FILE);
    myConfig.log_level=3;
    myConfig.verbose=0;    // also send logging to stderr 0:no 1:yes
    myConfig.daemon=0;     // run in background
    myConfig.loop=1;
    myConfig.period=DELAY_LOOP;    // 1 minute loop
    myConfig.expire=EXPIRE_TIME;    // 2 minute loop

    if ( parse_cmdline(&myConfig,argc,argv)<0) {
        fprintf(stderr,"error parsing cmd line options");
        usage(argv[0]);
        return 1;
    }
    debug_init(&myConfig);

    // socket address used for the server
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;

    // htons: host to network short: transforms a value in host byte
    // ordering format to a short value in network byte ordering format
    server_address.sin_port = htons(SERVER_PORT);

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

    clst_initData();
    // take care on term signal to end loop
    signal(SIGTERM, sig_handler);

    // become daemon if requested to do so
    if (myConfig.daemon) {
        pid_t pid=fork();
        switch (pid) {
            case -1: // error
                debug(DBG_ERROR,"fork");
                return 1;
            case 0: // child
                setsid();
                break;
            default: // parent
                debug(DBG_INFO,"fork pid:%d",pid);
                return 0;
        }
    }

    // run indefinitely
    time_t last_expire=time(NULL);
    while (myConfig.loop) {
        char buffer[500];
        char name[32];

        // read content into buffer from an incoming client
        int len = recvfrom(sock, buffer, sizeof(buffer), 0,
                           (struct sockaddr *)&client_address,
                           &client_address_len);
        // evaluate timestamp of received data
        time_t tstamp=time(NULL);
        if (len<0) {
            debug(DBG_TRACE,"recvfrom timeout/error");
        } else {
            // inet_ntoa prints user friendly representation of the ip address
            buffer[len] = '\0';
            printf("Time:%lu Received:'%s' from client:'%s'\n", tstamp,buffer, inet_ntoa(client_address.sin_addr));
            // extract host name
            snprintf(name,32,"%s",buffer);
            name[31]='\0';
            if (strpos(name,":")>0) name[strpos(name,":")]='\0';
            // insert into state table
            clst_setDataByName(name,buffer);
            // send same content back to the client ("echo")
            sendto(sock, buffer, len, 0, (struct sockaddr *)&client_address, sizeof(client_address));
        }
        // every DELAY_LOOP perform expire
        if ( (tstamp-last_expire) > EXPIRE_TIME ) {
            last_expire=tstamp;
            debug(DBG_TRACE,"Enter expiration routine");
            clst_expireData();
        }
    }

    clst_freeData();
    return 0;
}
