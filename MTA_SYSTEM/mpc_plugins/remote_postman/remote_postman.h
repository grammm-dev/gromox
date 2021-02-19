#ifndef _H_REMOTE_POSTMAN_
#define _H_REMOTE_POSTMAN_

#include "hook_common.h"

void remote_postman_init(int max_thr, int files_num,
	int times, int interval, int alarm_interval, BOOL tls_switch,
	int trying_times, int max_rcpt, const char *resource_path,
	const char* separator, const char *timer_path, int timer_threads,
	int scan_interval, int fresh_interval, int retrying_interval,
	int final_interval, const char *sender_list_script,
	const char *routing_path, const char *config_path);

int remote_postman_run();

int remote_postman_stop();

void remote_postman_free();

BOOL remote_postman_hook(MESSAGE_CONTEXT *pcontext);

void remote_postman_console_talk(int argc,
	char **argv, char *result, int length);


#endif /* _H_REMOTE_POSTMAN_ */
