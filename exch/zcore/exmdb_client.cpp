// SPDX-License-Identifier: GPL-2.0-only WITH linking exception
// SPDX-FileCopyrightText: 2021 grammm GmbH
// This file is part of Gromox.
#include <algorithm>
#include <cstdint>
#include <list>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <gromox/defs.h>
#include <gromox/config_file.hpp>
#include <gromox/exmdb_rpc.hpp>
#include <gromox/fileio.h>
#include <gromox/socket.h>
#include <libHX/string.h>
#include "exmdb_client.h"
#include <gromox/double_list.hpp>
#include "common_util.h"
#include <gromox/list_file.hpp>
#include "exmdb_ext.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <csignal>
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <poll.h>

struct REMOTE_CONN;
struct REMOTE_SVR : public EXMDB_ITEM {
	REMOTE_SVR(EXMDB_ITEM &&o) : EXMDB_ITEM(std::move(o)) {}
	std::list<REMOTE_CONN> conn_list;
};

struct REMOTE_CONN {
	time_t last_time;
	REMOTE_SVR *psvr;
	int sockd;
};

struct REMOTE_CONN_floating {
	REMOTE_CONN_floating() = default;
	REMOTE_CONN_floating(REMOTE_CONN_floating &&);
	~REMOTE_CONN_floating() { reset(true); }
	REMOTE_CONN *operator->() { return tmplist.size() != 0 ? &tmplist.front() : nullptr; }
	bool operator==(std::nullptr_t) const { return tmplist.size() == 0; }
	bool operator!=(std::nullptr_t) const { return tmplist.size() != 0; }
	void reset(bool lost = false);

	std::list<REMOTE_CONN> tmplist;
};

struct AGENT_THREAD {
	REMOTE_SVR *pserver;
	pthread_t thr_id;
	int sockd;
};

static int g_conn_num;
static int g_threads_num;
static BOOL g_notify_stop;
static pthread_t g_scan_id;
static std::list<REMOTE_CONN> g_lost_list;
static std::list<AGENT_THREAD> g_agent_list;
static std::list<REMOTE_SVR> g_server_list;
static std::mutex g_server_lock;

static void (*exmdb_client_event_proc)(const char *dir,
	BOOL b_table, uint32_t notify_id, const DB_NOTIFY *pdb_notify);

int exmdb_client_get_param(int param)
{
	int total_num;
	
	switch (param) {
	case ALIVE_PROXY_CONNECTIONS:
		total_num = 0;
		for (const auto &srv : g_server_list)
			total_num += srv.conn_list.size();
		return total_num;
	case LOST_PROXY_CONNECTIONS:
		return g_lost_list.size();
	}
	return -1;
}

static BOOL exmdb_client_read_socket(int sockd, BINARY *pbin)
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
		if (pbin->pb == nullptr) {
			read_len = read(sockd, resp_buff, 5);
			if (1 == read_len) {
				pbin->cb = 1;
				pbin->pb = cu_alloc<uint8_t>(1);
				if (pbin->pb == nullptr)
					return FALSE;
				*pbin->pb = resp_buff[0];
				return TRUE;
			} else if (5 == read_len) {
				uint32_t cb;
				memcpy(&cb, resp_buff + 1, sizeof(cb));
				pbin->cb = cb + 5;
				pbin->pb = cu_alloc<uint8_t>(pbin->cb);
				if (pbin->pb == nullptr)
					return FALSE;
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

static BOOL exmdb_client_write_socket(int sockd, const BINARY *pbin)
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

static int exmdb_client_connect_exmdb(REMOTE_SVR *pserver, BOOL b_listen)
{
	int process_id;
	BINARY tmp_bin;
	char remote_id[128];
	EXMDB_REQUEST request;
	uint8_t response_code;

	int sockd = gx_inet_connect(pserver->host.c_str(), pserver->port, 0);
	if (sockd < 0)
	        return -1;
	process_id = getpid();
	sprintf(remote_id, "zcore:%d", process_id);
	if (FALSE == b_listen) {
		request.call_id = exmdb_callid::CONNECT;
		request.payload.connect.prefix = deconst(pserver->prefix.c_str());
		request.payload.connect.remote_id = remote_id;
		request.payload.connect.b_private = pserver->type == EXMDB_ITEM::EXMDB_PRIVATE ? TRUE : false;
	} else {
		request.call_id = exmdb_callid::LISTEN_NOTIFICATION;
		request.payload.listen_notification.remote_id = remote_id;
	}
	if (EXT_ERR_SUCCESS != exmdb_ext_push_request(&request, &tmp_bin)) {
		close(sockd);
		return -1;
	}
	if (FALSE == exmdb_client_write_socket(sockd, &tmp_bin)) {
		free(tmp_bin.pb);
		close(sockd);
		return -1;
	}
	free(tmp_bin.pb);
	common_util_build_environment();
	if (FALSE == exmdb_client_read_socket(sockd, &tmp_bin)) {
		common_util_free_environment();
		close(sockd);
		return -1;
	}
	response_code = tmp_bin.pb[0];
	if (response_code == exmdb_response::SUCCESS) {
		uint32_t cb;
		memcpy(&cb, tmp_bin.pb + 1, sizeof(cb));
		if (tmp_bin.cb != 5 || cb != 0) {
			common_util_free_environment();
			printf("[exmdb_client]: response format error "
			       "during connect to [%s]:%hu/%s\n",
			       pserver->host.c_str(), pserver->port, pserver->prefix.c_str());
			close(sockd);
			return -1;
		}
		common_util_free_environment();
		return sockd;
	}
	printf("[exmdb_provider]: Failed to connect to [%s]:%hu/%s",
	       pserver->host.c_str(), pserver->port, pserver->prefix.c_str());
	common_util_free_environment();
	switch (response_code) {
	case exmdb_response::ACCESS_DENY:
		printf(": access denied\n");
		break;
	case exmdb_response::MAX_REACHED:
		printf(": maximum connections reached in server\n");
		break;
	case exmdb_response::LACK_MEMORY:
		printf(": server out of memory\n");
		break;
	case exmdb_response::MISCONFIG_PREFIX:
		printf(": prefix not served by server\n");
		break;
	case exmdb_response::MISCONFIG_MODE:
		printf(": misconfigured prefix mode\n");
		break;
	default:
		printf(": error code %d\n", response_code);
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
	struct pollfd pfd_read;
	std::list<REMOTE_CONN> temp_list;
	
	ping_buff = 0;
	while (FALSE == g_notify_stop) {
		std::unique_lock sv_hold(g_server_lock);
		time(&now_time);
		for (auto &srv : g_server_list) {
			auto tail = &*srv.conn_list.rbegin();
			while (srv.conn_list.size() > 0) {
				auto pconn = &srv.conn_list.front();
				if (now_time - pconn->last_time >= SOCKET_TIMEOUT - 3)
					temp_list.splice(temp_list.end(), srv.conn_list, srv.conn_list.begin());
				else
					srv.conn_list.splice(srv.conn_list.end(), srv.conn_list, srv.conn_list.begin());
				if (pconn == tail)
					break;
			}
		}
		sv_hold.unlock();

		while (temp_list.size() > 0) {
			auto pconn = &temp_list.front();
			if (TRUE == g_notify_stop) {
				close(pconn->sockd);
				temp_list.pop_front();
				continue;
			}
			if (sizeof(uint32_t) != write(pconn->sockd,
				&ping_buff, sizeof(uint32_t))) {
				close(pconn->sockd);
				pconn->sockd = -1;
				sv_hold.lock();
				g_lost_list.splice(g_lost_list.end(), temp_list, temp_list.begin());
				sv_hold.unlock();
				continue;
			}
			tv_msec = SOCKET_TIMEOUT * 1000;
			pfd_read.fd = pconn->sockd;
			pfd_read.events = POLLIN|POLLPRI;
			if (1 != poll(&pfd_read, 1, tv_msec) ||
				1 != read(pconn->sockd, &resp_buff, 1) ||
			    resp_buff != exmdb_response::SUCCESS) {
				close(pconn->sockd);
				pconn->sockd = -1;
				sv_hold.lock();
				g_lost_list.splice(g_lost_list.end(), temp_list, temp_list.begin());
				sv_hold.unlock();
			} else {
				time(&pconn->last_time);
				sv_hold.lock();
				pconn->psvr->conn_list.splice(pconn->psvr->conn_list.end(), temp_list, temp_list.begin());
				sv_hold.unlock();
			}
		}

		sv_hold.lock();
		temp_list = std::move(g_lost_list);
		sv_hold.unlock();

		while (temp_list.size() > 0) {
			auto pconn = &temp_list.front();
			if (TRUE == g_notify_stop) {
				close(pconn->sockd);
				temp_list.pop_front();
				continue;
			}
			pconn->sockd = exmdb_client_connect_exmdb(pconn->psvr, FALSE);
			if (-1 != pconn->sockd) {
				time(&pconn->last_time);
				sv_hold.lock();
				pconn->psvr->conn_list.splice(pconn->psvr->conn_list.end(), temp_list, temp_list.begin());
				sv_hold.unlock();
			} else {
				sv_hold.lock();
				g_lost_list.splice(g_lost_list.end(), temp_list, temp_list.begin());
				sv_hold.unlock();
			}
		}
		sleep(1);
	}
	return NULL;
}

static void *thread_work_func(void *pparam)
{
	int i;
	int tv_msec;
	int read_len;
	BINARY tmp_bin;
	uint32_t offset;
	uint8_t resp_code;
	uint32_t buff_len;
	uint8_t buff[0x8000];
	AGENT_THREAD *pagent;
	struct pollfd pfd_read;
	DB_NOTIFY_DATAGRAM notify;
	
	pagent = (AGENT_THREAD*)pparam;
	while (FALSE == g_notify_stop) {
		pagent->sockd = exmdb_client_connect_exmdb(
							pagent->pserver, TRUE);
		if (-1 == pagent->sockd) {
			sleep(1);
			continue;
		}
		buff_len = 0;
		while (TRUE) {
			tv_msec = SOCKET_TIMEOUT * 1000;
			pfd_read.fd = pagent->sockd;
			pfd_read.events = POLLIN|POLLPRI;
			if (1 != poll(&pfd_read, 1, tv_msec)) {
				close(pagent->sockd);
				pagent->sockd = -1;
				break;
			}
			if (0 == buff_len) {
				if (sizeof(uint32_t) != read(pagent->sockd,
					&buff_len, sizeof(uint32_t))) {
					close(pagent->sockd);
					pagent->sockd = -1;
					break;
				}
				/* ping packet */
				if (0 == buff_len) {
					resp_code = exmdb_response::SUCCESS;
					if (1 != write(pagent->sockd, &resp_code, 1)) {
						close(pagent->sockd);
						pagent->sockd = -1;
						break;
					}
				}
				offset = 0;
				continue;
			}
			read_len = read(pagent->sockd, buff + offset, buff_len - offset);
			if (read_len <= 0) {
				close(pagent->sockd);
				pagent->sockd = -1;
				break;
			}
			offset += read_len;
			if (offset == buff_len) {
				tmp_bin.cb = buff_len;
				tmp_bin.pb = buff;
				common_util_build_environment();
				if (EXT_ERR_SUCCESS == exmdb_ext_pull_db_notify(
					&tmp_bin, &notify)) {
					resp_code = exmdb_response::SUCCESS;
				} else {
					resp_code = exmdb_response::PULL_ERROR;
				}
				if (1 != write(pagent->sockd, &resp_code, 1)) {
					close(pagent->sockd);
					pagent->sockd = -1;
					common_util_free_environment();
					break;
				}
				if (resp_code == exmdb_response::SUCCESS) {
					for (i=0; i<notify.id_array.count; i++) {
						exmdb_client_event_proc(notify.dir,
							notify.b_table, notify.id_array.pl[i],
							&notify.db_notify);
					}
				}
				common_util_free_environment();
				buff_len = 0;
			}
		}
	}
	return nullptr;
}

static REMOTE_CONN_floating exmdb_client_get_connection(const char *dir)
{
	REMOTE_CONN_floating fc;
	auto i = std::find_if(g_server_list.begin(), g_server_list.end(),
	         [&](const REMOTE_SVR &s) { return strncmp(dir, s.prefix.c_str(), s.prefix.size()) == 0; });
	if (i == g_server_list.end()) {
		printf("[exmdb_client]: cannot find remote server for %s\n", dir);
		return fc;
	}
	std::unique_lock sv_hold(g_server_lock);
	if (i->conn_list.size() == 0) {
		sv_hold.unlock();
		printf("[exmdb_client]: no alive connection for [%s]:%hu/%s\n",
		       i->host.c_str(), i->port, i->prefix.c_str());
		return fc;
	}
	fc.tmplist.splice(fc.tmplist.end(), i->conn_list, i->conn_list.begin());
	return fc;
}

void REMOTE_CONN_floating::reset(bool lost)
{
	if (tmplist.size() == 0)
		return;
	auto pconn = &tmplist.front();
	if (!lost) {
		std::unique_lock sv_hold(g_server_lock);
		pconn->psvr->conn_list.splice(pconn->psvr->conn_list.end(), tmplist, tmplist.begin());
	} else {
		close(pconn->sockd);
		pconn->sockd = -1;
		std::unique_lock sv_hold(g_server_lock);
		g_lost_list.splice(g_lost_list.end(), tmplist, tmplist.begin());
	}
	tmplist.clear();
}

REMOTE_CONN_floating::REMOTE_CONN_floating(REMOTE_CONN_floating &&o)
{
	reset(true);
	tmplist = std::move(o.tmplist);
}

void exmdb_client_init(int conn_num, int threads_num)
{
	g_notify_stop = TRUE;
	g_conn_num = conn_num;
	g_threads_num = threads_num;
}

int exmdb_client_run(const char *configdir)
{
	std::vector<EXMDB_ITEM> xmlist;

	auto ret = list_file_read_exmdb("exmdb_list.txt", configdir, xmlist);
	if (ret < 0) {
		printf("[exmdb_client]: list_file_read_exmdb: %s\n", strerror(-ret));
		return 1;
	}
	g_notify_stop = FALSE;
	for (auto &&item : xmlist) {
		try {
			g_server_list.emplace_back(std::move(item));
		} catch (const std::bad_alloc &) {
			printf("[exmdb_client]: Failed to allocate memory for exmdb\n");
			g_notify_stop = TRUE;
			return 5;
		}
		auto &srv = g_server_list.back();
		for (decltype(g_conn_num) j = 0; j < g_conn_num; ++j) {
			REMOTE_CONN conn;
			conn.sockd = -1;
			static_assert(std::is_same_v<decltype(g_server_list), std::list<decltype(g_server_list)::value_type>>,
				"addrof REMOTE_SVRs must not change; REMOTE_CONN/AGENT_THREAD has a pointer to it");
			conn.psvr = &srv;
			try {
				g_lost_list.push_back(std::move(conn));
			} catch (const std::bad_alloc &) {
				printf("[exmdb_client]: fail to "
					"allocate memory for exmdb\n");
				g_notify_stop = TRUE;
				return 6;
			}
		}
		for (decltype(g_threads_num) j = 0; j < g_threads_num; ++j) {
			try {
				g_agent_list.push_back(AGENT_THREAD{});
			} catch (const std::bad_alloc &) {
				printf("[exmdb_client]: fail to "
					"allocate memory for exmdb\n");
				g_notify_stop = TRUE;
				return 7;
			}
			auto &ag = g_agent_list.back();
			ag.pserver = &srv;
			ag.sockd = -1;
			static_assert(std::is_same_v<decltype(g_agent_list), std::list<decltype(g_agent_list)::value_type>>,
				"addrof AGENT_THREADs must not change; other thread has its address in use");
			if (pthread_create(&ag.thr_id, nullptr, thread_work_func, &ag) != 0) {
				printf("[exmdb_client]: fail to "
					"create agent thread for exmdb\n");
				g_notify_stop = TRUE;
				g_agent_list.pop_back();
				return 8;
			}
			char buf[32];
			snprintf(buf, sizeof(buf), "exmdbcl/%u", j);
			pthread_setname_np(ag.thr_id, buf);
		}
	}
	if (0 == g_conn_num) {
		return 0;
	}
	ret = pthread_create(&g_scan_id, nullptr, scan_work_func, nullptr);
	if (ret != 0) {
		printf("[exmdb_client]: failed to create proxy scan thread: %s\n", strerror(ret));
		g_notify_stop = TRUE;
		return 9;
	}
	pthread_setname_np(g_scan_id, "mdbclient/scan");
	return 0;
}

int exmdb_client_stop()
{
	if (0 != g_conn_num) {
		if (FALSE == g_notify_stop) {
			g_notify_stop = TRUE;
			pthread_join(g_scan_id, NULL);
		}
	}
	g_notify_stop = TRUE;
	for (auto &ag : g_agent_list) {
		pthread_cancel(ag.thr_id);
		if (ag.sockd != -1)
			close(ag.sockd);
	}
	for (auto &srv : g_server_list)
		for (auto &conn : srv.conn_list)
			close(conn.sockd);
	return 0;
}

BOOL exmdb_client_do_rpc(const char *dir,
	const EXMDB_REQUEST *prequest, EXMDB_RESPONSE *presponse)
{
	BINARY tmp_bin;
	
	if (EXT_ERR_SUCCESS != exmdb_ext_push_request(prequest, &tmp_bin)) {
		return FALSE;
	}
	auto pconn = exmdb_client_get_connection(dir);
	if (pconn == nullptr) {
		free(tmp_bin.pb);
		return FALSE;
	}
	if (FALSE == exmdb_client_write_socket(pconn->sockd, &tmp_bin)) {
		free(tmp_bin.pb);
		return FALSE;
	}
	free(tmp_bin.pb);
	if (FALSE == exmdb_client_read_socket(pconn->sockd, &tmp_bin)) {
		return FALSE;
	}
	time(&pconn->last_time);
	pconn.reset();
	if (tmp_bin.cb < 5 || tmp_bin.pb[0] != exmdb_response::SUCCESS)
		return FALSE;
	presponse->call_id = prequest->call_id;
	tmp_bin.cb -= 5;
	tmp_bin.pb += 5;
	if (EXT_ERR_SUCCESS != exmdb_ext_pull_response(&tmp_bin, presponse)) {
		return FALSE;
	}
	return TRUE;
}

BOOL exmdb_client_get_named_propid(const char *dir,
	BOOL b_create, const PROPERTY_NAME *ppropname,
	uint16_t *ppropid)
{
	PROPID_ARRAY tmp_propids;
	PROPNAME_ARRAY tmp_propnames;
	
	tmp_propnames.count = 1;
	tmp_propnames.ppropname = (PROPERTY_NAME*)ppropname;
	if (FALSE == exmdb_client_get_named_propids(dir,
		b_create, &tmp_propnames, &tmp_propids)) {
		return FALSE;	
	}
	*ppropid = *tmp_propids.ppropid;
	return TRUE;
}

BOOL exmdb_client_get_named_propname(const char *dir,
	uint16_t propid, PROPERTY_NAME *ppropname)
{
	PROPID_ARRAY tmp_propids;
	PROPNAME_ARRAY tmp_propnames;
	
	tmp_propids.count = 1;
	tmp_propids.ppropid = &propid;
	if (FALSE == exmdb_client_get_named_propnames(dir,
		&tmp_propids, &tmp_propnames)) {
		return FALSE;	
	}
	*ppropname = *tmp_propnames.ppropname;
	return TRUE;
}

BOOL exmdb_client_get_folder_property(const char *dir,
	uint32_t cpid, uint64_t folder_id,
	uint32_t proptag, void **ppval)
{
	PROPTAG_ARRAY tmp_proptags;
	TPROPVAL_ARRAY tmp_propvals;
	
	tmp_proptags.count = 1;
	tmp_proptags.pproptag = &proptag;
	if (FALSE == exmdb_client_get_folder_properties(
		dir, cpid, folder_id, &tmp_proptags,
		&tmp_propvals)) {
		return FALSE;	
	}
	if (0 == tmp_propvals.count) {
		*ppval = NULL;
	} else {
		*ppval = tmp_propvals.ppropval->pvalue;
	}
	return TRUE;
}

BOOL exmdb_client_get_message_property(const char *dir,
	const char *username, uint32_t cpid, uint64_t message_id,
	uint32_t proptag, void **ppval)
{
	PROPTAG_ARRAY tmp_proptags;
	TPROPVAL_ARRAY tmp_propvals;
	
	tmp_proptags.count = 1;
	tmp_proptags.pproptag = &proptag;
	if (FALSE == exmdb_client_get_message_properties(dir,
		username, cpid, message_id, &tmp_proptags, &tmp_propvals)) {
		return FALSE;	
	}
	if (0 == tmp_propvals.count) {
		*ppval = NULL;
	} else {
		*ppval = tmp_propvals.ppropval->pvalue;
	}
	return TRUE;
}

BOOL exmdb_client_delete_message(const char *dir,
	int account_id, uint32_t cpid, uint64_t folder_id,
	uint64_t message_id, BOOL b_hard, BOOL *pb_done)
{
	BOOL b_partial;
	EID_ARRAY message_ids;
	
	message_ids.count = 1;
	message_ids.pids = &message_id;
	if (FALSE == exmdb_client_delete_messages(dir, account_id,
		cpid, NULL, folder_id, &message_ids, b_hard, &b_partial)) {
		return FALSE;	
	}
	if (FALSE == b_partial) {
		*pb_done = TRUE;
	} else {
		*pb_done = FALSE;
	}
	return TRUE;
}

BOOL exmdb_client_get_instance_property(
	const char *dir, uint32_t instance_id,
	uint32_t proptag, void **ppval)
{
	PROPTAG_ARRAY tmp_proptags;
	TPROPVAL_ARRAY tmp_propvals;
	
	tmp_proptags.count = 1;
	tmp_proptags.pproptag = &proptag;
	if (FALSE == exmdb_client_get_instance_properties(dir,
		0, instance_id, &tmp_proptags, &tmp_propvals)) {
		return FALSE;	
	}
	if (0 == tmp_propvals.count) {
		*ppval = NULL;
	} else {
		*ppval = tmp_propvals.ppropval->pvalue;
	}
	return TRUE;
}

BOOL exmdb_client_set_instance_property(
	const char *dir, uint32_t instance_id,
	const TAGGED_PROPVAL *ppropval, uint32_t *presult)
{
	PROBLEM_ARRAY tmp_problems;
	TPROPVAL_ARRAY tmp_propvals;
	
	tmp_propvals.count = 1;
	tmp_propvals.ppropval = (TAGGED_PROPVAL*)ppropval;
	if (FALSE == exmdb_client_set_instance_properties(dir,
		instance_id, &tmp_propvals, &tmp_problems)) {
		return FALSE;	
	}
	if (0 == tmp_problems.count) {
		*presult = 0;
	} else {
		*presult = tmp_problems.pproblem->err;
	}
	return TRUE;
}

BOOL exmdb_client_remove_instance_property(const char *dir,
	uint32_t instance_id, uint32_t proptag, uint32_t *presult)
{
	PROPTAG_ARRAY tmp_proptags;
	PROBLEM_ARRAY tmp_problems;
	
	tmp_proptags.count = 1;
	tmp_proptags.pproptag = &proptag;
	if (FALSE == exmdb_client_remove_instance_properties(
		dir, instance_id, &tmp_proptags, &tmp_problems)) {
		return FALSE;	
	}
	if (0 == tmp_problems.count) {
		*presult = 0;
	} else {
		*presult = tmp_problems.pproblem->err;
	}
	return TRUE;
}

BOOL exmdb_client_remove_message_property(const char *dir,
	uint32_t cpid, uint64_t message_id, uint32_t proptag)
{
	PROPTAG_ARRAY tmp_proptags;
	
	tmp_proptags.count = 1;
	tmp_proptags.pproptag = &proptag;
	if (FALSE == exmdb_client_remove_message_properties(
		dir, cpid, message_id, &tmp_proptags)) {
		return FALSE;	
	}
	return TRUE;
}

BOOL exmdb_client_check_message_owner(const char *dir,
	uint64_t message_id, const char *username, BOOL *pb_owner)
{
	BINARY *pbin;
	EXT_PULL ext_pull;
	char tmp_name[256];
	ADDRESSBOOK_ENTRYID ab_entryid;
	
	if (FALSE == exmdb_client_get_message_property(dir, NULL,
		0, message_id, PROP_TAG_CREATORENTRYID, (void**)&pbin)) {
		return FALSE;
	}
	if (NULL == pbin) {
		*pb_owner = FALSE;
		return TRUE;
	}
	ext_buffer_pull_init(&ext_pull, pbin->pb,
		pbin->cb, common_util_alloc, 0);
	if (EXT_ERR_SUCCESS != ext_buffer_pull_addressbook_entryid(
		&ext_pull, &ab_entryid)) {
		*pb_owner = false;
		return TRUE;
	}
	if (FALSE == common_util_essdn_to_username(
		ab_entryid.px500dn, tmp_name)) {
		*pb_owner = false;
		return TRUE;
	}
	if (0 == strcasecmp(username, tmp_name)) {
		*pb_owner = TRUE;
	} else {
		*pb_owner = FALSE;
	}
	return TRUE;
}

void exmdb_client_register_proc(void *pproc)
{
	exmdb_client_event_proc = reinterpret_cast<decltype(exmdb_client_event_proc)>(pproc);
}
