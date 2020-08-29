//
// Created by jantonio on 2/06/19.
//

#ifndef IMALIVE_SOCKETS_H
#define IMALIVE_SOCKETS_H

#include <sys/types.h>
#ifndef __WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#else
#include <winsock2.h>
#endif
#include <string.h>
#include <stdlib.h>

#ifndef	INADDR_NONE
#define	INADDR_NONE	0xffffffff
#endif	/* INADDR_NONE */

#ifndef IMALIVE_SOCKETS_C
#define EXTERN extern
#else
#define EXTERN
#endif

EXTERN int connectsock(const char *host, const char *service, const char *transport );
EXTERN int connectTCP(const char *host, const char *service );
EXTERN int connectUDP(const char *host, const char *service );
EXTERN int passivesock(const char *service, const char *transport, int qlen);
EXTERN int passiveUDP(const char *service);
EXTERN int passiveTCP(const char *service, int qlen);

#undef EXTERN

#endif //IMALIVE_SOCKETS_H
