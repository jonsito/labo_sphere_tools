#ifndef IMALIVE_IM_ALIVE_CLIENT_H
#define IMALIVE_IM_ALIVE_CLIENT_H

// #define DEBUG

#define SERVER_PORT 8877
#ifdef DEBUG
#define SERVER_HOST "localhost"
#define BINARIO "osito"
#else
#define SERVER_HOST "acceso.lab.dit.upm.es"
#define BINARIO "server"
#endif

#define DELAY_LOOP 10
#define BUFFER_LENGTH 1024

#endif //IMALIVE_IM_ALIVE_CLIENT_H
