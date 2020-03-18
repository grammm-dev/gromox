#include "as_common.h"
#include "mail_func.h"
#include "ext_buffer.h"
#include "config_file.h"
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>

#define SPAM_BIT						0x80
#define DISABLE_BIT						0x40
#define REVERSE_BIT						0x40
#define RETRY_BIT						0x20

#define MARK_SPAM						0x60


typedef void (*SPAM_STATISTIC)(int);
typedef BOOL (*WHITELIST_QUERY)(char*);
typedef void (*DISABLE_SMTP)(const char*);
typedef BOOL (*CHECK_RETRYING)(const char*, const char*, MEM_FILE*);

DECLARE_API;

static char g_as_script[256];
static char g_edm_script[256];
static char g_return_string[256];
static DISABLE_SMTP disable_smtp;
static CHECK_RETRYING check_retrying;
static SPAM_STATISTIC spam_statistic;
static WHITELIST_QUERY ip_whitelist_query;
static WHITELIST_QUERY from_whitelist_query;
static WHITELIST_QUERY domain_whitelist_query;

static BOOL (*fcgi_rpc)(const uint8_t *pbuff_in, uint32_t in_len,
	uint8_t **ppbuff_out, uint32_t *pout_len, const char *script_path);

static int paragraph_filter(int action, int context_ID,
	MAIL_BLOCK *pblock, char *reason, int length);

BOOL AS_LibMain(int reason, void **ppdata)
{
	char file_name[256];
	char temp_path[256];
	CONFIG_FILE *pconfig;
	char *str_value, *psearch;
	
	switch (reason) {
	case PLUGIN_INIT:
		LINK_API(ppdata);
		fcgi_rpc = query_service("fcgi_rpc");
        if (NULL == fcgi_rpc) {
			printf("[php_as]: fail to get \"fcgi_rpc\" service\n");
			return FALSE;
        }
		disable_smtp = (DISABLE_SMTP)query_service("disable_smtp");
		if (NULL == disable_smtp) {
			printf("[php_as]: fail to get \"disable_smtp\" service\n");
			return FALSE;
		}
		check_retrying = (CHECK_RETRYING)query_service("check_retrying");
		if (NULL == check_retrying) {
			printf("[php_as]: fail to get \"check_retrying\" service\n");
			return FALSE;
		}
		ip_whitelist_query = (WHITELIST_QUERY)
			query_service("ip_whitelist_query");
		if (NULL == ip_whitelist_query) {
			printf("[php_as]: fail to get \"ip_whitelist_query\" service\n");
			return FALSE;
		}
		from_whitelist_query = (WHITELIST_QUERY)
			query_service("from_whitelist_query");
		if (NULL == from_whitelist_query) {
			printf("[php_as]: fail to get "
				"\"from_whitelist_query\" service\n");
			return FALSE;
		}
		domain_whitelist_query = (WHITELIST_QUERY)
			query_service("domain_whitelist_query");
		if (NULL == domain_whitelist_query) {
			printf("[php_as]: fail to get "
				"\"domain_whitelist_query\" service\n");
			return FALSE;
		}
		spam_statistic = (SPAM_STATISTIC)query_service("spam_statistic");
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		sprintf(temp_path, "%s/%s.cfg", get_config_path(), file_name);
		pconfig = config_file_init(temp_path);
		if (NULL == pconfig) {
			printf("[php_as]: error to open config file!!!\n");
			return FALSE;
		}
		str_value = config_file_get_value(pconfig, "AS_SCRIPT_PATH");
		if (NULL == str_value) {
			config_file_free(pconfig);
			printf("[php_as]: fail to get AS_SCRIPT_PATH from config file\n");
			return FALSE;
		}
		strcpy(g_as_script, str_value);
		printf("[php_as]: as script path is %s\n", g_as_script);
		str_value = config_file_get_value(pconfig, "EDM_SCRIPT_PATH");
		if (NULL == str_value) {
			config_file_free(pconfig);
			printf("[php_as]: fail to get EDM_SCRIPT_PATH from config file\n");
			return FALSE;
		}
		strcpy(g_edm_script, str_value);
		printf("[php_as]: edm script path is %s\n", g_edm_script);
		str_value = config_file_get_value(pconfig, "RETURN_STRING");
		if (NULL == str_value) {
			strcpy(g_return_string, "%06d you are now sending spam mail!");
		} else {
			strcpy(g_return_string, str_value);
		}
		printf("[php_as]: return string is %s\n", g_return_string);
		config_file_free(pconfig);
		if (FALSE == register_filter("text/plain", paragraph_filter) ||
			FALSE == register_filter("text/html", paragraph_filter)) {
			return FALSE;
		}
		return TRUE;
	case PLUGIN_FREE:
		return TRUE;
	case SYS_THREAD_CREATE:
		return TRUE;
	case SYS_THREAD_DESTROY:
		return TRUE;
	}
}

static int paragraph_filter(int action, int context_ID,
	MAIL_BLOCK *pblock, char *reason, int length)
{
	int tmp_len;
	int h_errnop;
	char *pdomain;
	int rcpt_count;
	in_addr_t addr;
	char buff[4096];
	uint32_t out_len;
	EXT_PUSH push_ctx;
	uint8_t *pbuff_out;
	uint8_t result_mask;
	uint16_t result_code;
	CONFIG_FILE *pconfig;
	char tmp_buff[64*1024];
	MAIL_ENTITY mail_entity;
	CONNECTION *pconnection;
	struct hostent hostinfo, *phost;
	
	switch (action) {
	case ACTION_BLOCK_NEW:
		return MESSAGE_ACCEPT;
	case ACTION_BLOCK_PROCESSING:
		pconnection = get_connection(context_ID);
		mail_entity = get_mail_entity(context_ID);
		if (TRUE == mail_entity.penvelop->is_relay ||
			TRUE == mail_entity.penvelop->is_known) {
			return MESSAGE_ACCEPT;
		}
		if (TRUE == ip_whitelist_query(pconnection->client_ip)) {
			return MESSAGE_ACCEPT;
		}
		pdomain = strchr(mail_entity.penvelop->from, '@');
		pdomain ++;
		if (TRUE == domain_whitelist_query(pdomain)) {
			return MESSAGE_ACCEPT;
		}
		if (TRUE == from_whitelist_query(mail_entity.penvelop->from)) {
			return MESSAGE_ACCEPT;
		}
		if (FALSE == ext_buffer_push_init(&push_ctx, NULL, 0, 0)) {
			return MESSAGE_ACCEPT;
        }
		if (EXT_ERR_SUCCESS != ext_buffer_push_string(
			&push_ctx, pconnection->client_ip)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		if (NULL != pconnection->ssl) {
			if (EXT_ERR_SUCCESS != ext_buffer_push_bool(&push_ctx, TRUE)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;	
			}
		} else {
			if (EXT_ERR_SUCCESS != ext_buffer_push_bool(&push_ctx, FALSE)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;
			}
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_bool(
			&push_ctx, mail_entity.penvelop->is_outbound)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_string(&push_ctx,
			mail_entity.penvelop->hello_domain)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;	
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_string(
			&push_ctx, mail_entity.penvelop->from)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		rcpt_count = 0;
		mem_file_seek(&mail_entity.penvelop->f_rcpt_to,
			MEM_FILE_READ_PTR, 0, MEM_FILE_SEEK_BEGIN);
        while (MEM_END_OF_FILE != mem_file_readline(
			&mail_entity.penvelop->f_rcpt_to, tmp_buff, 256)) {
			rcpt_count ++;
        }
		if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
			&push_ctx, rcpt_count)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		mem_file_seek(&mail_entity.penvelop->f_rcpt_to,
			MEM_FILE_READ_PTR, 0, MEM_FILE_SEEK_BEGIN);
		while (MEM_END_OF_FILE != mem_file_readline(
			&mail_entity.penvelop->f_rcpt_to, tmp_buff, 256)) {
			if (EXT_ERR_SUCCESS != ext_buffer_push_string(
				&push_ctx, tmp_buff)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;
			}
		}
		tmp_len = mem_file_read(&mail_entity.phead->f_mime_from,
									tmp_buff, sizeof(tmp_buff));
		if (MEM_END_OF_FILE == tmp_len) {
			tmp_len = 0;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
			&push_ctx, tmp_buff, tmp_len) ||
			EXT_ERR_SUCCESS != ext_buffer_push_uint8(
			&push_ctx, 0)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		tmp_len = mem_file_read(&mail_entity.phead->f_mime_to,
									tmp_buff, sizeof(tmp_buff));
		if (MEM_END_OF_FILE == tmp_len) {
			tmp_len = 0;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
			&push_ctx, tmp_buff, tmp_len) ||
			EXT_ERR_SUCCESS != ext_buffer_push_uint8(
			&push_ctx, 0)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		tmp_len = mem_file_read(&mail_entity.phead->f_mime_cc,
									tmp_buff, sizeof(tmp_buff));
		if (MEM_END_OF_FILE == tmp_len) {
			tmp_len = 0;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
			&push_ctx, tmp_buff, tmp_len) ||
			EXT_ERR_SUCCESS != ext_buffer_push_uint8(
			&push_ctx, 0)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		tmp_len = mem_file_read(&mail_entity.phead->f_mime_delivered_to,
											tmp_buff, sizeof(tmp_buff));
		if (MEM_END_OF_FILE == tmp_len) {
			tmp_len = 0;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
			&push_ctx, tmp_buff, tmp_len) ||
			EXT_ERR_SUCCESS != ext_buffer_push_uint8(
			&push_ctx, 0)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		tmp_len = mem_file_read(&mail_entity.phead->f_xmailer,
									tmp_buff, sizeof(tmp_buff));
		if (MEM_END_OF_FILE == tmp_len) {
			tmp_len = 0;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
			&push_ctx, tmp_buff, tmp_len) ||
			EXT_ERR_SUCCESS != ext_buffer_push_uint8(
			&push_ctx, 0)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		tmp_len = mem_file_read(&mail_entity.phead->f_subject,
									tmp_buff, sizeof(tmp_buff));
		if (MEM_END_OF_FILE == tmp_len) {
			tmp_len = 0;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
			&push_ctx, tmp_buff, tmp_len) ||
			EXT_ERR_SUCCESS != ext_buffer_push_uint8(
			&push_ctx, 0)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		tmp_len = mem_file_read(&mail_entity.phead->f_content_type,
										tmp_buff, sizeof(tmp_buff));
		if (MEM_END_OF_FILE == tmp_len) {
			tmp_len = 0;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
			&push_ctx, tmp_buff, tmp_len) ||
			EXT_ERR_SUCCESS != ext_buffer_push_uint8(
			&push_ctx, 0)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_string(
			&push_ctx, mail_entity.phead->x_original_ip)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;	
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_string(
			&push_ctx, mail_entity.phead->compose_time)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		if (SINGLE_PART_MAIL == mail_entity.phead->mail_part) {
			if (EXT_ERR_SUCCESS != ext_buffer_push_bool(&push_ctx, FALSE)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;	
			}
		} else {
			if (EXT_ERR_SUCCESS != ext_buffer_push_bool(&push_ctx, TRUE)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;	
			}
		}
		while (MEM_END_OF_FILE != mem_file_read(
			&mail_entity.phead->f_others, &tmp_len,
			sizeof(int))) {
			mem_file_read(&mail_entity.phead->f_others,
									tmp_buff, tmp_len);
			if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
				&push_ctx, tmp_buff, tmp_len) ||
				EXT_ERR_SUCCESS != ext_buffer_push_uint8(
				&push_ctx, 0)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;
			}
			mem_file_read(&mail_entity.phead->f_others,
								&tmp_len, sizeof(int));
			mem_file_read(&mail_entity.phead->f_others,
									tmp_buff, tmp_len);
			if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
				&push_ctx, tmp_buff, tmp_len) ||
				EXT_ERR_SUCCESS != ext_buffer_push_uint8(
				&push_ctx, 0)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;
			}
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_uint8(&push_ctx, 0)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		while (MEM_END_OF_FILE != mem_file_read(
			pblock->fp_mime_info, &tmp_len, sizeof(int))) {
			mem_file_read(pblock->fp_mime_info, tmp_buff, tmp_len);
			if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
				&push_ctx, tmp_buff, tmp_len) ||
				EXT_ERR_SUCCESS != ext_buffer_push_uint8(
				&push_ctx, 0)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;
			}
			mem_file_read(pblock->fp_mime_info, &tmp_len, sizeof(int));
			mem_file_read(pblock->fp_mime_info, tmp_buff, tmp_len);
			if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
				&push_ctx, tmp_buff, tmp_len) ||
				EXT_ERR_SUCCESS != ext_buffer_push_uint8(
				&push_ctx, 0)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;
			}
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_uint8(&push_ctx, 0)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
		}
		if (FALSE == pblock->is_parsed) {
			if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
				&push_ctx, pblock->original_length)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;
			}
			if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(&push_ctx,
				pblock->original_buff, pblock->original_length)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;
			}
		} else {
			if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
				&push_ctx, pblock->parsed_length)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;
			}
			if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(&push_ctx,
				pblock->parsed_buff, pblock->parsed_length)) {
				ext_buffer_push_free(&push_ctx);
				return MESSAGE_ACCEPT;
			}
		}
		if (FALSE == fcgi_rpc(push_ctx.data, push_ctx.offset,
			&pbuff_out, &out_len, g_as_script)) {
			ext_buffer_push_free(&push_ctx);
			return MESSAGE_ACCEPT;
        }
        ext_buffer_push_free(&push_ctx);
		if (2 != out_len) {
			free(pbuff_out);
			return MESSAGE_ACCEPT;
		}
		result_mask = pbuff_out[0] & 0xE0;
		result_code = pbuff_out[0] & 0x1F;
		result_code <<= 5;
		result_code |= pbuff_out[1];
		free(pbuff_out);
		if (0 == (SPAM_BIT & result_mask)) {
			if (result_mask == MARK_SPAM) {
				mark_context_spam(context_ID);
			}
			return MESSAGE_ACCEPT;
		}
		if (TRUE == mail_entity.penvelop->is_outbound) {
			if (DISABLE_BIT & result_mask) {
				disable_smtp(mail_entity.penvelop->username);
				if (TRUE == fcgi_rpc(mail_entity.penvelop->username,
					strlen(mail_entity.penvelop->username) + 1,
					&pbuff_out, &out_len, g_edm_script)) {
					free(pbuff_out);	
				}
			}
			if (NULL!= spam_statistic) {
				spam_statistic(result_code);
			}
			snprintf(reason, length, g_return_string, (int)result_code);
			return MESSAGE_REJECT;
		}
		if (REVERSE_BIT & result_mask) {
			inet_pton(AF_INET, pconnection->client_ip, &addr);
			if (0 == gethostbyaddr_r((char*)&addr, sizeof(addr),
				AF_INET, &hostinfo, tmp_buff, sizeof(tmp_buff),
				&phost, &h_errnop) && NULL != phost && NULL ==
				extract_ip(phost->h_name, tmp_buff)) {
				return MESSAGE_ACCEPT;
			}
		}
		if (RETRY_BIT & result_mask) {
			 if (FALSE == check_retrying(
				pconnection->client_ip,
				mail_entity.penvelop->from,
				&mail_entity.penvelop->f_rcpt_to)) {
				if (NULL!= spam_statistic) {
					spam_statistic(result_code);
				}
				snprintf(reason, length, g_return_string, (int)result_code);
				return MESSAGE_RETRYING;
			}
			return MESSAGE_ACCEPT;
		}
		if (NULL!= spam_statistic) {
			spam_statistic(result_code);
		}
		snprintf(reason, length, g_return_string, (int)result_code);
		return MESSAGE_REJECT;
	case ACTION_BLOCK_FREE:
		return MESSAGE_ACCEPT;
	}
}
