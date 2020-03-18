#include "common_util.h"
#include "rpc_parser.h"
#include "dac_server.h"
#include "mapi_types.h"
#include "rpc_ext.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <poll.h>

static int g_max_threads;
static DOUBLE_LIST g_connection_list;
static pthread_mutex_t g_connection_lock;

void rpc_parser_init(int max_threads)
{
	g_max_threads = max_threads;
	pthread_mutex_init(&g_connection_lock, NULL);
	double_list_init(&g_connection_list);
}

void rpc_parser_free()
{
	double_list_free(&g_connection_list);
	pthread_mutex_destroy(&g_connection_lock);
}

RPC_CONNECTION* rpc_parser_get_connection()
{
	RPC_CONNECTION *pconnection;

	if (0 != g_max_threads && double_list_get_nodes_num(
		&g_connection_list) >= g_max_threads) {
		return NULL;
	}
	pconnection = (RPC_CONNECTION*)malloc(sizeof(RPC_CONNECTION));
	if (NULL == pconnection) {
		return NULL;
	}
	pconnection->node.pdata = pconnection;
	pconnection->b_stop = FALSE;
	return pconnection;
}

static BOOL rpc_parser_dispatch(const RPC_REQUEST *prequest,
	RPC_RESPONSE *presponse)
{
	presponse->call_id = prequest->call_id;
	switch (prequest->call_id) {
	case CALL_ID_INFOSTORAGE:
		presponse->result = dac_server_infostorage(
			&presponse->payload.infostorage.total_space,
			&presponse->payload.infostorage.total_used,
			&presponse->payload.infostorage.total_dir);
		break;
	case CALL_ID_ALLOCDIR:
		presponse->result = dac_server_allocdir(
				prequest->payload.allocdir.type,
				presponse->payload.allocdir.dir);
		break;
	case CALL_ID_FREEDIR:
		presponse->result = dac_server_freedir(prequest->dir);
		break;
	case CALL_ID_SETPROPVALS:
		presponse->result = dac_server_setpropvals(prequest->dir,
			prequest->payload.setpropvals.pnamespace,
			&prequest->payload.setpropvals.propvals);
		break;
	case CALL_ID_GETPROPVALS:
		presponse->result = dac_server_getpropvals(prequest->dir,
			prequest->payload.getpropvals.pnamespace,
			&prequest->payload.getpropvals.names,
			&presponse->payload.getpropvals.propvals);
		break;
	case CALL_ID_DELETEPROPVALS:
		presponse->result = dac_server_deletepropvals(prequest->dir,
			prequest->payload.deletepropvals.pnamespace,
			&prequest->payload.deletepropvals.names);
		break;
	case CALL_ID_OPENTABLE:
		presponse->result = dac_server_opentable(prequest->dir,
			prequest->payload.opentable.pnamespace,
			prequest->payload.opentable.ptablename,
			prequest->payload.opentable.flags,
			prequest->payload.opentable.puniquename,
			&presponse->payload.opentable.table_id);
		break;
	case CALL_ID_OPENCELLTABLE:
		presponse->result = dac_server_opencelltable(
			prequest->dir, prequest->payload.opencelltable.row_instance,
			prequest->payload.opencelltable.pcolname,
			prequest->payload.opencelltable.flags,
			prequest->payload.opencelltable.puniquename,
			&presponse->payload.opencelltable.table_id);
		break;
	case CALL_ID_RESTRICTTABLE:
		presponse->result = dac_server_restricttable(
			prequest->dir, prequest->payload.restricttable.table_id,
			&prequest->payload.restricttable.restrict);
		break;
	case CALL_ID_UPDATECELLS:
		presponse->result = dac_server_updatecells(prequest->dir,
			prequest->payload.updatecells.row_instance,
			&prequest->payload.updatecells.cells);
		break;
	case CALL_ID_SUMTABLE:
		presponse->result = dac_server_sumtable(prequest->dir,
			prequest->payload.sumtable.table_id,
			&presponse->payload.sumtable.count);
		break;
	case CALL_ID_QUERYROWS:
		presponse->result = dac_server_queryrows(prequest->dir,
			prequest->payload.queryrows.table_id,
			prequest->payload.queryrows.start_pos,
			prequest->payload.queryrows.row_count,
			&presponse->payload.queryrows.set);
		break;
	case CALL_ID_INSERTROWS:
		presponse->result = dac_server_insertrows(prequest->dir,
			prequest->payload.insertrows.table_id,
			prequest->payload.insertrows.flags,
			&prequest->payload.insertrows.set);
		break;
	case CALL_ID_DELETEROW:
		presponse->result = dac_server_deleterow(prequest->dir,
			prequest->payload.deleterow.row_instance);
		break;
	case CALL_ID_DELETECELLS:
		presponse->result = dac_server_deletecells(prequest->dir,
			prequest->payload.deletecells.row_instance,
			&prequest->payload.deletecells.colnames);
		break;
	case CALL_ID_DELETETABLE:
		presponse->result = dac_server_deletetable(prequest->dir,
			prequest->payload.deletetable.pnamespace,
			prequest->payload.deletetable.ptablename);
		break;
	case CALL_ID_CLOSETABLE:
		presponse->result = dac_server_closetable(prequest->dir,
			prequest->payload.closetable.table_id);
		break;
	case CALL_ID_MATCHROW:
		presponse->result = dac_server_matchrow(prequest->dir,
			prequest->payload.matchrow.table_id,
			prequest->payload.matchrow.type,
			prequest->payload.matchrow.pvalue,
			&presponse->payload.matchrow.row);
		break;
	case CALL_ID_GETNAMESPACES:
		presponse->result = dac_server_getnamespaces(prequest->dir,
			&presponse->payload.getnamespaces.namespaces);
		break;
	case CALL_ID_GETPROPNAMES:
		presponse->result = dac_server_getpropnames(prequest->dir,
			prequest->payload.getpropnames.pnamespace,
			&presponse->payload.getpropnames.propnames);
		break;
	case CALL_ID_GETTABLENAMES:
		presponse->result = dac_server_gettablenames(prequest->dir,
			prequest->payload.gettablenames.pnamespace,
			&presponse->payload.gettablenames.tablenames);
		break;
	case CALL_ID_READFILE:
		presponse->result = dac_server_readfile(
			prequest->dir, prequest->payload.readfile.path,
			prequest->payload.readfile.file_name,
			prequest->payload.readfile.offset,
			prequest->payload.readfile.length,
			&presponse->payload.readfile.content_bin);
		break;
	case CALL_ID_APPENDFILE:
		presponse->result = dac_server_appendfile(
			prequest->dir, prequest->payload.appendfile.path,
			prequest->payload.appendfile.file_name,
			&prequest->payload.appendfile.content_bin);
		break;
	case CALL_ID_REMOVEFILE:
		presponse->result = dac_server_removefile(
			prequest->dir, prequest->payload.removefile.path,
			prequest->payload.removefile.file_name);
		break;
	case CALL_ID_STATFILE:
		presponse->result = dac_server_statfile(
			prequest->dir, prequest->payload.statfile.path,
			prequest->payload.statfile.file_name,
			&presponse->payload.statfile.size);
		break;
	case CALL_ID_CHECKROW:
		presponse->result = dac_server_checkrow(prequest->dir,
			prequest->payload.checkrow.pnamespace,
			prequest->payload.checkrow.ptablename,
			prequest->payload.checkrow.type,
			prequest->payload.checkrow.pvalue);
		break;
	case CALL_ID_PHPRPC:
		presponse->result = dac_server_phprpc(prequest->dir,
			prequest->payload.phprpc.account,
			prequest->payload.phprpc.script_path,
			&prequest->payload.phprpc.bin_in,
			&presponse->payload.phprpc.bin_out);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

static void *thread_work_func(void *pparam)
{
	int tv_msec;
	void *pbuff;
	int read_len;
	BINARY tmp_bin;
	uint32_t offset;
	int written_len;
	BOOL is_writing;
	uint8_t tmp_byte;
	BOOL is_connected;
	uint32_t buff_len;
	RPC_REQUEST request;
	uint8_t resp_buff[5];
	RPC_RESPONSE response;
	struct pollfd pfd_read;
	RPC_CONNECTION *pconnection;
	
	memset(resp_buff, 0, 5);
	pconnection = (RPC_CONNECTION*)pparam;
	pbuff = NULL;
	offset = 0;
	buff_len = 0;
	is_writing = FALSE;
	is_connected = FALSE;
    while (FALSE == pconnection->b_stop) {
		if (TRUE == is_writing) {
			written_len = write(pconnection->sockd,
				pbuff + offset, buff_len - offset);
			if (written_len <= 0) {
				break;
			}
			offset += written_len;
			if (offset == buff_len) {
				free(pbuff);
				pbuff = NULL;
				buff_len = 0;
				offset = 0;
				is_writing = FALSE;
			}
			continue;
		}
		tv_msec = SOCKET_TIMEOUT * 1000;
		pfd_read.fd = pconnection->sockd;
		pfd_read.events = POLLIN|POLLPRI;
		if (1 != poll(&pfd_read, 1, tv_msec)) {
			break;
		}
		if (NULL == pbuff) {
			read_len = read(pconnection->sockd,
					&buff_len, sizeof(uint32_t));
			if (read_len != sizeof(uint32_t)) {
				break;
			}
			/* ping packet */
			if (0 == buff_len) {
				if (1 != write(pconnection->sockd, resp_buff, 1)) {
					break;
				}
				continue;
			}
			pbuff = malloc(buff_len);
			if (NULL == pbuff) {
				tmp_byte = RESPONSE_CODE_LACK_MEMORY;
				write(pconnection->sockd, &tmp_byte, 1);
				if (FALSE == is_connected) {
					break;
				}
				buff_len = 0;
			}
			offset = 0;
			continue;
		}
		read_len = read(pconnection->sockd,
				pbuff + offset, buff_len - offset);
		if (read_len <= 0) {
			break;
		}
		offset += read_len;
		if (offset < buff_len) {
			continue;
		}
		common_util_build_environment();
		tmp_bin.pb = pbuff;
		tmp_bin.cb = buff_len;
		if (FALSE == rpc_ext_pull_request(&tmp_bin, &request)) {
			free(pbuff);
			pbuff = NULL;
			tmp_byte = RESPONSE_CODE_PULL_ERROR;
		} else {
			free(pbuff);
			pbuff = NULL;
			if (FALSE == is_connected) {
				if (CALL_ID_CONNECT == request.call_id) {
					strcpy(pconnection->remote_id,
						request.payload.connect.remote_id);
					common_util_free_environment();
					is_connected = TRUE;
					if (5 != write(pconnection->sockd, resp_buff, 5)) {
						break;
					}
					offset = 0;
					buff_len = 0;
					continue;
				} else {
					tmp_byte = RESPONSE_CODE_CONNECT_UNCOMPLETE;
				}
			} else {
				if (FALSE == rpc_parser_dispatch(&request, &response)) {
					tmp_byte = RESPONSE_CODE_DISPATCH_ERROR;
				} else {
					if (FALSE == rpc_ext_push_response(
						&response, &tmp_bin)) {
						tmp_byte = RESPONSE_CODE_PUSH_ERROR;
					} else {
						common_util_free_environment();
						offset = 0;
						pbuff = tmp_bin.pb;
						buff_len = tmp_bin.cb;
						is_writing = TRUE;
						continue;
					}
				}
			}
		}
		common_util_free_environment();
		write(pconnection->sockd, &tmp_byte, 1);
		break;
	}
	close(pconnection->sockd);
	if (NULL != pbuff) {
		free(pbuff);
	}
	pthread_mutex_lock(&g_connection_lock);
	double_list_remove(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_connection_lock);
	if (FALSE == pconnection->b_stop) {
		pthread_detach(pthread_self());
	}
	free(pconnection);
	pthread_exit(0);
}

void rpc_parser_put_connection(RPC_CONNECTION *pconnection)
{
	pthread_mutex_lock(&g_connection_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_connection_lock);
	if (0 != pthread_create(&pconnection->thr_id,
		NULL, thread_work_func, pconnection)) {
		pthread_mutex_lock(&g_connection_lock);
		double_list_remove(&g_connection_list, &pconnection->node);
		pthread_mutex_unlock(&g_connection_lock);
		free(pconnection);
		return;
	}
}

int rpc_parser_run()
{
	return 0;
}

int rpc_parser_stop()
{
	int i, num;
	pthread_t *pthr_ids;
	DOUBLE_LIST_NODE *pnode;
	RPC_CONNECTION *pconnection;
	
	pthr_ids = NULL;
	pthread_mutex_lock(&g_connection_lock);
	num = double_list_get_nodes_num(&g_connection_list);
	if (num > 0) {
		pthr_ids = malloc(sizeof(pthread_t)*num);
		if (NULL == pthr_ids) {
			return -1;
		}
	}
	for (i=0,pnode=double_list_get_head(&g_connection_list);
		NULL!=pnode; pnode=double_list_get_after(
		&g_connection_list, pnode),i++) {
		pconnection = (RPC_CONNECTION*)pnode->pdata;
		pthr_ids[i] = pconnection->thr_id;
		pconnection->b_stop = TRUE;
		shutdown(pconnection->sockd, SHUT_RDWR);
	}
	pthread_mutex_unlock(&g_connection_lock);
	for (i=0; i<num; i++) {
		pthread_join(pthr_ids[i], NULL);
	}
	if (NULL != pthr_ids) {
		free(pthr_ids);
		pthr_ids = NULL;
	}
	return 0;
}
