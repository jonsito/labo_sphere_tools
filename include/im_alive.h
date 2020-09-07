#ifndef IM_ALIVE_CLIENT_H
#define IM_ALIVE_CLIENT_H

// #define DEBUG

#define SERVER_PORT 8877
#define SERVER_HOST "acceso.lab.dit.upm.es"
#define BINARIO "server"
#define EXPIRE_TIME 90 /* 1.5 minutes expire timeout on server*/
#define DELAY_LOOP 60 /* 1 minute poll period on client */
#define BUFFER_LENGTH 512

typedef struct configuracion_st {
    // server options
    char *server_host;
    int server_port;
    // debug options
    char *log_file;  // log file
    int log_level;   // log level 0:none 1:panic 2:alert 3:error 4:notice 5:info 6:debug 7:trace 8:all
    // runtime options
    int verbose;    // also send logging to stderr 0:no 1:yes
    int daemon;     // run in background
    int period;     // client poll loop period (seconds)
    int expire;     // server expire loop period (seconds)
    int loop;       // flago to mark end of loop
} configuration;


#endif //IM_ALIVE_CLIENT_H
