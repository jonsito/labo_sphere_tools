#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include<sys/utsname.h>
#include<utmpx.h>
#include <mntent.h>

#include "im_alive.h"

/* from https://stackoverflow.com/questions/9280759/linux-function-to-get-mount-points */
char *getServer() {
struct mntent *ent;
  char *result="-";
  FILE *aFile = setmntent("/proc/mounts", "r");
  if (aFile == NULL) {
    perror("setmntent");
    return result;
  }
  while (NULL != (ent = getmntent(aFile))) {
      if (!strcmp(ent->mnt_fsname,MOUNT_DEV)) result=strdup(ent->mnt_fsname);
  }
  endmntent(aFile);
  return result;
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
    // remove first comma and return
    return buff+1;
}

int main(int argc, char *argv[]) {

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;

    // creates binary representation of server name
    // and stores it as sin_addr
    // http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html
    inet_pton(AF_INET, SERVER_HOST, &server_address.sin_addr);

    // htons: port in network order format
    server_address.sin_port = htons(SERVER_PORT);

    // open socket
    int sock;
    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("could not create socket");
        return 1;
    }
    char *binario=getServer();

    // data that will be sent to the server
    char* data_to_send = calloc(BUFFER_LENGTH,sizeof(char));
    char hostname[100];
    gethostname(hostname,100);
    snprintf(data_to_send,BUFFER_LENGTH-1,"%s:%s:%s",hostname,binario,getUsers());
    // send data
    int len = sendto(sock, data_to_send, strlen(data_to_send), 0,
                   (struct sockaddr*)&server_address, sizeof(server_address));

    // received echoed data back
    char response[BUFFER_LENGTH];
    recvfrom(sock, response, len, 0, NULL, NULL);

    response[len] = '\0';
    printf("recieved: '%s'\n", response);

    // close the socket
    close(sock);
    return 0;
}