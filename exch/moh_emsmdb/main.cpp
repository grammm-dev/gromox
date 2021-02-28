#include <libHX/string.h>
#include <gromox/cookie_parser.hpp>
#include "emsmdb_bridge.h"
#include <gromox/cookie_parser.hpp>
#include <gromox/double_list.hpp>
#include <gromox/hpm_common.h>
#include <gromox/str_hash.hpp>
#include "mb_ext.h"
#include <gromox/guid.hpp>
#include <gromox/util.hpp>
#include <sys/time.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

DECLARE_API();

using namespace gromox;

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

#define SESSION_VALID_INTERVAL					900

#define RESPONSE_PENDING_PERIOD					30

#define DISPATCH_PENDING                        2

#define FLAG_NOTIFICATION_PENDING				0x00000001

#define PENDING_STATUS_NONE						0
#define PENDING_STATUS_WAITING					1
#define PENDING_STATUS_KEEPALIVE				2

#define NOTIFICATION_STATUS_NONE				0
#define NOTIFICATION_STATUS_TIMED				1
#define NOTIFICATION_STATUS_PENDING				2

struct SESSION_DATA {
	GUID session_guid;
	GUID sequence_guid;
	char username[256];
	time_t expire_time;
};

struct NOTIFICATION_CONTEXT {
	DOUBLE_LIST_NODE node;
	uint8_t pending_status;
	uint8_t notification_status;
	GUID session_guid;
	time_t pending_time; /* time for connection pending */
	struct timeval start_time;
};

struct ECDOASYNCWAITEX_IN {
	EMSMDB_HANDLE acxh;
	uint32_t flags_in;
};

struct ECDOASYNCWAITEX_OUT {
	uint32_t flags_out; /* record context_id in the variable
							for asyncemsmdb_wakeup_proc */
	int32_t result;
};

static BOOL emsmdb_preproc(int context_id);

static BOOL emsmdb_proc(int context_id,
	const void *pcontent, uint64_t length);

static int emsmdb_retr(int context_id);

static void emsmdb_term(int context_id);

static void* scan_work_func(void *pparam);

static void asyncemsmdb_wakeup_proc(int context_id, BOOL b_pending);
static int (*asyncemsmdb_interface_async_wait)(uint32_t async_id, ECDOASYNCWAITEX_IN *, ECDOASYNCWAITEX_OUT *);
static void (*asyncemsmdb_interface_register_active)(void *);
static void (*asyncemsmdb_interface_remove)(EMSMDB_HANDLE *);

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
static DOUBLE_LIST g_pending_list;
static pthread_mutex_t g_list_lock;
static pthread_mutex_t g_hash_lock;
static STR_HASH_TABLE *g_user_hash;
static STR_HASH_TABLE *g_session_hash;
static NOTIFICATION_CONTEXT *g_status_array;

BOOL HPM_LibMain(int reason, void **ppdata)
{
	int context_num;
	HPM_INTERFACE interface;
	
	switch (reason) {
    case PLUGIN_INIT:
		LINK_API(ppdata);
		pthread_mutex_init(&g_hash_lock, NULL);
		pthread_mutex_init(&g_list_lock, NULL);
		double_list_init(&g_pending_list);
		g_notify_stop = TRUE;
		context_num = get_context_num();
		g_status_array = static_cast<NOTIFICATION_CONTEXT *>(malloc(sizeof(NOTIFICATION_CONTEXT) * context_num));
		if (NULL == g_status_array) {
			printf("[moh_emsmdb]: fail to allocate status array\n");
			return FALSE;
		}
		g_session_hash = str_hash_init(
			context_num*AVERAGE_SESSION_PER_CONTEXT,
			sizeof(SESSION_DATA), NULL);
		if (NULL == g_session_hash) {
			printf("[moh_emsmdb]: fail to init session hash table\n");
			return FALSE;
		}
		g_user_hash = str_hash_init(
			context_num*AVERAGE_SESSION_PER_CONTEXT,
			sizeof(int), NULL);
		if (NULL == g_user_hash) {
			printf("[moh_emsmdb]: fail to init user hash table\n");
			return FALSE;
		}
		if (!query_service1(emsmdb_interface_connect_ex) ||
		    !query_service1(emsmdb_interface_rpc_ext2) ||
		    !query_service1(emsmdb_interface_disconnect) ||
		    !query_service1(emsmdb_interface_touch_handle) ||
		    !query_service1(asyncemsmdb_interface_register_active) ||
		    !query_service1(asyncemsmdb_interface_remove)) {
			printf("[moh_emsmdb]: exchange_emsmdb not loaded\n");
			return false;
		}
		g_notify_stop = FALSE;
		if (0 != pthread_create(&g_scan_id, NULL, scan_work_func, NULL)) {
			g_notify_stop = TRUE;
			printf("[moh_emsmdb]: fail create scanning thread\n");
			return FALSE;
		}
		interface.preproc = emsmdb_preproc;
		interface.proc = emsmdb_proc;
		interface.retr = emsmdb_retr;
		interface.send = NULL;
		interface.receive = NULL;
		interface.term = emsmdb_term;
		if (FALSE == register_interface(&interface)) {
			return FALSE;
		}
		asyncemsmdb_interface_register_active(reinterpret_cast<void *>(asyncemsmdb_wakeup_proc));
		printf("[moh_emsmdb]: plugin is loaded into system\n");
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
		if (NULL != g_status_array) {
			free(g_status_array);
			g_status_array = NULL;
		}
		double_list_free(&g_pending_list);
		pthread_mutex_destroy(&g_hash_lock);
		pthread_mutex_destroy(&g_list_lock);
		return TRUE;
	}
}

static void* scan_work_func(void *pparam)
{
	int *pcount;
	time_t cur_time;
	STR_HASH_ITER *iter;
	SESSION_DATA *psession;
	DOUBLE_LIST_NODE *pnode;
	NOTIFICATION_CONTEXT *pcontext;
	
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
		pthread_mutex_lock(&g_list_lock);
		for (pnode=double_list_get_head(&g_pending_list); NULL!=pnode;
			pnode=double_list_get_after(&g_pending_list, pnode)) {
			pcontext = (NOTIFICATION_CONTEXT*)pnode->pdata;
			if (cur_time - pcontext->pending_time
				>= RESPONSE_PENDING_PERIOD - 3) {
				pcontext->pending_time = cur_time;
				pcontext->pending_status = PENDING_STATUS_KEEPALIVE;
				wakeup_context(pcontext - g_status_array);
			}
		}
		pthread_mutex_unlock(&g_list_lock);
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

static BOOL emsmdb_preproc(int context_id)
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
	if (0 != strncasecmp(tmp_uri, "/mapi/emsmdb/?MailboxId=", 22)) {
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
		get_host_ID(), request_value, request_id,
		client_info, (int)RESPONSE_PENDING_PERIOD*1000,
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

static BOOL notification_response1(int context_id,
	const char *request_id, const char *client_info,
	const char *sid_string)
{
	int response_len;
	char dstring[128];
	char response_buff[4096];
	struct timeval current_time;
	
	gettimeofday(&current_time, NULL);
	rfc1123_dstring(current_time.tv_sec, dstring);
	response_len = snprintf(response_buff,
		sizeof(response_buff),
		"HTTP/1.1 200 OK\r\n"
		"Server: %s\r\n"
		"Cache-Control: private\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Content-Type: application/mapi-http\r\n"
		"X-RequestType: NotificationWait\r\n"
		"X-RequestId: %s\r\n"
		"X-ClientInfo: %s\r\n"
		"X-ResponseCode: 0\r\n"
		"X-PendingPeriod: %d\r\n"
		"X-ExpirationInfo: %d\r\n"
		"X-ServerApplication: Exchange/15.00.0847.4040\r\n"
		"Set-Cookie: sid=%s\r\n"
		"Date: %s\r\n\r\n", get_host_ID(),
		request_id, client_info,
		(int)RESPONSE_PENDING_PERIOD*1000,
		(int)SESSION_VALID_INTERVAL*1000,
		sid_string, dstring);
	if (FALSE == write_response(context_id,
		response_buff, response_len)) {
		return FALSE;
	}
	if (FALSE == write_response(context_id,
		"c\r\nPROCESSING\r\n\r\n", 17)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL notification_response2(int context_id,
	struct timeval *pstart_time, uint32_t result,
	uint32_t flags_out)
{
	int tmp_len;
	int text_len;
	int elapsed_time;
	char dstring[128];
	EXT_PUSH ext_push;
	char push_buff[32];
	char text_buff[128];
	char chunk_string[32];
	EMSMDB_RESPONSE response;
	struct timeval current_time;
	
	ext_buffer_push_init(&ext_push, push_buff, sizeof(push_buff), 0);
	response.notificationwait.status = 0;
	response.notificationwait.result = result;
	response.notificationwait.flags_out = flags_out;
	mb_ext_push_notificationwait_response(
		&ext_push, &response.notificationwait);
	gettimeofday(&current_time, NULL);
	elapsed_time = (current_time.tv_sec - pstart_time->tv_sec)
		*1000 + (current_time.tv_usec - pstart_time->tv_usec)/1000;
	rfc1123_dstring(pstart_time->tv_sec, dstring);
	text_len = sprintf(text_buff,
		"DONE\r\n"
		"X-ElapsedTime: %d\r\n"
		"X-StartTime: %s\r\n\r\n",
		elapsed_time, dstring);
	tmp_len = sprintf(chunk_string, "%x\r\n", text_len + ext_push.offset);
	if (FALSE == write_response(context_id, chunk_string, tmp_len) ||
		FALSE == write_response(context_id, text_buff, text_len) ||
		FALSE == write_response(context_id, ext_push.data, ext_push.offset)
		|| FALSE == write_response(context_id, "\r\n0\r\n\r\n", 7)) {
		return FALSE;	
	}
	return TRUE;
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

static void* temp_alloc(size_t size)
{
	return ndr_stack_alloc(NDR_STACK_IN, size);
}

static BOOL emsmdb_proc(int context_id,
	const void *pcontent, uint64_t length)
{
	int tmp_len;
	int *pcount;
	uint16_t cxr;
	int tmp_count;
	char dstring[128];
	EXT_PULL ext_pull;
	EXT_PUSH ext_push;
	GUID session_guid;
	GUID session_guid1;
	GUID sequence_guid;
	const char *pstring;
	char tmp_buff[1024];
	char request_id[256];
	char client_info[256];
	char request_value[32];
	HTTP_REQUEST *prequest;
	SESSION_DATA *psession;
	EMSMDB_REQUEST request;
	char session_string[64];
	char push_buff[0x80000];
	EMSMDB_RESPONSE response;
	SESSION_DATA tmp_session;
	HTTP_AUTH_INFO auth_info;
	struct timeval start_time;
	ECDOASYNCWAITEX_IN wait_in;
	ECDOASYNCWAITEX_OUT wait_out;
	
	memset(g_status_array + context_id,
		0, sizeof(NOTIFICATION_CONTEXT));
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
			0 != strcasecmp(request_value, "NotificationWait")) {
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
		if (0 == strcasecmp(request_value, "Execute")) {
			if (0 != guid_compare(&sequence_guid,
				&psession->sequence_guid)) {
				pthread_mutex_unlock(&g_hash_lock);
				return error_responsecode(context_id,
					&start_time, RESPONSE_CODE_INVALID_SEQUENCE);
			}
		}
		if (0 != strcasecmp(request_value, "PING") &&
			0 != strcasecmp(request_value, "Disconnect") &&
			0 != strcasecmp(request_value, "NotificationWait")) {
			sequence_guid = guid_random_new();
			psession->sequence_guid = sequence_guid;
		}
		psession->expire_time = start_time.tv_sec
					+ SESSION_VALID_INTERVAL + 60;
		pthread_mutex_unlock(&g_hash_lock);
	} else {
		if (0 != strcasecmp(request_value, "Connect")) {
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_MISSING_COOKIE);
		}
		psession = NULL;
	}
	if (0 == strcasecmp(request_value, "PING")) {
		emsmdb_bridge_touch_handle(session_guid);
		return ping_response(context_id, &start_time,
			request_id, client_info, session_string);
	}
	/* build environment for proc_plugin
	   service functions, e.g. get_rpc_info */
	set_context(context_id);
	rpc_new_environment();
	ext_buffer_pull_init(&ext_pull, pcontent, length,
		temp_alloc, EXT_FLAG_UTF16|EXT_FLAG_WCOUNT);
	if (0 == strcasecmp(request_value, "Connect")) {
		if (EXT_ERR_SUCCESS != mb_ext_pull_connect_request(
			&ext_pull, &request.connect)) {
			rpc_free_environment();
			return error_responsecode(context_id, &start_time,
						RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.connect.status = 0;
		response.connect.result = emsmdb_bridge_connect(
							request.connect.puserdn,
							request.connect.flags,
							request.connect.cpid,
							request.connect.lcid_string,
							request.connect.lcid_sort,
							request.connect.cb_auxin,
							request.connect.pauxin,
							&session_guid, &cxr,
							&response.connect.max_polls,
							&response.connect.max_retry,
							&response.connect.retry_delay,
							response.connect.pdn_prefix,
							response.connect.pdisplayname,
							&response.connect.cb_auxout, 
							response.connect.pauxout);
		if (response.connect.result == ecSuccess) {
			if (NULL != psession) {
				/* reconnecting and establishing of a new session */
				pthread_mutex_lock(&g_hash_lock);
				psession = static_cast<SESSION_DATA *>(str_hash_query(g_session_hash, session_string));
				if (NULL != psession) {
					session_guid1 = psession->session_guid;
					psession->session_guid = session_guid;
					pthread_mutex_unlock(&g_hash_lock);
					emsmdb_bridge_disconnect(session_guid1, 0, NULL);
				} else {
					pthread_mutex_unlock(&g_hash_lock);
				}
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
					emsmdb_bridge_disconnect(session_guid, 0, NULL);
					rpc_free_environment();
					return failure_response(context_id, &start_time,
						request_value, request_id, client_info,
						session_string, sequence_guid, ecInsufficientResrc);
				}
				pcount = static_cast<int *>(str_hash_query(g_user_hash, tmp_session.username));
				if (NULL == pcount) {
					tmp_count = 1;
					str_hash_add(g_user_hash,
						tmp_session.username, &tmp_count);
				} else {
					(*pcount) ++;
				}
				pthread_mutex_unlock(&g_hash_lock);
			}
		}
		ext_buffer_push_init(&ext_push, push_buff,
			sizeof(push_buff), EXT_FLAG_UTF16|EXT_FLAG_WCOUNT);
		if (EXT_ERR_SUCCESS != mb_ext_push_connect_response(
			&ext_push, &response.connect)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
				session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "Disconnect")) {
		if (EXT_ERR_SUCCESS != mb_ext_pull_disconnect_request(
			&ext_pull, &request.disconnect)) {
			rpc_free_environment();
			return error_responsecode(context_id,
				&start_time, RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.disconnect.status = 0;
		response.disconnect.result =
			emsmdb_bridge_disconnect(session_guid,
					request.disconnect.cb_auxin,
					request.disconnect.pauxin);
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
		ext_buffer_push_init(&ext_push, push_buff,
			sizeof(push_buff), EXT_FLAG_UTF16|EXT_FLAG_WCOUNT);
		if (EXT_ERR_SUCCESS != mb_ext_push_disconnect_response(
			&ext_push, &response.disconnect)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
				session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "Execute")) {
		if (EXT_ERR_SUCCESS != mb_ext_pull_execute_request(
			&ext_pull, &request.execute)) {
			rpc_free_environment();
			return error_responsecode(context_id, &start_time,
						RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		response.execute.flags = request.execute.flags;
		response.execute.cb_out = request.execute.cb_out;
		response.execute.status = 0;
		response.execute.result = 
			emsmdb_bridge_execute(session_guid,
						request.execute.cb_in,
						request.execute.pin,
						request.execute.cb_auxin,
						request.execute.pauxin,
						&response.execute.flags,
						&response.execute.cb_out,
						response.execute.pout,
						&response.execute.cb_auxout,
						response.execute.pauxout);
		ext_buffer_push_init(&ext_push, push_buff,
			sizeof(push_buff), EXT_FLAG_UTF16|EXT_FLAG_WCOUNT);
		if (EXT_ERR_SUCCESS != mb_ext_push_execute_response(
			&ext_push, &response.execute)) {
			rpc_free_environment();
			return failure_response(context_id, &start_time,
				request_value, request_id, client_info,
				session_string, sequence_guid, RPC_X_BAD_STUB_DATA);
		}
	} else if (0 == strcasecmp(request_value, "NotificationWait")) {
		if (EXT_ERR_SUCCESS != mb_ext_pull_notificationwait_request(
			&ext_pull, &request.notificationwait)) {
			rpc_free_environment();
			return error_responsecode(context_id, &start_time,
						RESPONSE_CODE_INVALID_REQUEST_BODY);
		}
		wait_in.acxh.handle_type = HANDLE_EXCHANGE_ASYNCEMSMDB;
		wait_in.acxh.guid = session_guid;
		wait_out.flags_out = context_id;
		if (FALSE == notification_response1(context_id,
			request_id, client_info, session_string)) {
			rpc_free_environment();
			return FALSE;
		}
		if (DISPATCH_PENDING == asyncemsmdb_interface_async_wait(
			0, &wait_in, &wait_out)) {
			g_status_array[context_id].pending_status =
								PENDING_STATUS_WAITING;
			g_status_array[context_id].notification_status =
									NOTIFICATION_STATUS_NONE;
			g_status_array[context_id].session_guid = session_guid;
			g_status_array[context_id].start_time = start_time;
			time(&g_status_array[context_id].pending_time);
			g_status_array[context_id].node.pdata = &g_status_array[context_id];
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_pending_list,
					&g_status_array[context_id].node);
			pthread_mutex_unlock(&g_list_lock);
			rpc_free_environment();
			return TRUE;
		}
		if (FALSE == notification_response2(context_id,
			&start_time, wait_out.result, wait_out.flags_out)) {
			rpc_free_environment();
			return FALSE;	
		}
		rpc_free_environment();
		return TRUE;
	} else {
		rpc_free_environment();
		return FALSE;
	}
	rpc_free_environment();
	return normal_response(context_id, request_value,
		&start_time, request_id, client_info, session_string,
		sequence_guid, ext_push.data, ext_push.offset);
}

static int emsmdb_retr(int context_id)
{
	char push_buff[32];
	
	switch (g_status_array[context_id].notification_status) {
	case NOTIFICATION_STATUS_TIMED:
		notification_response2(context_id,
			&g_status_array[context_id].start_time,
			ecSuccess, 0);
		g_status_array[context_id].notification_status =
								NOTIFICATION_STATUS_NONE;
		return HPM_RETRIEVE_WRITE;
	case NOTIFICATION_STATUS_PENDING:
		notification_response2(context_id,
			&g_status_array[context_id].start_time,
			ecSuccess, FLAG_NOTIFICATION_PENDING);
		g_status_array[context_id].notification_status =
								NOTIFICATION_STATUS_NONE;
		return HPM_RETRIEVE_WRITE;
	}
	switch (g_status_array[context_id].pending_status) {
	case PENDING_STATUS_NONE:
		return HPM_RETRIEVE_DONE;
	case PENDING_STATUS_KEEPALIVE:
		write_response(context_id, "7\r\nPENDING\r\n", 12);
		g_status_array[context_id].pending_status =
							PENDING_STATUS_WAITING;
		return HPM_RETRIEVE_WRITE;
	case PENDING_STATUS_WAITING:
		return HPM_RETRIEVE_WAIT;
	}
}

static void emsmdb_term(int context_id)
{
	EMSMDB_HANDLE acxh;
	
	if (PENDING_STATUS_NONE == g_status_array[context_id].pending_status) {
		return;
	}
	acxh.handle_type = 0;
	pthread_mutex_lock(&g_list_lock);
	if (PENDING_STATUS_NONE !=
		g_status_array[context_id].pending_status) {
		acxh.handle_type = HANDLE_EXCHANGE_ASYNCEMSMDB;
		acxh.guid = g_status_array[context_id].session_guid;
		double_list_remove(&g_pending_list,
			&g_status_array[context_id].node);
		g_status_array[context_id].pending_status =
								PENDING_STATUS_NONE;
	}
	pthread_mutex_unlock(&g_list_lock);
	if (HANDLE_EXCHANGE_ASYNCEMSMDB == acxh.handle_type) {
		asyncemsmdb_interface_remove(&acxh);
	}
}

static void asyncemsmdb_wakeup_proc(int context_id, BOOL b_pending)
{
	pthread_mutex_lock(&g_list_lock);
	if (PENDING_STATUS_NONE == g_status_array[context_id].pending_status) {
		pthread_mutex_unlock(&g_list_lock);
		return;
	}
	if (TRUE == b_pending) {
		g_status_array[context_id].notification_status =
							NOTIFICATION_STATUS_PENDING;
	} else {
		g_status_array[context_id].notification_status =
							NOTIFICATION_STATUS_TIMED;
	}
	double_list_remove(&g_pending_list,	&g_status_array[context_id].node);
	g_status_array[context_id].pending_status = PENDING_STATUS_NONE;
	pthread_mutex_unlock(&g_list_lock);
	wakeup_context(context_id);
}
