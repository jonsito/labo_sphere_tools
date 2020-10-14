//
// Created by jantonio on 15/9/20.
// from https://raw.githubusercontent.com/iamscottmoyers/simple-libwebsockets-example/master/server.c
//

#define WSSERVER_C
#include <string.h>
#include <libwebsockets.h>
#include <im_alive.h>
#include "debug.h"
#include "client_state.h"
#include "wsserver.h"

struct lws_context *context;

struct pss_data {
    struct lws *wsi;
    int index;
};

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
    struct payload pld;
    struct pss_data *pss=(struct pss_data *)user;
	switch( reason ) {
	    case LWS_CALLBACK_ESTABLISHED:
            debug(DBG_INFO,"websocket created");
            // PENDING: force expire of all entries, to make sure that client receives every updates
            // this is a bit brute-forece, as every ws clients will be force-updated,
            // but not expected so many clients
            clst_initData();
            // initialize user data
            pss->index=0;
            pss->wsi=wsi;
	        break;
	    case LWS_CALLBACK_CLOSED:
	        debug(DBG_INFO,"websocket closed");
		case LWS_CALLBACK_RECEIVE:
		    // not used. just notify unexpected data receive
			memcpy( &pld.data[LWS_SEND_BUFFER_PRE_PADDING], in, len );
			pld.len = len;
			pld.data[LWS_SEND_BUFFER_PRE_PADDING+len]='\0';
			debug(DBG_INFO,"unexpected data %s received from websocket",pld.data[LWS_SEND_BUFFER_PRE_PADDING]);
            // lws_callback_on_writable_all_protocol( lws_get_context( wsi ), lws_get_protocol( wsi ) );
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE:
		    // extract data from pessage buffer
		    memset(pld.data,0,sizeof(pld.data));
		    char *items=clst_getList(pss->index,pss->index+MSG_CHUNK_SIZE,0);
		    memcpy(&pld.data[LWS_SEND_BUFFER_PRE_PADDING],items,strlen(items));
		    pld.len=strlen(items);
            debug(DBG_TRACE,"websocket send index:%d padding:%d data:'%s' len:%d",
                  pss->index,LWS_SEND_BUFFER_PRE_PADDING,&pld.data[LWS_SEND_BUFFER_PRE_PADDING],pld.len);
		    // increase pss session buffer index
		    pss->index+=MSG_CHUNK_SIZE;
		    if (pss->index>=NUM_CLIENTS) pss->index=0;
		    free(items);
			lws_write( wsi, (unsigned char *) &pld.data[LWS_SEND_BUFFER_PRE_PADDING], pld.len, LWS_WRITE_TEXT );
			break;
		default:
			break;
	}
	return 0;
}

static struct lws_protocols protocols[] =  {
		/* The first protocol must always be the HTTP handler */
		/* name,callback,per session data, max frame size */
		{"http", callback_http,  0,0 	},
		{"imalive", callback_imalive,sizeof(struct pss_data),BUFFER_LENGTH },
		{ NULL, NULL, 0, 0 } /* terminator */
};

void ws_dataAvailable() {
    lws_callback_on_writable_all_protocol(context, &protocols[PROTOCOL_IMALIVE] );
}

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
    context = lws_create_context( &info );
    if (!context) {
        debug(DBG_ERROR,"Cannot create wss context");
        return;
    }
	while( myConfig->loop )	{
		lws_service( context, /* timeout_ms = */ 200 ); // do not set to zero to allow external close request
	}
	lws_context_destroy( context );
}

#undef WSSERVER_C