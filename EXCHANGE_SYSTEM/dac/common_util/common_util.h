#ifndef _H_COMMON_UTIL_
#define _H_COMMON_UTIL_
#include "mapi_types.h"
#include "dac_types.h"
#include <pthread.h>
#include <sqlite3.h>
#include <time.h>

#define DAC_VERSION									"directory access center 1.0"

#define SOCKET_TIMEOUT								60

#define CALL_ID_CONNECT								0x00
#define CALL_ID_INFOSTORAGE							0x01
#define CALL_ID_ALLOCDIR							0x02
#define CALL_ID_FREEDIR								0x03
#define CALL_ID_SETPROPVALS							0x04
#define CALL_ID_GETPROPVALS							0x05
#define CALL_ID_DELETEPROPVALS						0x06
#define CALL_ID_OPENTABLE							0x07
#define CALL_ID_OPENCELLTABLE						0x08
#define CALL_ID_RESTRICTTABLE						0x09
#define CALL_ID_UPDATECELLS							0x0a
#define CALL_ID_SUMTABLE							0x0b
#define CALL_ID_QUERYROWS							0x0c
#define CALL_ID_INSERTROWS							0x0d
#define CALL_ID_DELETEROW							0x0e
#define CALL_ID_DELETECELLS							0x0f
#define CALL_ID_DELETETABLE							0x10
#define CALL_ID_CLOSETABLE							0x11
#define CALL_ID_MATCHROW							0x12
#define CALL_ID_GETNAMESPACES						0x13
#define CALL_ID_GETPROPNAMES						0x14
#define CALL_ID_GETTABLENAMES						0x15
#define CALL_ID_READFILE							0x16
#define CALL_ID_APPENDFILE							0x17
#define CALL_ID_REMOVEFILE							0x18
#define CALL_ID_STATFILE							0x19

#define CALL_ID_CHECKROW							0x30
#define CALL_ID_PHPRPC								0x31


#define RESPONSE_CODE_SUCCESS						0x00
#define RESPONSE_CODE_ACCESS_DENY					0x01
#define RESPONSE_CODE_MAX_REACHED					0x02
#define RESPONSE_CODE_LACK_MEMORY					0x03
#define RESPONSE_CODE_CONNECT_UNCOMPLETE			0x04
#define RESPONSE_CODE_PULL_ERROR					0x05
#define RESPONSE_CODE_DISPATCH_ERROR				0x06
#define RESPONSE_CODE_PUSH_ERROR					0x07


typedef struct _DIR_ITEM {
	int reference;
	sqlite3 *psqlite;
	time_t last_time;
	uint32_t last_id;
	pthread_mutex_t lock;
	DOUBLE_LIST table_list;
} DIR_ITEM;

typedef struct _TABLE_NODE {
	DOUBLE_LIST_NODE node;
	time_t load_time;
	uint64_t object_id;
	uint32_t table_id;
	uint64_t *pinstances;
	int count;
} TABLE_NODE;

void common_util_init(int table_size, int valid_interval,
	BOOL b_async, BOOL b_wal, const char *sql_path);

int common_util_run();

int common_util_stop();

void common_util_free();

BOOL common_util_build_environment();

void common_util_free_environment();

void* common_util_alloc(size_t size);

char* common_util_dup(const char *pstr);

BOOL common_util_load_file(const char *path, BINARY *pbin);

uint32_t common_util_get_sequence_id();

DIR_ITEM* common_util_get_dir_item(const char *dir);

void common_util_put_dir_item(DIR_ITEM *pditem);

BOOL common_util_evaluate_restriction(sqlite3 *psqlite,
	sqlite3_stmt *pstmt, uint64_t row_id, const DAC_RES *pres);

BOOL common_util_bind_value(sqlite3_stmt *pstmt,
	int idx, uint16_t type, const void *pvalue);

void* common_util_column_value(sqlite3_stmt *pstmt,
	int idx, uint16_t type);

BOOL common_util_delete_table(sqlite3 *psqlite, uint64_t object_id);

BOOL common_util_delete_row(sqlite3 *psqlite, uint64_t row_id);

DAC_PROPVAL* common_util_get_propvals(
	const DAC_VARRAY *parray, const char *pname);

#endif /* _H_COMMON_UTIL_ */
