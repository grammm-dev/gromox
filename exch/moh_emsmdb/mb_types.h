#ifndef _H_MB_TYPES_
#define _H_MB_TYPES_
#include <gromox/mapi_types.hpp>

struct CONNECT_REQUEST {
	char *puserdn;
	uint32_t flags;
	uint32_t cpid;
	uint32_t lcid_string;
	uint32_t lcid_sort;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct CONNECT_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t max_polls;
	uint32_t max_retry;
	uint32_t retry_delay;
	char pdn_prefix[1024];
	char pdisplayname[1024];
	uint32_t cb_auxout;
	uint8_t pauxout[0x1008];
};

struct EXECUTE_REQUEST {
	uint32_t flags;
	uint32_t cb_in;
	uint8_t *pin;
	uint32_t cb_out;
	uint32_t cb_auxin;	
	uint8_t *pauxin;
};

struct EXECUTE_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t flags;
	uint32_t cb_out;
	uint8_t pout[0x40000];
	uint32_t cb_auxout;
	uint8_t pauxout[0x1008];
};

struct DISCONNECT_REQUEST {
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct DISCONNECT_RESPONSE {
	uint32_t status;
	uint32_t result;
};

struct NOTIFICATIONWAIT_REQUEST {
	uint32_t flags;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct NOTIFICATIONWAIT_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t flags_out;
};

union EMSMDB_REQUEST {
	CONNECT_REQUEST connect;
	DISCONNECT_REQUEST disconnect;
	EXECUTE_REQUEST execute;
	NOTIFICATIONWAIT_REQUEST notificationwait;
};

union EMSMDB_RESPONSE {
	CONNECT_RESPONSE connect;
	DISCONNECT_RESPONSE disconnect;
	EXECUTE_RESPONSE execute;
	NOTIFICATIONWAIT_RESPONSE notificationwait;
};

#endif /* _H_MB_TYPES_ */
