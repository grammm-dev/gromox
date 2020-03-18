#ifndef _H_DATA_SOURCE_
#define _H_DATA_SOURCE_
#include "common_types.h"
#include "double_list.h"
#include <time.h>


typedef struct _ROOM_ITEM {
	char roomname[256];
	time_t created_time;
} ROOM_ITEM;

typedef struct _DATA_COLLECT {
	DOUBLE_LIST list;
	DOUBLE_LIST_NODE *pnode;
} DATA_COLLECT;


DATA_COLLECT* data_source_collect_init();

void data_source_collect_free(DATA_COLLECT *pcollect);

int data_source_collect_total(DATA_COLLECT *pcollect);

void data_source_collect_begin(DATA_COLLECT *pcollect);

int data_source_collect_done(DATA_COLLECT *pcollect);

void* data_source_collect_get_value(DATA_COLLECT *pcollect);

int data_source_collect_forward(DATA_COLLECT *pcollect);


void data_source_init();

int data_source_run();

int data_source_stop();

void data_source_free();

BOOL data_source_add_room(const char *domainname, const char *roomname);

BOOL data_source_del_room(const char *domainname, const char *roomname);

BOOL data_source_list_rooms(const char *domainname, DATA_COLLECT *pcollect);

BOOL data_source_list_permissions(const char *domainname,
	const char *roomname, DATA_COLLECT *pcollect);

BOOL data_source_add_permission(const char *domainname,
	const char *roomname, const char *address);

BOOL data_source_del_permission(const char *domainname,
	const char *roomname, const char *address);


#endif
