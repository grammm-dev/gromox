#ifndef _H_DAC_EXT_
#define _H_DAC_EXT_
#include "mapi_types.h"
#include "common_util.h"
#include <stdint.h>

typedef struct _REQ_CONNECT {
	char *remote_id;
} REQ_CONNECT;

typedef struct _REQ_ALLOCDIR {
	uint8_t type;
} REQ_ALLOCDIR;

typedef struct _REQ_SETPROPVALS {
	const char *pnamespace;
	const DAC_VARRAY *ppropvals;
} REQ_SETPROPVALS;

typedef struct _REQ_GETPROPVALS {
	const char *pnamespace;
	const STRING_ARRAY *pnames;
} REQ_GETPROPVALS;

typedef struct _REQ_DELETEPROPVALS {
	const char *pnamespace;
	const STRING_ARRAY *pnames;
} REQ_DELETEPROPVALS;

typedef struct _REQ_OPENTABLE {
	const char *pnamespace;
	const char *ptablename;
	uint32_t flags;
	const char *puniquename;
} REQ_OPENTABLE;

typedef struct _REQ_OPENCELLTABLE {
	uint64_t row_instance;
	const char *pcolname;
	uint32_t flags;
	const char *puniquename;
} REQ_OPENCELLTABLE;

typedef struct _REQ_RESTRICTTABLE {
	uint32_t table_id;
	const DAC_RES *prestrict;
} REQ_RESTRICTTABLE;

typedef struct _REQ_UPDATECELLS {
	uint64_t row_instance;
	const DAC_VARRAY *pcells;
} REQ_UPDATECELLS;

typedef struct _REQ_SUMTABLE {
	uint32_t table_id;
} REQ_SUMTABLE;

typedef struct _REQ_QUERYROWS {
	uint32_t table_id;
	uint32_t start_pos;
	uint32_t row_count;
} REQ_QUERYROWS;

typedef struct _REQ_INSERTROWS {
	uint32_t table_id;
	uint32_t flags;
	const DAC_VSET *pset;
} REQ_INSERTROWS;

typedef struct _REQ_DELETEROW {
	uint64_t row_instance;
} REQ_DELETEROW;

typedef struct _REQ_DELETECELLS {
	uint64_t row_instance;
	const STRING_ARRAY *pcolnames;
} REQ_DELETECELLS;

typedef struct _REQ_DELETETABLE {
	const char *pnamespace;
	const char *ptablename;
} REQ_DELETETABLE;

typedef struct _REQ_CLOSETABLE {
	uint32_t table_id;
} REQ_CLOSETABLE;

typedef struct _REQ_MATCHROW {
	uint32_t table_id;
	uint16_t type;
	const void *pvalue;
} REQ_MATCHROW;

typedef struct _REQ_GETPROPNAMES {
	const const char *pnamespace;
} REQ_GETPROPNAMES;

typedef struct _REQ_GETTABLENAMES {
	const char *pnamespace;
} REQ_GETTABLENAMES;

typedef struct _REQ_READFILE {
	const char *path;
	const char *file_name;
	uint64_t offset;
	uint32_t length;
} REQ_READFILE;

typedef struct _REQ_APPENDFILE {
	const char *path;
	const char *file_name;
	const BINARY *pcontent_bin;
} REQ_APPENDFILE;

typedef struct _REQ_REMOVEFILE {
	const char *path;
	const char *file_name;
} REQ_REMOVEFILE;

typedef struct _REQ_STATFILE {
	const char *path;
	const char *file_name;
} REQ_STATFILE;

typedef struct _REQ_CHECKROW {
	const char *pnamespace;
	const char *ptablename;
	uint16_t type;
	const void *pvalue;
} REQ_CHECKROW;

typedef struct _REQ_PHPRPC {
	const char *account;
	const char *script_path;
	const BINARY *pbin_in;
} REQ_PHPRPC;

typedef union _REQUEST_PAYLOAD {
	REQ_CONNECT connect;
	REQ_ALLOCDIR allocdir;
	REQ_SETPROPVALS setpropvals;
	REQ_GETPROPVALS getpropvals;
	REQ_DELETEPROPVALS deletepropvals;
	REQ_OPENTABLE opentable;
	REQ_OPENCELLTABLE opencelltable;
	REQ_RESTRICTTABLE restricttable;
	REQ_UPDATECELLS updatecells;
	REQ_SUMTABLE sumtable;
	REQ_QUERYROWS queryrows;
	REQ_INSERTROWS insertrows;
	REQ_DELETEROW deleterow;
	REQ_DELETECELLS deletecells;
	REQ_DELETETABLE deletetable;
	REQ_CLOSETABLE closetable;
	REQ_MATCHROW matchrow;
	REQ_GETPROPNAMES getpropnames;
	REQ_GETTABLENAMES gettablenames;
	REQ_READFILE readfile;
	REQ_APPENDFILE appendfile;
	REQ_REMOVEFILE removefile;
	REQ_STATFILE statfile;
	REQ_CHECKROW checkrow;
	REQ_PHPRPC phprpc;
} REQUEST_PAYLOAD;

typedef struct _RPC_REQUEST {
	uint8_t call_id;
	char *dir;
	REQUEST_PAYLOAD payload;
} RPC_REQUEST;

typedef struct _RESP_INFOSTORAGE {
	uint64_t total_space;
	uint64_t total_used;
	uint32_t total_dir;
} RESP_INFOSTORAGE;

typedef struct _RESP_ALLOCDIR {
	char *pdir;
} RESP_ALLOCDIR;

typedef struct _RESP_GETPROPVALS {
	DAC_VARRAY propvals;
} RESP_GETPROPVALS;

typedef struct _RESP_OPENTABLE {
	uint32_t table_id;
} RESP_OPENTABLE;

typedef struct _RESP_OPENCELLTABLE {
	uint32_t table_id;
} RESP_OPENCELLTABLE;

typedef struct _RESP_SUMTABLE {
	uint32_t count;
} RESP_SUMTABLE;

typedef struct _RESP_QUERYROWS {
	DAC_VSET set;
} RESP_QUERYROWS;

typedef struct _RESP_MATCHROW {
	DAC_VARRAY row;
} RESP_MATCHROW;

typedef struct _RESP_GETNAMESPACES {
	STRING_ARRAY namespaces;
} RESP_GETNAMESPACES;

typedef struct _RESP_GETPROPNAMES {
	STRING_ARRAY propnames;
} RESP_GETPROPNAMES;

typedef struct _RESP_GETTABLENAMES {
	STRING_ARRAY tablenames;
} RESP_GETTABLENAMES;

typedef struct _RESP_READFILE {
	BINARY content_bin;
} RESP_READFILE;

typedef struct _RESP_STATFILE {
	uint64_t size;
} RESP_STATFILE;

typedef struct _RESP_PHPRPC {
	BINARY bin_out;
} RESP_PHPRPC;

typedef union _RESPONSE_PAYLOAD {
	RESP_INFOSTORAGE infostorage;
	RESP_ALLOCDIR allocdir;
	RESP_GETPROPVALS getpropvals;
	RESP_OPENTABLE opentable;
	RESP_OPENCELLTABLE opencelltable;
	RESP_SUMTABLE sumtable;
	RESP_QUERYROWS queryrows;
	RESP_MATCHROW matchrow;
	RESP_GETNAMESPACES getnamespaces;
	RESP_GETPROPNAMES getpropnames;
	RESP_GETTABLENAMES gettablenames;
	RESP_READFILE readfile;
	RESP_STATFILE statfile;
	RESP_PHPRPC phprpc;
} RESPONSE_PAYLOAD;

typedef struct _RPC_RESPONSE {
	uint8_t call_id;
	uint32_t result;
	RESPONSE_PAYLOAD payload;
} RPC_RESPONSE;

BOOL dac_ext_push_request(const RPC_REQUEST *prequest, BINARY *pbin_out);

BOOL dac_ext_pull_response(const BINARY *pbin_in, RPC_RESPONSE *presponse);

#endif /* _H_DAC_EXT_ */
