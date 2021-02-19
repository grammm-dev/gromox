#ifndef _H_SYSTEM_SERVICES_
#define _H_SYSTEM_SERVICES_

#include "common_types.h"
#include "mem_file.h"
#include "xarray.h"
#include "double_list.h"
#include "single_list.h"

void system_services_init();

int system_services_run();

int system_services_stop();

void system_services_free();

extern BOOL (*system_services_judge_ip)(const char*);
extern BOOL (*system_services_container_add_ip)(const char*);
extern BOOL (*system_services_container_remove_ip)(const char*);
extern BOOL (*system_services_judge_user)(const char*);
extern int (*system_services_add_user_into_temp_list)(const char*, int);
extern BOOL (*system_services_auth_login)(const char*, const char*, char*, char*, char*, int);
extern const char* (*system_services_extension_to_mime)(const char*);
extern void (*system_services_log_info)(int, char*, ...);
extern BOOL (*system_services_login_check_judge)(const char*);
extern int (*system_services_login_check_add)(const char*, int);
extern BOOL (*system_services_fcgi_rpc)(const uint8_t *pbuff_in,
	uint32_t in_len, uint8_t **ppbuff_out, uint32_t *pout_len,
	const char *script_path);

#endif /* _H_SYSTEM_SERVICES_ */
