#include "util.h"
#include "ab_tree.h"
#include "listener.h"
#include "mail_func.h"
#include "dac_client.h"
#include "common_util.h"
#include "config_file.h"
#include "mysql_adaptor.h"
#include "adac_processor.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

BOOL g_notify_stop = FALSE;

static void term_handler(int signo)
{
	g_notify_stop = TRUE;
}

int main(int argc, char **argv)
{
	int conn_num;
	int mysql_port;
	int table_size;
	int threads_num;
	char *str_value;
	int max_item_num;
	int mysql_timeout;
	int mysql_conn_num;
	int cache_interval;
	char temp_buff[64];
	char org_name[256];
	char dac_path[256];
	char data_path[256];
	CONFIG_FILE *pconfig;
	char mysql_host[256];
	char mysql_user[256];
	char mysql_db_name[256];
	char mysql_password[256];
	
	if (2 != argc) {
		printf("%s <cfg file>\n", argv[0]);
		return -1;
	}
	if (2 == argc && 0 == strcmp(argv[1], "--help")) {
		printf("%s <cfg file>\n", argv[0]);
		return 0;
	}
	signal(SIGPIPE, SIG_IGN);
	
	pconfig = config_file_init(argv[1]);
	if (NULL == pconfig) {
		printf("[system]: fail to open config file %s\n", argv[1]);
		return -2;
	}
	
	str_value = config_file_get_value(pconfig, "DATA_FILE_PATH");
	if (NULL == str_value) {
		strcpy(data_path, "../data");	
	} else {
		strcpy(data_path, str_value);
	}
	printf("[system]: data path is %s\n", data_path);
	
	sprintf(dac_path, "%s/dac_list.txt", data_path);
	printf("[system]: dac file path is %s\n", dac_path);
	
	
	str_value = config_file_get_value(pconfig, "X500_ORG_NAME");
	if (NULL == str_value) {
		strcpy(org_name, "gridware information");
		config_file_set_value(pconfig, "X500_ORG_NAME", org_name);
	} else {
		strcpy(org_name, str_value);
	}
	printf("[system]: x500 org name is \"%s\"\n", org_name);
	
	common_util_init(org_name);
	
	str_value = config_file_get_value(pconfig, "MYSQL_HOST");
	if (NULL == str_value) {
		strcpy(mysql_host, "localhost");
		config_file_set_value(pconfig, "MYSQL_HOST", "localhost");
	} else {
		strcpy(mysql_host, str_value);
	}

	printf("[system]: mysql host is %s\n", mysql_host);

	str_value = config_file_get_value(pconfig, "MYSQL_PORT");
	if (NULL == str_value) {
		mysql_port = 3306;
		config_file_set_value(pconfig, "MYSQL_PORT", "3306");
	} else {
		mysql_port = atoi(str_value);
		if (mysql_port <= 0) {
			mysql_port = 3306;
		}
	}
	printf("[system]: mysql port is %d\n", mysql_port);

	str_value = config_file_get_value(pconfig, "MYSQL_USERNAME");
	if (NULL == str_value) {
		strcpy(mysql_user, "herculiz");
		config_file_set_value(pconfig, "MYSQL_USERNAME", "herculiz");
	} else {
		strcpy(mysql_user, str_value);
	}
	printf("[system]: mysql username is %s\n", mysql_user);


	str_value = config_file_get_value(pconfig, "MYSQL_PASSWORD");
	if (NULL == str_value) {
		strcpy(mysql_password, "herculiz");
		config_file_set_value(pconfig, "MYSQL_PASSWORD", "herculiz");
	} else {
		strcpy(mysql_password, str_value);
	}
	printf("[system]: mysql password is ********\n");

	str_value = config_file_get_value(pconfig, "MYSQL_DBNAME");
	if (NULL == str_value) {
		strcpy(mysql_db_name, "email");
		config_file_set_value(pconfig, "MYSQL_DBNAME", "email");
	} else {
		strcpy(mysql_db_name, str_value);
	}
	printf("[system]: mysql database name is %s\n", mysql_db_name);

	str_value = config_file_get_value(pconfig, "MYSQL_RDWR_TIMEOUT");
	if (NULL == str_value) {
		mysql_timeout = 0;
	} else {
		mysql_timeout = atoi(str_value);
		if (mysql_timeout < 0) {
			mysql_timeout = 0;
		}
	}
	if (mysql_timeout > 0) {
		printf("[system]: mysql read write timeout is %d\n", mysql_timeout);
	}

	str_value = config_file_get_value(pconfig, "MYSQL_CONNECTION_NUM");
	if (NULL == str_value) {
		mysql_conn_num = 10;
		config_file_set_value(pconfig, "MYSQL_CONNECTION_NUM", "10");
	} else {
		mysql_conn_num = atoi(str_value);
		if (mysql_conn_num < 10 || mysql_conn_num > 100) {
			mysql_conn_num = 10;
		}
	}
	printf("[system]: mysql connectedtion number is %d\n", mysql_conn_num);
	
	mysql_adaptor_init(mysql_conn_num, mysql_host, mysql_port,
		mysql_user, mysql_password, mysql_db_name, mysql_timeout);
	
	str_value = config_file_get_value(pconfig, "ADDRESS_TABLE_SIZE");
	if (NULL == str_value) {
		table_size = 3000;
		config_file_set_value(pconfig, "ADDRESS_TABLE_SIZE", "3000");
	} else {
		table_size = atoi(str_value);
		if (table_size <= 0) {
			table_size = 3000;
			config_file_set_value(pconfig, "ADDRESS_TABLE_SIZE", "3000");
		}
	}
	printf("[system]: address table size is %d\n", table_size);
	str_value = config_file_get_value(pconfig, "ADDRESS_CACHE_INTERVAL");
	if (NULL == str_value) {
		cache_interval = 300;
		config_file_set_value(pconfig,
			"ADDRESS_CACHE_INTERVAL", "5minutes");
	} else {
		cache_interval = atoitvl(str_value);
		if (cache_interval > 24*3600 || cache_interval < 60) {
			cache_interval = 300;
			config_file_set_value(pconfig,
				"ADDRESS_CACHE_INTERVAL", "5minutes");
		}
	}
	itvltoa(cache_interval, temp_buff);
	printf("[system]: address book tree item"
		" cache interval is %s\n", temp_buff);
	str_value = config_file_get_value(pconfig, "ADDRESS_ITEM_NUM");
	if (NULL == str_value) {
		max_item_num = 100000;
		config_file_set_value(pconfig, "ADDRESS_ITEM_NUM", "100000");
	} else {
		max_item_num = atoi(str_value);
		if (max_item_num <= 0) {
			max_item_num = 100000;
			config_file_set_value(pconfig, "ADDRESS_ITEM_NUM", "100000");
		}
	}
	printf("[system]: maximum item number is %d\n", max_item_num);

	ab_tree_init(table_size, cache_interval, max_item_num);
	
	str_value = config_file_get_value(pconfig, "DAC_CONNECTION_NUM");
	if (NULL == str_value) {
		conn_num = 100;
		config_file_set_value(pconfig, "DAC_CONNECTION_NUM", "10");
	} else {
		conn_num = atoi(str_value);
		if (conn_num <= 0 || conn_num > 1000) {
			config_file_set_value(pconfig, "DAC_CONNECTION_NUM", "100");
			conn_num = 100;
		}
	}
	printf("[system]: dac connection number is %d\n", conn_num);
	
	dac_client_init(conn_num, dac_path);
	
	adac_processor_init();
	
	str_value = config_file_get_value(pconfig, "ADAC_THREADS_NUM");
	if (NULL == str_value) {
		config_file_set_value(pconfig, "ADAC_THREADS_NUM", "100");
		threads_num = 100;
	} else {
		threads_num = atoi(str_value);
		if (threads_num <= 0 || threads_num >= 1000) {
			config_file_set_value(pconfig, "ADAC_THREADS_NUM", "100");
			threads_num = 100;
		}
	}
	rpc_parser_init(threads_num);
	
	listener_init();
	
	config_file_save(pconfig);
	config_file_free(pconfig);
	
	if (0 != common_util_run()) {
		printf("[system]: fail to run common util\n");
		return -3;
	}
	
	if (0 != mysql_adaptor_run()) {
		common_util_stop();
		printf("[system]: fail to run mysql adaptor\n");
		return -4;
	}
	
	if (0 != ab_tree_run()) {
		mysql_adaptor_stop();
		common_util_stop();
		printf("[system]: fail to run ab tree\n");
		return -5;
	}
	
	if (0 != dac_client_run()) {
		ab_tree_stop();
		mysql_adaptor_stop();
		common_util_stop();
		printf("[system]: fail to run dac client\n");
		return -6;
	}
	
	if (0 != adac_processor_run()) {
		dac_client_stop();
		ab_tree_stop();
		mysql_adaptor_stop();
		common_util_stop();
		printf("[system]: fail to run adac processor\n");
		return -7;
	}
	
	if (0 != rpc_parser_run()) {
		adac_processor_stop();
		dac_client_stop();
		ab_tree_stop();
		mysql_adaptor_stop();
		common_util_stop();
		printf("[system]: fail to run rpc parser\n");
		return -8;
	}
	
	if (0 != listener_run()) {
		rpc_parser_stop();
		adac_processor_stop();
		dac_client_stop();
		ab_tree_stop();
		mysql_adaptor_stop();
		common_util_stop();
		printf("[system]: fail to run listener\n");
		return -9;
	}
	
	signal(SIGTERM, term_handler);
	printf("[system]: adac is now running\n");
	while (FALSE == g_notify_stop) {
		sleep(1);
	}
	listener_stop();
	rpc_parser_stop();
	adac_processor_stop();
	dac_client_stop();
	ab_tree_stop();
	mysql_adaptor_stop();
	common_util_stop();
	return 0;
}
