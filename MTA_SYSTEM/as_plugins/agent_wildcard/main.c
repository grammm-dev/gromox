#include "as_common.h"
#include "config_file.h"
#include "list_file.h"
#include "util.h"
#include <pthread.h>
#include <stdio.h>

#define SPAM_STATISTIC_AGENT_WILDCARD		18

typedef void (*SPAM_STATISTIC)(int);
typedef BOOL (*WHITELIST_QUERY)(char*);

static SPAM_STATISTIC spam_statistic;
static WHITELIST_QUERY ip_whitelist_query;
static WHITELIST_QUERY from_whitelist_query;
static WHITELIST_QUERY domain_whitelist_query;

DECLARE_API;

static LIST_FILE *g_agent_list;
static char g_listfile_name[256];
static char g_return_string[1024];
static pthread_rwlock_t g_reload_lock;

static int head_wildcard(int context_ID, MAIL_ENTITY *pmail,
	CONNECTION *pconnection, char *reason, int length);

static void console_talk(int argc, char **argv, char *result, int length);

int AS_LibMain(int reason, void **ppdata)
{
	char *psearch;
	char *str_value;
	char file_name[256];
	char temp_path[256];
	CONFIG_FILE *pconfig_file;

    switch (reason) {
    case PLUGIN_INIT:
		LINK_API(ppdata);
		ip_whitelist_query = (WHITELIST_QUERY)
			query_service("ip_whitelist_query");
		if (NULL == ip_whitelist_query) {
			printf("[agent_wildcard]: fail to get "
				"\"ip_whitelist_query\" service\n");
			return FALSE;
		}
		domain_whitelist_query = (WHITELIST_QUERY)
			query_service("domain_whitelist_query");
		if (NULL == domain_whitelist_query) {
			printf("[agent_wildcard]: fail to get "
				"\"domain_whitelist_query\" service\n");
			return FALSE;
		}
		from_whitelist_query = (WHITELIST_QUERY)
			query_service("from_whitelist_query");
		if (NULL == from_whitelist_query) {
			printf("[agent_wildcard]: fail to get "
				"\"from_whitelist_query\" service\n");
			return FALSE;
		}
		pthread_rwlock_init(&g_reload_lock, NULL);
		spam_statistic = (SPAM_STATISTIC)query_service("spam_statistic");
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		sprintf(temp_path, "%s/%s.cfg", get_config_path(), file_name);
		pconfig_file = config_file_init(temp_path);
		if (NULL == pconfig_file) {
			printf("[agent_wildcard]: fail to open config file!!!\n");
			return FALSE;
		}
		str_value = config_file_get_value(pconfig_file, "RETURN_STRING");
		if (NULL == str_value) {
			strcpy(g_return_string, "000018 your mail"
				" client is forbidden by our system!");
		} else {
			strcpy(g_return_string, str_value);
		}
		printf("[agent_wildcard]: return string is %s\n", g_return_string);
		config_file_free(pconfig_file);
		sprintf(g_listfile_name, "%s/%s.txt", get_data_path(), file_name);
        g_agent_list = list_file_init(g_listfile_name, "%s:256");
        if (NULL == g_agent_list) {
            printf("[agent_wildcard]: fail to open list file!!!\n");
            return FALSE;
        }
        /* invoke register_auditor for registering auditor of mime head */
        if (FALSE == register_auditor(head_wildcard)) {
			printf("[agent_wildcard]: fail to "
				"register auditor function!!!\n");
            return FALSE;
        }
		register_talk(console_talk);
        return TRUE;
    case PLUGIN_FREE:
        if (NULL != g_agent_list) {
            list_file_free(g_agent_list);
        }
		pthread_rwlock_destroy(&g_reload_lock);
        return TRUE;
    }
    return TRUE;
}

static int head_wildcard(int context_ID, MAIL_ENTITY *pmail,
	CONNECTION *pconnection, char *reason, int length)
{
	char *pitem;
	char *pdomain;
	int i, item_num;
	char xmailer_buf[512];

	if (TRUE == pmail->penvelop->is_relay ||
		TRUE == pmail->penvelop->is_known) {
		return MESSAGE_ACCEPT;
	}
	if (TRUE == ip_whitelist_query(pconnection->client_ip)) {
		return MESSAGE_ACCEPT;
	}
	pdomain = strchr(pmail->penvelop->from, '@');
	pdomain ++;
	if (TRUE == domain_whitelist_query(pdomain)) {
		return MESSAGE_ACCEPT;
	}
	if (TRUE == from_whitelist_query(pmail->penvelop->from)) {
		return MESSAGE_ACCEPT;
	}
	if (mem_file_get_total_length(&pmail->phead->f_xmailer) >= 512) {
		return MESSAGE_ACCEPT;
	}
	if (MEM_END_OF_FILE == mem_file_read(
		&pmail->phead->f_xmailer, xmailer_buf, 512)) {
		return MESSAGE_ACCEPT;
	}
	pthread_rwlock_rdlock(&g_reload_lock);
	item_num = list_file_get_item_num(g_agent_list);
	pitem = list_file_get_list(g_agent_list);
	for (i=0; i<item_num; i++) {
		if (0 != wildcard_match(xmailer_buf, pitem + 256*i, TRUE)) {
			pthread_rwlock_unlock(&g_reload_lock);	
			strncpy(reason, g_return_string, length);
			if (NULL != spam_statistic) {
				spam_statistic(SPAM_STATISTIC_AGENT_WILDCARD);
			}
			return MESSAGE_REJECT;
		}
	}
	pthread_rwlock_unlock(&g_reload_lock);	
	return MESSAGE_ACCEPT;
}

static void console_talk(int argc, char **argv, char *result, int length)
{
	LIST_FILE *plist, *plist_temp;
	char help_string[] = "250 agent wildcard help information:\r\n"
			             "\t%s reload\r\n"
						 "\t    --reload the agent wildcard list from file";
	if (1 == argc) {
		strncpy(result, "550 too few arguments", length);
		return;
	}
	if (2 == argc && 0 == strcmp("--help", argv[1])) {
		snprintf(result, length, help_string, argv[0]);
		result[length - 1] ='\0';
		return;
	}
	if (2 == argc && 0 == strcmp(argv[1], "reload")) {
		plist = list_file_init(g_listfile_name, "%s:256");
		if (NULL == plist) {
			strncpy(result, "550 agent wildcard file error", length);
			return;
		}
		pthread_rwlock_wrlock(&g_reload_lock);
		plist_temp = g_agent_list;
		g_agent_list = plist;
		pthread_rwlock_unlock(&g_reload_lock);
		list_file_free(plist_temp);
		strncpy(result, "250 agent wildcard list reload OK", length);
		return;
	}
	snprintf(result, length, "550 invalid argument %s", argv[1]);
	return;
}
