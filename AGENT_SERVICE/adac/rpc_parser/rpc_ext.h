#ifndef _H_RPC_EXT_
#define _H_RPC_EXT_
#include "mapi_types.h"
#include "common_util.h"
#include <stdint.h>

typedef struct _REQ_ALLOCDIR {
	uint8_t type;
} REQ_ALLOCDIR;

typedef struct _REQ_SETPROPVALS {
	char *pnamespace;
	DAC_VARRAY propvals;
} REQ_SETPROPVALS;

typedef struct _REQ_GETPROPVALS {
	char *pnamespace;
	STRING_ARRAY names;
} REQ_GETPROPVALS;

typedef struct _REQ_DELETEPROPVALS {
	char *pnamespace;
	STRING_ARRAY names;
} REQ_DELETEPROPVALS;

typedef struct _REQ_OPENTABLE {
	char *pnamespace;
	char *ptablename;
	uint32_t flags;
	char *puniquename;
} REQ_OPENTABLE;

typedef struct _REQ_OPENCELLTABLE {
	uint64_t row_instance;
	char *pcolname;
	uint32_t flags;
	char *puniquename;
} REQ_OPENCELLTABLE;

typedef struct _REQ_RESTRICTTABLE {
	uint32_t table_id;
	DAC_RES restrict;
} REQ_RESTRICTTABLE;

typedef struct _REQ_UPDATECELLS {
	uint64_t row_instance;
	DAC_VARRAY cells;
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
	DAC_VSET set;
} REQ_INSERTROWS;

typedef struct _REQ_DELETEROW {
	uint64_t row_instance;
} REQ_DELETEROW;

typedef struct _REQ_DELETECELLS {
	uint64_t row_instance;
	STRING_ARRAY colnames;
} REQ_DELETECELLS;

typedef struct _REQ_DELETETABLE {
	char *pnamespace;
	char *ptablename;
} REQ_DELETETABLE;

typedef struct _REQ_CLOSETABLE {
	uint32_t table_id;
} REQ_CLOSETABLE;

typedef struct _REQ_MATCHROW {
	uint32_t table_id;
	uint16_t type;
	void *pvalue;
} REQ_MATCHROW;

typedef struct _REQ_GETPROPNAMES {
	char *pnamespace;
} REQ_GETPROPNAMES;

typedef struct _REQ_GETTABLENAMES {
	char *pnamespace;
} REQ_GETTABLENAMES;

typedef struct _REQ_READFILE {
	char *path;
	char *file_name;
	uint64_t offset;
	uint32_t length;
} REQ_READFILE;

typedef struct _REQ_APPENDFILE {
	char *path;
	char *file_name;
	BINARY content_bin;
} REQ_APPENDFILE;

typedef struct _REQ_REMOVEFILE {
	char *path;
	char *file_name;
} REQ_REMOVEFILE;

typedef struct _REQ_STATFILE {
	char *path;
	char *file_name;
} REQ_STATFILE;

typedef struct _REQ_GETABCONTENT {
	uint32_t start_pos;
	uint16_t count;
} REQ_GETABCONTENT;

typedef struct _REQ_RESTRICTABCONTENT {
	DAC_RES restrict;
	uint16_t max_count;
} REQ_RESTRICTABCONTENT;

typedef struct _REQ_CHECKMLISTINCLUDE {
	char *account_address;
} REQ_CHECKMLISTINCLUDE;

typedef struct _REQ_CHECKROW {
	char *pnamespace;
	char *ptablename;
	uint16_t type;
	void *pvalue;
} REQ_CHECKROW;

typedef struct _REQ_PHPRPC {
	char *script_path;
	BINARY bin_in;
} REQ_PHPRPC;

typedef union _REQUEST_PAYLOAD {
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
	REQ_GETABCONTENT getabcontent;
	REQ_RESTRICTABCONTENT restrictabcontent;
	REQ_CHECKMLISTINCLUDE checkmlistinclude;
	REQ_CHECKROW checkrow;
	REQ_PHPRPC phprpc;
} REQUEST_PAYLOAD;

typedef struct _RPC_REQUEST {
	uint8_t call_id;
	char *address;
	REQUEST_PAYLOAD payload;
} RPC_REQUEST;

typedef struct _RESP_ALLOCDIR {
	char dir[256];
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

typedef struct _RESP_GETADDRESSBOOK {
	char ab_address[1024];
} RESP_GETADDRESSBOOK;

typedef struct _RESP_GETABHIERARCHY {
	DAC_VSET set;
} RESP_GETABHIERARCHY;

typedef struct _RESP_GETABCONTENT {
	DAC_VSET set;
} RESP_GETABCONTENT;

typedef struct _RESP_RESTRICTABCONTENT {
	DAC_VSET set;
} RESP_RESTRICTABCONTENT;

typedef struct _RESP_GETEXADDRESS {
	char ex_address[1024];
} RESP_GETEXADDRESS;

typedef struct _RESP_GETEXADDRESSTYPE {
	uint8_t address_type;
} RESP_GETEXADDRESSTYPE;

typedef struct _RESP_CHECKMLISTINCLUDE {
	BOOL b_include;
} RESP_CHECKMLISTINCLUDE;

typedef struct _RESP_PHPRPC {
	BINARY bin_out;
} RESP_PHPRPC;

typedef union _RESPONSE_PAYLOAD {
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
	RESP_GETADDRESSBOOK getaddressbook;
	RESP_GETABHIERARCHY getabhierarchy;
	RESP_GETABCONTENT getabcontent;
	RESP_RESTRICTABCONTENT restrictabcontent;
	RESP_GETEXADDRESS getexaddress;
	RESP_GETEXADDRESSTYPE getexaddresstype;
	RESP_CHECKMLISTINCLUDE checkmlistinclude;
	RESP_PHPRPC phprpc;
} RESPONSE_PAYLOAD;

typedef struct _RPC_RESPONSE {
	uint8_t call_id;
	uint32_t result;
	RESPONSE_PAYLOAD payload;
} RPC_RESPONSE;

BOOL rpc_ext_pull_request(const BINARY *pbin_in,
	RPC_REQUEST *prequest);

BOOL rpc_ext_push_response(const RPC_RESPONSE *presponse,
	BINARY *pbin_out);

#endif /* _H_RPC_EXT_ */
