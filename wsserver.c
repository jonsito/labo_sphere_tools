//
// Created by jantonio on 15/9/20.
// from https://raw.githubusercontent.com/iamscottmoyers/simple-libwebsockets-example/master/server.c
//

#define WSSERVER_C
#include <string.h>
#include <libwebsockets.h>
#include <im_alive.h>
#include "debug.h"
#include "wsserver.h"

static struct payload received_payload;

static int callback_http( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len ) {
    int s;
	switch( reason ) {
		case LWS_CALLBACK_HTTP:
		    s=lws_serve_http_file( wsi, "imalive.html", "text/html", NULL, 0 );
            if (s < 0 || ((s > 0) && lws_http_transaction_completed(wsi))) return -1;
            break;
        case LWS_CALLBACK_HTTP_FILE_COMPLETION:
            if (lws_http_transaction_completed(wsi))  return -1;
            // no break
        default:
			break;
	}
	return 0;
}

static int callback_imalive( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len ) {
	switch( reason ) {
		case LWS_CALLBACK_RECEIVE:
			memcpy( &received_payload.data[LWS_SEND_BUFFER_PRE_PADDING], in, len );
			received_payload.len = len;
			lws_callback_on_writable_all_protocol( lws_get_context( wsi ), lws_get_protocol( wsi ) );
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE:
			lws_write( wsi, &received_payload.data[LWS_SEND_BUFFER_PRE_PADDING], received_payload.len, LWS_WRITE_TEXT );
			break;
		default:
			break;
	}
	return 0;
}

static struct lws_protocols protocols[] =  {
		/* The first protocol must always be the HTTP handler */
		/* name,callback,per session data, max frame size */
		{"http-only", callback_http,  0,0 	},
		{"imalive", callback_imalive,0,IMALIVE_RX_BUFFER_BYTES, },
		{ NULL, NULL, 0, 0 } /* terminator */
};

void init_wsService(void ) {
    extern configuration *myConfig;
	struct lws_context_creation_info info;
	memset( &info, 0, sizeof(info) );

	info.port = myConfig->server_wssport;
	info.protocols = protocols;
	info.gid = -1; /* nobody */
	info.uid = -1;
    // for SSL
    info.ssl_cert_filepath = SSL_CERT_FILE_PATH;
    info.ssl_private_key_filepath = SSL_PRIVATE_KEY_PATH;
    info.options =  LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT |
                    LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT |
                    LWS_SERVER_OPTION_DISABLE_IPV6 |
                    LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED |
                    LWS_SERVER_OPTION_IGNORE_MISSING_CERT;
    struct lws_context *context = lws_create_context( &info );
    if (!context) {
        debug(DBG_ERROR,"Cannot create wss context");
        return;
    }
	while( myConfig->loop )	{
		lws_service( context, /* timeout_ms = */ 1000000 );
	}
	lws_context_destroy( context );
}

#undef WSSERVER_C