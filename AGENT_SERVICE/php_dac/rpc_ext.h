#ifndef _H_RPC_EXT_
#define _H_RPC_EXT_
#include "ext_pack.h"

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
#define CALL_ID_GETADDRESSBOOK						0x20
#define CALL_ID_GETABHIERARCHY						0x21
#define CALL_ID_GETABCONTENT						0x22
#define CALL_ID_RESTRICTABCONTENT					0x23
#define CALL_ID_GETEXADDRESS						0x24
#define CALL_ID_GETEXADDRESSTYPE					0x25
#define CALL_ID_CHECKMLISTINCLUDE					0x26

#define CALL_ID_PHPRPC								0x31

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
	DAC_RES *prestrict;
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
	DAC_RES *prestrict;
	uint16_t max_count;
} REQ_RESTRICTABCONTENT;

typedef struct _REQ_CHECKMLISTINCLUDE {
	char *account_address;
} REQ_CHECKMLISTINCLUDE;

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
	REQ_PHPRPC phprpc;
} REQUEST_PAYLOAD;

typedef struct _RPC_REQUEST {
	uint8_t call_id;
	const char *address;
	REQUEST_PAYLOAD payload;
} RPC_REQUEST;

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

typedef struct _RESP_GETADDRESSBOOK {
	char *pab_address;
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
	char *pex_address;
} RESP_GETEXADDRESS;

typedef struct _RESP_GETEXADDRESSTYPE {
	uint8_t address_type;
} RESP_GETEXADDRESSTYPE;

typedef struct _RESPONSE_CHECKMLISTINCLUDE {
	zend_bool b_include;
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

zend_bool rpc_ext_push_request(const RPC_REQUEST *prequest,
	BINARY *pbin_out);

zend_bool rpc_ext_pull_response(const BINARY *pbin_in,
	RPC_RESPONSE *presponse);

#endif /* _H_RPC_EXT_ */
