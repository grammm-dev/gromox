#ifndef _H_COMMON_UTIL_
#define _H_COMMON_UTIL_
#include "simple_tree.h"
#include "mapi_types.h"
#include "dac_types.h"
#include <pthread.h>
#include <sqlite3.h>
#include <time.h>

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

#define CALL_ID_GETADDRESSBOOK						0x20
#define CALL_ID_GETABHIERARCHY						0x21
#define CALL_ID_GETABCONTENT						0x22
#define CALL_ID_RESTRICTABCONTENT					0x23
#define CALL_ID_GETEXADDRESS						0x24
#define CALL_ID_GETEXADDRESSTYPE					0x25
#define CALL_ID_CHECKMLISTINCLUDE					0x26

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

#define DAC_DIR_TYPE_SIMPLE							0
#define DAC_DIR_TYPE_PRIVATE						1
#define DAC_DIR_TYPE_PUBLIC							2

#define ESSDN_TYPE_ERROR							0x00
#define ESSDN_TYPE_ORG								0x01
#define ESSDN_TYPE_DOMAIN							0x02
#define ESSDN_TYPE_GROUP							0x03
#define ESSDN_TYPE_CLASS							0x04
#define ESSDN_TYPE_USER								0x05
#define ESSDN_TYPE_GAL								0x06
#define ESSDN_TYPE_UNKNOWN							0xFF


void common_util_init(const char *org_name);

int common_util_run();

int common_util_stop();

void common_util_free();

BOOL common_util_build_environment();

void common_util_free_environment();

void* common_util_alloc(size_t size);

char* common_util_dup(const char *pstr);

int common_util_get_essdn_type(const char *essdn);

BOOL common_util_essdn_to_user_ids(const char *pessdn,
	int *pdomain_id, int *puser_id);

BOOL common_util_essdn_to_base_id(
	const char *pessdn, int *pbase_id);

BOOL common_util_essdn_to_domain_id(
	const char *pessdn, int *pdomain_id);

BOOL common_util_essdn_to_group_id(const char *pessdn,
	int *pdomain_id, int *pgroup_id);

BOOL common_util_essdn_to_class_id(const char *pessdn,
	int *pdomain_id, int *pclass_id);

BOOL common_util_get_abhierarchy_from_node(
	SIMPLE_TREE_NODE *pnode, DAC_VSET *pset);

BOOL common_util_get_dir(const char *address, char *dir);

const char* common_util_get_org_name();

#endif /* _H_COMMON_UTIL_ */
