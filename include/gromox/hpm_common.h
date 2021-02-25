#pragma once
#include <cstdint>
#include <typeinfo>
#include <gromox/mem_file.hpp>
#include <gromox/common_types.hpp>
#include <openssl/ssl.h>
#include <gromox/defs.h>
#define PLUGIN_INIT                 0
#define PLUGIN_FREE                 1

#define NDR_STACK_IN				0
#define NDR_STACK_OUT				1

#define HPM_RETRIEVE_ERROR			0
#define HPM_RETRIEVE_WRITE			1
#define HPM_RETRIEVE_NONE			2
#define HPM_RETRIEVE_WAIT			3
#define HPM_RETRIEVE_DONE			4
#define HPM_RETRIEVE_SOCKET			5

struct HPM_INTERFACE {
	BOOL (*preproc)(int);
	BOOL (*proc)(int, const void*, uint64_t);
	int (*retr)(int);
	BOOL (*send)(int, const void*, int);
	int (*receive)(int, void*, int length);
	void (*term)(int);
};

struct CONNECTION {
	char client_ip[32];
	int				client_port;
	char server_ip[32];
	int				server_port;
	int				sockd;
	SSL				*ssl;
	struct timeval	last_timestamp;
};

struct HTTP_REQUEST {
	char		method[32];
	MEM_FILE	f_request_uri;
	char		version[8];
	MEM_FILE	f_host;
	MEM_FILE    f_user_agent;
    MEM_FILE    f_accept;
	MEM_FILE	f_accept_language;
	MEM_FILE	f_accept_encoding;
	MEM_FILE	f_content_type;
	MEM_FILE	f_content_length;
	MEM_FILE	f_transfer_encoding;
	MEM_FILE	f_cookie;
    MEM_FILE    f_others;
};

struct HTTP_AUTH_INFO {
	BOOL b_authed;
	const char* username;
	const char* password;
	const char* maildir;
	const char* lang;
};

typedef void (*TALK_MAIN)(int, char**, char*, int);

#define DECLARE_API(x) \
	x void *(*query_serviceF)(const char *, const std::type_info &); \
	x BOOL (*register_interface)(HPM_INTERFACE *); \
	x BOOL (*register_talk)(TALK_MAIN); \
	x CONNECTION *(*get_connection)(int); \
	x HTTP_REQUEST *(*get_request)(int); \
	x HTTP_AUTH_INFO (*get_auth_info)(int); \
	x BOOL (*write_response)(int, const void *, int); \
	x void (*wakeup_context)(int); \
	x void (*activate_context)(int); \
	x void (*log_info)(int, const char *, ...); \
	x const char *(*get_host_ID)(); \
	x const char *(*get_default_domain)(); \
	x const char *(*get_plugin_name)(); \
	x const char *(*get_config_path)(); \
	x const char *(*get_data_path)(); \
	x const char *(*get_state_path)(); \
	x int (*get_context_num)(); \
	x void (*set_context)(int); \
	x void (*set_ep_info)(int, const char *, int); \
	x void *(*ndr_stack_alloc)(int, size_t); \
	x BOOL (*rpc_new_environment)(); \
	x void (*rpc_free_environment)();
#define query_service2(n, f) ((f) = reinterpret_cast<decltype(f)>(query_serviceF((n), typeid(*(f)))))
#define query_service1(n) query_service2(#n, n)
#ifdef DECLARE_API_STATIC
DECLARE_API(static);
#else
DECLARE_API(extern);
#endif
	
#define LINK_API(param) \
	query_serviceF = reinterpret_cast<decltype(query_serviceF)>(param[0]); \
	query_service1(register_interface); \
	query_service1(register_talk); \
	query_service1(get_connection); \
	query_service1(get_request); \
	query_service1(get_auth_info); \
	query_service1(write_response); \
	query_service1(wakeup_context); \
	query_service1(activate_context); \
	query_service1(log_info); \
	query_service1(get_host_ID); \
	query_service1(get_default_domain); \
	query_service1(get_plugin_name); \
	query_service1(get_config_path); \
	query_service1(get_data_path); \
	query_service1(get_state_path); \
	query_service1(get_context_num); \
	query_service1(set_context); \
	query_service1(set_ep_info); \
	query_service1(ndr_stack_alloc); \
	query_service1(rpc_new_environment); \
	query_service1(rpc_free_environment);
#define HPM_ENTRY(s) BOOL HPM_LibMain(int r, void **p) { return (s)((r), (p)); }

extern "C" { /* dlsym */
extern GX_EXPORT BOOL HPM_LibMain(int reason, void **ptrs);
}
