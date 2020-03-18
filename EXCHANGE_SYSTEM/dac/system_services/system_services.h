#ifndef _H_SYSTEM_SERVICES_
#define _H_SYSTEM_SERVICES_
#include "common_types.h"
#include "mem_file.h"

void system_services_init();

int system_services_run();

int system_services_stop();

void system_services_free();

extern BOOL (*system_services_fcgi_rpc)(const uint8_t *pbuff_in,
	uint32_t in_len, uint8_t **ppbuff_out, uint32_t *pout_len,
	const char *script_path);

#endif /* _H_SYSTEM_SERVICES_ */
