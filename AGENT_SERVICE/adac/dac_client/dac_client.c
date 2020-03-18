#include "dac_ext.h"
#include "list_file.h"
#include "ext_buffer.h"
#include "dac_client.h"
#include "double_list.h"
#include "common_util.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <net/if.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <poll.h>


typedef struct _DAC_ITEM {
	char prefix[256];
	char type[16];
	char ip_addr[16];
	int port;
} DAC_ITEM;

typedef struct _REMOTE_SVR {
	DOUBLE_LIST_NODE node;
	DOUBLE_LIST conn_list;
	char ip_addr[16];
	char prefix[256];
	int prefix_len;
	uint8_t type;
	int port;
} REMOTE_SVR;

typedef struct _REMOTE_CONN {
    DOUBLE_LIST_NODE node;
	time_t last_time;
	REMOTE_SVR *psvr;
	int sockd;
} REMOTE_CONN;

static int g_conn_num;
static BOOL g_notify_stop;
static pthread_t g_scan_id;
static char g_list_path[256];
static DOUBLE_LIST g_lost_list;
static DOUBLE_LIST g_server_list;
static pthread_mutex_t g_server_lock;


static BOOL dac_client_read_socket(int sockd, BINARY *pbin)
{
	int tv_msec;
	int read_len;
	uint32_t offset;
	uint8_t resp_buff[5];
	struct pollfd pfd_read;
	
	pbin->pb = NULL;
	while (TRUE) {
		tv_msec = SOCKET_TIMEOUT * 1000;
		pfd_read.fd = sockd;
		pfd_read.events = POLLIN|POLLPRI;
		if (1 != poll(&pfd_read, 1, tv_msec)) {
			return FALSE;
		}
		if (NULL == pbin->pb) {
			read_len = read(sockd, resp_buff, 5);
			if (1 == read_len) {
				pbin->cb = 1;
				pbin->pb = common_util_alloc(1);
				if (NULL == pbin->pb) {
					return FALSE;
				}
				*(uint8_t*)pbin->pb = resp_buff[0];
				return TRUE;
			} else if (5 == read_len) {
				pbin->cb = *(uint32_t*)(resp_buff + 1) + 5;
				pbin->pb = common_util_alloc(pbin->cb);
				if (NULL == pbin->pb) {
					return FALSE;
				}
				memcpy(pbin->pb, resp_buff, 5);
				offset = 5;
				if (offset == pbin->cb) {
					return TRUE;
				}
				continue;
			} else {
				return FALSE;
			}
		}
		read_len = read(sockd, pbin->pb + offset, pbin->cb - offset);
		if (read_len <= 0) {
			return FALSE;
		}
		offset += read_len;
		if (offset == pbin->cb) {
			return TRUE;
		}
	}
}

static BOOL dac_client_write_socket(int sockd, const BINARY *pbin)
{
	int written_len;
	uint32_t offset;
	
	offset = 0;
	while (TRUE) {
		written_len = write(sockd, pbin->pb + offset, pbin->cb - offset);
		if (written_len <= 0) {
			return FALSE;
		}
		offset += written_len;
		if (offset == pbin->cb) {
			return TRUE;
		}
	}
}

static int dac_client_connect(REMOTE_SVR *pserver)
{
	int sockd;
	int process_id;
	BINARY tmp_bin;
	struct timeval tv;
	char remote_id[128];
	RPC_REQUEST request;
	uint8_t response_code;
	struct sockaddr_in servaddr;
	
    sockd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(pserver->port);
    inet_pton(AF_INET, pserver->ip_addr, &servaddr.sin_addr);
    if (0 != connect(sockd, (struct sockaddr*)&servaddr, sizeof(servaddr))) {
        close(sockd);
        return -1;
    }
	process_id = getpid();
	sprintf(remote_id, "adac:%d", process_id);
	request.call_id = CALL_ID_CONNECT;
	request.payload.connect.remote_id = remote_id;
	if (TRUE != dac_ext_push_request(&request, &tmp_bin)) {
		close(sockd);
		return -1;
	}
	if (FALSE == dac_client_write_socket(sockd, &tmp_bin)) {
		free(tmp_bin.pb);
		close(sockd);
		return -1;
	}
	free(tmp_bin.pb);
	common_util_build_environment();
	if (FALSE == dac_client_read_socket(sockd, &tmp_bin)) {
		common_util_free_environment();
		close(sockd);
		return -1;
	}
	response_code = tmp_bin.pb[0];
	if (RESPONSE_CODE_SUCCESS == response_code) {
		if (5 != tmp_bin.cb || 0 != *(uint32_t*)(tmp_bin.pb + 1)) {
			common_util_free_environment();
			printf("[dac_client]: response format error "
				"when connect to %s:%d for prefix \"%s\"\n",
				pserver->ip_addr, pserver->port, pserver->prefix);
			close(sockd);
			return -1;
		}
		common_util_free_environment();
		return sockd;
	}
	common_util_free_environment();
	switch (response_code) {
	case RESPONSE_CODE_ACCESS_DENY:
		printf("[dac_client]: fail to connect to "
			"%s:%d for prefix \"%s\", access deny!\n",
			pserver->ip_addr, pserver->port, pserver->prefix);
		break;
	case RESPONSE_CODE_MAX_REACHED:
		printf("[dac_client]: fail to connect to %s:%d for "
			"prefix \"%s\", maximum connections reached in server!\n",
			pserver->ip_addr, pserver->port, pserver->prefix);
		break;
	case RESPONSE_CODE_LACK_MEMORY:
		printf("[dac_client]: fail to connect to %s:%d "
			"for prefix \"%s\", server out of memory!\n",
			pserver->ip_addr, pserver->port, pserver->prefix);
		break;
	default:
		printf("[dac_client]: fail to connect to "
			"%s:%d for prefix \"%s\", error code %d!\n",
			pserver->ip_addr, pserver->port,
			pserver->prefix, (int)response_code);
		break;
	}
	close(sockd);
	return -1;
}

static void *scan_work_func(void *pparam)
{
	int tv_msec;
	time_t now_time;
	uint8_t resp_buff;
	uint32_t ping_buff;
	REMOTE_CONN *pconn;
	REMOTE_SVR *pserver;
	DOUBLE_LIST temp_list;
	struct pollfd pfd_read;
	DOUBLE_LIST_NODE *pnode;
	DOUBLE_LIST_NODE *ptail;
	DOUBLE_LIST_NODE *pnode1;
	
	
	ping_buff = 0;
	double_list_init(&temp_list);

	while (FALSE == g_notify_stop) {
		pthread_mutex_lock(&g_server_lock);
		time(&now_time);
		for (pnode=double_list_get_head(&g_server_list); NULL!=pnode;
			pnode=double_list_get_after(&g_server_list, pnode)) {
			pserver = (REMOTE_SVR*)pnode->pdata;
			ptail = double_list_get_tail(&pserver->conn_list);
			while (pnode1=double_list_get_from_head(&pserver->conn_list)) {
				pconn = (REMOTE_CONN*)pnode1->pdata;
				if (now_time - pconn->last_time >= SOCKET_TIMEOUT - 3) {
					double_list_append_as_tail(&temp_list, &pconn->node);
				} else {
					double_list_append_as_tail(&pserver->conn_list,
						&pconn->node);
				}

				if (pnode1 == ptail) {
					break;
				}
			}
		}
		pthread_mutex_unlock(&g_server_lock);

		while (pnode=double_list_get_from_head(&temp_list)) {
			pconn = (REMOTE_CONN*)pnode->pdata;
			if (TRUE == g_notify_stop) {
				close(pconn->sockd);
				free(pconn);
				continue;
			}
			if (sizeof(uint32_t) != write(pconn->sockd,
				&ping_buff, sizeof(uint32_t))) {
				close(pconn->sockd);
				pconn->sockd = -1;
				pthread_mutex_lock(&g_server_lock);
				double_list_append_as_tail(&g_lost_list, &pconn->node);
				pthread_mutex_unlock(&g_server_lock);
				continue;
			}
			tv_msec = SOCKET_TIMEOUT * 1000;
			pfd_read.fd = pconn->sockd;
			pfd_read.events = POLLIN|POLLPRI;
			if (1 != poll(&pfd_read, 1, tv_msec) ||
				1 != read(pconn->sockd, &resp_buff, 1) ||
				RESPONSE_CODE_SUCCESS != resp_buff) {
				close(pconn->sockd);
				pconn->sockd = -1;
				pthread_mutex_lock(&g_server_lock);
				double_list_append_as_tail(&g_lost_list, &pconn->node);
				pthread_mutex_unlock(&g_server_lock);
			} else {
				time(&pconn->last_time);
				pthread_mutex_lock(&g_server_lock);
				double_list_append_as_tail(&pconn->psvr->conn_list,
					&pconn->node);
				pthread_mutex_unlock(&g_server_lock);
			}
		}

		pthread_mutex_lock(&g_server_lock);
		while (pnode=double_list_get_from_head(&g_lost_list)) {
			double_list_append_as_tail(&temp_list, pnode);
		}
		pthread_mutex_unlock(&g_server_lock);

		while (pnode=double_list_get_from_head(&temp_list)) {
			pconn = (REMOTE_CONN*)pnode->pdata;
			if (TRUE == g_notify_stop) {
				close(pconn->sockd);
				free(pconn);
				continue;
			}
			pconn->sockd = dac_client_connect(pconn->psvr);
			if (-1 != pconn->sockd) {
				time(&pconn->last_time);
				pthread_mutex_lock(&g_server_lock);
				double_list_append_as_tail(&pconn->psvr->conn_list,
					&pconn->node);
				pthread_mutex_unlock(&g_server_lock);
			} else {
				pthread_mutex_lock(&g_server_lock);
				double_list_append_as_tail(&g_lost_list, &pconn->node);
				pthread_mutex_unlock(&g_server_lock);
			}
		}
		sleep(1);
	}
}

static REMOTE_CONN *dac_client_get_connection(const char *dir)
{
	REMOTE_SVR *pserver;
	DOUBLE_LIST_NODE *pnode;

	for (pnode=double_list_get_head(&g_server_list); NULL!=pnode;
		pnode=double_list_get_after(&g_server_list, pnode)) {
		pserver = (REMOTE_SVR*)pnode->pdata;
		if (0 == strncmp(dir, pserver->prefix, pserver->prefix_len)) {
			break;
		}
	}
	if (NULL == pnode) {
		printf("[dac_client]: cannot find remote server for %s\n", dir);
		return NULL;
	}
	pthread_mutex_lock(&g_server_lock);
	pnode = double_list_get_from_head(&pserver->conn_list);
	pthread_mutex_unlock(&g_server_lock);
	if (NULL == pnode) {
		printf("[dac_client]: no alive connection for"
			" remote server for %s\n", pserver->prefix);
		return NULL;
	}
	return (REMOTE_CONN*)pnode->pdata;
}

static void dac_client_put_connection(REMOTE_CONN *pconn, BOOL b_lost)
{
	if (FALSE == b_lost) {
		pthread_mutex_lock(&g_server_lock);
		double_list_append_as_tail(&pconn->psvr->conn_list, &pconn->node);
		pthread_mutex_unlock(&g_server_lock);
	} else {
		close(pconn->sockd);
		pconn->sockd = -1;
		pthread_mutex_lock(&g_server_lock);
		double_list_append_as_tail(&g_lost_list, &pconn->node);
		pthread_mutex_unlock(&g_server_lock);
	}
}

void dac_client_init(int conn_num, const char *list_path)
{
	g_notify_stop = TRUE;
	g_conn_num = conn_num;
	strcpy(g_list_path, list_path);
	double_list_init(&g_server_list);
	double_list_init(&g_lost_list);
	pthread_mutex_init(&g_server_lock, NULL);
}

int dac_client_run()
{
	int i, j;
	int list_num;
	DAC_ITEM *pitem;
	LIST_FILE *plist;
	REMOTE_CONN *pconn;
	REMOTE_SVR *pserver;
	
	plist = list_file_init(g_list_path, "%s:256%s:16%s:16%d");
	if (NULL == plist) {
		printf("[dac_client]: fail to open dac list file\n");
		return -1;
	}
	g_notify_stop = FALSE;
	list_num = list_file_get_item_num(plist);
	pitem = (DAC_ITEM*)list_file_get_list(plist);
	for (i=0; i<list_num; i++) {
		pserver = malloc(sizeof(REMOTE_SVR));
		if (NULL == pserver) {
			printf("[dac_client]: fail to allocate memory\n");
			list_file_free(plist);
			g_notify_stop = TRUE;
			return -2;
		}
		pserver->node.pdata = pserver;
		strcpy(pserver->prefix, pitem[i].prefix);
		pserver->prefix_len = strlen(pserver->prefix);
		if (0 == strcasecmp(pitem[i].type, "simple")) {
			pserver->type = DAC_DIR_TYPE_SIMPLE;
		} else if (0 == strcasecmp(pitem[i].type, "private")) {
			pserver->type = DAC_DIR_TYPE_PRIVATE;
		} else if (0 == strcasecmp(pitem[i].type, "public")) {
			pserver->type = DAC_DIR_TYPE_PUBLIC;
		} else {
			printf("[dac_client]: unknown type \"%s\", only can be "
				"\"private\", \"public\" or \"simple\"", pitem[i].type);
			list_file_free(plist);
			g_notify_stop = TRUE;
			return 2;
		}
		strcpy(pserver->ip_addr, pitem[i].ip_addr);
		pserver->port = pitem[i].port;
		double_list_init(&pserver->conn_list);
		double_list_append_as_tail(&g_server_list, &pserver->node);
		for (j=0; j<g_conn_num; j++) {
		   pconn = malloc(sizeof(REMOTE_CONN));
			if (NULL == pconn) {
				printf("[dac_client]: fail to allocate memory\n");
				list_file_free(plist);
				g_notify_stop = TRUE;
				return -3;
			}
			pconn->node.pdata = pconn;
			pconn->sockd = -1;
			pconn->psvr = pserver;
			double_list_append_as_tail(&g_lost_list, &pconn->node);
		}
	}
	list_file_free(plist);
	if (0 == g_conn_num) {
		return 0;
	}
	if (0 != pthread_create(&g_scan_id, NULL, scan_work_func, NULL)) {
		printf("[dac_client]: fail to create connection scan thread\n");
		g_notify_stop = TRUE;
		return -4;
	}
	return 0;
}

int dac_client_stop()
{
	REMOTE_CONN *pconn;
	REMOTE_SVR *pserver;
	DOUBLE_LIST_NODE *pnode;
	
	if (0 != g_conn_num) {
		if (FALSE == g_notify_stop) {
			g_notify_stop = TRUE;
			pthread_join(g_scan_id, NULL);
		}
	}
	g_notify_stop = TRUE;
	while (pnode=double_list_get_from_head(&g_lost_list)) {
		free(pnode->pdata);
	}
	while (pnode=double_list_get_from_head(&g_server_list)) {
		pserver = (REMOTE_SVR*)pnode->pdata;
		while (pnode=double_list_get_from_head(&pserver->conn_list)) {
			pconn = (REMOTE_CONN*)pnode->pdata;
			close(pconn->sockd);
			free(pconn);
		}
		free(pserver);
	}
	return 0;
}

void dac_client_free()
{
	double_list_free(&g_lost_list);
	double_list_free(&g_server_list);
	pthread_mutex_destroy(&g_server_lock);
}

static BOOL dac_client_do_rpc(const char *dir,
	const RPC_REQUEST *prequest, RPC_RESPONSE *presponse)
{
	BINARY tmp_bin;
	REMOTE_CONN *pconn;
	
	if (TRUE != dac_ext_push_request(prequest, &tmp_bin)) {
		return FALSE;
	}
	pconn = dac_client_get_connection(dir);
	if (NULL == pconn) {
		free(tmp_bin.pb);
		return FALSE;
	}
	if (FALSE == dac_client_write_socket(pconn->sockd, &tmp_bin)) {
		free(tmp_bin.pb);
		dac_client_put_connection(pconn, TRUE);
		return FALSE;
	}
	free(tmp_bin.pb);
	if (FALSE == dac_client_read_socket(pconn->sockd, &tmp_bin)) {
		dac_client_put_connection(pconn, TRUE);
		return FALSE;
	}
	time(&pconn->last_time);
	dac_client_put_connection(pconn, FALSE);
	if (tmp_bin.cb < 5 || RESPONSE_CODE_SUCCESS != tmp_bin.pb[0]) {
		return FALSE;
	}
	presponse->call_id = prequest->call_id;
	tmp_bin.cb -= 5;
	tmp_bin.pb += 5;
	return dac_ext_pull_response(&tmp_bin, presponse);
}

static uint32_t dac_client_infostorage(const char *dir,
	uint64_t *ptotal_space, uint64_t *ptotal_used, uint32_t *ptotal_dir)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_INFOSTORAGE;
	request.dir = (void*)dir;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*ptotal_space = response.payload.infostorage.total_space;
		*ptotal_used = response.payload.infostorage.total_used;
		*ptotal_dir = response.payload.infostorage.total_dir;
	}
	return response.result;
}

uint32_t dac_client_allocdir(uint8_t type, char *dir)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_ALLOCDIR;
	
	//TODO get info of all storage and try to allocate from one storage
	
	request.payload.allocdir.type = type;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		strcpy(dir, response.payload.allocdir.pdir);
	}
	return response.result;
}

uint32_t dac_client_freedir(const char *dir)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_FREEDIR;
	request.dir = (void*)dir;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t dac_client_setpropvals(const char *dir,
	const char *pnamespace, const DAC_VARRAY *ppropvals)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_SETPROPVALS;
	request.dir = (void*)dir;
	request.payload.setpropvals.pnamespace = pnamespace;
	request.payload.setpropvals.ppropvals = ppropvals;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t dac_client_getpropvals(const char *dir,
	const char *pnamespace, const STRING_ARRAY *pnames,
	DAC_VARRAY *ppropvals)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_GETPROPVALS;
	request.dir = (void*)dir;
	request.payload.getpropvals.pnamespace = pnamespace;
	request.payload.getpropvals.pnames = pnames;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*ppropvals = response.payload.getpropvals.propvals;
	}
	return response.result;
}

uint32_t dac_client_deletepropvals(const char *dir,
	const char *pnamespace, const STRING_ARRAY *pnames)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_DELETEPROPVALS;
	request.dir = (void*)dir;
	request.payload.deletepropvals.pnamespace = pnamespace;
	request.payload.deletepropvals.pnames = pnames;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t dac_client_opentable(const char *dir,
	const char *pnamespace, const char *ptablename,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_OPENTABLE;
	request.dir = (void*)dir;
	request.payload.opentable.pnamespace = pnamespace;
	request.payload.opentable.ptablename = ptablename;
	request.payload.opentable.flags = flags;
	request.payload.opentable.puniquename = puniquename;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*ptable_id = response.payload.opentable.table_id;
	}
	return response.result;
}

uint32_t dac_client_opencelltable(const char *dir,
	uint64_t row_instance, const char *pcolname,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_OPENCELLTABLE;
	request.dir = (void*)dir;
	request.payload.opencelltable.row_instance = row_instance;
	request.payload.opencelltable.pcolname = pcolname;
	request.payload.opencelltable.flags = flags;
	request.payload.opencelltable.puniquename = puniquename;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*ptable_id = response.payload.opencelltable.table_id;
	}
	return response.result;
}

uint32_t dac_client_restricttable(const char *dir,
	uint32_t table_id, const DAC_RES *prestrict)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_RESTRICTTABLE;
	request.dir = (void*)dir;
	request.payload.restricttable.table_id = table_id;
	request.payload.restricttable.prestrict = prestrict;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t dac_client_updatecells(const char *dir,
	uint64_t row_instance, const DAC_VARRAY *pcells)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_UPDATECELLS;
	request.dir = (void*)dir;
	request.payload.updatecells.row_instance = row_instance;
	request.payload.updatecells.pcells = pcells;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t dac_client_sumtable(const char *dir,
	uint32_t table_id, uint32_t *pcount)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_SUMTABLE;
	request.dir = (void*)dir;
	request.payload.sumtable.table_id = table_id;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pcount = response.payload.sumtable.count;
	}
	return response.result;
}

uint32_t dac_client_queryrows(const char *dir,
	uint32_t table_id, uint32_t start_pos,
	uint32_t row_count, DAC_VSET *pset)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_QUERYROWS;
	request.dir = (void*)dir;
	request.payload.queryrows.table_id = table_id;
	request.payload.queryrows.start_pos = start_pos;
	request.payload.queryrows.row_count = row_count;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pset = response.payload.queryrows.set;
	}
	return response.result;
}

uint32_t dac_client_insertrows(const char *dir,
	uint32_t table_id, uint32_t flags, const DAC_VSET *pset)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_INSERTROWS;
	request.dir = (void*)dir;
	request.payload.insertrows.table_id = table_id;
	request.payload.insertrows.flags = flags;
	request.payload.insertrows.pset = pset;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}
	
uint32_t dac_client_deleterow(const char *dir, uint64_t row_instance)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_DELETEROW;
	request.dir = (void*)dir;
	request.payload.deleterow.row_instance = row_instance;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t dac_client_deletecells(const char *dir,
	uint64_t row_instance, const STRING_ARRAY *pcolnames)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_DELETECELLS;
	request.dir = (void*)dir;
	request.payload.deletecells.row_instance = row_instance;
	request.payload.deletecells.pcolnames = pcolnames;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t dac_client_deletetable(const char *dir,
	const char *pnamespace, const char *ptablename)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_DELETETABLE;
	request.dir = (void*)dir;
	request.payload.deletetable.pnamespace = pnamespace;
	request.payload.deletetable.ptablename = ptablename;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t dac_client_closetable(
	const char *dir, uint32_t table_id)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_CLOSETABLE;
	request.dir = (void*)dir;
	request.payload.closetable.table_id = table_id;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t dac_client_matchrow(const char *dir,
	uint32_t table_id, uint16_t type,
	const void *pvalue, DAC_VARRAY *prow)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_MATCHROW;
	request.dir = (void*)dir;
	request.payload.matchrow.table_id = table_id;
	request.payload.matchrow.type = type;
	request.payload.matchrow.pvalue = pvalue;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*prow = response.payload.matchrow.row;
	}
	return response.result;
}

uint32_t dac_client_getnamespaces(const char *dir,
	STRING_ARRAY *pnamespaces)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_GETNAMESPACES;
	request.dir = (void*)dir;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pnamespaces = response.payload.getnamespaces.namespaces;
	}
	return response.result;
}

uint32_t dac_client_getpropnames(const char *dir,
	const char *pnamespace, STRING_ARRAY *ppropnames)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_GETPROPNAMES;
	request.dir = (void*)dir;
	request.payload.getpropnames.pnamespace = pnamespace;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*ppropnames = response.payload.getpropnames.propnames;
	}
	return response.result;
}

uint32_t dac_client_gettablenames(const char *dir,
	const char *pnamespace, STRING_ARRAY *ptablenames)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_GETTABLENAMES;
	request.dir = (void*)dir;
	request.payload.gettablenames.pnamespace = pnamespace;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*ptablenames = response.payload.gettablenames.tablenames;
	}
	return response.result;
}

uint32_t dac_client_readfile(const char *dir, const char *path,
	const char *file_name, uint64_t offset, uint32_t length,
	BINARY *pcontent_bin)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_READFILE;
	request.dir = (void*)dir;
	request.payload.readfile.path = path;
	request.payload.readfile.file_name = file_name;
	request.payload.readfile.offset = offset;
	request.payload.readfile.length = length;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pcontent_bin = response.payload.readfile.content_bin;
	}
	return response.result;
}

uint32_t dac_client_appendfile(const char *dir,
	const char *path, const char *file_name,
	const BINARY *pcontent_bin)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_APPENDFILE;
	request.dir = (void*)dir;
	request.payload.appendfile.path = path;
	request.payload.appendfile.file_name = file_name;
	request.payload.appendfile.pcontent_bin = pcontent_bin;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;	
}

uint32_t dac_client_removefile(const char *dir,
	const char *path, const char *file_name)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_REMOVEFILE;
	request.dir = (void*)dir;
	request.payload.removefile.path = path;
	request.payload.removefile.file_name = file_name;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;	
}

uint32_t dac_client_statfile(const char *dir,
	const char *path, const char *file_name,
	uint64_t *psize)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_STATFILE;
	request.dir = (void*)dir;
	request.payload.statfile.path = path;
	request.payload.statfile.file_name = file_name;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*psize = response.payload.statfile.size;
	}
	return response.result;
}

uint32_t dac_client_checkrow(const char *dir,
	const char *pnamespace, const char *ptablename,
	uint16_t type, const void *pvalue)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_CHECKROW;
	request.dir = (void*)dir;
	request.payload.checkrow.pnamespace = pnamespace;
	request.payload.checkrow.ptablename = ptablename;
	request.payload.checkrow.type = type;
	request.payload.checkrow.pvalue = pvalue;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t dac_client_phprpc(const char *dir, const char *account,
	const char *script_path, const BINARY *pbin_in, BINARY *pbin_out)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;

	request.call_id = CALL_ID_PHPRPC;
	request.dir = (void*)dir;
	request.payload.phprpc.account = account;
	request.payload.phprpc.script_path = script_path;
	request.payload.phprpc.pbin_in = pbin_in;
	if (FALSE == dac_client_do_rpc(dir, &request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pbin_out = response.payload.phprpc.bin_out;
	}
	return response.result;
}
