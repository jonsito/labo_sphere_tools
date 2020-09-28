#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <utmpx.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include "im_alive.h"
#include "debug.h"

configuration myConfig;

static void sig_handler(int sig) {
    switch (sig) {
        case SIGTERM: myConfig.loop=0; break;
        case SIGUSR1: myConfig.period= MAX(0,MIN(myConfig.period+30,300));  break;
        case SIGUSR2: myConfig.period= MAX(0,MIN(myConfig.period-30,300));  break;
        default: debug(DBG_ERROR,"unexpected signal '%d' received\n",sig); break;
    }
}

char *getServer() {
  struct hostent *ent;
  sethostent(0);
  while ( (ent=gethostent()) ) {
      if (strcmp(ent->h_name,BINARIO)!=0) continue;
      endhostent();
      struct hostent *ent2= gethostbyaddr( ent->h_addr_list[0], sizeof(ent->h_addr_list[0]), AF_INET);
      return ent2->h_name;
  }
  endhostent();
  return "-";
}

/*
 * to register gdm sessions don't forget to include /etc/gdm/PreSession/Default
 * sessreg -a -w /var/log/wtmp -u /var/run/utmp -x "$X_SERVERS" -h "$REMOTE_HOST" -l "$DISPLAY" "$USER"
 */

/* from https://stackoverflow.com/questions/31472040/program-to-display-all-logged-in-users-via-c-program-on-ubuntu */
char * getUsers() {
    struct utmpx *n;
    setutxent();
    n=getutxent();
    char *buff=calloc(1000,sizeof(char));
    if (!buff) return "-";
    while(n) {
        if(n->ut_type==USER_PROCESS) {
            // insert into list if not already inserted
            if (!strstr(buff,n->ut_user)) {
                size_t len=strlen(buff);
                snprintf(buff+len,1000-len,",%s",n->ut_user);
            }
        }
        n=getutxent();
    }
    if (*buff=='\0') return "-";
    // remove first comma and return
    return buff+1;
}

// call to uptime. return number of seconds from client start
long getUptime() {
    size_t num_bytes_read;
    int fd;
    char read_buf[128];
    double uptime_secs;
    double idle_secs;

    fd = open("/proc/uptime", O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH);

    if(fd <0) {
        debug(DBG_ERROR,"cannot open /proc/uptime");
        return -1;
    }
    num_bytes_read=read(fd, read_buf, 128);
    if (num_bytes_read<0) {
        debug(DBG_ERROR,"read() error in /proc/uptime");
        close(fd);
        return -1;
    }
    if(sscanf(read_buf, "%lf %lf", &(uptime_secs), &(idle_secs)) == 2) {
        debug(DBG_ERROR,"Erron on sscanf, %s, line no %d\n", strerror(errno), __LINE__);
        close(fd);
    }
    return (long) uptime_secs;
}

static int usage(char *progname) {
    fprintf(stderr,"%s command line options:\n",progname);
    fprintf(stderr,"\t -h host      Server host [%s]\n",SERVER_HOST);
    fprintf(stderr,"\t -p port      UDP Server port [%d] \n",SERVER_UDPPORT);
    fprintf(stderr,"\t -w port      WSS Server port [%d] \n",SERVER_WSSPORT);
    fprintf(stderr,"\t -l log_level Set debug/logging level 0:none thru 8:all. [3:error]\n");
    fprintf(stderr,"\t -f log_file  Set log file [%s]\n",LOG_FILE);
    fprintf(stderr,"\t -t period    Set loop period [%d] (secs)\n",DELAY_LOOP);
    fprintf(stderr,"\t -d           Run in backgroud (daemon). Implies no verbose [0]\n");
    fprintf(stderr,"\t -v           Send debug to stderr (verbose). Implies no daemon [0]\n");
    fprintf(stderr,"\t -?           Show this help\n");
    return 0;
}

static int parse_cmdline(configuration *config,int argc, char *argv[]) {
    int option;
    while ((option = getopt(argc, argv,"h:w:p:l:f:t:dv")) != -1) {
        switch (option) {
            case 'h' : config->server_host = strdup(optarg);     break;
            case 'p' : config->server_udpport = (int)strtol(optarg,NULL,10);break;
            case 'w' : config->server_wssport = (int)strtol(optarg,NULL,10);break;
            case 'l' : config->log_level = (int)strtol(optarg,NULL,10)%9;  break;
            case 't' : config->period = MAX(0,MIN((int)strtol(optarg,NULL,10),300));  break;
            case 'f' : config->log_file = strdup(optarg);  break;
            case 'v' : config->verbose = 1; config->daemon = 0; break;
            case 'd' : config->verbose = 0; config->daemon = 1; break;
            case '?' : usage(argv[0]); exit(0);
            default: return -1;
        }
    }
    return 0;
}

// im_alive_client [-v] [-d] [ -h host ] [ -p port ] [-l level]
int main(int argc, char *argv[]) {

    struct sockaddr_in server_address;

    // default configuration
    myConfig.server_host=strdup(SERVER_HOST);
    myConfig.server_udpport=SERVER_UDPPORT;
    myConfig.server_wssport=SERVER_WSSPORT;
    myConfig.log_file=strdup(LOG_FILE);
    myConfig.log_level=3;
    myConfig.verbose=0;    // also send logging to stderr 0:no 1:yes
    myConfig.daemon=0;     // run in background
    myConfig.loop=1;
    myConfig.period=DELAY_LOOP;    // 1 minute loop on client
    myConfig.expire=EXPIRE_TIME;    // 90 seconds loop on server

    if ( parse_cmdline(&myConfig,argc,argv)<0) {
        fprintf(stderr,"error parsing cmd line options");
        usage(argv[0]);
        return 1;
    }
    debug_init(&myConfig);

    // set server_address data
    struct hostent *ent=gethostbyname(myConfig.server_host);
    if (!ent) {
        debug(DBG_ERROR,"gethostbyname");
        return -1;
    }
    memset(&server_address, 0, sizeof(server_address));
    memcpy((void *)&server_address.sin_addr, ent->h_addr_list[0], ent->h_length);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_UDPPORT);

    // open socket
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        debug(DBG_ERROR,"socket");
        return 1;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000; /*  wait for response 0.5 seg. otherwise ignore */
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        debug(DBG_ERROR,"setsockopt");
        return 1;
    }

    // extract binario nbd server
    char *binario=getServer();
    char *dot=strchr(binario,'.');
    if (dot) *dot='\0'; // remove FQDN part if any

    // extract client host name
    char hostname[100];
    gethostname(hostname,100);
    dot=strchr(hostname,'.');
    if (dot) *dot='\0'; // remove FQDN part if any on hostname

    char* data_to_send = calloc(BUFFER_LENGTH,sizeof(char));
    char* response = calloc(BUFFER_LENGTH,sizeof(char));
    if (!data_to_send) { debug(DBG_ERROR,"calloc(data_to_send"); return 1; }
    if (!response) { debug(DBG_ERROR,"calloc(response)"); return 1; }

    // take care on term signal to end loop
    signal(SIGTERM, sig_handler);

    // become daemon if requested
    if (myConfig.daemon) {
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

    time_t start_time=time(NULL);

    // enter loop. exit on kill or SIGTERM
    while (myConfig.loop) {
        // compose string to be sent
        // snprintf(data_to_send,BUFFER_LENGTH-1,"%s:%ld:%s:%s",hostname,getUptime(),binario,getUsers());
        snprintf(data_to_send,BUFFER_LENGTH-1,"%s:%ld:%s:%s",hostname,start_time,binario,getUsers());
        // send data
        debug(DBG_INFO,"sent: '%s'\n", data_to_send);
        sendto(sock, data_to_send, strlen(data_to_send), 0,(struct sockaddr*)&server_address, sizeof(server_address));
        // received echoed data back
        int len=recvfrom(sock, response, BUFFER_LENGTH, 0, NULL, NULL); // timeout at 0.5 segs
        if (len<0) {
            debug(DBG_ERROR,"recvfrom",strerror(errno));
        } else {
            response[len] = '\0';
            debug(DBG_INFO,"received: '%s'\n", response);
        }
        sleep(DELAY_LOOP);
    }
    // close the socket
    close(sock);
    return 0;
}