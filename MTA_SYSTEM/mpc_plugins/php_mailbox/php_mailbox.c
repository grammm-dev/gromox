#include "util.h"
#include "guid.h"
#include "oxcmail.h"
#include "int_hash.h"
#include "str_hash.h"
#include "list_file.h"
#include "ext_buffer.h"
#include "php_mailbox.h"
#include "hook_common.h"
#include "alloc_context.h"
#include "tpropval_array.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

static BOOL g_notify_stop;
static pthread_t g_thread_id;
static int g_speed_limitation;
static char g_propname_path[256];
static pthread_key_t g_alloc_key;
static INT_HASH_TABLE *g_int_hash;
static STR_HASH_TABLE *g_str_hash;

static BOOL (*php_mailbox_get_user_info)(const char *username,
	char *maildir, char *charset, char *timezone);

static BOOL (*php_mailbox_lang_to_charset)(
	const char *lang, char *charset);

static BOOL (*php_mailbox_fcgi_rpc)(const uint8_t *pbuff_in,
	uint32_t in_len, uint8_t **ppbuff_out, uint32_t *pout_len,
	const char *script_path);

static void* thread_work_func(void *pparam);

void php_mailbox_init(const char *propname_path, int speed_limitation)
{
	g_notify_stop = TRUE;
	pthread_key_create(&g_alloc_key, NULL);
	strcpy(g_propname_path, propname_path);
	g_speed_limitation = speed_limitation;
}

int php_mailbox_run()
{
	DIR *dirp;
	int i, num;
	char *pitem;
	char *ptoken;
	int last_propid;
	LIST_FILE *plist;
	char tmp_path[256];
	char temp_line[256];
	PROPERTY_NAME tmp_propname;
	
	php_mailbox_get_user_info = query_service("get_user_info");
	if (NULL == php_mailbox_get_user_info) {
		printf("[php_mailbox]: fail to get \"get_user_info\" service\n");
		return -1;
	}
	php_mailbox_lang_to_charset = query_service("lang_to_charset");
	if (NULL == php_mailbox_lang_to_charset) {
		printf("[php_mailbox]: fail to get \"lang_to_charset\" service\n");
		return -1;
	}
	php_mailbox_fcgi_rpc = query_service("fcgi_rpc");
	if (NULL == php_mailbox_fcgi_rpc) {
		printf("[php_mailbox]: fail to get \"fcgi_rpc\" service\n");
		return -1;
	}
	plist = list_file_init(g_propname_path, "%s:256");
	if (NULL == plist) {
		printf("[php_mailbox]: fail to load property"
			" name list from %s\n", g_propname_path);
		return -2;
	}
	num = list_file_get_item_num(plist);
	pitem = list_file_get_list(plist);
	g_int_hash = int_hash_init(num + 1, sizeof(PROPERTY_NAME), NULL);
	if (NULL == g_int_hash) {
		printf("[php_mailbox]: fail to init hash table\n");
		return -3;
	}
	g_str_hash = str_hash_init(num + 1, sizeof(uint16_t), NULL);
	if (NULL == g_str_hash) {
		printf("[php_mailbox]: fail to init hash table\n");
		return -3;
	}
	last_propid = 0x8001;
	for (i=0; i<num; i++) {
		strcpy(temp_line, pitem + 256*i);
		lower_string(temp_line);
		str_hash_add(g_str_hash, temp_line, &last_propid);
		ptoken = strchr(temp_line, ',');
		if (NULL != ptoken) {
			*ptoken = '\0';
			ptoken ++;
			if (0 == strncasecmp(temp_line, "GUID=", 5) &&
				TRUE == guid_from_string(&tmp_propname.guid,
				temp_line + 5)) {
				if (0 == strncasecmp(ptoken, "NAME=", 5)) {
					tmp_propname.kind = KIND_NAME;
					tmp_propname.pname = strdup(ptoken + 5);
					if (NULL == tmp_propname.pname) {
						printf("[php_mailbox]: fail to allocate memory\n");
						return -4;
					}
					tmp_propname.plid = NULL;
					int_hash_add(g_int_hash, last_propid, &tmp_propname);
				} else if (0 == strncasecmp(ptoken, "LID=", 4)) {
					tmp_propname.kind = KIND_LID;
					tmp_propname.plid = malloc(sizeof(uint32_t));
					if (NULL == tmp_propname.plid) {
						printf("[php_mailbox]: fail to allocate memory\n");
						return -4;
					}
					*tmp_propname.plid = atoi(ptoken + 4);
					tmp_propname.pname = NULL;
					int_hash_add(g_int_hash, last_propid, &tmp_propname);
				}
			}
		}
		last_propid ++;
	}
	sprintf(tmp_path, "%s/mapi", get_queue_path());
	dirp = opendir(tmp_path);
	if (NULL == dirp) {
		printf("[php_mailbox]: fail to open \"%s\" dir\n", tmp_path);
		return -5;
	}
	g_notify_stop = FALSE;
	if (0 != pthread_create(&g_thread_id, NULL, thread_work_func, dirp)) {
		g_notify_stop = TRUE;
		closedir(dirp);
		printf("[php_mailbox]: fail to create message dequeue thread\n");
		return -6;
	}
	return 0;
}

int php_mailbox_stop()
{
	INT_HASH_ITER *iter;
	PROPERTY_NAME *ppropname;
	
	if (FALSE == g_notify_stop) {
		g_notify_stop = TRUE;
		pthread_join(g_thread_id, NULL);
	}
	if (NULL != g_int_hash) {
		iter = int_hash_iter_init(g_int_hash);
		for (int_hash_iter_begin(iter);
			FALSE == int_hash_iter_done(iter);
			int_hash_iter_forward(iter)) {
			ppropname = int_hash_iter_get_value(iter, NULL);
			if (KIND_LID == ppropname->kind) {
				free(ppropname->plid);
			} else {
				free(ppropname->pname);
			}
		}
		int_hash_free(g_int_hash);
		g_int_hash = NULL;
	}
	if (NULL != g_str_hash) {
		str_hash_free(g_str_hash);
		g_str_hash = NULL;
	}
	return 0;
}

void php_mailbox_free()
{
	pthread_key_delete(g_alloc_key);
}

static void php_mailbox_log_info(MESSAGE_CONTEXT *pcontext,
	int level, char *format, ...)
{
	int i;
	va_list ap;
	int rcpt_len;
	size_t size_read;
	char log_buf[2048];
	char rcpt_buff[2048];

	va_start(ap, format);
	vsnprintf(log_buf, sizeof(log_buf) - 1, format, ap);
	log_buf[sizeof(log_buf) - 1] = '\0';
	
	rcpt_len = 0;
	/* maximum record 8 rcpt to address */
	mem_file_seek(&pcontext->pcontrol->f_rcpt_to,
		MEM_FILE_READ_PTR, 0, MEM_FILE_SEEK_BEGIN);
	for (i=0; i<8; i++) {
		size_read = mem_file_readline(
			&pcontext->pcontrol->f_rcpt_to,
			rcpt_buff + rcpt_len, 256);
		if (size_read == MEM_END_OF_FILE) {
			break;
		}
		rcpt_len += size_read;
		rcpt_buff[rcpt_len] = ' ';
		rcpt_len ++;
	}
	rcpt_buff[rcpt_len] = '\0';
	switch (pcontext->pcontrol->bound_type) {
	case BOUND_IN:
	case BOUND_OUT:
	case BOUND_RELAY:
		log_info(level, "SMTP message queue-ID: %d, FROM: %s, TO: %s %s",
				pcontext->pcontrol->queue_ID, pcontext->pcontrol->from,
				rcpt_buff, log_buf);
		break;
	default:
		log_info(level, "APP created message FROM: %s, TO: %s %s",
					pcontext->pcontrol->from, rcpt_buff, log_buf);
		break;
	}
}

static void* php_mailbox_alloc(size_t size)
{
	ALLOC_CONTEXT *pctx;
	
	pctx = pthread_getspecific(g_alloc_key);
	if (NULL == pctx) {
		return NULL;
	}
	return alloc_context_alloc(pctx, size);
}

static BOOL php_mailbox_get_propids(
	const PROPNAME_ARRAY *ppropnames,
	PROPID_ARRAY *ppropids)
{
	int i;
	uint16_t *ppropid;
	char str_guid[64];
	char tmp_string[256];
	
	ppropids->count = ppropnames->count;
	ppropids->ppropid = php_mailbox_alloc(
		sizeof(uint16_t)*ppropnames->count);
	for (i=0; i<ppropnames->count; i++) {
		guid_to_string(&ppropnames->ppropname[i].guid, str_guid, 64);
		if (KIND_LID == ppropnames->ppropname[i].kind) {
			snprintf(tmp_string, 256, "GUID=%s,LID=%u",
				str_guid, *ppropnames->ppropname[i].plid);
		} else {
			snprintf(tmp_string, 256, "GUID=%s,NAME=%s",
				str_guid, ppropnames->ppropname[i].pname);
		}
		lower_string(tmp_string);
		ppropid = str_hash_query(g_str_hash, tmp_string);
		if (NULL == ppropid) {
			ppropids->ppropid[i] = 0;
		} else {
			ppropids->ppropid[i] = *ppropid;
		}
	}
	return TRUE;
}

static BOOL php_mailbox_get_propname(
	uint16_t propid, PROPERTY_NAME **ppname)
{
	*ppname = int_hash_query(g_int_hash, propid);
	return TRUE;
}

BOOL php_mailbox_process(MESSAGE_CONTEXT *pcontext,
	const char *script_path)
{
	int i, j;
	int count;
	MAIL imail;
	char lang[32];
	char *paddress;
	uint32_t out_len;
	char charset[32];
	char maildir[256];
	char timezone[64];
	EXT_PUSH ext_push;
	EXT_PULL ext_pull;
	uint16_t msg_count;
	uint8_t *pbuff_out;
	char rcpt_buff[256];
	MESSAGE_CONTENT *pmsg;
	MESSAGE_CONTENT tmp_msg;
	ALLOC_CONTEXT alloc_ctx;
	MESSAGE_CONTEXT *pcontext1;
	
	if (FALSE == php_mailbox_get_user_info(
		pcontext->pcontrol->from, maildir,
		lang, timezone)) {
		return FALSE;
	}
	if ('\0' == lang[0] || FALSE ==
		php_mailbox_lang_to_charset(lang,
		charset) || '\0' == charset[0]) {
		strcpy(charset, "utf-8");
	}
	alloc_context_init(&alloc_ctx);
	pthread_setspecific(g_alloc_key, &alloc_ctx);
	if (FALSE == ext_buffer_push_init(&ext_push,
		NULL, 0, EXT_FLAG_WCOUNT)) {
		alloc_context_free(&alloc_ctx);
		return FALSE;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		&ext_push, pcontext->pcontrol->from)) {
		ext_buffer_push_free(&ext_push);
		alloc_context_free(&alloc_ctx);
		return FALSE;
	}
	mem_file_seek(&pcontext->pcontrol->f_rcpt_to,
		MEM_FILE_READ_PTR, 0, MEM_FILE_SEEK_BEGIN);
	count = 0;
	while (MEM_END_OF_FILE != mem_file_readline(
		&pcontext->pcontrol->f_rcpt_to, rcpt_buff, 256)) {
		count ++;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		&ext_push, count)) {
		ext_buffer_push_free(&ext_push);
		alloc_context_free(&alloc_ctx);
		return FALSE;
	}
	mem_file_seek(&pcontext->pcontrol->f_rcpt_to,
		 MEM_FILE_READ_PTR, 0, MEM_FILE_SEEK_BEGIN);
	for (i=0; i<count; i++) {
		mem_file_readline(&pcontext->pcontrol->f_rcpt_to,
			rcpt_buff, 256);
		if (EXT_ERR_SUCCESS != ext_buffer_push_string(
			&ext_push, rcpt_buff)) {
			ext_buffer_push_free(&ext_push);
			alloc_context_free(&alloc_ctx);
			return FALSE;
		}
	}
	pmsg = oxcmail_import(charset, timezone, pcontext->pmail,
				php_mailbox_alloc, php_mailbox_get_propids);
	if (NULL == pmsg) {
		ext_buffer_push_free(&ext_push);
		alloc_context_free(&alloc_ctx);
		return FALSE;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_message_content(
        &ext_push, pmsg)) {
		ext_buffer_push_free(&ext_push);
		alloc_context_free(&alloc_ctx);
		message_content_free(pmsg);
		return FALSE;
	}
	message_content_free(pmsg);
	alloc_context_free(&alloc_ctx);
	if (FALSE == php_mailbox_fcgi_rpc(ext_push.data,
		ext_push.offset, &pbuff_out, &out_len, script_path)) {
		ext_buffer_push_free(&ext_push);
		return FALSE;
	}
	ext_buffer_push_free(&ext_push);
	/* 
		we do not init allocator here, because that
		ext_buffer_pull_uint16 needn't allocate memory
	*/
	ext_buffer_pull_init(&ext_pull, pbuff_out,
		out_len, php_mailbox_alloc, EXT_FLAG_WCOUNT);
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint16(
		&ext_pull, &msg_count)) {
		free(pbuff_out);
		return FALSE;	
	}
	for (i=0; i<msg_count; i++) {
		alloc_context_init(&alloc_ctx);
		pcontext1 = get_context();
		if (NULL == pcontext1) {
			free(pbuff_out);
			alloc_context_free(&alloc_ctx);
			return FALSE;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_pull_message_content(
			&ext_pull, &tmp_msg)) {
			put_context(pcontext1);
			free(pbuff_out);
			alloc_context_free(&alloc_ctx);
			return FALSE;
		}
		paddress = tpropval_array_get_propval(&tmp_msg.proplist,
							PROP_TAG_SENTREPRESENTINGSMTPADDRESS);
		if (NULL == paddress || NULL == tmp_msg.children.prcpts) {
			put_context(pcontext1);
			free(pbuff_out);
			alloc_context_free(&alloc_ctx);
			return FALSE;
		}
		strcpy(pcontext1->pcontrol->from, paddress);
		for (j=0; j<tmp_msg.children.prcpts->count; j++) {
			paddress = tpropval_array_get_propval(
				tmp_msg.children.prcpts->pparray[j],
				PROP_TAG_SMTPADDRESS);
			if (NULL == paddress) {
				put_context(pcontext1);
				free(pbuff_out);
				alloc_context_free(&alloc_ctx);
				return FALSE;
			}
			mem_file_writeline(&pcontext1->pcontrol->f_rcpt_to, paddress);
		}
		if (FALSE == oxcmail_export(&tmp_msg,
			FALSE, OXCMAIL_BODY_PLAIN_AND_HTML,
			pcontext->pmail->pmime_pool, &imail,
			php_mailbox_alloc, php_mailbox_get_propids,
			php_mailbox_get_propname)) {
			put_context(pcontext1);
			free(pbuff_out);
			alloc_context_free(&alloc_ctx);
			return FALSE;
		}
		alloc_context_free(&alloc_ctx);
		if (FALSE == mail_dup(&imail, pcontext1->pmail)) {
			mail_free(&imail);
			free(pbuff_out);
			put_context(pcontext1);
			return FALSE;
		}
		mail_free(&imail);
		throw_context(pcontext1);
	}
	free(pbuff_out);
	php_mailbox_log_info(pcontext, 0, "mail has been processed by php program");
	return TRUE;
}

static BOOL php_mailbox_enqueue(const char *pbuff, uint32_t length)
{
	int i;
	MAIL imail;
	char *paddress;
	EXT_PULL ext_pull;
	ALLOC_CONTEXT alloc_ctx;
	MESSAGE_CONTENT tmp_msg;
	MESSAGE_CONTEXT *pcontext;
	
	ext_buffer_pull_init(&ext_pull, pbuff,
		length, php_mailbox_alloc, EXT_FLAG_WCOUNT);
	alloc_context_init(&alloc_ctx);
	pthread_setspecific(g_alloc_key, &alloc_ctx);
	pcontext = get_context();
	if (NULL == pcontext) {
		alloc_context_free(&alloc_ctx);
		return FALSE;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_pull_message_content(
		&ext_pull, &tmp_msg)) {
		put_context(pcontext);
		alloc_context_free(&alloc_ctx);
		return FALSE;
	}
	if (NULL == tmp_msg.children.prcpts) {
		put_context(pcontext);
		alloc_context_free(&alloc_ctx);
		return FALSE;
	}
	paddress = tpropval_array_get_propval(&tmp_msg.proplist,
						PROP_TAG_SENTREPRESENTINGSMTPADDRESS);
	if (NULL == paddress) {
		strcpy(pcontext->pcontrol->from, "none@none");
	} else {
		strcpy(pcontext->pcontrol->from, paddress);
	}
	for (i=0; i<tmp_msg.children.prcpts->count; i++) {
		paddress = tpropval_array_get_propval(
			tmp_msg.children.prcpts->pparray[i],
			PROP_TAG_SMTPADDRESS);
		if (NULL == paddress) {
			put_context(pcontext);
			alloc_context_free(&alloc_ctx);
			return FALSE;
		}
		mem_file_writeline(&pcontext->pcontrol->f_rcpt_to, paddress);
	}
	if (FALSE == oxcmail_export(&tmp_msg,
		FALSE, OXCMAIL_BODY_PLAIN_AND_HTML,
		pcontext->pmail->pmime_pool, &imail,
		php_mailbox_alloc, php_mailbox_get_propids,
		php_mailbox_get_propname)) {
		put_context(pcontext);
		alloc_context_free(&alloc_ctx);
		return FALSE;
	}
	alloc_context_free(&alloc_ctx);
	if (FALSE == mail_dup(&imail, pcontext->pmail)) {
		mail_free(&imail);
		put_context(pcontext);
		return FALSE;
	}
	mail_free(&imail);
	enqueue_context(pcontext);
	return TRUE;
}

static void* thread_work_func(void *pparam)
{
	int fd;
	int count;
	DIR *dirp;
	char *pbuff;
	GUID msg_guid;
	char tmp_path[256];
	struct stat node_stat;
	struct dirent *direntp;

	dirp = pparam;
	while (FALSE == g_notify_stop) {
		sleep(3);
		seekdir(dirp, 0);
		count = 0;
		while ((direntp = readdir(dirp)) != NULL) {
			if (0 == strcmp(direntp->d_name, ".") ||
				0 == strcmp(direntp->d_name, "..") ||
				FALSE == guid_from_string(&msg_guid, direntp->d_name)) {
				continue;
			}
			sprintf(tmp_path, "%s/mapi/%s", get_queue_path(), direntp->d_name);
			if (0 != stat(tmp_path, &node_stat)) {
				continue;
			}
			pbuff = malloc(node_stat.st_size);
			if (NULL == pbuff) {
				continue;
			}
			fd = open(tmp_path, O_RDONLY);
			if (-1 == fd) {
				free(pbuff);
				continue;
			}
			if (node_stat.st_size != read(fd, pbuff, node_stat.st_size)) {
				close(fd);
				free(pbuff);
				continue;
			}
			close(fd);
			if (TRUE == php_mailbox_enqueue(pbuff, node_stat.st_size)) {
				remove(tmp_path);
				count ++;
			}
			free(pbuff);
			if (g_speed_limitation > 0 && count >= g_speed_limitation) {
				break;
			}
		}
	}
	closedir(dirp);
}
