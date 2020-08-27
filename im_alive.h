#ifndef IMALIVE_IM_ALIVE_CLIENT_H
#define IMALIVE_IM_ALIVE_CLIENT_H

#define SERVER_PORT 8877
#ifdef DEBUG
#define SERVER_HOST "localhost"
#define MOUNT_DEV "/dev/md127"
#else
#define SERVER_HOST "acceso.lab.dit.upm.es"
#define MOUNT_DEV "/dev/nbd0"
#endif

#define BUFFER_LENGTH 1024

#endif //IMALIVE_IM_ALIVE_CLIENT_H
