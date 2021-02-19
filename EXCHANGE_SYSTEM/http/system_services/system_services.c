#include "system_services.h"
#include "service.h"
#include <stdio.h>

BOOL (*system_services_judge_ip)(const char*);
BOOL (*system_services_judge_user)(const char*);
BOOL (*system_services_container_add_ip)(const char*);
BOOL (*system_services_container_remove_ip)(const char*);
int (*system_services_add_user_into_temp_list)(const char*, int);
BOOL (*system_services_auth_login)(const char*, const char*, char*, char*, char*, int);
const char* (*system_services_extension_to_mime)(const char*);
void (*system_services_log_info)(int, char*, ...);
BOOL (*system_services_login_check_judge)(const char*);
int (*system_services_login_check_add)(const char*, int);
BOOL (*system_services_fcgi_rpc)(const uint8_t *pbuff_in,
	uint32_t in_len, uint8_t **ppbuff_out, uint32_t *pout_len,
	const char *script_path);

/*
 *	module's construct function
 */
void system_services_init()
{
	/* do nothing */
}

/*
 *	run system services module
 *	@return
 *		0		OK
 *		<>0		fail
 */
int system_services_run()
{
	system_services_judge_ip = service_query("ip_filter_judge", "system");
	if (NULL == system_services_judge_ip) {
		printf("[system_services]: fail to get \"ip_filter_judge\" service\n");
		return -1;
	}
	system_services_container_add_ip = service_query("ip_container_add",
												"system");
	if (NULL == system_services_container_add_ip) {
		printf("[system_services]: fail to get \"ip_container_add\" service\n");
		return -2;
	}
	system_services_container_remove_ip = service_query("ip_container_remove",
												"system");
	if (NULL == system_services_container_remove_ip) {
		printf("[system_services]: fail to get \"ip_container_remove\" "
			"service\n");
		return -3;
	}
	system_services_log_info = service_query("log_info", "system");
	if (NULL == system_services_log_info) {
		printf("[system_services]: fail to get \"log_info\" service\n");
		return -4;
	}
	system_services_judge_user = service_query("user_filter_judge", "system");
	if (NULL == system_services_judge_user) {
		printf("[system_services]: fail to get \"user_filter_judge\" service\n");
		return -5;
	}
	system_services_add_user_into_temp_list = service_query("user_filter_add", 
												"system");
	if (NULL == system_services_add_user_into_temp_list) {
		printf("[system_services]: fail to get \"user_filter_add\" service\n");
		return -6;
	}
	system_services_auth_login = service_query("auth_login", "system");
	if (NULL == system_services_auth_login) {
		printf("[system_services]: fail to get \"auth_login\" service\n");
		return -7;
	}
	system_services_extension_to_mime = service_query("extension_to_mime", "system");
	if (NULL == system_services_extension_to_mime) {
		printf("[system_services]: fail to get \"extension_to_mime\" service\n");
		return -8;
	}
	system_services_login_check_judge = service_query(
						"login_check_judge", "system");
	if (NULL == system_services_login_check_judge) {
		printf("[system_services]: fail to get"
			" \"login_check_judge\" service\n");
		return -9;
	}
	system_services_login_check_add = service_query(
						"login_check_add", "system");
	if (NULL == system_services_login_check_add) {
		printf("[system_services]: fail to get"
			" \"login_check_add\" service\n");
		return -10;
	}
	system_services_fcgi_rpc = service_query("fcgi_rpc", "system");
	if (NULL == system_services_fcgi_rpc) {
		printf("[system_services]: fail to get \"fcgi_rpc\" service\n");
		return -11;
	}
	return 0;
}

/*
 *	stop the system services
 *	@return
 *		0		OK
 *		<>0		fail
 */
int system_services_stop()
{
	service_release("ip_filter_judge", "system");
	service_release("ip_container_add", "system");
	service_release("ip_container_remove", "system");
	service_release("log_info", "system");
	service_release("user_filter_judge", "system");
	service_release("user_filer_add", "system");
	service_release("auth_login", "system");
	service_release("extension_to_mime", "system");
	return 0;
}

/*
 *	module's destruct function
 */
void system_services_free()
{
	/* do nothing */

}


