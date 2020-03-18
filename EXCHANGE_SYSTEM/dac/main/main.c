#include "util.h"
#include "service.h"
#include "listener.h"
#include "mail_func.h"
#include "dac_server.h"
#include "rpc_parser.h"
#include "common_util.h"
#include "config_file.h"
#include "console_server.h"
#include "system_services.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>

BOOL g_notify_stop = FALSE;

static void term_handler(int signo)
{
	g_notify_stop = TRUE;
}

int main(int argc, char **argv)
{
	BOOL b_wal;
	BOOL b_async;
	int table_size;
	int threads_num;
	char *str_value;
	int listen_port;
	int console_port;
	struct rlimit rl;
	char listen_ip[16];
	int valid_interval;
	char temp_buff[64];
	char acl_path[256];
	char sql_path[256];
	char map_path[256];
	char console_ip[16];
	char data_path[256];
	CONFIG_FILE *pconfig;
	char config_path[256];
	char service_path[256];
	
	
	if (2 != argc) {
		printf("%s <cfg file>\n", argv[0]);
		return -1;
	}
	if (2 == argc && 0 == strcmp(argv[1], "--help")) {
		printf("%s <cfg file>\n", argv[0]);
		return 0;
	}
	if (2 == argc && 0 == strcmp(argv[1], "--version")) {
		printf("version: %s\n", DAC_VERSION);
		return 0;
	}
	umask(0);	
	signal(SIGPIPE, SIG_IGN);
	
	pconfig = config_file_init(argv[1]);
	if (NULL == pconfig) {
		printf("[system]: fail to open config file %s\n", argv[1]);
		return -2;
	}
	
	str_value = config_file_get_value(pconfig, "SERVICE_PLUGIN_PATH");
	if (NULL == str_value) {
		strcpy(service_path, "../service_plugins/dac");
		config_file_set_value(pconfig, "SERVICE_PLUGIN_PATH",
			"../service_plugins/dac");
	} else {
		strcpy(service_path, str_value);
	}
	printf("[system]: service plugin path is %s\n", service_path);
	
	str_value = config_file_get_value(pconfig, "CONFIG_FILE_PATH");
	if (NULL == str_value) {
		strcpy(config_path, "../config/dac");	
	} else {
		strcpy(config_path, str_value);
	}
	printf("[system]: config path is %s\n", config_path);
	
	str_value = config_file_get_value(pconfig, "DATA_FILE_PATH");
	if (NULL == str_value) {
		strcpy(data_path, "../data/dac");	
	} else {
		strcpy(data_path, str_value);
	}
	printf("[system]: data path is %s\n", data_path);
	sprintf(acl_path, "%s/dac_acl.txt", data_path);
	sprintf(sql_path, "%s/sqlite3_dac.txt", data_path);
	sprintf(map_path, "%s/propname_maps.txt", data_path);
	
	str_value = config_file_get_value(pconfig, "DAC_LISTEN_IP");
	if (NULL == str_value) {
		listen_ip[0] = '\0';
		printf("[system]: listen ip is ANY\n");
	} else {
		strncpy(listen_ip, str_value, 16);
		printf("[system]: listen ip is %s\n", listen_ip);
	}

	str_value = config_file_get_value(pconfig, "DAC_LISTEN_PORT");
	if (NULL == str_value) {
		listen_port = 5001;
		config_file_set_value(pconfig, "DAC_LISTEN_PORT", "5001");
	} else {
		listen_port = atoi(str_value);
		if (listen_port <= 0) {
			listen_port = 5001;
			config_file_set_value(pconfig, "DAC_LISTEN_PORT", "5001");
		}
	}
	printf("[system]: listen port is %d\n", listen_port);
	
	listener_init(listen_ip, listen_port, acl_path);
	
	str_value = config_file_get_value(pconfig, "TABLE_SIZE");
	if (NULL == str_value) {
		table_size = 10000;
	}
	table_size = atoi(str_value);
	if (table_size < 1000) {
		table_size = 1000;
		config_file_set_value(pconfig, "TABLE_SIZE", "1000");
	}
	printf("[system]: hash table size is %d\n", table_size);
	str_value = config_file_get_value(pconfig, "VALID_INTERVAL");
	if (NULL == str_value) {
		valid_interval = 600;
		config_file_set_value(pconfig, "VALID_INTERVAL", "10minutes");
	} else {
		valid_interval = atoitvl(str_value);
		if (valid_interval < 180) {
			valid_interval = 600;
			config_file_set_value(pconfig, "VALID_INTERVAL", "10minutes");
		}
	}
	itvltoa(valid_interval, temp_buff);
	printf("[system]: valid interval is %s\n", temp_buff);
	
	str_value = config_file_get_value(pconfig, "SQLITE_SYNCHRONOUS");
	if (NULL == str_value) {
		b_async = FALSE;
		config_file_set_value(pconfig, "SQLITE_SYNCHRONOUS", "OFF");
	} else {
		if (0 == strcasecmp(str_value, "OFF") ||
			0 == strcasecmp(str_value, "FALSE")) {
			b_async = FALSE;
		} else {
			b_async = TRUE;
		}
	}
	if (FALSE == b_async) {
		printf("[system]: sqlite synchronous PRAGMA is OFF\n");
	} else {
		printf("[system]: sqlite synchronous PRAGMA is ON\n");
	}
	
	str_value = config_file_get_value(pconfig, "SQLITE_WAL_MODE");
	if (NULL == str_value) {
		b_wal = TRUE;
		config_file_set_value(pconfig, "SQLITE_WAL_MODE", "ON");
	} else {
		if (0 == strcasecmp(str_value, "OFF") ||
			0 == strcasecmp(str_value, "FALSE")) {
			b_wal = FALSE;
		} else {
			b_wal = TRUE;
		}
	}
	if (FALSE == b_wal) {
		printf("[system]: sqlite journal mode is DELETE\n");
	} else {
		printf("[system]: sqlite journal mode is WAL\n");
	}
	
	common_util_init(table_size, valid_interval, b_async, b_wal, sql_path);

	str_value = config_file_get_value(pconfig, "DAC_THREADS_NUM");
	if (NULL == str_value) {
		threads_num = 20;
		config_file_set_value(pconfig, "DAC_THREADS_NUM", "20");
	} else {
		threads_num = atoi(str_value);
		if (threads_num < 5) {
			threads_num = 5;
			config_file_set_value(pconfig, "DAC_THREADS_NUM", "5");
		} else if (threads_num > 1000) {
			threads_num = 100;
			config_file_set_value(pconfig, "DAC_THREADS_NUM", "100");
		}
	}
	printf("[system]: connection threads number is %d\n", threads_num);
	
	rpc_parser_init(threads_num);
	
	dac_server_init(map_path);
	
	str_value = config_file_get_value(pconfig, "CONSOLE_SERVER_IP");
	if (NULL == str_value || NULL == extract_ip(str_value, console_ip)) {
		strcpy(console_ip, "127.0.0.1");
		config_file_set_value(pconfig, "CONSOLE_SERVER_IP", "127.0.0.1");
	}
	printf("[system]: console server ip is %s\n", console_ip);
	str_value = config_file_get_value(pconfig, "CONSOLE_SERVER_PORT");
	if (NULL == str_value) {
		console_port = 9900;
		config_file_set_value(pconfig, "CONSOLE_SERVER_PORT", "9900");
	} else {
		console_port = atoi(str_value);
		if (console_port <= 0) {
			console_port = 9900;
			config_file_set_value(pconfig, "CONSOLE_SERVER_PORT", "9900");
		}
	}
	printf("[system]: console server port is %d\n", console_port);
	
	console_server_init(console_ip, console_port);
	
	config_file_save(pconfig);
	config_file_free(pconfig);
	
	service_init(threads_num, service_path, config_path, data_path);
	
	system_services_init();
	
	if (0 != getrlimit(RLIMIT_NOFILE, &rl)) {
		printf("[system]: fail to get file limitation\n");
	} else {
		if (rl.rlim_cur < 5*table_size ||
			rl.rlim_max < 5*table_size) {
			rl.rlim_cur = 5*table_size;
			rl.rlim_max = 5*table_size;
			if (0 != setrlimit(RLIMIT_NOFILE, &rl)) {
				printf("[system]: fail to set file limitation\n");
			} else {
				printf("[system]: set file limitation to %d\n", 5*table_size);
			}
		}
	}
	
	if (0 != service_run()) {
		printf("[system]: fail to run service\n");
		return -3;
	}
	
	if (0 != system_services_run()) {
		printf("[system]: fail to run system services\n");
		return -4;
	}
	
	if (0 != common_util_run()) {
		printf("[system]: fail to run common util\n");
		system_services_stop();
		service_stop();
		return -5;
	}
	
	if (0 != rpc_parser_run()) {
		common_util_stop();
		system_services_stop();
		service_stop();
		printf("[system]: fail to run rpc parser\n");
		return -6;
	}

	if (0 != dac_server_run()) {
		rpc_parser_stop();
		common_util_stop();
		system_services_stop();
		service_stop();
		printf("[system]: fail to run dac server\n");
		return -7;
	}
	
	if (0 != console_server_run()) {
		dac_server_stop();
		rpc_parser_stop();
		common_util_stop();
		system_services_stop();
		service_stop();
		printf("[system]: fail to run listener\n");
		return -8;
	}
	
	if (0 != listener_run()) {
		console_server_stop();
		dac_server_stop();
		rpc_parser_stop();
		common_util_stop();
		system_services_stop();
		service_stop();
		printf("[system]: fail to run listener\n");
		return -9;
	}
	
	if (0 != listener_trigger_accept()) {
		listener_stop();
		console_server_stop();
		rpc_parser_stop();
		dac_server_stop();
		common_util_stop();
		system_services_stop();
		service_stop();
		printf("[system]: fail to trigger tcp listener\n");
		return -10;
	}
	
	signal(SIGTERM, term_handler);
	printf("[system]: dac is now running\n");
	while (FALSE == g_notify_stop) {
		sleep(1);
	}
	listener_stop();
	console_server_stop();
	rpc_parser_stop();
	dac_server_stop();
	common_util_stop();
	system_services_stop();
	service_stop();
	return 0;
}
