#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#else
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <sys/sysinfo.h>
#endif
#include <unistd.h>
#include <utmpx.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

#include "im_alive.h"
#include "debug.h"

struct interface {
    int     flags;      /* IFF_UP etc. */
    long    speed;      /* Mbps; -1 is unknown */
    int     duplex;     /* DUPLEX_FULL, DUPLEX_HALF, or unknown */
    char    name[IF_NAMESIZE + 1];
};

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
    static char *result=NULL;
    if (result) return result; // already evaluated
    struct hostent *ent;
    sethostent(0);
    while ( (ent=gethostent()) ) {
          if (strcmp(ent->h_name,BINARIO)!=0) continue;
          endhostent();
          struct hostent *ent2= gethostbyaddr( ent->h_addr_list[0], sizeof(ent->h_addr_list[0]), AF_INET);
          char *dot=strchr(ent2->h_name,'.');
          if (dot) *dot='\0'; // remove FQDN part if any
          result=calloc(1+strlen(ent2->h_name),sizeof(char));
          strncpy(result,ent2->h_name,strlen(ent2->h_name));
          return result;
    }
    endhostent();
    result="-";
    return result;
}

/*
 * to register gdm sessions don't forget to include /etc/gdm/PreSession/Default
 * sessreg -a -w /var/log/wtmp -u /var/run/utmp -x "$X_SERVERS" -h "$REMOTE_HOST" -l "$DISPLAY" "$USER"
 */

/* from https://stackoverflow.com/questions/31472040/program-to-display-all-logged-in-users-via-c-program-on-ubuntu */
char * getUsers() {
    static char *buff=NULL;
    struct utmpx *n;
    char where[6];
    if (!buff) buff=calloc(1024,sizeof(char));
    if (!buff) return "-";
    memset(buff,0,1024);
    setutxent();
    n=getutxent();
    while(n) {
        if(n->ut_type==USER_PROCESS) {
            // insert into list if not already inserted
            if (!strstr(buff,&n->ut_user[0])) {
                // look for access type (G)raphics,(C)console,(R)remote,(S)sh
                if (strlen(n->ut_host)==0) snprintf(where,6,"(txt)"); // tty console
                else if (strstr(&n->ut_host[0],"127")) snprintf(where,6,"(rem)"); // XVnc
                else if (strstr(&n->ut_host[0],":")) snprintf(where,6,"(con)"); // Console
                else snprintf(where,6,"(ssh)"); // remote ssh access
                size_t len=strlen(buff);
                snprintf(buff+len,1000-len,",%s %s",n->ut_user,where);
            }
        }
        n=getutxent();
    }
    endutxent();
    if (*buff=='\0') return "-";
    // notice that browser will translate "," to "<br/> on returning text, so don't skip first comma
    return buff;
}

// call to uptime. return number of seconds from client start
long getUptime() {
#ifdef __APPLE__
    // from https://stackoverflow.com/questions/3269321/osx-programmatically-get-uptime
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    if( sysctl(mib, 2, &boottime, &len, NULL, 0) < 0 ) return -1;
    time_t bsec = boottime.tv_sec; // running time
    time_t csec = time(NULL); // current time
    return (long) difftime(csec, bsec);
#else
    double uptime_secs=1;
    size_t num_bytes_read;
    int fd;
    char read_buf[128];
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
    if(sscanf(read_buf, "%lf %lf", &(uptime_secs), &(idle_secs)) != 2) {
        debug(DBG_ERROR,"Erron on sscanf /proc/uptime, %s, expected two floats", strerror(errno));
    }
    close(fd);
    return (long) uptime_secs;
#endif
}

char *getComputerModel() {
    static char buffer[64];
    memset(buffer,0,sizeof(buffer));
#ifdef __APPLE__
    size_t len = 0;
    int mib[2]={CTL_HW,HW_MODEL};
    // according to doc need first to eval length
    sysctl(mib,2, NULL, &len, NULL, 0);
    if ((len==0)||(len>63)) {
        debug(DBG_ERROR,"cannot retrieve hardware model");
        snprintf(buffer,63,"Mac-unknown");
    } else {
        // and then get data
        sysctl(mib,2, buffer, &len, NULL, 0);
    }
#else
    FILE *fp=popen("dmidecode -s system-product-name", "r");
    if (fp == NULL) {
        debug(DBG_ERROR,"cannot retrieve (dmidecode) hardware model");
        snprintf(buffer,63,"PC-unknown");
        return buffer;
    }
    /* Read the output a line at a time - output it. */
    if (! fgets(buffer, sizeof(buffer), fp)) {
        debug(DBG_ERROR,"Dmidecode returns no Product Name info");
        snprintf(buffer,63,"PC-unknown");
    }
    /* close */
    pclose(fp);
    // dmidecode returns data ending with '\n', so remove it
    buffer[strlen(buffer)-1]='\0';
#endif
    return buffer;
}

#ifdef __APPLE__
char *getIfStatus() {
    debug(DBG_INFO,"Cannot evaluate network parameters in Mac-OSX");
    return "-";
}
#else
static int get_interface_common(const int fd, struct ifreq *const ifr, struct interface *const info) {
    struct ethtool_cmd  cmd;
    int                 result;
    /* Interface flags. */
    if (ioctl(fd, SIOCGIFFLAGS, ifr) == -1) info->flags = 0;
    else info->flags = ifr->ifr_flags;
    /* "Get interface settings" */
    ifr->ifr_data = (void *)&cmd;
    cmd.cmd = ETHTOOL_GSET;
    if (ioctl(fd, SIOCETHTOOL, ifr) == -1) { // ioctl failed: set parameters to unknown
        info->speed = -1L;
        info->duplex = DUPLEX_UNKNOWN;
    } else { // ioctl success: set parameters
        info->speed = ethtool_cmd_speed(&cmd);
        info->duplex = cmd.duplex;
    }
    // try to close socket
    do { result = close(fd); } while (result == -1 && errno == EINTR);
    if (result == -1) return errno;
    return 0;
}

char *getIfStatus() {
    static char buffer[48]; // where result is stored
    struct interface iface;
    struct ifreq ifr;
    int result;
    // creamos un socket para ir buscando interfaces
    int socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socketfd == -1) {
        debug(DBG_ERROR,"Cannot create socket to iterate interfaces");
        snprintf(buffer,sizeof(buffer),"-");
        return buffer;
    }
    for (int index = 1; ; index++) { // buscamos el interface con indice index
        ifr.ifr_ifindex = index;
        if (ioctl(socketfd, SIOCGIFNAME, &ifr) == -1) { // search interface at index "index"
            // arriving here means that there are no more interfaces
            do { result = close(socketfd); } while (result == -1 && errno == EINTR);
            debug(DBG_ERROR,"Cannot find more interfaces. index:%d",index);
            snprintf(buffer,sizeof(buffer),"-");
            return buffer;
        }
        strncpy(iface.name, ifr.ifr_name, IF_NAMESIZE);
        iface.name[IF_NAMESIZE] = '\0';

        // si no tiene la ip que buscamos, continuamos la busqueda
        ioctl(socketfd, SIOCGIFADDR, &ifr);
        if (!strstr(inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),"138.4.3")) continue;

        // obtenemos datos y cerramos socket
        result=get_interface_common(socketfd, &ifr, &iface);
        if (result!=0) {
            debug(DBG_ERROR,"Cannot get interface data '%s'",iface.name);
            snprintf(buffer,sizeof(buffer),"%s ? Mbps ? duplex",iface.name);
            return buffer;
        }
        // ok: evaluamos velocidad, modo y retornamos valores
        snprintf(buffer,sizeof(buffer),"%s%s %ld Mbps %s duplex",
                iface.name,
                (iface.flags & IFF_UP)? " up":"",
                (iface.speed > 0)? iface.speed:0,
                (iface.duplex == DUPLEX_FULL)? "full": ((iface.duplex == DUPLEX_HALF)?"half":"?")
                );
        debug(DBG_INFO,"Interface index: %d status %s",index,buffer);
        return buffer;
    }
    // arriving here means error. should never happen
    debug(DBG_ALERT,"Cannot locate any network interface !!!");
    sprintf(buffer,"-");
    return buffer;
}
#endif

char *getLoad() {
    static char buffer[32];
    double load[3];
    memset(buffer,0,sizeof(buffer));
    if (getloadavg(load,3)<0) {
        debug(DBG_ERROR,"cannot get loadavg");
        snprintf(buffer,32,"0.0 / 0.0 / 0.0");
    } else {
        snprintf(buffer,32,"%.2f / %.2f / %.2f",load[0],load[1],load[2]);
    }
    return buffer;
}

#ifdef __APPLE__
// from https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
char *getMemInfo() {
    static char buffer[32];

    // get physical memory
    int mib[2];
    int64_t physical_memory;
    size_t lenght;
    mib[0]=CTL_HW;
    mib[1]=HW_MEMSIZE;
    lenght= sizeof(int64_t);
    sysctl(mib,2, &physical_memory,&lenght,NULL,0);

    // get used memory
    vm_size_t page_size;
    mach_port_t mach_port;
    mach_msg_type_number_t count;
    vm_statistics64_data_t vm_stats;
    // long long free_memory=0;
    long long used_memory=0;

    mach_port = mach_host_self();
    count = sizeof(vm_stats) / sizeof(natural_t);
    if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
        KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO, (host_info64_t)&vm_stats, &count))  {
            // free_memory = (int64_t)vm_stats.free_count * (int64_t)page_size;
            used_memory = ((int64_t)vm_stats.active_count +
                                 (int64_t)vm_stats.inactive_count +
                                 (int64_t)vm_stats.wire_count) *  (int64_t)page_size;
    }
    // return data in kilobytes
    snprintf(buffer,32,"%lld / %lld", used_memory/1024,physical_memory/1024);
    return buffer;
}

#else
// Linux: used memory evaluated as https://github.com/brndnmtthws/conky/issues/130
char *getMemInfo() {
    static char buffer[32];
    struct sysinfo s;
    int res=sysinfo(&s);
    if (res<0) {
        debug(DBG_ERROR,"cannot get memory information");
        snprintf(buffer,32,"0 / 0");
    } else {
        debug(DBG_DEBUG,"totalram:%ld used:%ld buffers:%ld",s.totalram,s.freeram,s.bufferram);
        // return data in kilobytes.
        snprintf(buffer,32,"%ld / %ld",
                 ( (s.totalram - s.freeram - s.bufferram ) * s.mem_unit ) / 1024,
                 ( s.totalram * s.mem_unit ) / 1024 );
    }
    return buffer;
}

#endif

char * getHostName() {
    if (myConfig.client_host) return myConfig.client_host; // already evaluated
    // extract client host name
    char hostname[100];
    memset(hostname,0,sizeof(hostname));
    gethostname(hostname,100);
    char *dot=strchr(hostname,'.');
    if (dot) *dot='\0'; // remove FQDN part if any on hostname
    myConfig.client_host=calloc(1+strlen(hostname),sizeof(char));
    strncpy(myConfig.client_host,hostname,strlen(hostname));
    return myConfig.client_host;
}

static int usage(char *progname) {
    fprintf(stderr,"%s command line options:\n",progname);
    fprintf(stderr,"\t -c client    Client hostname [auto]\n");
    fprintf(stderr,"\t -h host      Server hostname [%s]\n",SERVER_HOST);
    fprintf(stderr,"\t -p port      UDP Server port [%d]\n",SERVER_UDPPORT);
    fprintf(stderr,"\t -w port      WSS Server port [%d]\n",SERVER_WSSPORT);
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
            case 'c' : config->client_host = strdup(optarg);     break;
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
    server_address.sin_port = htons(myConfig.server_udpport);

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

    // retrieve computer model
    char computermodel[100];
    memset(computermodel,0,sizeof(computermodel));
    snprintf(computermodel,sizeof(computermodel),"%s",getComputerModel());

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

    // create pid file
    pid_t pid=getpid();
    FILE *fp=fopen("/var/run/ImAlive_client.pid","w");
    if (!fp) debug(DBG_ALERT,"Cannot create pid file %d",pid);
    else { fprintf(fp,"%d",pid); fclose(fp); }

    // enter loop. exit on kill or SIGTERM
    while (myConfig.loop) {
        // compose string to be sent
        // snprintf(data_to_send,BUFFER_LENGTH-1,"%s:%ld:%s:%s",hostname,getUptime(),binario,getUsers());
        snprintf(data_to_send,BUFFER_LENGTH-1,"%s:%ld:%s:%s:%s:%s:%s:%s",
                 getHostName(),
                 getUptime(),
                 getServer(),
                 getUsers(),
                 getLoad(),
                 getMemInfo(),
                 computermodel,
                 getIfStatus()
                 );
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
    // remove pid file and exit
    unlink("/var/run/ImAlive_client.pid");
    return 0;
}