#ifndef _H_MB_TYPES_
#define _H_MB_TYPES_
#include "mapi_types.h"

typedef struct _CONNECT_REQUEST {
	char *puserdn;
	uint32_t flags;
	uint32_t cpid;
	uint32_t lcid_string;
	uint32_t lcid_sort;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} CONNECT_REQUEST;

typedef struct _CONNECT_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t max_polls;
	uint32_t max_retry;
	uint32_t retry_delay;
	char pdn_prefix[1024];
	char pdisplayname[1024];
	uint32_t cb_auxout;
	uint8_t pauxout[0x1008];
} CONNECT_RESPONSE;

typedef struct _EXECUTE_REQUEST {
	uint32_t flags;
	uint32_t cb_in;
	uint8_t *pin;
	uint32_t cb_out;
	uint32_t cb_auxin;	
	uint8_t *pauxin;
} EXECUTE_REQUEST;

typedef struct _EXECUTE_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t flags;
	uint32_t cb_out;
	uint8_t pout[0x40000];
	uint32_t cb_auxout;
	uint8_t pauxout[0x1008];
} EXECUTE_RESPONSE;

typedef struct _DISCONNECT_REQUEST {
	uint32_t cb_auxin;
	uint8_t *pauxin;
} DISCONNECT_REQUEST;

typedef struct _DISCONNECT_RESPONSE {
	uint32_t status;
	uint32_t result;
} DISCONNECT_RESPONSE;

typedef struct _NOTIFICATIONWAIT_REQUEST {
	uint32_t flags;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} NOTIFICATIONWAIT_REQUEST;

typedef struct _NOTIFICATIONWAIT_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t flags_out;
} NOTIFICATIONWAIT_RESPONSE;

typedef union _EMSMDB_REQUEST {
	CONNECT_REQUEST connect;
	DISCONNECT_REQUEST disconnect;
	EXECUTE_REQUEST execute;
	NOTIFICATIONWAIT_REQUEST notificationwait;
} EMSMDB_REQUEST;

typedef union _EMSMDB_RESPONSE {
	CONNECT_RESPONSE connect;
	DISCONNECT_RESPONSE disconnect;
	EXECUTE_RESPONSE execute;
	NOTIFICATIONWAIT_RESPONSE notificationwait;
} EMSMDB_RESPONSE;

#endif /* _H_MB_TYPES_ */
