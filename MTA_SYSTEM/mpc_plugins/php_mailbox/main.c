#include "hook_common.h"
#include "config_file.h"
#include "list_file.h"
#include "str_hash.h"
#include "util.h"
#include <pthread.h>

DECLARE_API;

enum{
	REFRESH_OK,
	REFRESH_FILE_ERROR,
	REFRESH_HASH_FAIL
};

static char g_list_path[256];
static STR_HASH_TABLE* g_hash_table;
static pthread_rwlock_t g_refresh_lock;


static int table_refresh();

static BOOL table_query(const char* str, char *buff);

static void console_talk(int argc, char **argv, char *result, int length);
	
static BOOL mail_hook(MESSAGE_CONTEXT *pcontext);

BOOL HOOK_LibMain(int reason, void **ppdata)
{
	char *psearch;
	char tmp_path[256];
	CONFIG_FILE *pfile;
	char file_name[256];
	const char *str_val;
	int speed_limitation;
	char propname_path[256];
	
    /* path conatins the config files directory */
    switch (reason) {
    case PLUGIN_INIT:
		LINK_API(ppdata);
		
		/* get the plugin name from system api */
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		sprintf(g_list_path, "%s/%s.txt", get_data_path(), file_name);
		sprintf(propname_path, "%s/propnames.txt", get_data_path());
		sprintf(tmp_path, "%s/%s.cfg", get_config_path(), file_name);
		pfile = config_file_init(tmp_path);
		if (NULL == pfile) {
			printf("[php_mailbox]: error to open config file!!!\n");
			return FALSE;
		}
		str_val = config_file_get_value(pfile, "RETRIEVE_SPEED_LIMITATION");
		if (NULL == str_val) {
			speed_limitation = 0;
		} else {
			speed_limitation = atoi(str_val);
		}
		if (speed_limitation < 0) {
			speed_limitation = 0;
		}
		
		pthread_rwlock_init(&g_refresh_lock, NULL);
		
		g_hash_table = NULL;
		php_mailbox_init(propname_path, speed_limitation);
		if (0 != php_mailbox_run()) {
			return FALSE;
		}
		if (REFRESH_OK != table_refresh()) {
			printf("[php_mailbox]: fail to load replace table\n");
			return FALSE;
		}
        if (FALSE == register_hook(mail_hook)) {
			printf("[php_mailbox]: fail to register the hook function\n");
            return FALSE;
        }
		register_talk(console_talk);
		printf("[php_mailbox]: plugin is loaded into system\n");
        return TRUE;
    case PLUGIN_FREE:
		php_mailbox_stop();
    	if (NULL != g_hash_table) {
        	str_hash_free(g_hash_table);
        	g_hash_table = NULL;
    	}
    	pthread_rwlock_destroy(&g_refresh_lock);
		php_mailbox_free();
        return TRUE;
    }
}

static BOOL mail_hook(MESSAGE_CONTEXT *pcontext)
{
	BOOL b_found;
	char rcpt_buff[256];
	char script_path[256];
	
	b_found = FALSE;
	while (MEM_END_OF_FILE != mem_file_readline(
		&pcontext->pcontrol->f_rcpt_to, rcpt_buff, 256)) {
		if (TRUE == table_query(rcpt_buff, script_path)) {
			b_found = TRUE;
			break;
		}
	}
	if (FALSE == b_found) {
		return FALSE;
	}
	return php_mailbox_process(pcontext, script_path);
}

static BOOL table_query(const char* str, char *buff)
{
	char *presult;
	char temp_string[256];
	
	if (NULL == str) {
		return FALSE;
	}
	strncpy(temp_string, str, sizeof(temp_string));
	temp_string[sizeof(temp_string) - 1] = '\0';
	lower_string(temp_string);
	pthread_rwlock_rdlock(&g_refresh_lock);
    presult = str_hash_query(g_hash_table, temp_string);
    if (NULL != presult) {
    	strcpy(buff, presult);	
    }
	pthread_rwlock_unlock(&g_refresh_lock);
	if (NULL == presult) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/*
 *  refresh the string list, the list is from the
 *  file which is specified in configuration file.
 *
 *  @return
 *		REFRESH_OK			OK
 *		FILE_ERROR	fail to open list file
 *		REFRESH_HASH_FAIL		fail to open hash map
 */
static int table_refresh()
{
	char *pitem;
	int i, list_len;
	STR_HASH_TABLE *phash;
	LIST_FILE *plist_file;

	/* initialize the list filter */
	plist_file = list_file_init(g_list_path, "%s:256%s:256");
	if (NULL == plist_file) {
		return REFRESH_FILE_ERROR;
	}
	pitem = (char*)list_file_get_list(plist_file);
	list_len = list_file_get_item_num(plist_file);

	phash = str_hash_init(list_len + 1, 256, NULL);
	if (NULL == phash) {
		printf("[php_mailbox]: fail to allocate hash map\n");
		list_file_free(plist_file);
		return REFRESH_HASH_FAIL;
	}
	for (i=0; i<list_len; i++) {
		if (NULL == strchr(pitem + 2*256*i, '@') ||
			NULL != strchr(pitem + 2*256*i, ' ')) {
			printf("[php_mailbox]: mailbox format error in line %d\n", i);
			continue;
		}
		lower_string(pitem + 2*256*i);
		str_hash_add(phash, pitem + 2*256*i, pitem + 2*256*i + 256);   
	}
	list_file_free(plist_file);

	pthread_rwlock_wrlock(&g_refresh_lock);
	if (NULL != g_hash_table) {
		str_hash_free(g_hash_table);
	}
	g_hash_table = phash;
	pthread_rwlock_unlock(&g_refresh_lock);

	return REFRESH_OK;
}


/*
 *	string table's console talk
 *	@param
 *		argc			arguments number
 *		argv [in]		arguments value
 *		result [out]	buffer for retrieving result
 *		length			result buffer length
 */
static void console_talk(int argc, char **argv, char *result, int length)
{
	char help_string[] = "250 from php mailbox help information:\r\n"
						 "\t%s reload\r\n"
						 "\t    --reload the mailbox table from list file";

	if (1 == argc) {
		strncpy(result, "550 too few arguments", length);
		return;
	}
	if (2 == argc && 0 == strcmp("--help", argv[1])) {
		snprintf(result, length, help_string, argv[0]);
		result[length - 1] ='\0';
		return;
	}
	if (2 == argc && 0 == strcmp("reload", argv[1])) {
		switch(table_refresh()) {
		case REFRESH_OK:
			strncpy(result, "250 mailbox table reload OK", length);
			return;
		case REFRESH_FILE_ERROR:
			strncpy(result, "550 mailbox list file error", length);
			return;
		case REFRESH_HASH_FAIL:
			strncpy(result, "550 hash map error for mailbox table", length);
			return;
		}
	}
	snprintf(result, length, "550 invalid argument %s", argv[1]);
	return;
}
