#include "as_common.h"
#include "util.h"
#include "list_file.h"
#include "config_file.h"
#include <stdio.h>
#include <pthread.h>

#define SPAM_STATISTIC_ATTACH_WILDCARD		2

typedef void (*SPAM_STATISTIC)(int);
typedef BOOL (*WHITELIST_QUERY)(char*);

static SPAM_STATISTIC spam_statistic;
static WHITELIST_QUERY ip_whitelist_query;
static WHITELIST_QUERY from_whitelist_query;
static WHITELIST_QUERY domain_whitelist_query;

DECLARE_API;

static int  attach_name_filter(int action, int context_ID,
	MAIL_BLOCK* mail_blk, char* reason, int length);

static void console_talk(int argc, char **argv, char *result, int length);

static BOOL extract_attachment_name(MEM_FILE *pmem_file, char *file_name);

static BOOL reload_list();

static BOOL *g_context_list;
static char g_list_path[256];
static char g_return_reason[1024];
static LIST_FILE *g_wildcard_list;
static pthread_rwlock_t g_list_lock;

int AS_LibMain(int reason, void **ppdata, char *path)
{
	char file_name[256];
	char temp_path[256];
	char *str_value, *psearch;
	CONFIG_FILE *pconfig_file;
	
    switch (reason) {
    case PLUGIN_INIT:
		LINK_API(ppdata);
        ip_whitelist_query = (WHITELIST_QUERY)
			query_service("ip_whitelist_query");
		if (NULL == ip_whitelist_query) {
			printf("[attach_wildcard]: fail to get"
				" \"ip_whitelist_query\" service\n");
			return FALSE;
		}
		from_whitelist_query = (WHITELIST_QUERY)
			query_service("from_whitelist_query");
		if (NULL == from_whitelist_query) {
			printf("[attach_wildcard]: fail to get"
				" \"from_whitelist_query\" service\n");
			return FALSE;
		}
		spam_statistic = (SPAM_STATISTIC)query_service("spam_statistic");
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		sprintf(g_list_path, "%s/%s.txt", get_data_path(), file_name);
		sprintf(temp_path, "%s/%s.cfg", get_config_path(), file_name);
		pconfig_file = config_file_init(temp_path);
		if (NULL == pconfig_file) {
			printf("[attach_wildcard]: error to open config file!!!\n");
			return FALSE;
		}
		str_value = config_file_get_value(pconfig_file, "RETURN_STRING");
		if (NULL == str_value) {
			strcpy(g_return_reason,
				"000002 attachment filename is not allowed");
		} else {
			strcpy(g_return_reason, str_value);
		}
		printf("[attach_wildcard]: return string is %s\n", g_return_reason);
		config_file_free(pconfig_file);
		pthread_rwlock_init(&g_list_lock, NULL);
		g_context_list = malloc(get_context_num()*sizeof(BOOL));
		if (NULL == g_context_list) {
			printf("[attach_wildcard]: fail to allocate context list memory\n");
			return FALSE;	
		}
		g_wildcard_list = NULL;
		if (FALSE == reload_list()) {
			printf("[attach_wildcard]: fail to load wildcard list\n");
			return FALSE;
		}
        /* invoke register_filter for registering all type of mime paragraph*/
        if (FALSE == register_filter(NULL , attach_name_filter)) {
			free(g_context_list);
			g_context_list = NULL;
			printf("[attach_wildcard]: fail to register filter function\n");
            return FALSE;
        }
		register_talk(console_talk);
        return TRUE;
    case PLUGIN_FREE:
		if (NULL != g_context_list) {
			free(g_context_list);
			g_context_list = NULL;
		}
		pthread_rwlock_destroy(&g_list_lock);
		if (NULL != g_wildcard_list) {
			list_file_free(g_wildcard_list);
			g_wildcard_list = NULL;
		}
        return TRUE;
	}
    return TRUE;
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

static int attach_name_filter(int action, int context_ID,
	MAIL_BLOCK* mail_blk, char* reason, int length)
{
	int i, num;
	char *pwildcards;
	char file_name[256];
	MAIL_ENTITY mail_entity;
	CONNECTION *pconnection;
    char attachment_name[1024];

    switch (action) {
    case ACTION_BLOCK_NEW:
		g_context_list[context_ID] = FALSE;
        return MESSAGE_ACCEPT;
    case ACTION_BLOCK_PROCESSING:
		if (TRUE == g_context_list[context_ID]) {
        	return MESSAGE_ACCEPT;
		}
		mail_entity = get_mail_entity(context_ID);
		if (TRUE == mail_entity.penvelop->is_relay ||
			TRUE == mail_entity.penvelop->is_known) {
			return MESSAGE_ACCEPT;
		}
		pconnection = get_connection(context_ID);
		if (TRUE == ip_whitelist_query(pconnection->client_ip) ||
			TRUE == from_whitelist_query(mail_entity.penvelop->from)) {
			g_context_list[context_ID] = TRUE;
			return MESSAGE_ACCEPT;
		}
        if (FALSE == extract_attachment_name(
			mail_blk->fp_mime_info, attachment_name)) {
			g_context_list[context_ID] = TRUE;
			return MESSAGE_ACCEPT;
		}

		pthread_rwlock_rdlock(&g_list_lock);
		pwildcards = list_file_get_list(g_wildcard_list);
		num = list_file_get_item_num(g_wildcard_list);
		for (i=0; i<num; i++) {
			if (0 != wildcard_match(attachment_name,
				pwildcards + 128*i, TRUE)) {
				pthread_rwlock_unlock(&g_list_lock);
				if (NULL!= spam_statistic) {
					spam_statistic(SPAM_STATISTIC_ATTACH_WILDCARD);
				}
				strncpy(reason, g_return_reason, length);
				return MESSAGE_REJECT;
			}
		}
		pthread_rwlock_unlock(&g_list_lock);

		g_context_list[context_ID] = TRUE;
		return MESSAGE_ACCEPT;

    case ACTION_BLOCK_FREE:
		g_context_list[context_ID] = FALSE;
        return MESSAGE_ACCEPT;
    }
    return MESSAGE_ACCEPT;
}


static BOOL extract_attachment_name(MEM_FILE *pmem_file, char *file_name)
{
	int temp_len;
	int tag_len, value_len;
	char tag[256], value[1024];
	char *ptr1, *ptr2;

	while (MEM_END_OF_FILE != mem_file_read(pmem_file, &tag_len,
			sizeof(int))) {
		if (tag_len > 255) {
			mem_file_seek(pmem_file, MEM_FILE_READ_PTR, tag_len,
						MEM_FILE_SEEK_CUR);
		} else {
			mem_file_read(pmem_file, tag, tag_len);
			tag[tag_len] = '\0';
		}
		mem_file_read(pmem_file, &value_len, sizeof(int));
		if (value_len > 1023) {
			mem_file_seek(pmem_file, MEM_FILE_READ_PTR, value_len,
						MEM_FILE_SEEK_CUR);
		} else {
			mem_file_read(pmem_file, value, value_len);
			value[value_len] = '\0';
        }
		if (tag_len <= 255 && value_len <= 1023) {
			if (0 == strcasecmp(tag, "Content-Type")) {
				ptr1 = search_string(value, "name=", value_len);
				if (NULL == ptr1) {
					continue;
				}
				ptr1 += 5;
				ptr2 = strchr(ptr1, ';');
				if (NULL == ptr2) {
					ptr2 = value + value_len;
				}
				temp_len = ptr2 - ptr1;
				memcpy(file_name, ptr1, temp_len);
				file_name[temp_len] = '\0';
				if ('"' == file_name[0]) {
					temp_len --;
					memcpy(file_name, file_name + 1, temp_len);
					file_name[temp_len] = '\0';
				}
				if ('"' == file_name[temp_len - 1]) {
					temp_len --;
					file_name[temp_len] = '\0';
				}
				ltrim_string(file_name);
				rtrim_string(file_name);
				temp_len = strlen(file_name);
				if (0 == strncasecmp(value, "application/zip", 15) &&
					0 != strcasecmp(file_name + temp_len - 4, ".zip")) {
					strcat(file_name, ".zip");
				}
				return TRUE;
				
			}
			if (0 == strcasecmp(tag, "Content-Disposition")) {
				ptr1 = search_string(value, "filename=", value_len);
				if (NULL == ptr1) {
					continue;
				}
				ptr1 += 9;
				ptr2 = strchr(ptr1, ';');
				if (NULL == ptr2) {
					ptr2 = value + value_len;
				}
				temp_len = ptr2 - ptr1;
				memcpy(file_name, ptr1, temp_len);
				file_name[temp_len] = '\0';
				if ('"' == file_name[0]) {
					temp_len --;
					memcpy(file_name, file_name + 1, temp_len);
					file_name[temp_len] = '\0';
				}
				if ('"' == file_name[temp_len - 1]) {
					temp_len --;
					file_name[temp_len] = '\0';
				}
				return TRUE;
			}
		}
    }
	return FALSE;
}

static void console_talk(int argc, char **argv, char *result, int length)
{
	char help_string[] = "250 attach wildcard help information:\r\n"
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
