#include "mysql_adaptor.h"
#include "double_list.h"
#include "mem_file.h"
#include "util.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <mysql/mysql.h>

#define SUB_TYPE_USER					0
#define SUB_TYPE_ROOM					1
#define SUB_TYPE_EQUIPMENT				2

#define MLIST_TYPE_NORMAL				0
#define MLIST_TYPE_GROUP				1
#define MLIST_TYPE_DOMAIN				2
#define MLIST_TYPE_CLASS				3

#define USER_PRIVILEGE_CHGPASSWD		0x4

#define SCAN_INTERVAL					60

typedef struct _CONNECTION_NODE {
	DOUBLE_LIST_NODE node;
	DOUBLE_LIST_NODE node_temp;
	MYSQL *pmysql;
} CONNECTION_NODE;

static int g_port;
static int g_timeout;
static int g_conn_num;
static char g_host[256];
static char g_user[256];
static char *g_password;
static char g_db_name[256];
static pthread_t g_thread_id;
static char g_password_buff[256];
static DOUBLE_LIST g_invalid_list;
static pthread_mutex_t g_list_lock;
static pthread_mutex_t g_crypt_lock;
static DOUBLE_LIST g_connection_list;


static BOOL g_notify_stop = TRUE;

static void *thread_work_func(void *param);

static void mysql_adaptor_encode_squote(const char *in, char *out);

void mysql_adaptor_init(int conn_num, const char *host, int port,
	const char *user, const char *password, const char *db_name,
	int timeout)
{
	g_notify_stop = TRUE;
	g_conn_num = conn_num;
	strcpy(g_host, host);
	g_port = port;
	g_timeout = timeout;
	strcpy(g_user, user);
	if (NULL == password || '\0' == password[0]) {
		g_password = NULL;
	} else {
		strcpy(g_password_buff, password);
		g_password = g_password_buff;
	}
	strcpy(g_db_name, db_name);
	double_list_init(&g_connection_list);
	double_list_init(&g_invalid_list);
	pthread_mutex_init(&g_list_lock, NULL);
	pthread_mutex_init(&g_crypt_lock, NULL);
}

int mysql_adaptor_run()
{
	int i;
	CONNECTION_NODE *pconnection;

	
	for (i=0; i<g_conn_num; i++) {
		pconnection = (CONNECTION_NODE*)malloc(sizeof(CONNECTION_NODE));
		if (NULL == pconnection) {
			continue;
		}
		pconnection->node.pdata = pconnection;
		pconnection->node_temp.pdata = pconnection;
		pconnection->pmysql = mysql_init(NULL);
		if (NULL != pconnection->pmysql) {
			if (g_timeout > 0) {
				mysql_options(pconnection->pmysql,
					MYSQL_OPT_READ_TIMEOUT, &g_timeout);
				mysql_options(pconnection->pmysql,
					MYSQL_OPT_WRITE_TIMEOUT, &g_timeout);
			}
			if (NULL != mysql_real_connect(pconnection->pmysql, g_host,
				g_user, g_password, g_db_name, g_port, NULL, 0)) {
				double_list_append_as_tail(&g_connection_list,
					&pconnection->node);
			} else {
				printf("[mysql_adaptor]: fail to connect to mysql server,"
						" reason: %s\n", mysql_error(pconnection->pmysql));
				mysql_close(pconnection->pmysql);
				pconnection->pmysql = NULL;
				double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			}
		} else {
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
		}
	}

	if (0 == double_list_get_nodes_num(&g_connection_list) &&
		0 == double_list_get_nodes_num(&g_invalid_list)) {
		printf("[mysql_adaptor]: fail to init connection list\n");
		return -1;
	}
	
	g_notify_stop = FALSE;
	if (0 != pthread_create(&g_thread_id, NULL, thread_work_func, NULL)) {
		g_notify_stop = TRUE;
		printf("[mysql_adaptor]: fail to create scanning thread\n");
		return -2;
	}

	return 0;

}

int mysql_adaptor_stop()
{
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;

	if (FALSE == g_notify_stop) {
		g_notify_stop = TRUE;
		pthread_join(g_thread_id, NULL);
	}
	while (pnode=double_list_get_from_head(&g_connection_list)) {
		pconnection = (CONNECTION_NODE*)pnode->pdata;
		if (NULL != pconnection->pmysql) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
		}
		free(pconnection);
	}
	while (pnode=double_list_get_from_head(&g_invalid_list)) {
		pconnection = (CONNECTION_NODE*)pnode->pdata;
		free(pconnection);
	}
	return 0;
}

void mysql_adaptor_free()
{
	double_list_free(&g_connection_list);
	double_list_free(&g_invalid_list);
	pthread_mutex_destroy(&g_list_lock);
	pthread_mutex_destroy(&g_crypt_lock);
}

BOOL mysql_adaptor_get_user_ids(const char *username,
	int *puser_id, int *pdomain_id, int *paddress_type)
{
	int i;
	char temp_name[512];
	char sql_string[1024];
	MYSQL_RES *pmyres;
	MYSQL_ROW myrow;
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* if no valid connection node available, return immediately */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}

	mysql_adaptor_encode_squote(username, temp_name);
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT id, domain_id, address_type,"
			" sub_type FROM users WHERE username='%s'", temp_name);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);
	
	if (1 != mysql_num_rows(pmyres)) {
		mysql_free_result(pmyres);
		return FALSE;	
	}
	myrow = mysql_fetch_row(pmyres);
	*puser_id = atoi(myrow[0]);
	*pdomain_id = atoi(myrow[1]);
	*paddress_type = atoi(myrow[2]);
	if (ADDRESS_TYPE_NORMAL == *paddress_type) {
		switch (atoi(myrow[3])) {
		case SUB_TYPE_ROOM:
			*paddress_type = ADDRESS_TYPE_ROOM;
			break;
		case SUB_TYPE_EQUIPMENT:
			*paddress_type = ADDRESS_TYPE_EQUIPMENT;
			break;
		}
	}
	mysql_free_result(pmyres);
	return TRUE;
}

BOOL mysql_adaptor_get_user_dir(const char *username, char *maildir)
{
	int i;
	char temp_name[512];
	char sql_string[1024];
	MYSQL_RES *pmyres;
	MYSQL_ROW myrow;
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* if no valid connection node available, return immediately */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}

	mysql_adaptor_encode_squote(username, temp_name);
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT maildir "
		"FROM users WHERE username='%s'", temp_name);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host,
			g_user, g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);
	
	if (1 != mysql_num_rows(pmyres)) {
		mysql_free_result(pmyres);
		return FALSE;	
	}
	myrow = mysql_fetch_row(pmyres);
	strcpy(maildir, myrow[0]);
	mysql_free_result(pmyres);
	return TRUE;
}

BOOL mysql_adaptor_get_domain_ids(const char *domainname,
	int *pdomain_id, int *porg_id)
{
	int i;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	MYSQL_RES *pmyres;
	char temp_name[512];
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}
	
	mysql_adaptor_encode_squote(domainname, temp_name);
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT id, org_id FROM "
			"domains WHERE domainname='%s'", temp_name);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);

	if (1 != mysql_num_rows(pmyres)) {
		mysql_free_result(pmyres);
		return FALSE;
	}
	
	myrow = mysql_fetch_row(pmyres);
	*pdomain_id = atoi(myrow[0]);
	*porg_id = atoi(myrow[1]);
	mysql_free_result(pmyres);
	return TRUE;
}

BOOL mysql_adaptor_get_org_id(int domain_id, int *porg_id)
{
	int i;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	MYSQL_RES *pmyres;
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT org_id "
		"FROM domains WHERE id=%d", domain_id);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);

	if (1 != mysql_num_rows(pmyres)) {
		mysql_free_result(pmyres);
		return FALSE;
	}
	
	myrow = mysql_fetch_row(pmyres);
	*porg_id = atoi(myrow[0]);
	mysql_free_result(pmyres);
	return TRUE;
}

BOOL mysql_adaptor_get_org_domains(int org_id, MEM_FILE *pfile)
{
	int i;
	int rows;
	int domain_id;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	MYSQL_RES *pmyres;
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024,
		"SELECT id FROM domains WHERE org_id=%d", org_id);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);

	rows = mysql_num_rows(pmyres);
	for (i=0; i<rows; i++) {
		myrow = mysql_fetch_row(pmyres);
		domain_id = atoi(myrow[0]);
		mem_file_write(pfile, &domain_id, sizeof(int));
	}
	mysql_free_result(pmyres);
	return TRUE;
}

BOOL mysql_adaptor_get_domain_info(int domain_id, char *name,
	char *homedir, char *title, char *address, int *pprivilege_bits)
{
	int i;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	MYSQL_RES *pmyres;
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT domainname, homedir, title,"
		" address, privilege_bits FROM domains WHERE id=%d", domain_id);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);

	if (1 != mysql_num_rows(pmyres)) {
		mysql_free_result(pmyres);
		return FALSE;
	}
	
	myrow = mysql_fetch_row(pmyres);
	strncpy(name, myrow[0], 256);
	strcpy(homedir, myrow[1]);
	strncpy(title, myrow[2], 1024);
	strncpy(address, myrow[3], 1024);
	*pprivilege_bits = atoi(myrow[4]);
	mysql_free_result(pmyres);
	return TRUE;
}

BOOL mysql_adaptor_get_domain_groups(int domain_id, MEM_FILE *pfile)
{
	int i;
	int rows;
	int temp_len;
	int group_id;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	MYSQL_RES *pmyres;
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT id, groupname, title "
		"FROM groups WHERE domain_id=%d", domain_id);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);

	rows = mysql_num_rows(pmyres);
	for (i=0; i<rows; i++) {
		myrow = mysql_fetch_row(pmyres);
		group_id = atoi(myrow[0]);
		mem_file_write(pfile, &group_id, sizeof(int));
		temp_len = strlen(myrow[1]);
		mem_file_write(pfile, &temp_len, sizeof(int));
		mem_file_write(pfile, myrow[1], temp_len);
		temp_len = strlen(myrow[2]);
		mem_file_write(pfile, &temp_len, sizeof(int));
		mem_file_write(pfile, myrow[2], temp_len);
	}
	mysql_free_result(pmyres);
	return TRUE;
}

BOOL mysql_adaptor_get_group_classes(int group_id, MEM_FILE *pfile)
{
	int i;
	int rows;
	int temp_len;
	int class_id;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	MYSQL_RES *pmyres;
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT child_id, classname FROM "
			"hierarchy INNER JOIN classes ON class_id=0 AND "
			"hierarchy.group_id=%d AND child_id=classes.id", group_id);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);

	rows = mysql_num_rows(pmyres);
	for (i=0; i<rows; i++) {
		myrow = mysql_fetch_row(pmyres);
		class_id = atoi(myrow[0]);
		mem_file_write(pfile, &class_id, sizeof(int));
		temp_len = strlen(myrow[1]);
		mem_file_write(pfile, &temp_len, sizeof(int));
		mem_file_write(pfile, myrow[1], temp_len);
	}
	mysql_free_result(pmyres);
	return TRUE;
}

BOOL mysql_adaptor_get_sub_classes(int class_id, MEM_FILE *pfile)
{
	int i;
	int rows;
	int temp_len;
	int child_id;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	MYSQL_RES *pmyres;
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT child_id, classname FROM "
		"hierarchy INNER JOIN classes ON class_id=%d AND "
		"child_id=classes.id", class_id);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);

	rows = mysql_num_rows(pmyres);
	for (i=0; i<rows; i++) {
		myrow = mysql_fetch_row(pmyres);
		child_id = atoi(myrow[0]);
		mem_file_write(pfile, &child_id, sizeof(int));
		temp_len = strlen(myrow[1]);
		mem_file_write(pfile, &temp_len, sizeof(int));
		mem_file_write(pfile, myrow[1], temp_len);
	}
	mysql_free_result(pmyres);
	return TRUE;
}

static BOOL mysql_adaptor_get_group_title(const char *groupname, char *title)
{
	int i;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	MYSQL_RES *pmyres;
	char temp_name[512];
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}
	
	mysql_adaptor_encode_squote(groupname, temp_name);
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT title FROM"
		" groups WHERE groupname='%s'", temp_name);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql,
				MYSQL_OPT_READ_TIMEOUT, &g_timeout);
			mysql_options(pconnection->pmysql,
				MYSQL_OPT_WRITE_TIMEOUT, &g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);

	if (1 != mysql_num_rows(pmyres)) {
		mysql_free_result(pmyres);
		return FALSE;
	}
	myrow = mysql_fetch_row(pmyres);
	strncpy(title, myrow[0], 1024);
	mysql_free_result(pmyres);
	return TRUE;
}

static BOOL mysql_adaptor_get_class_title(
	const char *listname, char *classname)
{
	int i;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	MYSQL_RES *pmyres;
	char temp_name[512];
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}
	
	mysql_adaptor_encode_squote(listname, temp_name);
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT classname FROM"
			" classes WHERE listname='%s'", temp_name);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql,
				MYSQL_OPT_READ_TIMEOUT, &g_timeout);
			mysql_options(pconnection->pmysql,
				MYSQL_OPT_WRITE_TIMEOUT, &g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);

	if (1 != mysql_num_rows(pmyres)) {
		mysql_free_result(pmyres);
		return FALSE;
	}
	myrow = mysql_fetch_row(pmyres);
	strncpy(classname, myrow[0], 1024);
	mysql_free_result(pmyres);
	return TRUE;
}

static BOOL mysql_adaptor_get_mlist_info(const char *listname,
	int *plist_type, int *plist_privilege, char *title)
{
	int i;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	MYSQL_RES *pmyres;
	char temp_name[512];
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}
	
	mysql_adaptor_encode_squote(listname, temp_name);
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT list_type, list_privilege"
					" FROM mlists WHERE listname='%s'", temp_name);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);

	if (1 != mysql_num_rows(pmyres)) {
		mysql_free_result(pmyres);
		return FALSE;
	}
	
	myrow = mysql_fetch_row(pmyres);
	*plist_type = atoi(myrow[0]);
	*plist_privilege = atoi(myrow[1]);
	mysql_free_result(pmyres);
	if (MLIST_TYPE_GROUP == *plist_type) {
		return mysql_adaptor_get_group_title(listname, title);
	} else if (MLIST_TYPE_CLASS == *plist_type) {
		return mysql_adaptor_get_class_title(listname, title);
	}
	return TRUE;
}

int mysql_adaptor_get_class_users(int class_id, MEM_FILE *pfile)
{
	int i;
	int rows;
	int count;
	int temp_id;
	int temp_len;
	char *ptoken;
	int list_type;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	char title[1024];
	int address_type;
	MYSQL_RES *pmyres;
	int privilege_bits;
	int list_privilege;
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return -1;
	}
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return -1;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT users.id, privilege_bits, "
		"users.username, real_name, title, memo, cell, tel, nickname,"
		" homeaddress, address_type, create_day, sub_type, maildir, "
		"lang FROM users INNER JOIN members ON members.class_id=%d AND"
		" members.username=users.username", class_id);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);
	
	count = 0;
	rows = mysql_num_rows(pmyres);
	for (i=0; i<rows; i++) {
		myrow = mysql_fetch_row(pmyres);
		address_type = atoi(myrow[10]);
		if (ADDRESS_TYPE_NORMAL == address_type) {
			switch (atoi(myrow[12])) {
			case SUB_TYPE_ROOM:
				address_type = ADDRESS_TYPE_ROOM;
				break;
			case SUB_TYPE_EQUIPMENT:
				address_type = ADDRESS_TYPE_EQUIPMENT;
				break;
			}
		}
		switch (address_type) {
		case ADDRESS_TYPE_NORMAL:
		case ADDRESS_TYPE_ALIAS:
		case ADDRESS_TYPE_ROOM:
		case ADDRESS_TYPE_EQUIPMENT:
			mem_file_write(pfile, &address_type, sizeof(int));
			temp_id = atoi(myrow[0]);
			mem_file_write(pfile, &temp_id, sizeof(int));
			temp_len = strlen(myrow[2]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[2], temp_len);
			temp_len = strlen(myrow[3]);
			if (0 == temp_len) {
				ptoken = strchr(myrow[2], '@');
				temp_len = ptoken - myrow[2];
				mem_file_write(pfile, &temp_len, sizeof(int));
				mem_file_write(pfile, myrow[2], temp_len);
			} else {
				mem_file_write(pfile, &temp_len, sizeof(int));
				mem_file_write(pfile, myrow[3], temp_len);
			}
			temp_len = strlen(myrow[4]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[4], temp_len);
			temp_len = strlen(myrow[5]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[5], temp_len);
			temp_len = strlen(myrow[6]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[6], temp_len);
			temp_len = strlen(myrow[7]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[7], temp_len);
			temp_len = strlen(myrow[8]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[8], temp_len);
			temp_len = strlen(myrow[9]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[9], temp_len);
			temp_len = strlen(myrow[11]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[11], temp_len);
			temp_len = strlen(myrow[13]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[13], temp_len);
			temp_len = strlen(myrow[14]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[14], temp_len);
			if (ADDRESS_TYPE_NORMAL == address_type ||
				ADDRESS_TYPE_ALIAS == address_type) {
				privilege_bits = atoi(myrow[1]);
				mem_file_write(pfile, &privilege_bits, sizeof(int));
			}
			count ++;
			break;
		case ADDRESS_TYPE_MLIST:
			temp_id = atoi(myrow[0]);
			if (FALSE == mysql_adaptor_get_mlist_info(myrow[2],
				&list_type, &list_privilege, title)) {
				mem_file_clear(pfile);
				mysql_free_result(pmyres);
				return -1;
			}
			mem_file_write(pfile, &address_type, sizeof(int));
			mem_file_write(pfile, &temp_id, sizeof(int));
			temp_len = strlen(myrow[2]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[2], temp_len);
			temp_len = strlen(myrow[11]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[11], temp_len);
			mem_file_write(pfile, &list_type, sizeof(int));
			mem_file_write(pfile, &list_privilege, sizeof(int));
			if (MLIST_TYPE_GROUP == list_type ||
				MLIST_TYPE_CLASS == list_type) {
				temp_len = strlen(title);
				mem_file_write(pfile, &temp_len, sizeof(int));
				mem_file_write(pfile, title, temp_len);
			}
			count ++;
			break;
		}
	}
	mysql_free_result(pmyres);
	return count;
}

int mysql_adaptor_get_group_users(int group_id, MEM_FILE *pfile)
{
	int i;
	int rows;
	int count;
	int temp_id;
	int temp_len;
	char *ptoken;
	int list_type;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	int address_type;
	char title[1024];
	MYSQL_RES *pmyres;
	int privilege_bits;
	int list_privilege;
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return -1;
	}
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return -1;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT id, privilege_bits, username, "
		"real_name, title, memo, cell, tel, nickname, homeaddress, "
		"address_type, create_day, sub_type, maildir, lang FROM users "
		"WHERE group_id=%d AND (SELECT count(*) AS num FROM members "
		"WHERE users.username=members.username)=0", group_id);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);
	
	count = 0;
	rows = mysql_num_rows(pmyres);
	for (i=0; i<rows; i++) {
		myrow = mysql_fetch_row(pmyres);
		address_type = atoi(myrow[10]);
		if (ADDRESS_TYPE_NORMAL == address_type) {
			switch (atoi(myrow[12])) {
			case SUB_TYPE_ROOM:
				address_type = ADDRESS_TYPE_ROOM;
				break;
			case SUB_TYPE_EQUIPMENT:
				address_type = ADDRESS_TYPE_EQUIPMENT;
				break;
			}
		}
		switch (address_type) {
		case ADDRESS_TYPE_NORMAL:
		case ADDRESS_TYPE_ALIAS:
		case ADDRESS_TYPE_ROOM:
		case ADDRESS_TYPE_EQUIPMENT:
			mem_file_write(pfile, &address_type, sizeof(int));
			temp_id = atoi(myrow[0]);
			mem_file_write(pfile, &temp_id, sizeof(int));
			temp_len = strlen(myrow[2]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[2], temp_len);
			temp_len = strlen(myrow[3]);
			if (0 == temp_len) {
				ptoken = strchr(myrow[2], '@');
				temp_len = ptoken - myrow[2];
				mem_file_write(pfile, &temp_len, sizeof(int));
				mem_file_write(pfile, myrow[2], temp_len);
			} else {
				mem_file_write(pfile, &temp_len, sizeof(int));
				mem_file_write(pfile, myrow[3], temp_len);
			}
			temp_len = strlen(myrow[4]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[4], temp_len);
			temp_len = strlen(myrow[5]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[5], temp_len);
			temp_len = strlen(myrow[6]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[6], temp_len);
			temp_len = strlen(myrow[7]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[7], temp_len);
			temp_len = strlen(myrow[8]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[8], temp_len);
			temp_len = strlen(myrow[9]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[9], temp_len);
			temp_len = strlen(myrow[11]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[11], temp_len);
			temp_len = strlen(myrow[13]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[13], temp_len);
			temp_len = strlen(myrow[14]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[14], temp_len);
			if (ADDRESS_TYPE_NORMAL == address_type ||
				ADDRESS_TYPE_ALIAS == address_type) {
				privilege_bits = atoi(myrow[1]);
				mem_file_write(pfile, &privilege_bits, sizeof(int));
			}
			count ++;
			break;
		case ADDRESS_TYPE_MLIST:
			temp_id = atoi(myrow[0]);
			if (FALSE == mysql_adaptor_get_mlist_info(myrow[2],
				&list_type, &list_privilege, title)) {
				mem_file_clear(pfile);
				mysql_free_result(pmyres);
				return -1;
			}
			mem_file_write(pfile, &address_type, sizeof(int));
			mem_file_write(pfile, &temp_id, sizeof(int));
			temp_len = strlen(myrow[2]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[2], temp_len);
			temp_len = strlen(myrow[11]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[11], temp_len);
			mem_file_write(pfile, &list_type, sizeof(int));
			mem_file_write(pfile, &list_privilege, sizeof(int));
			if (MLIST_TYPE_GROUP == list_type ||
				MLIST_TYPE_CLASS == list_type) {
				temp_len = strlen(title);
				mem_file_write(pfile, &temp_len, sizeof(int));
				mem_file_write(pfile, title, temp_len);
			}
			count ++;
			break;
		}
	}
	mysql_free_result(pmyres);
	return count;
}

int mysql_adaptor_get_domain_users(int domain_id, MEM_FILE *pfile)
{
	int i;
	int rows;
	int count;
	int temp_id;
	int temp_len;
	char *ptoken;
	int list_type;
	MYSQL *pmysql;
	MYSQL_ROW myrow;
	int address_type;
	char title[1024];
	MYSQL_RES *pmyres;
	int privilege_bits;
	int list_privilege;
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	/* 
	 * if no valid connection node available, it means the
	 * database is down, return immediately!!!
	 */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return -1;
	}
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return -1;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT id, privilege_bits, username,"
		" real_name, title, memo, cell, tel, nickname, homeaddress, "
		"address_type, create_day, sub_type, maildir, lang FROM users"
		" WHERE domain_id=%d and group_id=0", domain_id);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	pthread_mutex_lock(&g_list_lock);
	double_list_append_as_tail(&g_connection_list, &pconnection->node);
	pthread_mutex_unlock(&g_list_lock);
	
	count = 0;
	rows = mysql_num_rows(pmyres);
	for (i=0; i<rows; i++) {
		myrow = mysql_fetch_row(pmyres);
		address_type = atoi(myrow[10]);
		if (ADDRESS_TYPE_NORMAL == address_type) {
			switch (atoi(myrow[12])) {
			case SUB_TYPE_ROOM:
				address_type = ADDRESS_TYPE_ROOM;
				break;
			case SUB_TYPE_EQUIPMENT:
				address_type = ADDRESS_TYPE_EQUIPMENT;
				break;
			}
		}
		switch (address_type) {
		case ADDRESS_TYPE_NORMAL:
		case ADDRESS_TYPE_ALIAS:
		case ADDRESS_TYPE_ROOM:
		case ADDRESS_TYPE_EQUIPMENT:
			mem_file_write(pfile, &address_type, sizeof(int));
			temp_id = atoi(myrow[0]);
			mem_file_write(pfile, &temp_id, sizeof(int));
			temp_len = strlen(myrow[2]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[2], temp_len);
			temp_len = strlen(myrow[3]);
			if (0 == temp_len) {
				ptoken = strchr(myrow[2], '@');
				temp_len = ptoken - myrow[2];
				mem_file_write(pfile, &temp_len, sizeof(int));
				mem_file_write(pfile, myrow[2], temp_len);
			} else {
				mem_file_write(pfile, &temp_len, sizeof(int));
				mem_file_write(pfile, myrow[3], temp_len);
			}
			temp_len = strlen(myrow[4]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[4], temp_len);
			temp_len = strlen(myrow[5]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[5], temp_len);
			temp_len = strlen(myrow[6]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[6], temp_len);
			temp_len = strlen(myrow[7]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[7], temp_len);
			temp_len = strlen(myrow[8]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[8], temp_len);
			temp_len = strlen(myrow[9]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[9], temp_len);
			temp_len = strlen(myrow[11]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[11], temp_len);
			temp_len = strlen(myrow[13]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[13], temp_len);
			temp_len = strlen(myrow[14]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[14], temp_len);
			if (ADDRESS_TYPE_NORMAL == address_type ||
				ADDRESS_TYPE_ALIAS == address_type) {
				privilege_bits = atoi(myrow[1]);
				mem_file_write(pfile, &privilege_bits, sizeof(int));
			}
			count ++;
			break;
		case ADDRESS_TYPE_MLIST:
			temp_id = atoi(myrow[0]);
			if (FALSE == mysql_adaptor_get_mlist_info(myrow[2],
				&list_type, &list_privilege, title)) {
				mem_file_clear(pfile);
				mysql_free_result(pmyres);
				return -1;
			}
			mem_file_write(pfile, &address_type, sizeof(int));
			mem_file_write(pfile, &temp_id, sizeof(int));
			temp_len = strlen(myrow[2]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[2], temp_len);
			temp_len = strlen(myrow[11]);
			mem_file_write(pfile, &temp_len, sizeof(int));
			mem_file_write(pfile, myrow[11], temp_len);
			mem_file_write(pfile, &list_type, sizeof(int));
			mem_file_write(pfile, &list_privilege, sizeof(int));
			if (MLIST_TYPE_GROUP == list_type ||
				MLIST_TYPE_CLASS == list_type) {
				temp_len = strlen(title);
				mem_file_write(pfile, &temp_len, sizeof(int));
				mem_file_write(pfile, title, temp_len);
			}
			count ++;
			break;
		}
	}
	mysql_free_result(pmyres);
	return count;
}

static void* thread_work_func(void *arg)
{
	int i;
	CONNECTION_NODE *pconnection;
	DOUBLE_LIST_NODE *phead, *ptail, *pnode;
	DOUBLE_LIST temp_list;
	
	i = 0;
	double_list_init(&temp_list);
	while (FALSE == g_notify_stop) {
		if (i < SCAN_INTERVAL) {
			sleep(1);
			i ++;
			continue;
		}
		pthread_mutex_lock(&g_list_lock);
		phead = double_list_get_head(&g_invalid_list);
		ptail = double_list_get_tail(&g_invalid_list);
		pthread_mutex_unlock(&g_list_lock);
		for (pnode=phead; NULL!=pnode; pnode=double_list_get_after(
			&g_invalid_list, pnode)) {
			pconnection = (CONNECTION_NODE*)pnode->pdata;
			pconnection->pmysql = mysql_init(NULL);
			if (NULL != pconnection->pmysql) {
				if (g_timeout > 0) {
					mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
						&g_timeout);
					mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
						&g_timeout);
				}
				if (NULL != mysql_real_connect(pconnection->pmysql, g_host,
					g_user, g_password, g_db_name, g_port, NULL, 0)) {
					double_list_append_as_tail(&temp_list,
						&pconnection->node_temp);
				} else {
					mysql_close(pconnection->pmysql);
					pconnection->pmysql = NULL;
				}
			}
			if (pnode == ptail) {
				break;
			}
		}
		pthread_mutex_lock(&g_list_lock);
		while (pnode=double_list_get_from_head(&temp_list)) {
			pconnection = (CONNECTION_NODE*)pnode->pdata;
			double_list_remove(&g_invalid_list, &pconnection->node);
			double_list_append_as_tail(&g_connection_list, &pconnection->node);
		}
		pthread_mutex_unlock(&g_list_lock);
		i = 0;
	}
	double_list_free(&temp_list);
}

static BOOL mysql_adaptor_hierarchy_include(
	MYSQL *pmysql, const char *account, int class_id)
{
	int i, rows;
	int child_id;
	MYSQL_ROW myrow;
	MYSQL_RES *pmyres;
	char sql_string[512];
	
	snprintf(sql_string, 1024, "SELECT username FROM members WHERE"
		" class_id=%d AND username='%s'", class_id, account);
	if (0 != mysql_query(pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pmysql))) {
		return FALSE;
	}
	if (mysql_num_rows(pmyres) > 0) {
		mysql_free_result(pmyres);
		return TRUE;
	}
	mysql_free_result(pmyres);
	snprintf(sql_string, 1024, "SELECT child_id FROM"
			" hierarchy WHERE class_id=%d", class_id);
	if (0 != mysql_query(pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pmysql))) {
		return FALSE;
	}
	rows = mysql_num_rows(pmyres);
	for (i=0; i<rows; i++) {
		myrow = mysql_fetch_row(pmyres);
		child_id = atoi(myrow[0]);
		if (TRUE == mysql_adaptor_hierarchy_include(
			pmysql, account, child_id)) {
			mysql_free_result(pmyres);
			return TRUE;
		}
	}
	mysql_free_result(pmyres);
	return FALSE;
}

BOOL mysql_adaptor_check_mlist_include(
	const char *mlist_name, const char *account)
{
	int group_id;
	int class_id;
	int domain_id;
	BOOL b_result;
	int i, id, type;
	MYSQL_ROW myrow;
	MYSQL_RES *pmyres;
	char temp_name[512];
	char *pencode_domain;
	char temp_account[512];
	char sql_string[1024];
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	
	
	if (NULL == strchr(mlist_name, '@')) {
		return FALSE;
	}
	/* if no valid connection node available, return immediately */
	if (g_conn_num == double_list_get_nodes_num(&g_invalid_list)) {
		return FALSE;
	}
	mysql_adaptor_encode_squote(mlist_name, temp_name);
	pencode_domain = strchr(temp_name, '@') + 1;
	mysql_adaptor_encode_squote(account, temp_account);
	
	i = 0;
	
RETRYING:
	if (i > 3) {
		return FALSE;
	}
	pthread_mutex_lock(&g_list_lock);
	pnode = double_list_get_from_head(&g_connection_list);
	pthread_mutex_unlock(&g_list_lock);
	
	if (NULL == pnode) {
		i ++;
		sleep(1);
		goto RETRYING;
	}

	pconnection = (CONNECTION_NODE*)pnode->pdata;
	
	snprintf(sql_string, 1024, "SELECT id, list_type "
		"FROM mlists WHERE listname='%s'", temp_name);
	
	if (0 != mysql_query(pconnection->pmysql, sql_string) ||
		NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
		/* try to reconnect mysql database */
		mysql_close(pconnection->pmysql);
		pconnection->pmysql = mysql_init(NULL);
		if (NULL == pconnection->pmysql) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}

		if (g_timeout > 0) {
			mysql_options(pconnection->pmysql, MYSQL_OPT_READ_TIMEOUT,
				&g_timeout);
			mysql_options(pconnection->pmysql, MYSQL_OPT_WRITE_TIMEOUT,
				&g_timeout);
		}

		if (NULL == mysql_real_connect(pconnection->pmysql, g_host, g_user,
			g_password, g_db_name, g_port, NULL, 0) ||
			0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			i ++;
			sleep(1);
			goto RETRYING;
		}
	}
	
	if (1 != mysql_num_rows(pmyres)) {
		pthread_mutex_lock(&g_list_lock);
		double_list_append_as_tail(&g_connection_list, &pconnection->node);
		pthread_mutex_unlock(&g_list_lock);
		mysql_free_result(pmyres);
		return FALSE;
	}
	myrow = mysql_fetch_row(pmyres);
	
	id = atoi(myrow[0]);
	type = atoi(myrow[1]);
	mysql_free_result(pmyres);
	
	b_result = FALSE;
	switch (type) {
	case MLIST_TYPE_NORMAL:
		snprintf(sql_string, 1024, "SELECT username FROM associations"
			" WHERE list_id=%d AND username='%s'", id, temp_account);
		if (0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			return FALSE;
		}
		if (mysql_num_rows(pmyres) > 0) {
			b_result = TRUE;
		}
		mysql_free_result(pmyres);
		pthread_mutex_lock(&g_list_lock);
		double_list_append_as_tail(&g_connection_list, &pconnection->node);
		pthread_mutex_unlock(&g_list_lock);
		return b_result;
	case MLIST_TYPE_GROUP:
		snprintf(sql_string, 1024, "SELECT id FROM "
			"groups WHERE groupname='%s'", temp_name);
		if (0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			return FALSE;
		}
		if (1 != mysql_num_rows(pmyres)) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_connection_list,
				&pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			mysql_free_result(pmyres);
			return FALSE;
		}
		myrow = mysql_fetch_row(pmyres);
		group_id = atoi(myrow[0]);
		mysql_free_result(pmyres);
		
		snprintf(sql_string, 1024, "SELECT username FROM users WHERE"
			" group_id=%d AND username='%s'", group_id, temp_account);
		if (0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			return FALSE;
		}
		if (mysql_num_rows(pmyres) > 0) {
			b_result = TRUE;
		}
		mysql_free_result(pmyres);
		pthread_mutex_lock(&g_list_lock);
		double_list_append_as_tail(&g_connection_list, &pconnection->node);
		pthread_mutex_unlock(&g_list_lock);
		return b_result;
	case MLIST_TYPE_DOMAIN:
		snprintf(sql_string, 1024, "SELECT id FROM domains"
				" WHERE domainname='%s'", pencode_domain);
		if (0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			return FALSE;
		}
		if (1 != mysql_num_rows(pmyres)) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_connection_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			mysql_free_result(pmyres);
			return FALSE;
		}
		myrow = mysql_fetch_row(pmyres);
		domain_id = atoi(myrow[0]);
		mysql_free_result(pmyres);
		
		snprintf(sql_string, 1024, "SELECT username FROM users WHERE"
			" domain_id=%d AND username='%s'", domain_id, temp_account);
		if (0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			return FALSE;
		}
		if (mysql_num_rows(pmyres) > 0) {
			b_result = TRUE;
		}
		mysql_free_result(pmyres);
		pthread_mutex_lock(&g_list_lock);
		double_list_append_as_tail(&g_connection_list, &pconnection->node);
		pthread_mutex_unlock(&g_list_lock);
		return b_result;
	case MLIST_TYPE_CLASS:
		snprintf(sql_string, 1024, "SELECT id FROM "
			"classes WHERE listname='%s'", temp_name);
		if (0 != mysql_query(pconnection->pmysql, sql_string) ||
			NULL == (pmyres = mysql_store_result(pconnection->pmysql))) {
			mysql_close(pconnection->pmysql);
			pconnection->pmysql = NULL;
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_invalid_list, &pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			return FALSE;
		}
		if (1 != mysql_num_rows(pmyres)) {
			pthread_mutex_lock(&g_list_lock);
			double_list_append_as_tail(&g_connection_list,
				&pconnection->node);
			pthread_mutex_unlock(&g_list_lock);
			mysql_free_result(pmyres);
			return FALSE;		
		}
		myrow = mysql_fetch_row(pmyres);
		class_id = atoi(myrow[0]);
		mysql_free_result(pmyres);
		b_result = mysql_adaptor_hierarchy_include(
			pconnection->pmysql, temp_account, class_id);
		pthread_mutex_lock(&g_list_lock);
		double_list_append_as_tail(&g_connection_list, &pconnection->node);
		pthread_mutex_unlock(&g_list_lock);
		return b_result;
	default:
		pthread_mutex_lock(&g_list_lock);
		double_list_append_as_tail(&g_connection_list, &pconnection->node);
		pthread_mutex_unlock(&g_list_lock);
		return FALSE;
	}
}

static void mysql_adaptor_encode_squote(const char *in, char *out)
{
	int len, i, j;

	len = strlen(in);
	for (i=0, j=0; i<len; i++, j++) {
		if ('\'' == in[i] || '\\' == in[i]) {
			out[j] = '\\';
			j ++;
	}
		out[j] = in[i];
	}
	out[j] = '\0';
}
