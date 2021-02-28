#include <libHX/string.h>
#include <gromox/cookie_parser.hpp>
#include <gromox/defs.h>
#include "common_util.h"
#include <gromox/hpm_common.h>
#include <gromox/str_hash.hpp>
#include "ab_ext.h"
#include <gromox/guid.hpp>
#include <gromox/util.hpp>
#include <sys/time.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "nsp_bridge.h"

using namespace gromox;

DECLARE_API();

#define RESPONSE_CODE_SUCCESS					0
#define RESPONSE_CODE_UNKNOWN_FAILURE			1
#define RESPONSE_CODE_INVALID_VERB				2
#define RESPONSE_CODE_INVALID_PATH				3
#define RESPONSE_CODE_INVALID_HEADER			4
#define RESPONSE_CODE_INVALID_REQUEST_TYPE		5
#define RESPONSE_CODE_INVALID_CONTEXT_COOKIE	6
#define RESPONSE_CODE_MISSING_HEADER			7
#define RESPONSE_CODE_ANONYMOUS_NOT_ALLOWED		8
#define RESPONSE_CODE_TOO_LARGE					9
#define RESPONSE_CODE_CONTEXT_NOT_FOUND			10
#define RESPONSE_CODE_NO_PRIVILEGE				11
#define RESPONSE_CODE_INVALID_REQUEST_BODY		12
#define RESPONSE_CODE_MISSING_COOKIE			13
#define RESPONSE_CODE_RESERVED					14
#define RESPONSE_CODE_INVALID_SEQUENCE			15
#define RESPONSE_CODE_ENDPOINT_DISABLED			16
#define RESPONSE_CODE_INVALID_RESPONSE			17
#define RESPONSE_CODE_ENDPOINT_SHUTTING_DOWN	18

#define AVERAGE_SESSION_PER_CONTEXT				10

#define RESPONSE_PENDING_PERIOD					30

#define SESSION_VALID_INTERVAL					900

struct SESSION_DATA {
	GUID session_guid;
	GUID sequence_guid;
	char username[256];
	time_t expire_time;
};

static BOOL nsp_preproc(int context_id);

static BOOL nsp_proc(int context_id, const void *pcontent, uint64_t length);

static int nsp_retr(int context_id);

static void* scan_work_fun(void *pparam);

static BOOL (*get_id_from_username)(const char *username, int *puser_id);

static constexpr const char *g_error_text[] = {
	"The request was properly formatted and accepted.",
	"The request produced an unknown failure.",
	"The request has an invalid verb.",
	"The request has an invalid path.",
	"The request has an invalid header.",
	"The request has an invalid X-RequestType header.",
	"The request has an invalid session context cookie.",
	"The request has a missing required header.",
	"The request is anonymous, but anonymous requests are not accepted.",
	"The request is too large.",
	"The Session Context is not found.",
	"The client has no privileges to the Session Context.",
	"The request body is invalid.",
	"The request is missing a required cookie.",
	"This value MUST be ignored by the client.",
	"The request has violated the sequencing requirement"
		" of one request at a time per Session Context.",
	"The endpoint is disabled.",
	"The response is invalid.",
	"The endpoint is shutting down."
};

static BOOL g_notify_stop;
static pthread_t g_scan_id;
static pthread_mutex_t g_hash_lock;
static STR_HASH_TABLE *g_user_hash;
static STR_HASH_TABLE *g_session_hash;

BOOL HPM_LibMain(int reason, void **ppdata)
{
	int context_num;
	HPM_INTERFACE interface;
	
	switch (reason) {
    case PLUGIN_INIT:
		LINK_API(ppdata);
		pthread_mutex_init(&g_hash_lock, NULL);
		if (!query_service1(get_id_from_username)) {
			printf("[moh_nsp]: fail to get "
				"\"get_id_from_username\" service\n");
			return FALSE;
		}
		g_notify_stop = TRUE;
		context_num = get_context_num();
		g_session_hash = str_hash_init(
			context_num*AVERAGE_SESSION_PER_CONTEXT,
			sizeof(SESSION_DATA), NULL);
		if (NULL == g_session_hash) {
			printf("[moh_nsp]: fail to init session hash table\n");
			return FALSE;
		}
		g_user_hash = str_hash_init(
			context_num*AVERAGE_SESSION_PER_CONTEXT,
			sizeof(int), NULL);
		if (NULL == g_user_hash) {
			printf("[moh_nsp]: fail to init user hash table\n");
			return FALSE;
		}
		if (!query_service1(nsp_interface_bind) ||
		    !query_service1(nsp_interface_compare_mids) ||
		    !query_service1(nsp_interface_dntomid) ||
		    !query_service1(nsp_interface_get_matches) ||
		    !query_service1(nsp_interface_get_proplist) ||
		    !query_service1(nsp_interface_get_props) ||
		    !query_service1(nsp_interface_get_specialtable) ||
		    !query_service1(nsp_interface_get_templateinfo) ||
		    !query_service1(nsp_interface_mod_linkatt) ||
		    !query_service1(nsp_interface_mod_props) ||
		    !query_service1(nsp_interface_query_columns) ||
		    !query_service1(nsp_interface_query_rows) ||
		    !query_service1(nsp_interface_resolve_namesw) ||
		    !query_service1(nsp_interface_resort_restriction) ||
		    !query_service1(nsp_interface_seek_entries) ||
		    !query_service1(nsp_interface_unbind) ||
		    !query_service1(nsp_interface_update_stat)) {
			printf("[moh_nsp]: exchange_nsp not loaded\n");
			return false;
		}
		g_notify_stop = FALSE;
		if (0 != pthread_create(&g_scan_id, NULL, scan_work_fun, NULL)) {
			g_notify_stop = TRUE;
			printf("[moh_nsp]: fail create scanning thread\n");
			return FALSE;
		}
		interface.preproc = nsp_preproc;
		interface.proc = nsp_proc;
		interface.retr = nsp_retr;
		interface.send = NULL;
		interface.receive = NULL;
		interface.term = NULL;
		if (FALSE == register_interface(&interface)) {
			return FALSE;
		}
		printf("[moh_nsp]: plugin is loaded into system\n");
		return TRUE;
	case PLUGIN_FREE:
		if (FALSE == g_notify_stop) {
			g_notify_stop = TRUE;
			pthread_join(g_scan_id, NULL);
		}
		if (NULL != g_session_hash) {
			str_hash_free(g_session_hash);
			g_session_hash = NULL;
		}
		if (NULL != g_user_hash) {
			str_hash_free(g_user_hash);
			g_user_hash = NULL;
		}
		pthread_mutex_destroy(&g_hash_lock);
		return TRUE;
	}
}

static void* scan_work_fun(void *pparam)
{
	int *pcount;
	time_t cur_time;
	STR_HASH_ITER *iter;
	SESSION_DATA *psession;
	
	while (FALSE == g_notify_stop) {
		time(&cur_time);
		pthread_mutex_lock(&g_hash_lock);
		iter = str_hash_iter_init(g_session_hash);
		for (str_hash_iter_begin(iter);
			FALSE == str_hash_iter_done(iter);
			str_hash_iter_forward(iter)) {
			psession = static_cast<SESSION_DATA *>(str_hash_iter_get_value(iter, nullptr));
			if (psession->expire_time < cur_time) {
				pcount = static_cast<int *>(str_hash_query(g_user_hash, psession->username));
				if (NULL != pcount) {
					(*pcount) --;
					if (0 == *pcount) {
						str_hash_remove(g_user_hash, psession->username);
					}
				}
				str_hash_iter_remove(iter);
			}
		}
		str_hash_iter_free(iter);
		pthread_mutex_unlock(&g_hash_lock);
		sleep(3);
	}
	pthread_exit(0);
}

static void rfc1123_dstring(time_t unix_time, char *dstring)
{
	struct tm tmp_tm;

	gmtime_r(&unix_time, &tmp_tm);
	strftime(dstring, 128, "%a, %d %b %Y %T GMT", &tmp_tm);
}

static BOOL nsp_preproc(int context_id)
{
	int tmp_len;
	char tmp_uri[1024];
	HTTP_REQUEST *prequest;
	CONNECTION *pconnection;
	
	prequest = get_request(context_id);
	if (0 != strcasecmp(prequest->method, "POST")) {
		return FALSE;
	}
	if (MEM_END_OF_FILE == (tmp_len = mem_file_read(
		&prequest->f_request_uri, tmp_uri, sizeof(tmp_uri)))) {
		return FALSE;	
	}
	tmp_uri[tmp_len] = '\0';
	if (0 != strncasecmp(tmp_uri, "/mapi/nspi/?MailboxId=", 22)) {
		return FALSE;
	}
	pconnection = get_connection(context_id);
	set_ep_info(context_id, tmp_uri + 22, pconnection->server_port);
	return TRUE;
}

static BOOL error_responsecode(int context_id,
	struct timeval *pstart_time, int response_code)
{
	int text_len;
	int response_len;
	char dstring[128];
	char text_buff[512];
	char response_buff[4096];
	
	text_len = sprintf(text_buff,
		"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
		"<html><head>\r\n"
		"<title>MAPI OVER HTTP ERROR</title>\r\n"
		"</head><body>\r\n"
		"<h1>Diagnostic Information</h1>\r\n"
		"<p>%s</p>\r\n"
		"</body></html>\r\n", g_error_text[response_code]);
	rfc1123_dstring(pstart_time->tv_usec, dstring);
	response_len = snprintf(response_buff,
		sizeof(response_buff),
		"HTTP/1.1 200 OK\r\n"
		"Server: %s\r\n"
		"Cache-Control: private\r\n"
		"Content-Type: text/html\r\n"
		"X-ResponseCode: %d\r\n"
		"Content-Length: %d\r\n"
		"X-ServerApplication: Exchange/15.00.0847.4040\r\n"
		"Date: %s\r\n\r\n%s", get_host_ID(),
		response_code, text_len, dstring, text_buff);
	return write_response(context_id, response_buff, response_len);
}

static BOOL ping_response(int context_id,
	struct timeval *pstart_time, const char *request_id,
	const char *client_info,  const char *sid_string)
{
	int text_len;
	int elapsed_time;
	int response_len;
	char dstring[128];
	char text_buff[256];
	char response_buff[4096];
	struct timeval current_time;
	
	gettimeofday(&current_time, NULL);
	elapsed_time = (current_time.tv_sec - pstart_time->tv_sec)
		*1000 + (current_time.tv_usec - pstart_time->tv_usec)/1000;
	rfc1123_dstring(pstart_time->tv_sec, dstring);
	text_len = sprintf(text_buff,
		"PROCESSING\r\nDONE\r\n"
		"X-ElapsedTime: %d\r\n"
		"X-StartTime: %s\r\n\r\n",
		elapsed_time, dstring);
	rfc1123_dstring(current_time.tv_sec, dstring);
	response_len = snprintf(response_buff,
		sizeof(response_buff),
		"HTTP/1.1 200 OK\r\n"
		"Server: %s\r\n"
		"Cache-Control: private\r\n"
		"Content-Type: application/mapi-http\r\n"
		"X-RequestType: PING\r\n"
		"X-RequestId: %s\r\n"
		"X-ClientInfo: %s\r\n"
		"X-ResponseCode: 0\r\n"
		"X-PendingPeriod: %d\r\n"
		"X-ExpirationInfo: %d\r\n"
		"Content-Length: %d\r\n"
		"X-ServerApplication: Exchange/15.00.0847.4040\r\n"
		"Set-Cookie: sid=%s\r\n"
		"Date: %s\r\n\r\n%s",
		get_host_ID(), request_id, client_info,
		(int)RESPONSE_PENDING_PERIOD*1000,
		(int)SESSION_VALID_INTERVAL*1000,
		text_len, sid_string, dstring, text_buff);
	return write_response(context_id, response_buff, response_len);
}

static BOOL failure_response(int context_id,
	struct timeval *pstart_time, const char *request_value,
	const char *request_id, const char *client_info,
	const char *sid_string, GUID sequence_guid, uint32_t status)
{
	int text_len;
	int elapsed_time;
	int response_len;
	EXT_PUSH ext_push;
	char dstring[128];
	char text_buff[256];
	char seq_string[40];
	char response_buff[4096];
	struct timeval current_time;
	
	gettimeofday(&current_time, NULL);
	elapsed_time = (current_time.tv_sec - pstart_time->tv_sec)
		*1000 + (current_time.tv_usec - pstart_time->tv_usec)/1000;
	rfc1123_dstring(pstart_time->tv_sec, dstring);
	text_len = sprintf(text_buff,
		"PROCESSING\r\nDONE\r\n"
		"X-ElapsedTime: %d\r\n"
		"X-StartTime: %s\r\n\r\n",
		elapsed_time, dstring);
	ext_buffer_push_init(&ext_push, text_buff + text_len, 8, 0);
	ext_buffer_push_uint32(&ext_push, status);
	ext_buffer_push_uint32(&ext_push, 0);
	text_len += 8;
	rfc1123_dstring(current_time.tv_sec, dstring);
	guid_to_string(&sequence_guid, seq_string, 40);
	response_len = snprintf(response_buff,
		sizeof(response_buff),
		"HTTP/1.1 200 OK\r\n"
		"Server: %s\r\n"
		"Cache-Control: private\r\n"
		"Content-Type: application/mapi-http\r\n"
		"X-RequestType: %s\r\n"
		"X-RequestId: %s\r\n"
		"X-ClientInfo: %s\r\n"
		"X-ResponseCode: 0\r\n"
		"X-PendingPeriod: %d\r\n"
		"X-ExpirationInfo: %d\r\n"
		"Content-Length: %d\r\n"
		"X-ServerApplication: Exchange/15.00.0847.4040\r\n"
		"Set-Cookie: sid=%s\r\n"
		"Set-Cookie: sequence=%s\r\n"
		"Date: %s\r\n\r\n%s",
		get_host_ID(), request_value,
		request_id, client_info,
		(int)RESPONSE_PENDING_PERIOD*1000,
		(int)SESSION_VALID_INTERVAL*1000, text_len,
		sid_string, seq_string, dstring, text_buff);
	return write_response(context_id, response_buff, response_len);
}

static BOOL normal_response(int context_id,
	const char *request_value, struct timeval *pstart_time,
	const char *request_id, const char *client_info, 
	const char *sid_string, GUID sequence_guid,
	const void *pcontent, int content_length)
{
	int tmp_len;
	int text_len;
	int elapsed_time;
	int response_len;
	char dstring[128];
	char text_buff[256];
	char seq_string[40];
	char chunk_string[32];
	char response_buff[4096];
	struct timeval current_time;
	
	gettimeofday(&current_time, NULL);
	rfc1123_dstring(current_time.tv_sec, dstring);
	guid_to_string(&sequence_guid, seq_string, 40);
	response_len = snprintf(response_buff,
		sizeof(response_buff),
		"HTTP/1.1 200 OK\r\n"
		"Server: %s\r\n"
		"Cache-Control: private\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Content-Type: application/mapi-http\r\n"
		"X-RequestType: %s\r\n"
		"X-RequestId: %s\r\n"
		"X-ClientInfo: %s\r\n"
		"X-ResponseCode: 0\r\n"
		"X-PendingPeriod: %d\r\n"
		"X-ExpirationInfo: %d\r\n"
		"X-ServerApplication: Exchange/15.00.0847.4040\r\n"
		"Set-Cookie: sid=%s\r\n"
		"Set-Cookie: sequence=%s\r\n"
		"Date: %s\r\n\r\n",
		get_host_ID(), request_value,
		request_id, client_info,
		(int)RESPONSE_PENDING_PERIOD*1000,
		(int)SESSION_VALID_INTERVAL*1000,
		sid_string, seq_string, dstring);
	if (FALSE == write_response(context_id,
		response_buff, response_len)) {
		return FALSE;
	}
	elapsed_time = (current_time.tv_sec - pstart_time->tv_sec)
		*1000 + (current_time.tv_usec - pstart_time->tv_usec)/1000;
	rfc1123_dstring(pstart_time->tv_sec, dstring);
	text_len = sprintf(text_buff,
		"PROCESSING\r\nDONE\r\n"
		"X-ElapsedTime: %d\r\n"
		"X-StartTime: %s\r\n\r\n",
		elapsed_time, dstring);
	tmp_len = sprintf(chunk_string, "%x\r\n", text_len);
	if (FALSE == write_response(context_id, chunk_string, tmp_len) ||
		FALSE == write_response(context_id, text_buff, text_len) ||
		FALSE == write_response(context_id, "\r\n", 2)) {
		return FALSE;	
	}
	tmp_len = sprintf(chunk_string, "%x\r\n", content_length);
	if (FALSE == write_response(context_id, chunk_string, tmp_len) ||
		FALSE == write_response(context_id, pcontent, content_length) ||
		FALSE == write_response(context_id, "\r\n0\r\n\r\n", 7)) {
		return FALSE;
	}
	return TRUE;
}

static uint32_t getaddressbookurl(GUID session_guid,
	const char *username, uint32_t flags, const char *puser_dn,
	uint32_t cb_auxin, const uint8_t *pauxin, char *server_url)
{
	int user_id;
	char *ptoken;
	char username1[256];
	char hex_string[32];
	
	get_id_from_username(username, &user_id);
	memset(username1, 0, sizeof(username1));
	strcpy(username1, username);
	ptoken = strchr(username1, '@');
	HX_strlower(username1);
	if (NULL != ptoken) {
		ptoken ++;
	} else {
		ptoken = username1;
	}
	encode_hex_int(user_id, hex_string);
	sprintf(server_url, "https://%s/mapi/nspi/?MailboxId="
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%s@%s",
		get_host_ID(), username1[0], username1[1], username1[2], username1[3],
		username1[4], username1[5], username1[6], username1[7], username1[8],
		username1[9], username1[10], username1[11], hex_string, ptoken);
	return ecSuccess;
}

static uint32_t getmailboxurl(GUID session_guid,
	const char *username, uint32_t flags,
	const char *pserver_dn, uint32_t cb_auxin,
	const uint8_t *pauxin, char *server_url)
{
	char *ptoken;
	char tmp_buff[1024];
	
	strncpy(tmp_buff, pserver_dn, sizeof(tmp_buff));
	ptoken = strrchr(tmp_buff, '/');
	if (NULL == ptoken || 0 != strncasecmp(ptoken, "/cn=", 4)) {
		return getaddressbookurl(session_guid, username,
			flags, NULL, cb_auxin, pauxin, server_url);
	}
	*ptoken = '\0';
	ptoken = strrchr(tmp_buff, '/');
	if (NULL == ptoken || 0 != strncasecmp(ptoken, "/cn=", 4)) {
		return getaddressbookurl(session_guid, username,
			flags, NULL, cb_auxin, pauxin, server_url);
	}
	sprintf(server_url, "https://%s/mapi/emsmdb/?MailboxId=%s",
									get_host_ID(), ptoken + 4);
	return ecSuccess;
}

static void produce_session(const char *tag, char *session)
{
	char *pitem;
	time_t cur_time;
	int i, pos, mod;
	char temp_time[16];
	char temp_name[16];
	
	time(&cur_time);
	/* fill 'g' if length is too short */
	sprintf(temp_time, "%x", cur_time);
	if (strlen(tag) >= 16) {
		memcpy(temp_name, tag, 16);
	} else {
		memset(temp_name, '0', 16);
		memcpy(temp_name, tag, strlen(tag));
	}
	for (i=0; i<16; i++) {
		if (0 == isalpha(temp_name[i]) && 0 == isdigit(temp_name[i])) {
			temp_name[i] = '0' + rand()%10;
		} else {
			temp_name[i] = tolower(temp_name[i]);
		}
	}
	for (i=0; i<32; i++) {
		mod = i%4;
		pos = i/4;
		if (0 == mod || 1 == mod) {
			session[i] = temp_name[pos*2 + mod];
		} else if (2 == mod) {
			session[i] = 'a' + rand()%26;
		} else {
			session[i] = temp_time[pos];
		}
	}
	session[32] = '\0';
}

static BOOL nsp_proc(int context_id, const void *pcontent, uint64_t length)
{
	int tmp_len;
	int *pcount;
	int tmp_count;
	char dstring[128];
	EXT_PULL ext_pull;
	EXT_PUSH ext_push;
	GUID session_guid;
	GUID sequence_guid;
	const char *pstring;
	char tmp_buff[1024];
	NSP_REQUEST request;
	char request_id[256];
	char client_info[256];
	NSP_RESPONSE response;
	char request_value[32];
	HTTP_REQUEST *prequest;
	SESSION_DATA *psession;
	char session_string[64];
	SESSION_DATA tmp_session;
	HTTP_AUTH_INFO auth_info;
	struct timeval start_time;
	
	request_id[0] = '\0';
	client_info[0] = '\0';
	request_value[0] = '\0';
	gettimeofday(&start_time, NULL);
	auth_info = get_auth_info(context_id);
	if (FALSE == auth_info.b_authed) {
		rfc1123_dstring(start_time.tv_sec, dstring);
		tmp_len = snprintf(tmp_buff, sizeof(tmp_buff),
			"HTTP/1.1 401 Unauthorized\r\n"
			"Date: %s\r\n"
			"Server: %s\r\n"
			"Content-Length: 0\r\n"
			"Connection: Keep-Alive\r\n"
			"WWW-Authenticate: Basic realm=\"msrpc realm\"\r\n"
			"\r\n", dstring, get_host_ID());
		return write_response(context_id, tmp_buff, tmp_len);
	}
	prequest = get_request(context_id);
	while (MEM_END_OF_FILE != mem_file_read(
		&prequest->f_others, &tmp_len, sizeof(int))) {
		if (11 == tmp_len) {
			mem_file_read(&prequest->f_others, tmp_buff, 11);
			if (0 == strncasecmp(tmp_buff, "X-RequestId", 11)) {
				mem_file_read(&prequest->f_others, &tmp_len, sizeof(int));
				if (tmp_len >= sizeof(request_id)) {
					 return FALSE;
				}
				mem_file_read(&prequest->f_others, request_id, tmp_len);
				request_id[tmp_len] = '\0';
				continue;
			}
		} else if (12 == tmp_len) {
			mem_file_read(&prequest->f_others, tmp_buff, 12);
			if (0 == strncasecmp(tmp_buff, "X-ClientInfo", 12)) {
				mem_file_read(&prequest->f_others, &tmp_len, sizeof(int));
				if (tmp_len >= sizeof(client_info)) {
					 return FALSE;
				}
				mem_file_read(&prequest->f_others, client_info, tmp_len);
				client_info[tmp_len] = '\0';
				continue;
			}
		} else if (13 == tmp_len) {
			mem_file_read(&prequest->f_others, tmp_buff, 13);
			if (0 == strncasecmp(tmp_buff, "X-RequestType", 13)) {
				mem_file_read(&prequest->f_others, &tmp_len, sizeof(int));
				if (tmp_len >= sizeof(request_value)) {
					 return FALSE;
				}
				mem_file_read(&prequest->f_others, request_value, tmp_len);
				request_value[tmp_len] = '\0';
				continue;
			}
		} else {
			mem_file_seek(&prequest->f_others, MEM_FILE_READ_PTR,
									tmp_len, MEM_FILE_SEEK_CUR);
		}
		mem_file_read(&prequest->f_others, &tmp_len, sizeof(int));
		mem_file_seek(&prequest->f_others, MEM_FILE_READ_PTR,
								tmp_len, MEM_FILE_SEEK_CUR);
	}
	if ('\0' == request_value[0]) {
		return error_responsecode(context_id,
			&start_time, RESPONSE_CODE_INVALID_VERB);
	}
	if ('\0' == request_id[0] || '\0' == client_info[0]) {
		return error_responsecode(context_id,
			&start_time, RESPONSE_CODE_MISSING_HEADER);
	}
	tmp_len = mem_file_read(&prequest->f_cookie,
				tmp_buff, sizeof(tmp_buff) - 1);
	if (MEM_END_OF_FILE != tmp_len) {
		tmp_buff[tmp_len] = '\0';
		auto pparser = cookie_parser_init(tmp_buff);
		pstring = cookie_parser_get(pparser, "sid");
		if (NULL == pstring || strlen(pstring) >= sizeof(session_string)) {
			return error_responsecode(context_id, &start_time,
						RESPONSE_CODE_INVALID_CONTEXT_COOKIE);
		}
		strcpy(session_string, pstring);
		if (0 != strcasecmp(request_value, "PING") &&
			0 != strcasecmp(request_value, "Unbind")) {
			pstring = cookie_parser_get(pparser, "sequence");
			if (NULL == pstring || FALSE == guid_from_string(
				&sequence_guid, pstring)) {
				return error_responsecode(context_id, &start_time,
							RESPONSE_CODE_INVALID_CONTEXT_COOKIE);
			}
		}
		pthread_mutex_lock(&g_hash_lock);
		psession = static_cast<SESSION_DATA *>(str_hash_query(g_session_hash, session_string));
		if (NULL == psession) {
			pthread_mutex_unlock(&g_hash_lock);
			return error_responsecode(context_id, &start_time,
						RESPONSE_CODE_INVALID_CONTEXT_COOKIE);
		}
		if (psession->expire_time < start_time.tv_sec) {
			str_hash_remove(g_session_hash, session_string);
			pcount = static_cast<int *>(str_hash_query(g_user_hash, psession->username));
			if (NULL != pcount) {
				(*pcount) --;
				if (0 == *pcount) {
					str_hash_remove(g_user_hash, psession->username);
				}
			}
			pthread_mutex_unlock(&g_hash_lock);
			return error_responsecode(context_id, &start_time,
						RESPONSE_CODE_INVALID_CONTEXT_COOKIE);
		}
		if (0 != strcasecmp(psession->username, auth_info.username)) {
			pthread_mutex_unlock(&g_hash_lock);
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_NO_PRIVILEGE);
		}
		session_guid = psession->session_guid;
		if (0 != strcasecmp(request_value, "PING") &&
			0 != strcasecmp(request_value, "Bind") &&
			0 != strcasecmp(request_value, "Unbind")) {
			if (0 != guid_compare(&sequence_guid,
				&psession->sequence_guid)) {
				pthread_mutex_unlock(&g_hash_lock);
				return error_responsecode(context_id,
					&start_time, RESPONSE_CODE_INVALID_SEQUENCE);
			}
		}
		if (0 != strcasecmp(request_value, "PING") &&
			0 != strcasecmp(request_value, "Unbind")) {
			sequence_guid = guid_random_new();
			psession->sequence_guid = sequence_guid;
		}
		psession->expire_time = start_time.tv_sec
					+ SESSION_VALID_INTERVAL + 60;
		pthread_mutex_unlock(&g_hash_lock);
	} else {
		if (0 != strcasecmp(request_value, "Bind")) {
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_MISSING_COOKIE);
		}
		psession = NULL;
	}
	if (0 == strcasecmp(request_value, "PING")) {
		nsp_bridge_touch_handle(session_guid);
		return ping_response(context_id, &start_time,
			request_id, client_info, session_string);
	}
	/* build environment for proc_plugin
	   service functions, e.g. get_rpc_info */
	set_context(context_id);
	rpc_new_environment();
	ext_buffer_pull_init(&ext_pull, pcontent, length,
		common_util_alloc, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT);
	if (0 == strcasecmp(request_value, "Bind")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_bind_request(
			&ext_pull, &request.bind)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.bind.status = 0;
		response.bind.result = nsp_bridge_bind(
			request.bind.flags, request.bind.pstat,
			request.bind.cb_auxin, request.bind.pauxin,
			&session_guid, &response.bind.server_guid);
		if (response.bind.result == ecSuccess) {
			if (NULL != psession) {
				/* reconnecting and establishing of a new session */
				pthread_mutex_lock(&g_hash_lock);
				psession = static_cast<SESSION_DATA *>(str_hash_query(g_session_hash, session_string));
				if (NULL != psession) {
					nsp_bridge_unbind(psession->session_guid, 0, 0, 0);
					psession->session_guid = session_guid;
				}
				pthread_mutex_unlock(&g_hash_lock);
			} else {
				produce_session(auth_info.username, session_string);
				tmp_session.session_guid = session_guid;
				sequence_guid = guid_random_new();
				tmp_session.sequence_guid = sequence_guid;
				strcpy(tmp_session.username, auth_info.username);
				HX_strlower(tmp_session.username);
				time(&tmp_session.expire_time);
				tmp_session.expire_time += SESSION_VALID_INTERVAL + 60;
				pthread_mutex_lock(&g_hash_lock);
				if (1 != str_hash_add(g_session_hash,
					session_string, &tmp_session)) {
					pthread_mutex_unlock(&g_hash_lock);
					nsp_bridge_unbind(session_guid, 0, 0, NULL);
					rpc_free_environment();
					return failure_response(context_id, &start_time,
						request_value, request_id, client_info,
					       session_string, sequence_guid, ecInsufficientResrc);
				}
				pcount = static_cast<int *>(str_hash_query(g_user_hash, tmp_session.username));
				if (NULL == pcount) {
					tmp_count = 1;
					str_hash_add(g_user_hash, tmp_session.username, &tmp_count);
				} else {
					(*pcount) ++;
				}
				pthread_mutex_unlock(&g_hash_lock);
			}
		}
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_bind_response(
			&ext_push, &response.bind)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "Unbind")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_unbind_request(
			&ext_pull, &request.unbind)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.unbind.status = 0;
		response.unbind.result = nsp_bridge_unbind(
			session_guid, request.unbind.reserved,
			request.unbind.cb_auxin, request.unbind.pauxin);
		pthread_mutex_lock(&g_hash_lock);
		psession = static_cast<SESSION_DATA *>(str_hash_query(g_session_hash, session_string));
		if (NULL != psession) {
			pcount = static_cast<int *>(str_hash_query(g_user_hash, psession->username));
			if (NULL != pcount) {
				(*pcount) --;
				if (0 == *pcount) {
					str_hash_remove(g_user_hash, psession->username);
				}
			}
			str_hash_remove(g_session_hash, session_string);
		}
		pthread_mutex_unlock(&g_hash_lock);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_unbind_response(
			&ext_push, &response.unbind)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "CompareMIds")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_comparemids_request(
			&ext_pull, &request.comparemids)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.comparemids.status = 0;
		response.comparemids.result =
			nsp_bridge_comparemids(session_guid,
					request.comparemids.reserved,
					request.comparemids.pstat,
					request.comparemids.mid1,
					request.comparemids.mid2,
					request.comparemids.cb_auxin,
					request.comparemids.pauxin,
					&response.comparemids.result1);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_comparemids_response(
			&ext_push, &response.comparemids)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "DNToMId")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_dntomid_request(
			&ext_pull, &request.dntomid)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.dntomid.status = 0;
		response.dntomid.result =
			nsp_bridge_dntomid(session_guid,
					request.dntomid.reserved,
					request.dntomid.pnames,
					request.dntomid.cb_auxin,
					request.dntomid.pauxin,
					&response.dntomid.poutmids);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_dntomid_response(
			&ext_push, &response.dntomid)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "GetMatches")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_getmatches_request(
			&ext_pull, &request.getmatches)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.getmatches.status = 0;
		response.getmatches.result =
			nsp_bridge_getmatches(session_guid,
				request.getmatches.reserved1,
				request.getmatches.pstat,
				request.getmatches.pinmids,
				request.getmatches.reserved2,
				request.getmatches.pfilter,
				request.getmatches.ppropname,
				request.getmatches.row_count,
				request.getmatches.pcolumns,
				request.getmatches.cb_auxin,
				request.getmatches.pauxin,
				&response.getmatches.pmids,
				&response.getmatches.column_rows);
		response.getmatches.pstat = request.getmatches.pstat;
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_getmatches_response(
			&ext_push, &response.getmatches)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "GetPropList")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_getproplist_request(
			&ext_pull, &request.getproplist)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.getproplist.status = 0;
		response.getproplist.result =
			nsp_bridge_getproplist(session_guid,
					request.getproplist.flags,
					request.getproplist.mid,
					request.getproplist.codepage,
					request.getproplist.cb_auxin,
					request.getproplist.pauxin,
					&response.getproplist.pproptags);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_getproplist_response(
			&ext_push, &response.getproplist)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "GetProps")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_getprops_request(
			&ext_pull, &request.getprops)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.getprops.status = 0;
		response.getprops.result =
			nsp_bridge_getprops(session_guid,
					request.getprops.flags,
					request.getprops.pstat,
					request.getprops.pproptags,
					request.getprops.cb_auxin,
					request.getprops.pauxin,
					&response.getprops.codepage,
					&response.getprops.prow);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_getprops_response(
			&ext_push, &response.getprops)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "GetSpecialTable")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_getspecialtable_request(
			&ext_pull, &request.getspecialtable)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.getspecialtable.status = 0;
		response.getspecialtable.result =
			nsp_bridge_getspecialtable(session_guid,
						request.getspecialtable.flags,
						request.getspecialtable.pstat,
						request.getspecialtable.pversion,
						request.getspecialtable.cb_auxin,
						request.getspecialtable.pauxin,
						&response.getspecialtable.codepage,
						&response.getspecialtable.pversion,
						&response.getspecialtable.count,
						&response.getspecialtable.prow);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_getspecialtable_response(
			&ext_push, &response.getspecialtable)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "GetTemplateInfo")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_gettemplateinfo_request(
			&ext_pull, &request.gettemplateinfo)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.gettemplateinfo.status = 0;
		response.gettemplateinfo.result =
			nsp_bridge_gettemplateinfo(session_guid,
					request.gettemplateinfo.flags,
					request.gettemplateinfo.type,
					request.gettemplateinfo.pdn,
					request.gettemplateinfo.codepage,
					request.gettemplateinfo.locale_id,
					request.gettemplateinfo.cb_auxin,
					request.gettemplateinfo.pauxin,
					&response.gettemplateinfo.codepage,
					&response.gettemplateinfo.prow);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_gettemplateinfo_response(
			&ext_push, &response.gettemplateinfo)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "ModLinkAtt")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_modlinkatt_request(
			&ext_pull, &request.modlinkatt)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.modlinkatt.status = 0;
		response.modlinkatt.result =
			nsp_bridge_modlinkatt(session_guid,
						request.modlinkatt.flags,
						request.modlinkatt.proptag,
						request.modlinkatt.mid,
						&request.modlinkatt.entryids,
						request.modlinkatt.cb_auxin,
						request.modlinkatt.pauxin);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_modlinkatt_response(
			&ext_push, &response.modlinkatt)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "ModProps")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_modprops_request(
			&ext_pull, &request.modprops)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.modprops.status = 0;
		response.modprops.result =
			nsp_bridge_modprops(session_guid,
					request.modprops.reserved,
					request.modprops.pstat,
					request.modprops.pproptags,
					request.modprops.pvalues,
					request.modprops.cb_auxin,
					request.modprops.pauxin);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_modprops_response(
			&ext_push, &response.modprops)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "QueryColumns")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_querycolumns_request(
			&ext_pull, &request.querycolumns)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.querycolumns.status = 0;
		response.querycolumns.result =
			nsp_bridge_querycolumns(session_guid,
					request.querycolumns.reserved,
					request.querycolumns.flags,
					request.querycolumns.cb_auxin,
					request.querycolumns.pauxin,
					&response.querycolumns.pcolumns);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_querycolumns_response(
			&ext_push, &response.querycolumns)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "QueryRows")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_queryrows_request(
			&ext_pull, &request.queryrows)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.queryrows.status = 0;
		response.queryrows.result =
			nsp_bridge_queryrows(session_guid,
					request.queryrows.flags,
					request.queryrows.pstat,
					request.queryrows.explicit_table,
					request.queryrows.count,
					request.queryrows.pcolumns,
					request.queryrows.cb_auxin,
					request.queryrows.pauxin,
					&response.queryrows.column_rows);
		response.queryrows.pstat = request.queryrows.pstat;
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_queryrows_response(
			&ext_push, &response.queryrows)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "ResolveNames")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_resolvenames_request(
			&ext_pull, &request.resolvenames)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.resolvenames.status = 0;
		response.resolvenames.result =
			nsp_bridge_resolvenames(session_guid,
					request.resolvenames.reserved,
					request.resolvenames.pstat,
					request.resolvenames.pproptags,
					request.resolvenames.pnames,
					request.resolvenames.cb_auxin,
					request.resolvenames.pauxin,
					&response.resolvenames.codepage,
					&response.resolvenames.pmids,
					&response.resolvenames.column_rows);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_resolvenames_response(
			&ext_push, &response.resolvenames)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "ResortRestriction")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_resortrestriction_request(
			&ext_pull, &request.resortrestriction)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.resortrestriction.status = 0;
		response.resortrestriction.result =
			nsp_bridge_resortrestriction(session_guid,
					request.resortrestriction.reserved,
					request.resortrestriction.pstat,
					request.resortrestriction.pinmids,
					request.resortrestriction.cb_auxin,
					request.resortrestriction.pauxin,
					&response.resortrestriction.poutmids);
		response.resortrestriction.pstat = request.resortrestriction.pstat;
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_resortrestriction_response(
			&ext_push, &response.resortrestriction)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "SeekEntries")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_seekentries_request(
			&ext_pull, &request.seekentries)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.seekentries.status = 0;
		response.seekentries.result =
			nsp_bridge_seekentries(session_guid,
					request.seekentries.reserved,
					request.seekentries.pstat,
					request.seekentries.ptarget,
					request.seekentries.pexplicit_table,
					request.seekentries.pcolumns,
					request.seekentries.cb_auxin,
					request.seekentries.pauxin,
					&response.seekentries.column_rows);
		response.seekentries.pstat = request.seekentries.pstat;
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_seekentries_response(
			&ext_push, &response.seekentries)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "UpdateStat")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_updatestat_request(
			&ext_pull, &request.updatestat)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.updatestat.status = 0;
		response.updatestat.result =
			nsp_bridge_updatestat(session_guid,
					request.updatestat.reserved,
					request.updatestat.pstat,
					request.updatestat.delta_requested,
					request.updatestat.cb_auxin,
					request.updatestat.pauxin,
					&response.updatestat.pdelta);
		response.updatestat.pstat = request.updatestat.pstat;
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_updatestat_response(
			&ext_push, &response.updatestat)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "GetMailboxUrl")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_getmailboxurl_request(
			&ext_pull, &request.getmailboxurl)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.getmailboxurl.status = 0;
		response.getmailboxurl.result = getmailboxurl(
					session_guid, auth_info.username,
					request.getmailboxurl.flags,
					request.getmailboxurl.puser_dn,
					request.getmailboxurl.cb_auxin,
					request.getmailboxurl.pauxin,
					response.getmailboxurl.server_url);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_getmailboxurl_response(
			&ext_push, &response.getmailboxurl)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "GetAddressBookUrl")) {
		if (EXT_ERR_SUCCESS != ab_ext_pull_getaddressbookurl_request(
			&ext_pull, &request.getaddressbookurl)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.getaddressbookurl.status = 0;
		response.getaddressbookurl.result = getaddressbookurl(
							session_guid, auth_info.username,
							request.getaddressbookurl.flags,
							request.getaddressbookurl.puser_dn,
							request.getaddressbookurl.cb_auxin,
							request.getaddressbookurl.pauxin,
							response.getaddressbookurl.server_url);
		if (FALSE == ext_buffer_push_init(&ext_push,
			0, 0, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, ecMAPIOOM);
		}
		if (EXT_ERR_SUCCESS != ab_ext_push_getaddressbookurl_response(
			&ext_push, &response.getaddressbookurl)) {
			ext_buffer_push_free(&ext_push);
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
			       session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else {
		rpc_free_environment();
		return FALSE;
	}
	rpc_free_environment();
	if (FALSE == normal_response(context_id, request_value,
		&start_time, request_id, client_info, session_string,
		sequence_guid, ext_push.data, ext_push.offset)) {
		ext_buffer_push_free(&ext_push);
		return FALSE;	
	}
	ext_buffer_push_free(&ext_push);
	return TRUE;
}

static int nsp_retr(int context_id)
{
	return HPM_RETRIEVE_DONE;
}
