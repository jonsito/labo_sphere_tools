#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utmpx.h>
#include <netdb.h>

#include "im_alive.h"

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

int main(int argc, char *argv[]) {

    struct sockaddr_in server_address;

    // set server_address data
    struct hostent *ent=gethostbyname(SERVER_HOST);
    if (!ent) {
        perror("gethostbyname");
        return 1;
    }
    memset(&server_address, 0, sizeof(server_address));
    memcpy((void *)&server_address.sin_addr, ent->h_addr_list[0], ent->h_length);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);

    // open socket
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000; /*  0.5 seg */
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
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
    if (!data_to_send) {
        perror("calloc");
        return 1;
    }
    /* pending: get a way to end loop */

    while (true) {
        // compose string to be sent
        snprintf(data_to_send,BUFFER_LENGTH-1,"%s:%s:%s",hostname,binario,getUsers());
        // send data
        int len = sendto(sock, data_to_send, strlen(data_to_send), 0,
                         (struct sockaddr*)&server_address, sizeof(server_address));
        // received echoed data back
        char response[BUFFER_LENGTH];
        recvfrom(sock, response, len, 0, NULL, NULL); // timeout at 0.5 segs
        response[len] = '\0';
        fprintf(stderr,"received: '%s'\n", response);
        sleep(DELAY_LOOP);
    }
    // close the socket
    close(sock);
    return 0;
}