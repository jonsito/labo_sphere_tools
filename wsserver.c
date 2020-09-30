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

static struct payload msg_buffer[MSG_BUFFER_SIZE]; // circular buffer
static int msg_index; // index to next-to-be-receiver message
struct lws_context *context;

struct per_session_data {
        struct lws *wsi;
        int msg_index;
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
    struct per_session_data *pss=(struct per_session_data *) user;
	switch( reason ) {
	    case LWS_CALLBACK_ESTABLISHED:
            debug(DBG_INFO,"websocket created");
            // PENDING: force expire of all entries, to make sure that client receives every updates
            // this is a bit brute-forece, as every ws clients will be force-updated,
            // but not expected so many clients
            clst_initData();
	        // initialize our pss data to current index
	        pss->wsi=wsi;
	        pss->msg_index=msg_index;
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
		    if (pss->msg_index==msg_index) return 0; // mark no more data available
		    // extract data from pessage buffer
		    memset(pld.data,0,sizeof(pld.data));
		    memcpy(&pld.data[LWS_SEND_BUFFER_PRE_PADDING],msg_buffer[pss->msg_index].data,msg_buffer[pss->msg_index].len);
		    pld.len=msg_buffer[pss->msg_index].len;
            debug(DBG_INFO,"websocket send index:%d padding:%d data:'%s' len:%d",
                  LWS_SEND_BUFFER_PRE_PADDING,pss->msg_index,&pld.data[LWS_SEND_BUFFER_PRE_PADDING],pld.len);
		    // increase pss session buffer index
		    pss->msg_index=(pss->msg_index+1)%MSG_BUFFER_SIZE;
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
		{"imalive", callback_imalive,sizeof(struct per_session_data),BUFFER_LENGTH },
		{ NULL, NULL, 0, 0 } /* terminator */
};


int ws_sendData(char *data) {
    // insert data into buffer
    snprintf(msg_buffer[msg_index].data,BUFFER_LENGTH-1,"%s",data);
    msg_buffer[msg_index].data[BUFFER_LENGTH-1]='\0';
    msg_buffer[msg_index].len=strlen(msg_buffer[msg_index].data);
    msg_index=(msg_index+1)%MSG_BUFFER_SIZE;
    // notify connected clients that there is data available
    lws_callback_on_writable_all_protocol(context, &protocols[PROTOCOL_IMALIVE] );
    return msg_index;
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
		lws_service( context, /* timeout_ms = */ 1000000 ); // do not set to zero to allow external close request
	}
	lws_context_destroy( context );
}

#undef WSSERVER_C