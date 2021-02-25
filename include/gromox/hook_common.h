#pragma once
#include <typeinfo>
#include <gromox/defs.h>
#include <gromox/common_types.hpp>
#include <gromox/mem_file.hpp>
#include <gromox/mail.hpp>
#define PLUGIN_INIT                 0
#define PLUGIN_FREE                 1
#define SYS_THREAD_CREATE           2
#define SYS_THREAD_DESTROY          3

#define BOUND_IN					1    /* message smtp in */
#define BOUND_OUT					2    /* message smtp out */
#define BOUND_RELAY					3    /* message smtp relay */
#define	BOUND_SELF					4    /* message creted by hook */

struct CONTROL_INFO {
	int         queue_ID;
	int			bound_type;
	BOOL        is_spam;
	BOOL        need_bounce;
	char from[324];
	MEM_FILE    f_rcpt_to;
};

struct MESSAGE_CONTEXT {
	CONTROL_INFO *pcontrol;
	MAIL         *pmail;
};

typedef BOOL (*HOOK_FUNCTION)(MESSAGE_CONTEXT*);
typedef void (*TALK_MAIN)(int, char**, char*, int);

#define DECLARE_API(x) \
	x void *(*query_serviceF)(const char *, const std::type_info &); \
	x BOOL (*register_hook)(HOOK_FUNCTION); \
	x BOOL (*register_local)(HOOK_FUNCTION); \
	x BOOL (*register_talk)(TALK_MAIN); \
	x void (*log_info)(int, const char *, ...); \
	x const char *(*get_host_ID)(); \
	x const char *(*get_default_domain)(); \
	x const char *(*get_admin_mailbox)(); \
	x const char *(*get_plugin_name)(); \
	x const char *(*get_config_path)(); \
	x const char *(*get_data_path)(); \
	x const char *(*get_state_path)(); \
	x const char *(*get_queue_path)(); \
	x int (*get_context_num)(); \
	x int (*get_threads_num)(); \
	x MESSAGE_CONTEXT *(*get_context)(); \
	x void (*put_context)(MESSAGE_CONTEXT *); \
	x void (*enqueue_context)(MESSAGE_CONTEXT *); \
	x BOOL (*throw_context)(MESSAGE_CONTEXT *); \
	x BOOL (*check_domain)(const char *); \
	x BOOL (*is_domainlist_valid)();
#define query_service2(n, f) ((f) = reinterpret_cast<decltype(f)>(query_serviceF((n), typeid(*(f)))))
#define query_service1(n) query_service2(#n, n)
#ifdef DECLARE_API_STATIC
DECLARE_API(static);
#else
DECLARE_API(extern);
#endif

#define LINK_API(param) \
	query_serviceF = reinterpret_cast<decltype(query_serviceF)>(param[0]); \
	query_service1(register_hook); \
	query_service1(register_local); \
	query_service1(register_talk); \
	query_service1(log_info); \
	query_service1(get_host_ID); \
	query_service1(get_default_domain); \
	query_service1(get_admin_mailbox); \
	query_service1(get_plugin_name); \
	query_service1(get_config_path); \
	query_service1(get_data_path); \
	query_service1(get_state_path); \
	query_service1(get_queue_path); \
	query_service1(get_context_num); \
	query_service1(get_threads_num); \
	query_service1(get_context); \
	query_service1(put_context); \
	query_service1(enqueue_context); \
	query_service1(throw_context); \
	query_service2("domain_list_query", check_domain); \
	query_service1(is_domainlist_valid);
#define HOOK_ENTRY(s) BOOL HOOK_LibMain(int r, void **p) { return (s)((r), (p)); }

extern "C" { /* dlsym */
extern GX_EXPORT BOOL HOOK_LibMain(int reason, void **ptrs);
}
