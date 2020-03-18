#include "config_file.h"
#include <mysql/mysql.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#define USER_PRIVILEGE_SMTP								0x2

int main(int argc, char **argv)
{
	MYSQL *pmysql;
	int mysql_port;
	char *str_value;
	MYSQL_ROW myrow;
	char db_name[256];
	MYSQL_RES *pmyres;
	char *mysql_passwd;
	int privilege_bits;
	CONFIG_FILE *pconfig;
	char mysql_host[256];
	char mysql_user[256];
	char sql_string[1024];
	
	
	if (2 != argc) {
		printf("usage: %s address\n", argv[0]);
		exit(-1);
	}

	if (0 == strcmp(argv[1], "--help")) {
		printf("usage: %s address\n", argv[0]);
		exit(0);
	}
	
	pconfig = config_file_init("../config/athena.cfg");
	if (NULL == pconfig) {
		printf("fail to init config file\n");
		exit(-2);
	}

	str_value = config_file_get_value(pconfig, "MYSQL_HOST");
	if (NULL == str_value) {
		strcpy(mysql_host, "localhost");
	} else {
		strcpy(mysql_host, str_value);
	}
	
	str_value = config_file_get_value(pconfig, "MYSQL_PORT");
	if (NULL == str_value) {
		mysql_port = 3306;
	} else {
		mysql_port = atoi(str_value);
		if (mysql_port <= 0) {
			mysql_port = 3306;
		}
	}

	str_value = config_file_get_value(pconfig, "MYSQL_USERNAME");
	if (NULL == str_value) {
		mysql_user[0] = '\0';
	} else {
		strcpy(mysql_user, str_value);
	}

	mysql_passwd = config_file_get_value(pconfig, "MYSQL_PASSWORD");

	str_value = config_file_get_value(pconfig, "MYSQL_DBNAME");
	if (NULL == str_value) {
		strcpy(db_name, "email");
	} else {
		strcpy(db_name, str_value);
	}

	if (NULL == (pmysql = mysql_init(NULL))) {
		printf("fail to init mysql object\n");
		config_file_free(pconfig);
		exit(-3);
	}

	if (NULL == mysql_real_connect(pmysql, mysql_host, mysql_user,
		mysql_passwd, db_name, mysql_port, NULL, 0)) {
		mysql_close(pmysql);
		config_file_free(pconfig);
		printf("fail to connect database\n");
		exit(-4);
	}
	config_file_free(pconfig);
	
	sprintf(sql_string, "SELECT privilege_bits "
		"FROM users WHERE username='%s'", argv[1]);
	if (0 != mysql_query(pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pmysql))) {
		printf("fail to query database\n");
		mysql_close(pmysql);
		exit(-5);
	}
	if (1 != mysql_num_rows(pmyres)) {
		printf("cannot find information from database"
						" for username %s\n", argv[1]);
		mysql_free_result(pmyres);
		mysql_close(pmysql);
		exit(-6);
	}
	myrow = mysql_fetch_row(pmyres);
	privilege_bits = atoi(myrow[0]);
	mysql_free_result(pmyres);
	privilege_bits &= ~USER_PRIVILEGE_SMTP;
	sprintf(sql_string, "UPDATE users SET "
		"privilege_bits=%d WHERE username='%s'",
		privilege_bits, argv[1]);
	if (0 != mysql_query(pmysql, sql_string)) {
		printf("fail to query database\n");
		mysql_close(pmysql);
		exit(-7);
	}
	mysql_close(pmysql);
	printf("%s's privilege of smtp has been disabled\n", argv[1]);
	exit(0);
}

