//
// Created by jantonio on 4/9/20.
//

#ifndef WSSERVER_H
#define WSSERVER_H

#ifndef WSSERVER_C
#define EXTERN extern
#else
#define EXTERN
#endif

#include <libwebsockets.h>
#define IMALIVE_RX_BUFFER_BYTES (256)
struct payload {
    unsigned char data[LWS_SEND_BUFFER_PRE_PADDING + IMALIVE_RX_BUFFER_BYTES + LWS_SEND_BUFFER_POST_PADDING];
    size_t len;
};

enum protocols {
    PROTOCOL_HTTP = 0,
    PROTOCOL_IMALIVE,
    PROTOCOL_COUNT
};

EXTERN void init_wsService( configuration *myConfig );

#undef EXTERN
#endif //WSSERVER_H
