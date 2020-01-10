#include <errno.h>
#include <string.h>
#include <gromox/as_common.h>
#include "config_file.h"
#include "mail_func.h"
#include <stdio.h>

#define SPAM_STATISTIC_INBOUND_AUTH			6

typedef void (*SPAM_STATISTIC)(int);
typedef BOOL (*WHITELIST_QUERY)(char*);

static SPAM_STATISTIC		spam_statistic;
static WHITELIST_QUERY      ip_whitelist_query;
static WHITELIST_QUERY      domain_whitelist_query;

DECLARE_API;

static int envelop_judge(int context_ID, ENVELOP_INFO *penvelop,
	CONNECTION *pconnection, char *reason, int length);
	
static char g_return_reason[1024];

BOOL AS_LibMain(int reason, void **ppdata)
{
	CONFIG_FILE *pconfig_file;
	char file_name[256], temp_path[256];
	char *str_value, *psearch;
	
    /* path conatins the config files directory */
    switch (reason) {
    case PLUGIN_INIT:
		LINK_API(ppdata);
		spam_statistic = (SPAM_STATISTIC)query_service("spam_statistic");
		ip_whitelist_query = (WHITELIST_QUERY)query_service(
								"ip_whitelist_query");
		if (NULL == ip_whitelist_query) {
			printf("[inbound_auth]: failed to get service \"ip_whitelist_query\"\n");
			return FALSE;
		}
		domain_whitelist_query = (WHITELIST_QUERY)query_service(
									"domain_whitelist_query");
		if (NULL == domain_whitelist_query) {
			printf("[inbound_auth]: failed to get service \"domain_whitelist_query\"\n");
			return FALSE;
		}
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		sprintf(temp_path, "%s/%s.cfg", get_config_path(), file_name);
		pconfig_file = config_file_init2(NULL, temp_path);
		if (NULL == pconfig_file) {
			printf("[inbound_auth]: config_file_init %s: %s\n", temp_path, strerror(errno));
			return FALSE;
		}
		str_value = config_file_get_value(pconfig_file, "RETURN_STRING");
		if (NULL == str_value) {
			strcpy(g_return_reason, "000006 authentification needed");
		} else {
			strcpy(g_return_reason, str_value);
		}
		printf("[inbound_auth]: return string is \"%s\"\n", g_return_reason);
		config_file_free(pconfig_file);
        /* invoke register_judge for registering judge of mail envelop */
        if (FALSE == register_judge(envelop_judge)) {
			printf("[inbound_auth]: failed to register judge function\n");
            return FALSE;
        }
        return TRUE;
    case PLUGIN_FREE:
        return TRUE;
    }
	return TRUE;
}

static int envelop_judge(int context_ID, ENVELOP_INFO *penvelop,
	CONNECTION *pconnection, char *reason, int length)
{
	char *pdomain;
	char *from_domain;
	char rcpt_to[256];
	
	if (TRUE == penvelop->is_outbound || TRUE == penvelop->is_relay) {
		return MESSAGE_ACCEPT;
	}
	if (TRUE == ip_whitelist_query(pconnection->client_ip)) {
		return MESSAGE_ACCEPT;
	}
	from_domain = strchr(penvelop->from, '@') + 1;
	if (TRUE == domain_whitelist_query(from_domain)) {
		return MESSAGE_ACCEPT;
	}
	mem_file_readline(&penvelop->f_rcpt_to, rcpt_to, 256);
	pdomain = strchr(rcpt_to, '@') + 1;
	if (0 == strcasecmp(from_domain, pdomain)) {
		if (NULL != spam_statistic) {
			spam_statistic(SPAM_STATISTIC_INBOUND_AUTH);
		}
		snprintf(reason, length, g_return_reason);
		return MESSAGE_REJECT;
	}
	return MESSAGE_ACCEPT;
}

