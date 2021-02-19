#include "as_common.h"
#include "list_file.h"
#include "config_file.h"
#include "util.h"
#include <stdio.h>

#define SPAM_STATISTIC_HEADER_FILTER		11

typedef struct _FIELD_NODE {
	DOUBLE_LIST_NODE node;
	int tag_len;
	char *tag;
	int value_len;
	char *value;
} FIELD_NODE;

typedef void (*SPAM_STATISTIC)(int);
typedef BOOL (*WHITELIST_QUERY)(char*);

static int header_filter(int context_ID, MAIL_ENTITY *pmail,
	CONNECTION *pconnection, char *reason, int length);

DECLARE_API;

static SPAM_STATISTIC spam_statistic;
static WHITELIST_QUERY ip_whitelist_query;
static WHITELIST_QUERY from_whitelist_query;
static WHITELIST_QUERY domain_whitelist_query;

static char g_return_string[1024];
static DOUBLE_LIST g_field_list;

int AS_LibMain(int reason, void **ppdata)
{
	char *pitem;
	int i, item_num;
	FIELD_NODE *pfnode;
	LIST_FILE *plist_file;
	DOUBLE_LIST_NODE *pnode;
	CONFIG_FILE *pconfig_file;
	char *str_value, *psearch;
	char file_name[256], temp_path[256];

    switch (reason) {
    case PLUGIN_INIT:
		LINK_API(ppdata);
		ip_whitelist_query = (WHITELIST_QUERY)
			query_service("ip_whitelist_query");
		if (NULL == ip_whitelist_query) {
			printf("[header_filter]: fail to get "
				"\"ip_whitelist_query\" service\n");
			return FALSE;
		}
		domain_whitelist_query = (WHITELIST_QUERY)
			query_service("domain_whitelist_query");
		if (NULL == domain_whitelist_query) {
			printf("[header_filter]: fail to get "
				"\"domain_whitelist_query\" service\n");
			return FALSE;
		}
		from_whitelist_query = (WHITELIST_QUERY)
			query_service("from_whitelist_query");
		if (NULL == from_whitelist_query) {
			printf("[header_filter]: fail to get "
				"\"from_whitelist_query\" service\n");
			return FALSE;
		}
		spam_statistic = (SPAM_STATISTIC)query_service("spam_statistic");
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		sprintf(temp_path, "%s/%s.cfg", get_config_path(), file_name);
		pconfig_file = config_file_init(temp_path);
		if (NULL == pconfig_file) {
			printf("[header_filter]: error to open config file!!!\n");
			return FALSE;
		}
		str_value = config_file_get_value(pconfig_file, "RETURN_STRING");
		if (NULL == str_value) {
			strcpy(g_return_string, "000011 your "
				"mail contains illegal header field");
		} else {
			strcpy(g_return_string, str_value);
		}
		printf("[header_filter]: return string is %s\n", g_return_string);
		config_file_free(pconfig_file);
		sprintf(temp_path, "%s/%s.txt", get_data_path(), file_name);
		plist_file = list_file_init(temp_path, "%s:128%s:128");
		if (NULL == plist_file) {
			printf("[header_filter]: error to open list file!!!\n");
			return FALSE;
		}
		pitem = list_file_get_list(plist_file);
		item_num = list_file_get_item_num(plist_file);
		for (i=0; i<item_num; i++) {
			pfnode = malloc(sizeof(FIELD_NODE));
			if (NULL == pfnode) {
				continue;
			}
			pfnode->node.pdata = pfnode;
			pfnode->tag_len = strlen(pitem + 256*i);
			pfnode->tag = malloc(pfnode->tag_len);
			if (NULL == pfnode->tag) {
				free(pfnode);
				continue;
			}
			memcpy(pfnode->tag, pitem + 256*i, pfnode->tag_len);
			if (0 == strcasecmp(pitem + 256*i + 128, "none")) {
				pfnode->value = NULL;
				pfnode->value_len = 0;
			} else {
				pfnode->value_len = strlen(pitem + 256*i + 128);
				pfnode->value = malloc(pfnode->value_len);
				if (NULL == pfnode->value) {
					free(pfnode->tag);
					free(pfnode);
					continue;
				}
				memcpy(pfnode->value, pitem + 256*i + 128, pfnode->value_len);
			}
			double_list_append_as_tail(&g_field_list, &pfnode->node);
		}
		list_file_free(plist_file);
        /* invoke register_auditor for registering auditor of mime head */
        if (FALSE == register_auditor(header_filter)) {
			printf("[header_filter]: fail to register auditor function\n");
            return FALSE;
        }
        return TRUE;
    case PLUGIN_FREE:
		while (pnode=double_list_get_from_head(&g_field_list)) {
			pfnode = (FIELD_NODE*)pnode->pdata;
			free(pfnode->tag);
			if (NULL != pfnode->value) {
				free(pfnode->value);
			}
			free(pfnode);
		}
		double_list_free(&g_field_list);
        return TRUE;
    }
    return TRUE;
}

static int header_filter(int context_ID, MAIL_ENTITY *pmail,
	CONNECTION *pconnection,  char *reason, int length)
{
	char buf[256];
	char *pdomain;
	FIELD_NODE *pfnode;
	int tag_len, val_len;
	DOUBLE_LIST_NODE *pnode;

	if (TRUE == pmail->penvelop->is_relay ||
		TRUE == pmail->penvelop->is_known) {
		return MESSAGE_ACCEPT;
	}
	pdomain = strchr(pmail->penvelop->from, '@');
	pdomain ++;
	if (TRUE == ip_whitelist_query(pconnection->client_ip) ||
		TRUE == domain_whitelist_query(pdomain) ||
		TRUE == from_whitelist_query(pmail->penvelop->from)) {
		return MESSAGE_ACCEPT;
	}
	while (MEM_END_OF_FILE != mem_file_read(
		&pmail->phead->f_others, &tag_len, sizeof(int))) {
		if (tag_len > 128) {
			mem_file_seek(&pmail->phead->f_others,
				MEM_FILE_READ_PTR, tag_len, MEM_FILE_SEEK_CUR);
		} else {
			mem_file_read(&pmail->phead->f_others, buf, tag_len);
			for (pnode=double_list_get_head(&g_field_list); NULL!=pnode;
				pnode=double_list_get_after(&g_field_list, pnode)) {
				pfnode = (FIELD_NODE*)pnode->pdata;
				if (pfnode->tag_len == tag_len) {
					if (0 == strncasecmp(buf, pfnode->tag, tag_len)) {
						if (0 != pfnode->value_len) {
							mem_file_read(&pmail->phead->f_others,
											&val_len, sizeof(int));
							if (val_len > 128 || val_len != pfnode->value_len) {
								return MESSAGE_ACCEPT;
							}
							mem_file_read(&pmail->phead->f_others, buf, val_len);
							if (0 != strncasecmp(buf, pfnode->value, val_len)) {
								return MESSAGE_ACCEPT;
							}
						}
						if (NULL != spam_statistic) {
							spam_statistic(SPAM_STATISTIC_HEADER_FILTER);
						}
						strncpy(reason, g_return_string, length);
						return MESSAGE_REJECT;
					}
				}
			}
		}
		mem_file_read(&pmail->phead->f_others, &val_len, sizeof(int));
		mem_file_seek(&pmail->phead->f_others,
			MEM_FILE_READ_PTR, val_len, MEM_FILE_SEEK_CUR);
	}
	return MESSAGE_ACCEPT;
}
