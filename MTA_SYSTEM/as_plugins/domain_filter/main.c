#include "as_common.h"
#include "list_file.h"
#include "mail_func.h"
#include "config_file.h"
#include <stdio.h>

#define SPAM_STATISTIC_DOMAIN_FILTER		6

typedef BOOL (*DOMAIN_FILTER_QUERY)(char*);
typedef void (*SPAM_STATISTIC)(int);

static DOMAIN_FILTER_QUERY domain_filter_query;
static SPAM_STATISTIC spam_statistic;

DECLARE_API;

static BOOL reload_list();

static int envelop_judge(int context_ID, ENVELOP_INFO *penvelop,
	CONNECTION *pconnection, char *reason, int length);

static void console_talk(int argc, char **argv, char *result, int length);

static char g_list_path[256];
static char g_return_reason[1024];
static LIST_FILE *g_wildcard_list;
static pthread_rwlock_t g_list_lock;

BOOL AS_LibMain(int reason, void **ppdata)
{
	CONFIG_FILE *pconfig_file;
	char file_name[256], temp_path[256];
	char *str_value, *psearch;
	
    /* path conatins the config files directory */
    switch (reason) {
    case PLUGIN_INIT:
		LINK_API(ppdata);
		domain_filter_query = (DOMAIN_FILTER_QUERY)query_service(
							  "domain_filter_query");
		if (NULL == domain_filter_query) {
			printf("[domain_filter]: fail to get \"domain_filter_query\" "
					"service\n");
			return FALSE;
		}
		spam_statistic = (SPAM_STATISTIC)query_service("spam_statistic");
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		sprintf(g_list_path, "%s/domain_wildcard.txt", get_data_path());
		sprintf(temp_path, "%s/%s.cfg", get_config_path(), file_name);
		pconfig_file = config_file_init(temp_path);
		if (NULL == pconfig_file) {
			printf("[domain_filter]: error to open config file!!!\n");
			return FALSE;
		}
		str_value = config_file_get_value(pconfig_file, "RETURN_STRING");
		if (NULL == str_value) {
			strcpy(g_return_reason, "000006 domain %s is forbidden");
		} else {
			strcpy(g_return_reason, str_value);
		}
		printf("[domain_filter]: return string is %s\n", g_return_reason);
		config_file_free(pconfig_file);
		pthread_rwlock_init(&g_list_lock, NULL);
		g_wildcard_list = NULL;
		if (FALSE == reload_list()) {
			printf("[domain_filter]: fail to load wildcard list\n");
			return FALSE;
		}
        /* invoke register_judge for registering judge of mail envelop */
        if (FALSE == register_judge(envelop_judge)) {
			printf("[domain_filter]: fail to register judge function!!!\n");
            return FALSE;
        }
		register_talk(console_talk);
        return TRUE;
    case PLUGIN_FREE:
		pthread_rwlock_destroy(&g_list_lock);
		if (NULL != g_wildcard_list) {
			list_file_free(g_wildcard_list);
			g_wildcard_list = NULL;
		}
        return TRUE;
    }
	return TRUE;
}

static int envelop_judge(int context_ID, ENVELOP_INFO *penvelop,
	CONNECTION *pconnection, char *reason, int length)
{
	int i, num;
	char *pdomain;
	char *pwildcards;
	
	if (TRUE == penvelop->is_outbound ||
		TRUE == penvelop->is_relay || 
		TRUE == penvelop->is_known) {
		return MESSAGE_ACCEPT;
	}
	pdomain = strchr(penvelop->from, '@');
	if (NULL == pdomain) {
		return MESSAGE_ACCEPT;
	}
	pdomain ++;
	if (TRUE == domain_filter_query(pdomain)) {
		if (NULL != spam_statistic) {
			spam_statistic(SPAM_STATISTIC_DOMAIN_FILTER);
		}
		snprintf(reason, length, g_return_reason, pdomain);
		return MESSAGE_REJECT;
	}
	pthread_rwlock_rdlock(&g_list_lock);
	pwildcards = list_file_get_list(g_wildcard_list);
	num = list_file_get_item_num(g_wildcard_list);
	for (i=0; i<num; i++) {
		if (0 != wildcard_match(pdomain, pwildcards + 128*i, TRUE)) {
			pthread_rwlock_unlock(&g_list_lock);
			if (NULL != spam_statistic) {
				spam_statistic(SPAM_STATISTIC_DOMAIN_FILTER);
			}
			snprintf(reason, length, g_return_reason, pdomain);
			return MESSAGE_REJECT;
		}
	}
	pthread_rwlock_unlock(&g_list_lock);
	return MESSAGE_ACCEPT;
}

static BOOL reload_list()
{
	LIST_FILE *pfile;
	LIST_FILE *pfile_tmp;

	pfile = list_file_init(g_list_path, "%s:128");
	if (NULL == pfile) {
		return FALSE;
	}
	pthread_rwlock_wrlock(&g_list_lock);
	pfile_tmp = g_wildcard_list;
	g_wildcard_list = pfile;
	pthread_rwlock_unlock(&g_list_lock);

	if (NULL != pfile_tmp) {
		list_file_free(pfile_tmp);
	}
	return TRUE;
}

static void console_talk(int argc, char **argv, char *result, int length)
{
	char help_string[] = "250 domain filter help information:\r\n"
						 "\t%s reload\r\n"
						 "\t    --reload the wildcard list from file";
	
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
		if (TRUE == reload_list()) {
			strncpy(result, "250 wildcard list reload OK", length);
			return;
		} else {
			strncpy(result, "550 fail to reload wildcard list", length);
			return;
		}
	}
	
	snprintf(result, length, "550 invalid argument %s", argv[1]);
	return;
}
