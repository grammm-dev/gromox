#ifndef _H_HPM_PROCESSOR_
#define _H_HPM_PROCESSOR_
#include "plugin.h"
#include "double_list.h"
#include "common_types.h"

#define HPM_RETRIEVE_ERROR					0
#define HPM_RETRIEVE_WRITE					1
#define HPM_RETRIEVE_NONE					2
#define HPM_RETRIEVE_WAIT					3
#define HPM_RETRIEVE_DONE					4
#define HPM_RETRIEVE_SCOKET					5

struct _HTTP_CONTEXT;

typedef struct _HPM_INTERFACE {
	BOOL (*preproc)(int);
	BOOL (*proc)(int, const void*, uint64_t);
	int (*retr)(int);
	BOOL (*send)(int, const void*, int);
	int (*receive)(int, void*, int length);
	void (*term)(int);
} HPM_INTERFACE;

typedef struct _HPM_PLUGIN {
	DOUBLE_LIST_NODE node;
	DOUBLE_LIST list_reference;
	HPM_INTERFACE interface;
	void *handle;
	PLUGIN_MAIN lib_main;
	TALK_MAIN talk_main;
	char file_name[256];
} HPM_PLUGIN;

void hpm_processor_init(int context_num, const char *plugins_path,
	uint64_t cache_size, uint64_t max_size);

int hpm_processor_run();

int hpm_processor_stop();

void hpm_processor_free();

BOOL hpm_processor_get_context(struct _HTTP_CONTEXT *phttp);

void hpm_processor_put_context(struct _HTTP_CONTEXT *phttp);

BOOL hpm_processor_check_context(struct _HTTP_CONTEXT *phttp);

BOOL hpm_processor_write_request(struct _HTTP_CONTEXT *phttp);

BOOL hpm_processor_check_end_of_request(struct _HTTP_CONTEXT *phttp);

BOOL hpm_processor_proc(struct _HTTP_CONTEXT *phttp);

int hpm_processor_retrieve_response(struct _HTTP_CONTEXT *phttp);

BOOL hpm_processor_send(struct _HTTP_CONTEXT *phttp,
	const void *pbuff, int length);

int hpm_processor_receive(struct _HTTP_CONTEXT *phttp,
	char *pbuff, int length);

void hpm_processor_enum_plugins(ENUM_PLUGINS enum_func);

#endif /* _H_HPM_PROCESSOR_ */
