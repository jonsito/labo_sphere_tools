//
// This code is a "must" for every socket programmer oldman :-)
//

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
#include <errno.h>

#define SOCKETS_C
#include "sockets.h"
#include "debug.h"

/*------------------------------------------------------------------------
 * connectsock - allocate & connect a socket using TCP or UDP
 *------------------------------------------------------------------------
 * Arguments:
 *      host      - name of host to which connection is desired
 *      service   - service associated with the desired port
 *      transport - name of transport protocol to use ("tcp" or "udp")
 */
int connectsock(const char *host, const char *service, const char *transport ) {
    struct hostent	*phe;	/* pointer to host information entry	*/
    struct servent	*pse;	/* pointer to service information entry	*/
    struct protoent *ppe;	/* pointer to protocol information entry*/
    struct sockaddr_in sin;	/* an Internet endpoint address		*/
    int	s, type;	/* socket descriptor and socket type	*/

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    /* Map service name to port number */
    if ( (pse = getservbyname(service, transport)) )
        sin.sin_port = pse->s_port;
    else if ((sin.sin_port=htons((unsigned short)atoi(service))) == 0){
        debug(DBG_ERROR,"can't get \"%s\" service entry\n", service);
        return -1;
    }

    /* Map host name to IP address, allowing for dotted decimal */
    if ( (phe = gethostbyname(host)) )
        memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
    else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE ) {
        debug(DBG_ERROR,"can't get \"%s\" host entry", host);
        return -1;
    }

    /* Map transport protocol name to protocol number */
    if ( (ppe = getprotobyname(transport)) == 0){
        debug(DBG_ERROR,"can't get \"%s\" protocol entry", transport);
        return -1;
    }

    /* Use protocol to choose a socket type */
    type= (strcmp(transport, "udp") == 0)? SOCK_DGRAM: SOCK_STREAM;

    /* Allocate a socket */
    s = socket(PF_INET, type, ppe->p_proto);
    if (s < 0) {
        debug(DBG_ERROR,"can't create socket: %s", strerror(errno));
        return -1;
    }

    /* Connect the socket */
    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        debug(DBG_ERROR,"can't connect to %s.%s: %s", host, service,strerror(errno));
        return -1;
    }
    return s;
}

/*------------------------------------------------------------------------
 * connectTCP - connect to a specified TCP service on a specified host
 *------------------------------------------------------------------------
 * Arguments:
 *      host    - name of host to which connection is desired
 *      service - service associated with the desired port
 */
int connectTCP(const char *host, const char *service ) {
    return connectsock( host, service, "tcp");
}

/*------------------------------------------------------------------------
 * connectUDP - connect to a specified UDP service on a specified host
 *------------------------------------------------------------------------
 * Arguments:
 *      host    - name of host to which connection is desired
 *      service - service associated with the desired port
 */
int connectUDP(const char *host, const char *service ) {
    return connectsock(host, service, "udp");
}

/*------------------------------------------------------------------------
 * passivesock - allocate & bind a server socket using TCP or UDP
 *------------------------------------------------------------------------
 * Arguments:
 *      service   - service associated with the desired port
 *      transport - transport protocol to use ("tcp" or "udp")
 *      qlen      - maximum server request queue length
 */
int passivesock(const char *service, const char *transport, int qlen) {
    struct servent	*pse;	/* pointer to service information entry	*/
    struct protoent *ppe;	/* pointer to protocol information entry*/
    struct sockaddr_in sin;	/* an Internet endpoint address		*/
    int	s, type;	/* socket descriptor and socket type	*/

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;

    /* Map service name to port number */
    if ( (pse = getservbyname(service, transport)) )
        sin.sin_port = htons(ntohs((unsigned short)pse->s_port));
    else if ((sin.sin_port=htons((unsigned short)atoi(service))) == 0) {
        debug(DBG_ERROR,"can't get \"%s\" service entry", service);
        return -1;
    }

    /* Map protocol name to protocol number */
    if ( (ppe = getprotobyname(transport)) == 0) {
        debug(DBG_ERROR,"can't get \"%s\" protocol entry", transport);
        return -1;
    }

    /* Use protocol to choose a socket type */
    type= (strcmp(transport, "udp") == 0)? SOCK_DGRAM: SOCK_STREAM;

    /* Allocate a socket */
    s = socket(PF_INET, type, ppe->p_proto);
    if (s < 0){
        debug(DBG_ERROR,"can't create socket: %s", strerror(errno));
        return -1;
    }

    /* Bind the socket */
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        debug(DBG_ERROR,"can't bind to %s port: %s", service,strerror(errno));
        return -1;
    }
    if (type == SOCK_STREAM && listen(s, qlen) < 0) {
        debug(DBG_ERROR,"can't listen on %s port: %s", service, strerror(errno));
        return -1;
    }
    return s;
}

/*------------------------------------------------------------------------
 * passiveUDP - create a passive socket for use in a UDP server
 *------------------------------------------------------------------------
 * Arguments:
 *      service - service associated with the desired port
 */
int passiveUDP(const char *service) {
    return passivesock(service, "udp", 0);
}


/*------------------------------------------------------------------------
 * passiveTCP - create a passive socket for use in a TCP server
 *------------------------------------------------------------------------
 * Arguments:
 *      service - service associated with the desired port
 *      qlen    - maximum server request queue length
 */
int passiveTCP(const char *service, int qlen) {
    return passivesock(service, "tcp", qlen);
}
