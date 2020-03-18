#include "as_common.h"
#include "config_file.h"
#include "util.h"
#include <stdio.h>

#define SPAM_STATISTIC_OUTMAIL_FREQUENCY			14


typedef void (*SPAM_STATISTIC)(int);
typedef void (*DISABLE_SMTP)(const char*);
typedef BOOL (*WHITELIST_QUERY)(const char*);
typedef BOOL (*OUTMAIL_FREQUENCY_AUDIT)(const char*);

DECLARE_API;

static DISABLE_SMTP disable_smtp;
static SPAM_STATISTIC spam_statistic;
static WHITELIST_QUERY from_whitelist_query;
static OUTMAIL_FREQUENCY_AUDIT outmail_frequency_audit;

static char g_edm_script[256];
static char g_return_string_1[1024];
static char g_return_string_2[1024];

static BOOL (*check_virtual)(const char *username,
	const char *from, BOOL *pb_expanded, MEM_FILE *pfile);

static int mail_statistic(int context_ID, MAIL_WHOLE *pmail,
    CONNECTION *pconnection, char *reason, int length);

static BOOL (*fcgi_rpc)(const uint8_t *pbuff_in, uint32_t in_len,
	uint8_t **ppbuff_out, uint32_t *pout_len, const char *script_path);


BOOL AS_LibMain(int reason, void **ppdata)
{
	char *psearch;
	char *str_value;
	char temp_buff[64];
	char file_name[256];
	char temp_path[256];
	CONFIG_FILE *pconfig_file;
	
    /* path conatins the config files directory */
    switch (reason) {
    case PLUGIN_INIT:
		LINK_API(ppdata);
		fcgi_rpc = query_service("fcgi_rpc");
        if (NULL == fcgi_rpc) {
			printf("[outmail_frequency]: fail to "
					"get \"fcgi_rpc\" service\n");
			return FALSE;
        }
		disable_smtp = (DISABLE_SMTP)query_service("disable_smtp");
		if (NULL == disable_smtp) {
			printf("[outmail_frequency]: fail to"
				" get \"disable_smtp\" service\n");
			return FALSE;
		}
		check_virtual = query_service("check_virtual_mailbox");
		if (NULL == check_virtual) {
			printf("[outmail_frequency]: fail to get "
				"\"check_virtual_mailbox\" service\n");
			return FALSE;
		}
		outmail_frequency_audit = (OUTMAIL_FREQUENCY_AUDIT)
					query_service("outmail_frequency_audit");
		if (NULL == outmail_frequency_audit) {
			printf("[outmail_frequency]: fail to get "
				"\"outmail_frequency_audit\" service\n");
			return FALSE;
		}
		spam_statistic = (SPAM_STATISTIC)query_service("spam_statistic");
		from_whitelist_query = (WHITELIST_QUERY)
			query_service("from_whitelist_query");
		if (NULL == from_whitelist_query) {
			printf("[outmail_frequency]: fail to get "
				"\"from_whitelist_query\" service\n");
			return FALSE;
		}
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		sprintf(temp_path, "%s/%s.cfg", get_config_path(), file_name);
		pconfig_file = config_file_init(temp_path);
		if (NULL == pconfig_file) {
			printf("[outmail_frequency]: error to open config file!!!\n");
			return FALSE;
		}
		str_value = config_file_get_value(pconfig_file, "EDM_SCRIPT_PATH");
		if (NULL == str_value) {
			config_file_free(pconfig_file);
			printf("[outmail_frequency]: fail to get "
				"EDM_SCRIPT_PATH from config file\n");
			return FALSE;
		}
		strcpy(g_edm_script, str_value);
		printf("[outmail_frequency]: edm script path is %s\n", g_edm_script);
		str_value = config_file_get_value(pconfig_file, "RETURN_STRING_1");
		if (NULL == str_value) {
			strcpy(g_return_string_1, "000014 this account has "
				"sent too many mails, will be blocked for a while");
		} else {
			strcpy(g_return_string_1, str_value);
		}
		printf("[outmail_frequency]: return string 1 is %s\n",
				g_return_string_1);
		str_value = config_file_get_value(pconfig_file, "RETURN_STRING_2");
		if (NULL == str_value) {
			strcpy(g_return_string_2, "mail from "
				"address and account name differ");
		} else {
			strcpy(g_return_string_2, str_value);
		}
		printf("[outmail_frequency]: return "
			"string 2 is %s\n", g_return_string_2);
		if (FALSE == config_file_save(pconfig_file)) {
			printf("[outmail_frequency]: fail to save config file\n");
			config_file_free(pconfig_file);
			return FALSE;
		}
		config_file_free(pconfig_file);
        /* invoke register_statistic for registering statistic of mail */
        if (FALSE == register_statistic(mail_statistic)) {
            return FALSE;
        }
        return TRUE;
    case PLUGIN_FREE:
        return TRUE;
    }
    return TRUE;
}

static int mail_statistic(int context_ID, MAIL_WHOLE *pmail,
    CONNECTION *pconnection, char *reason, int length)
{
	BOOL b_expanded;
	uint32_t out_len;
	MEM_FILE tmp_file;
	uint8_t *pbuff_out;
	char rcpt_buff[256];
	const char *psrc_domain;
	const char *pdst_domain;
	
	
	if (TRUE == pmail->penvelop->is_relay) {
		return MESSAGE_ACCEPT;
	}
	if (FALSE == pmail->penvelop->is_outbound) {
		return MESSAGE_ACCEPT;
	}
	if (0 == strcmp(pmail->penvelop->from, "none@none")) {
		return MESSAGE_ACCEPT;
	}
	
	if (TRUE == pmail->penvelop->is_login && 0 != strcasecmp(
		pmail->penvelop->username, pmail->penvelop->from)) {
		mem_file_init(&tmp_file, pmail->penvelop->f_rcpt_to.allocator);
		check_virtual(pmail->penvelop->username,
			pmail->penvelop->from, &b_expanded, &tmp_file);
		if (TRUE != b_expanded) {
			mem_file_free(&tmp_file);
			strncpy(reason, g_return_string_2, length);
			return MESSAGE_REJECT;
		}
		while (MEM_END_OF_FILE != mem_file_readline(
			&tmp_file, rcpt_buff, 256)) {
			if (0 == strcasecmp(rcpt_buff, pmail->penvelop->username)) {
				mem_file_free(&tmp_file);
				goto CHECK_FREQUENCY;
			}
		}
		mem_file_free(&tmp_file);
		strncpy(reason, g_return_string_2, length);
		return MESSAGE_REJECT;
	}
	
CHECK_FREQUENCY:
	if (TRUE == from_whitelist_query(pmail->penvelop->username)) {
		return MESSAGE_ACCEPT;
	}
	if (TRUE == from_whitelist_query(pmail->penvelop->from)) {
		return MESSAGE_ACCEPT;
	}
	psrc_domain = strchr(pmail->penvelop->from, '@') + 1;
	while (MEM_END_OF_FILE != mem_file_readline(
		&pmail->penvelop->f_rcpt_to, rcpt_buff, 256)) {
		pdst_domain = strchr(rcpt_buff, '@') + 1;
		if (0 == strcasecmp(pdst_domain, psrc_domain)) {
			continue;
		}
		if (FALSE == outmail_frequency_audit(pmail->penvelop->username)) {
			disable_smtp(pmail->penvelop->username);
			if (TRUE == fcgi_rpc(pmail->penvelop->username,
				strlen(pmail->penvelop->username) + 1,
				&pbuff_out, &out_len, g_edm_script)) {
				free(pbuff_out);	
			}
			if (NULL!= spam_statistic) {
				spam_statistic(SPAM_STATISTIC_OUTMAIL_FREQUENCY);
			}
			strncpy(reason, g_return_string_1, length);
			return MESSAGE_REJECT;
		 }
	}
	return MESSAGE_ACCEPT;
}
