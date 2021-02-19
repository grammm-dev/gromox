#include "util.h"
#include "service.h"
#include "listener.h"
#include "mail_func.h"
#include "cmd_parser.h"
#include "common_util.h"
#include "config_file.h"
#include "mail_engine.h"
#include "exmdb_client.h"
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

#define MIDB_VERSION		"3.0"

BOOL g_notify_stop = FALSE;

static void term_handler(int signo)
{
	g_notify_stop = TRUE;
}

int main(int argc, char **argv)
{
	BOOL b_wal;
	BOOL b_async;
	int stub_num;
	int mime_num;
	int proxy_num;
	int table_size;
	int listen_port;
	int threads_num;
	char *str_value;
	struct rlimit rl;
	char charset[32];
	int console_port;
	char timezone[64];
	char org_name[256];
	int cache_interval;
	char temp_buff[32];
	char listen_ip[16];
	char acl_path[256];
	uint64_t mmap_size;
	char console_ip[16];
	char data_path[256];
	char exmdb_path[256];
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
		printf("version: %s\n", MIDB_VERSION);
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
		strcpy(service_path, "../service_plugins/midb");
		config_file_set_value(pconfig, "SERVICE_PLUGIN_PATH",
			"../service_plugins/midb");
	} else {
		strcpy(service_path, str_value);
	}
	printf("[system]: service plugin path is %s\n", service_path);

	str_value = config_file_get_value(pconfig, "CONFIG_FILE_PATH");
	if (NULL == str_value) {
		strcpy(config_path, "../config/midb");	
	} else {
		strcpy(config_path, str_value);
	}
	printf("[system]: config path is %s\n", config_path);
	
	str_value = config_file_get_value(pconfig, "DATA_FILE_PATH");
	if (NULL == str_value) {
		strcpy(data_path, "../data/midb");	
	} else {
		strcpy(data_path, str_value);
	}
	printf("[system]: data path is %s\n", data_path);
	
	sprintf(acl_path, "%s/midb_acl.txt", data_path);
	sprintf(exmdb_path, "%s/exmdb_list.txt", data_path);
	printf("[system]: acl file path is %s\n", acl_path);
	printf("[system]: exmdb file path is %s\n", exmdb_path);
	
	str_value = config_file_get_value(pconfig, "RPC_PROXY_CONNECTION_NUM");
	if (NULL == str_value) {
		proxy_num = 10;
		config_file_set_value(pconfig, "RPC_PROXY_CONNECTION_NUM", "10");
	} else {
		proxy_num = atoi(str_value);
		if (proxy_num <= 0 || proxy_num > 200) {
			config_file_set_value(pconfig, "RPC_PROXY_CONNECTION_NUM", "10");
			proxy_num = 10;
		}
	}
	printf("[system]: exmdb proxy connection number is %d\n", proxy_num);
	
	str_value = config_file_get_value(pconfig, "NOTIFY_STUB_THREADS_NUM");
	if (NULL == str_value) {
		stub_num = 10;
		config_file_set_value(pconfig, "NOTIFY_STUB_THREADS_NUM", "10");
	} else {
		stub_num = atoi(str_value);
		if (stub_num <= 0 || stub_num > 200) {
			stub_num = 10;
			config_file_set_value(pconfig, "NOTIFY_STUB_THREADS_NUM", "10");
		}
	}
	printf("[system]: exmdb notify stub threads number is %d\n", stub_num);
	
	str_value = config_file_get_value(pconfig, "MIDB_LISTEN_IP");
	if (NULL == str_value) {
		listen_ip[0] = '\0';
		printf("[system]: listen ip is ANY\n");
	} else {
		strncpy(listen_ip, str_value, 16);
		printf("[system]: listen ip is %s\n", listen_ip);
	}

	str_value = config_file_get_value(pconfig, "MIDB_LISTEN_PORT");
	if (NULL == str_value) {
		listen_port = 5555;
		config_file_set_value(pconfig, "MIDB_LISTEN_PORT", "5555");
	} else {
		listen_port = atoi(str_value);
		if (listen_port <= 0) {
			listen_port = 5555;
			config_file_set_value(pconfig, "MIDB_LISTEN_PORT", "5555");
		}
	}
	printf("[system]: listen port is %d\n", listen_port);

	str_value = config_file_get_value(pconfig, "MIDB_THREADS_NUM");
	if (NULL == str_value) {
		threads_num = 50;
		config_file_set_value(pconfig, "MIDB_THREADS_NUM", "50");
	} else {
		threads_num = atoi(str_value);
		if (threads_num < 20) {
			threads_num = 20;
			config_file_set_value(pconfig, "MIDB_THREADS_NUM", "20");
		} else if (threads_num > 1000) {
			threads_num = 1000;
			config_file_set_value(pconfig, "MIDB_THREADS_NUM", "1000");
		}
	}
	printf("[system]: connection threads number is %d\n", threads_num);
	
	str_value = config_file_get_value(pconfig, "MIDB_TABLE_SIZE");
	if (NULL == str_value) {
		table_size = 3000;
		config_file_set_value(pconfig, "MIDB_TABLE_SIZE", "3000");
	} else {
		table_size = atoi(str_value);
		if (table_size < 100) {
			table_size = 100;
			config_file_set_value(pconfig, "MIDB_TABLE_SIZE", "100");
		} else if (table_size > 50000) {
			table_size = 50000;
			config_file_set_value(pconfig, "MIDB_TABLE_SIZE", "50000");
		}
	}
	printf("[system]: hash table size is %d\n", table_size);

	str_value = config_file_get_value(pconfig, "MIDB_CACHE_INTERVAL");
	if (NULL == str_value) {
		cache_interval = 600;
		config_file_set_value(pconfig, "MIDB_CACHE_INTERVAL", "10minutes");
	} else {
		cache_interval = atoitvl(str_value);
		if (cache_interval < 60 || cache_interval > 1800) {
			cache_interval = 600;
			config_file_set_value(pconfig, "MIDB_CACHE_INTERVAL", "10minutes");
		}
	}
	itvltoa(cache_interval, temp_buff);
	printf("[system]: cache interval is %s\n", temp_buff);
	
	str_value = config_file_get_value(pconfig, "MIDB_MIME_NUMBER");
	if (NULL == str_value) {
		mime_num = 4096;
		config_file_set_value(pconfig, "MIDB_MIME_NUMBER", "4096");
	} else {
		mime_num = atoi(str_value);
		if (mime_num < 1024) {
			mime_num = 4096;
			config_file_set_value(pconfig, "MIDB_MIME_NUMBER", "4096");
		}
	}
	printf("[system]: mime number is %d\n", mime_num);
	
	
	str_value = config_file_get_value(pconfig, "X500_ORG_NAME");
	if (NULL == str_value) {
		strcpy(org_name, "gridware information");
		config_file_set_value(pconfig, "X500_ORG_NAME", org_name);
	} else {
		strcpy(org_name, str_value);
	}
	printf("[system]: x500 org name is \"%s\"\n", org_name);
	
	str_value = config_file_get_value(pconfig, "DEFAULT_CHARSET");
	if (NULL == str_value) {
		strcpy(charset, "windows-1252");
		config_file_set_value(pconfig, "DEFAULT_CHARSET", charset);
	} else {
		strcpy(charset, str_value);
	}
	printf("[system]: default charset is \"%s\"\n", charset);

	str_value = config_file_get_value(pconfig, "DEFAULT_TIMEZONE");
	if (NULL == str_value) {
		strcpy(timezone, "Asia/Shanghai");
		config_file_set_value(pconfig, "DEFAULT_TIMEZONE", timezone);
	} else {
		strcpy(timezone, str_value);
	}
	printf("[system]: default timezone is \"%s\"\n", timezone);
	
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
	str_value = config_file_get_value(pconfig, "SQLITE_MMAP_SIZE");
	if (NULL != str_value) {
		mmap_size = atobyte(str_value);
	} else {
		mmap_size = 0;
	}
	if (0 == mmap_size) {
		printf("[system]: sqlite mmap_size is disabled\n");
	} else {
		bytetoa(mmap_size, temp_buff);
		printf("[system]: sqlite mmap_size is %s\n", temp_buff);
	}
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
	config_file_save(pconfig);
	config_file_free(pconfig);
	
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

	service_init(threads_num, service_path, config_path, data_path);
	
	system_services_init();
	
	common_util_init();
	
	exmdb_client_init(proxy_num, stub_num, exmdb_path);
	
	listener_init(listen_ip, listen_port, acl_path);
	
	mail_engine_init(charset, timezone, org_name, table_size,
		b_async, b_wal, mmap_size, cache_interval, mime_num);

	cmd_parser_init(threads_num, SOCKET_TIMEOUT);

	console_server_init(console_ip, console_port);

	if (0 != service_run()) {
		printf("[system]: fail to run service\n");
		return -3;
	}
	
	if (0 != system_services_run()) {
		printf("[system]: fail to run system services\n");
		return -4;
	}
	
	if (0 != common_util_run()) {
		system_services_stop();
		service_stop();
		printf("[system]: fail to run common util\n");
		return -5;
	}
	
	if (0 != listener_run()) {
		common_util_stop();
		system_services_stop();
		service_stop();
		printf("[system]: fail to run tcp listener\n");
		return -6;
	}

	if (0 != cmd_parser_run()) {
		listener_stop();
		common_util_stop();
		system_services_stop();
		service_stop();
		printf("[system]: fail to run command parser\n");
		return -7;
	}

	if (0 != mail_engine_run()) {
		cmd_parser_stop();
		listener_stop();
		common_util_stop();
		system_services_stop();
		service_stop();
		printf("[system]: fail to run mail engine\n");
		return -8;
	}
	
	if (0 != exmdb_client_run()) {
		mail_engine_stop();
		cmd_parser_stop();
		listener_stop();
		common_util_stop();
		system_services_stop();
		service_stop();
		printf("[system]: fail to run exmdb client\n");
		return -9;
	}

	
	if (0 != console_server_run()) {
		exmdb_client_stop();
		mail_engine_stop();
		cmd_parser_stop();
		listener_stop();
		common_util_stop();
		system_services_stop();
		service_stop();
		printf("[system]: fail to run console server\n");
		return -10;
	}

	if (0 != listener_trigger_accept()) {
		console_server_stop();
		exmdb_client_stop();
		mail_engine_stop();
		cmd_parser_stop();
		listener_stop();
		common_util_stop();
		system_services_stop();
		service_stop();
		printf("[system]: fail to trigger tcp listener\n");
		return -11;
	}
	
	signal(SIGTERM, term_handler);
	printf("[system]: MIDB is now running\n");
	while (FALSE == g_notify_stop) {
		sleep(1);
	}
	listener_stop();
	console_server_stop();
	cmd_parser_stop();
	exmdb_client_stop();
	mail_engine_stop();
	common_util_stop();
	system_services_stop();
	service_stop();
	return 0;
}
