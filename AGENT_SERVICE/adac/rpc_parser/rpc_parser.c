#include "rpc_ext.h"
#include "mapi_types.h"
#include "rpc_parser.h"
#include "common_util.h"
#include "adac_processor.h"
#include <sys/socket.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>


typedef struct _CLIENT_NODE {
	DOUBLE_LIST_NODE node;
	int clifd;
} CLIENT_NODE;

static int g_thread_num;
static BOOL g_notify_stop;
static pthread_t *g_thread_ids;
static DOUBLE_LIST g_conn_list;
static pthread_cond_t g_waken_cond;
static pthread_mutex_t g_conn_lock;
static pthread_mutex_t g_cond_mutex;


void rpc_parser_init(int thread_num)
{
	g_notify_stop = TRUE;
	g_thread_num = thread_num;
	pthread_mutex_init(&g_conn_lock, NULL);
	pthread_mutex_init(&g_cond_mutex, NULL);
	pthread_cond_init(&g_waken_cond, NULL);
}

void rpc_parser_free()
{
	pthread_mutex_destroy(&g_conn_lock);
	pthread_mutex_destroy(&g_cond_mutex);
	pthread_cond_destroy(&g_waken_cond);
}

BOOL rpc_parser_activate_connection(int clifd)
{
	CLIENT_NODE *pclient;
	
	pclient = malloc(sizeof(CLIENT_NODE));
	if (NULL == pclient) {
		return FALSE;
	}
	pclient->node.pdata = pclient;
	pclient->clifd = clifd;
	pthread_mutex_lock(&g_conn_lock);
	double_list_append_as_tail(&g_conn_list, &pclient->node);
	pthread_mutex_unlock(&g_conn_lock);
	pthread_cond_signal(&g_waken_cond);
	return TRUE;
}

static BOOL rpc_parser_dispatch(const RPC_REQUEST *prequest,
	RPC_RESPONSE *presponse)
{
	presponse->call_id = prequest->call_id;
	switch (prequest->call_id) {
	case CALL_ID_ALLOCDIR:
		presponse->result = adac_processor_allocdir(
				prequest->payload.allocdir.type,
				presponse->payload.allocdir.dir);
		break;
	case CALL_ID_FREEDIR:
		presponse->result = adac_processor_freedir(prequest->address);
		break;
	case CALL_ID_SETPROPVALS:
		presponse->result = adac_processor_setpropvals(
			prequest->address,
			prequest->payload.setpropvals.pnamespace,
			&prequest->payload.setpropvals.propvals);
		break;
	case CALL_ID_GETPROPVALS:
		presponse->result = adac_processor_getpropvals(
			prequest->address,
			prequest->payload.getpropvals.pnamespace,
			&prequest->payload.getpropvals.names,
			&presponse->payload.getpropvals.propvals);
		break;
	case CALL_ID_DELETEPROPVALS:
		presponse->result = adac_processor_deletepropvals(
			prequest->address,
			prequest->payload.deletepropvals.pnamespace,
			&prequest->payload.deletepropvals.names);
		break;
	case CALL_ID_OPENTABLE:
		presponse->result = adac_processor_opentable(
			prequest->address,
			prequest->payload.opentable.pnamespace,
			prequest->payload.opentable.ptablename,
			prequest->payload.opentable.flags,
			prequest->payload.opentable.puniquename,
			&presponse->payload.opentable.table_id);
		break;
	case CALL_ID_OPENCELLTABLE:
		presponse->result = adac_processor_opencelltable(
			prequest->address,
			prequest->payload.opencelltable.row_instance,
			prequest->payload.opencelltable.pcolname,
			prequest->payload.opencelltable.flags,
			prequest->payload.opencelltable.puniquename,
			&presponse->payload.opencelltable.table_id);
		break;
	case CALL_ID_RESTRICTTABLE:
		presponse->result = adac_processor_restricttable(
			prequest->address,
			prequest->payload.restricttable.table_id,
			&prequest->payload.restricttable.restrict);
		break;
	case CALL_ID_UPDATECELLS:
		presponse->result = adac_processor_updatecells(
			prequest->address,
			prequest->payload.updatecells.row_instance,
			&prequest->payload.updatecells.cells);
		break;
	case CALL_ID_SUMTABLE:
		presponse->result = adac_processor_sumtable(
			prequest->address,
			prequest->payload.sumtable.table_id,
			&presponse->payload.sumtable.count);
		break;
	case CALL_ID_QUERYROWS:
		presponse->result = adac_processor_queryrows(
			prequest->address,
			prequest->payload.queryrows.table_id,
			prequest->payload.queryrows.start_pos,
			prequest->payload.queryrows.row_count,
			&presponse->payload.queryrows.set);
		break;
	case CALL_ID_INSERTROWS:
		presponse->result = adac_processor_insertrows(
			prequest->address,
			prequest->payload.insertrows.table_id,
			prequest->payload.insertrows.flags,
			&prequest->payload.insertrows.set);
		break;
	case CALL_ID_DELETEROW:
		presponse->result = adac_processor_deleterow(
			prequest->address,
			prequest->payload.deleterow.row_instance);
		break;
	case CALL_ID_DELETECELLS:
		presponse->result = adac_processor_deletecells(
			prequest->address,
			prequest->payload.deletecells.row_instance,
			&prequest->payload.deletecells.colnames);
		break;
	case CALL_ID_DELETETABLE:
		presponse->result = adac_processor_deletetable(
			prequest->address,
			prequest->payload.deletetable.pnamespace,
			prequest->payload.deletetable.ptablename);
		break;
	case CALL_ID_CLOSETABLE:
		presponse->result = adac_processor_closetable(
			prequest->address,
			prequest->payload.closetable.table_id);
		break;
	case CALL_ID_MATCHROW:
		presponse->result = adac_processor_matchrow(
			prequest->address,
			prequest->payload.matchrow.table_id,
			prequest->payload.matchrow.type,
			prequest->payload.matchrow.pvalue,
			&presponse->payload.matchrow.row);
		break;
	case CALL_ID_GETNAMESPACES:
		presponse->result = adac_processor_getnamespaces(
			prequest->address,
			&presponse->payload.getnamespaces.namespaces);
		break;
	case CALL_ID_GETPROPNAMES:
		presponse->result = adac_processor_getpropnames(
			prequest->address,
			prequest->payload.getpropnames.pnamespace,
			&presponse->payload.getpropnames.propnames);
		break;
	case CALL_ID_GETTABLENAMES:
		presponse->result = adac_processor_gettablenames(
			prequest->address,
			prequest->payload.gettablenames.pnamespace,
			&presponse->payload.gettablenames.tablenames);
		break;
	case CALL_ID_READFILE:
		presponse->result = adac_processor_readfile(
			prequest->address,
			prequest->payload.readfile.path,
			prequest->payload.readfile.file_name,
			prequest->payload.readfile.offset,
			prequest->payload.readfile.length,
			&presponse->payload.readfile.content_bin);
		break;
	case CALL_ID_APPENDFILE:
		presponse->result = adac_processor_appendfile(
			prequest->address,
			prequest->payload.appendfile.path,
			prequest->payload.appendfile.file_name,
			&prequest->payload.appendfile.content_bin);
		break;
	case CALL_ID_REMOVEFILE:
		presponse->result = adac_processor_removefile(
			prequest->address,
			prequest->payload.removefile.path,
			prequest->payload.removefile.file_name);
		break;
	case CALL_ID_STATFILE:
		presponse->result = adac_processor_statfile(
			prequest->address,
			prequest->payload.statfile.path,
			prequest->payload.statfile.file_name,
			&presponse->payload.statfile.size);
		break;
	case CALL_ID_GETADDRESSBOOK:
		presponse->result = adac_processor_getaddressbook(
			prequest->address,
			presponse->payload.getaddressbook.ab_address);
		break;
	case CALL_ID_GETABHIERARCHY:
		presponse->result = adac_processor_getabhierarchy(
			prequest->address,
			&presponse->payload.getabhierarchy.set);
		break;
	case CALL_ID_GETABCONTENT:
		presponse->result = adac_processor_getabcontent(
			prequest->address,
			prequest->payload.getabcontent.start_pos,
			prequest->payload.getabcontent.count,
			&presponse->payload.getabcontent.set);
		break;
	case CALL_ID_RESTRICTABCONTENT:
		presponse->result = adac_processor_restrictabcontent(
			prequest->address,
			&prequest->payload.restrictabcontent.restrict,
			prequest->payload.restrictabcontent.max_count,
			&presponse->payload.restrictabcontent.set);
		break;
	case CALL_ID_GETEXADDRESS:
		presponse->result = adac_processor_getexaddress(
			prequest->address,
			presponse->payload.getexaddress.ex_address);
		break;
	case CALL_ID_GETEXADDRESSTYPE:
		presponse->result = adac_processor_getexaddresstype(
			prequest->address,
			&presponse->payload.getexaddresstype.address_type);
		break;
	case CALL_ID_CHECKMLISTINCLUDE:
		presponse->result = adac_processor_checkmlistinclude(
			prequest->address,
			prequest->payload.checkmlistinclude.account_address,
			&presponse->payload.checkmlistinclude.b_include);
		break;
	case CALL_ID_CHECKROW:
		presponse->result = adac_processor_checkrow(
			prequest->address,
			prequest->payload.checkrow.pnamespace,
			prequest->payload.checkrow.ptablename,
			prequest->payload.checkrow.type,
			prequest->payload.checkrow.pvalue);
		break;
	case CALL_ID_PHPRPC:
		presponse->result = adac_processor_phprpc(
			prequest->address,
			prequest->payload.phprpc.script_path,
			&prequest->payload.phprpc.bin_in,
			&presponse->payload.phprpc.bin_out);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

static void *thread_work_func(void *param)
{
	int clifd;
	void *pbuff;
	int tv_msec;
	int read_len;
	BINARY tmp_bin;
	uint32_t offset;
	uint8_t tmp_byte;
	uint32_t buff_len;
	RPC_REQUEST request;
	struct pollfd fdpoll;
	RPC_RESPONSE response;
	DOUBLE_LIST_NODE *pnode;


WAIT_CLIFD:
	pthread_mutex_lock(&g_cond_mutex);
	pthread_cond_wait(&g_waken_cond, &g_cond_mutex);
	pthread_mutex_unlock(&g_cond_mutex);
NEXT_CLIFD:
	pthread_mutex_lock(&g_conn_lock);
	pnode = double_list_get_from_head(&g_conn_list);
	pthread_mutex_unlock(&g_conn_lock);

	if (NULL == pnode) {
		if (TRUE == g_notify_stop) {
			pthread_exit(0);
		}
		goto WAIT_CLIFD;
	}
	clifd = ((CLIENT_NODE*)pnode->pdata)->clifd;
	free(pnode->pdata);
	
	offset = 0;
	buff_len = 0;
	
	tv_msec = SOCKET_TIMEOUT * 1000;
	fdpoll.fd = clifd;
	fdpoll.events = POLLIN|POLLPRI;
	if (1 != poll(&fdpoll, 1, tv_msec)) {
		close(clifd);
		goto NEXT_CLIFD;
	}
	read_len = read(clifd, &buff_len, sizeof(uint32_t));
	if (read_len != sizeof(uint32_t)) {
		close(clifd);
		goto NEXT_CLIFD;
	}
	pbuff = malloc(buff_len);
	if (NULL == pbuff) {
		tmp_byte = RESPONSE_CODE_LACK_MEMORY;
		fdpoll.events = POLLOUT|POLLWRBAND;
		if (1 == poll(&fdpoll, 1, tv_msec)) {
			write(clifd, &tmp_byte, 1);
		}
		close(clifd);
		goto NEXT_CLIFD;
	}
	while (TRUE) {
		if (1 != poll(&fdpoll, 1, tv_msec)) {
			close(clifd);
			free(pbuff);
			goto NEXT_CLIFD;
		}
		read_len = read(clifd, pbuff + offset, buff_len - offset);
		if (read_len <= 0) {
			close(clifd);
			free(pbuff);
			goto NEXT_CLIFD;
		}
		offset += read_len;
		if (offset == buff_len) {
			break;
		}
	}
	common_util_build_environment();
	tmp_bin.pb = pbuff;
	tmp_bin.cb = buff_len;
	if (FALSE == rpc_ext_pull_request(&tmp_bin, &request)) {
		free(pbuff);
		common_util_free_environment();
		tmp_byte = RESPONSE_CODE_PULL_ERROR;
		fdpoll.events = POLLOUT|POLLWRBAND;
		if (1 == poll(&fdpoll, 1, tv_msec)) {
			write(clifd, &tmp_byte, 1);
		}
		close(clifd);
		goto NEXT_CLIFD;
	}
	free(pbuff);
	if (FALSE == rpc_parser_dispatch(&request, &response)) {
		common_util_free_environment();
		tmp_byte = RESPONSE_CODE_DISPATCH_ERROR;
		fdpoll.events = POLLOUT|POLLWRBAND;
		if (1 == poll(&fdpoll, 1, tv_msec)) {
			write(clifd, &tmp_byte, 1);
		}
		close(clifd);
		goto NEXT_CLIFD;
	}
	if (FALSE == rpc_ext_push_response(
		&response, &tmp_bin)) {
		common_util_free_environment();
		tmp_byte = RESPONSE_CODE_PUSH_ERROR;
		fdpoll.events = POLLOUT|POLLWRBAND;
		if (1 == poll(&fdpoll, 1, tv_msec)) {
			write(clifd, &tmp_byte, 1);
		}
		close(clifd);
		goto NEXT_CLIFD;
	}
	common_util_free_environment();
	offset = 0;
	fdpoll.events = POLLOUT|POLLWRBAND;
	if (1 == poll(&fdpoll, 1, tv_msec)) {
		write(clifd, tmp_bin.pb, tmp_bin.cb);
	}
	shutdown(clifd, SHUT_WR);
	read(clifd, &tmp_byte, 1);
	close(clifd);
	free(tmp_bin.pb);
	goto NEXT_CLIFD;
}

int rpc_parser_run()
{
	int i;
	
	g_thread_ids = malloc(sizeof(pthread_t)*g_thread_num);
	if (NULL == g_thread_ids) {
		return -1;
	}
	g_notify_stop = FALSE;
	for (i=0; i<g_thread_num; i++) {
		if (0 != pthread_create(&g_thread_ids[i],
			NULL, thread_work_func, NULL)) {
			break;
		}
	}
	if (i < g_thread_num) {
		g_notify_stop = TRUE;
		for (i=0; i<g_thread_num; i++) {
			pthread_cancel(g_thread_ids[i]);
		}
		free(g_thread_ids);
		printf("[rpc_parser]: fail to creat pool thread\n");
		return -2;
	}
	return 0;
}

int rpc_parser_stop()
{
	int i;
	
	g_notify_stop = TRUE;
	pthread_cond_broadcast(&g_waken_cond);
	for (i=0; i<g_thread_num; i++) {
		pthread_join(g_thread_ids[i], NULL);
	}
	free(g_thread_ids);
	return 0;
}
