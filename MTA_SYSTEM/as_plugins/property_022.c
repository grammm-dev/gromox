#include <stdbool.h>
#include "as_common.h"
#include "config_file.h"
#include "util.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>


#define SPAM_STATISTIC_PROPERTY_022		59


typedef void (*SPAM_STATISTIC)(int);

static SPAM_STATISTIC spam_statistic;

DECLARE_API;

static char g_return_reason[1024];

static int head_filter(int context_ID, MAIL_ENTITY *pmail,
	CONNECTION *pconnection, char *reason, int length);

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
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		sprintf(temp_path, "%s/%s.cfg", get_config_path(), file_name);
		pconfig_file = config_file_init(temp_path);
		if (NULL == pconfig_file) {
			printf("[property_022]: error to open config file!!!\n");
			return FALSE;
		}
		str_value = config_file_get_value(pconfig_file, "RETURN_STRING");
		if (NULL == str_value) {
			strcpy(g_return_reason, "000059 you are now sending spam mail!");
		} else {
			strcpy(g_return_reason, str_value);
		}
		printf("[property_022]: return string is %s\n", g_return_reason);
		config_file_free(pconfig_file);
		if (FALSE == register_auditor(head_filter)) {
			return FALSE;
		}
        return TRUE;
    case PLUGIN_FREE:
        return TRUE;
    case SYS_THREAD_CREATE:
        return TRUE;
        /* a pool thread is created */
    case SYS_THREAD_DESTROY:
        return TRUE;
    }
	return false;
}

static int head_filter(int context_ID, MAIL_ENTITY *pmail,
	CONNECTION *pconnection, char *reason, int length)
{
	int i;
	char *ptr;
	int tmp_len;
	char buff[1024];
	
	if (TRUE == pmail->penvelop->is_outbound ||
		TRUE == pmail->penvelop->is_relay) {
		return MESSAGE_ACCEPT;
	}
	tmp_len = mem_file_read(&pmail->phead->f_xmailer, buff, 1024);
	if (MEM_END_OF_FILE == tmp_len) {
		return MESSAGE_ACCEPT;
	}
	if (0 != strncasecmp(buff, "Microsoft Outlook ", 18)) {
		return MESSAGE_ACCEPT;
	}
	tmp_len = mem_file_read(&pmail->phead->f_content_type, buff, 1024);
	if (MEM_END_OF_FILE == tmp_len) {
		return MESSAGE_ACCEPT;
	}
	ptr = search_string(buff, "boundary=\"----=_000_", tmp_len);
	if (NULL == ptr) {
		return MESSAGE_ACCEPT;
	}
	ptr += 20;
	for (i=0; i<8; i++) {
		if (0 == isdigit(ptr[i])) {
			return MESSAGE_ACCEPT;
		}
	}
	ptr += 8;
	if (0 != strncmp(ptr, "_=----\"", 7)) {
		return MESSAGE_ACCEPT;
	}
	if (NULL != spam_statistic) {
		spam_statistic(SPAM_STATISTIC_PROPERTY_022);
	}
	strncpy(reason, g_return_reason, length);
	return MESSAGE_REJECT;	
}
