#ifndef _H_RPC_PARSER_
#define _H_RPC_PARSER_
#include "common_types.h"
#include "double_list.h"
#include <pthread.h>


typedef struct _RPC_CONNECTION {
	DOUBLE_LIST_NODE node;
	char remote_id[128];
	BOOL b_stop;
	pthread_t thr_id;
	int sockd;
} RPC_CONNECTION;

void rpc_parser_init(int max_threads);

int rpc_parser_run();

int rpc_parser_stop();

void rpc_parser_free();

RPC_CONNECTION* rpc_parser_get_connection();

void rpc_parser_put_connection(RPC_CONNECTION *pconnection);

#endif	/* _H_RPC_PARSER_ */
