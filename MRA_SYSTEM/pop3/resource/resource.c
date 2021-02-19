/*
 *  user config resource file, which provide some interface for 
 *  programmer to set and get the configuration dynamicly
 *
 */
#include "resource.h"
#include "config_file.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>


#define MAX_FILE_NAME_LEN       256
#define MAX_FILE_LINE_LEN       1024

static struct {
#define MAX_VAR_LEN     256
    int  var_id;
    char name[MAX_VAR_LEN];
} g_string_table[MAX_RES_CONFG_VAR_NUM] = {
    {RES_LISTEN_PORT, "LISTEN_PORT"}, 
	{RES_LISTEN_SSL_PORT, "LISTEN_SSL_PORT"},
    {RES_HOST_ID, "HOST_ID"},
	{RES_DEFAULT_DOMAIN, "DEFAULT_DOMAIN"},
    
    {RES_CONTEXT_NUM, "CONTEXT_NUM"},
    {RES_CONTEXT_AVERAGE_MEM, "CONTEXT_AVERAGE_MEM"},
    {RES_CONTEXT_MAX_MEM, "CONTEXT_MAX_MEM"},
    {RES_CONTEXT_AVERAGE_UNITS, "CONTEXT_AVERAGE_UNITS"},

    {RES_POP3_AUTH_TIMES, "POP3_AUTH_TIMES"},
    {RES_POP3_CONN_TIMEOUT, "POP3_CONN_TIMEOUT"},
	{RES_POP3_SUPPORT_STLS, "POP3_SUPPORT_STLS"},
	{RES_POP3_CERTIFICATE_PATH, "POP3_CERTIFICATE_PATH"},
	{RES_POP3_CERTIFICATE_PASSWD, "POP3_CERTIFICATE_PASSWD"},
	{RES_POP3_PRIVATE_KEY_PATH, "POP3_PRIVATE_KEY_PATH"},
	{RES_POP3_FORCE_STLS, "POP3_FORCE_STLS"},

    {RES_THREAD_INIT_NUM, "THREAD_INIT_NUM"},
    {RES_THREAD_CHARGE_NUM, "THREAD_CHARGE_NUM"},

    {RES_POP3_RETURN_CODE_PATH, "POP3_RETURN_CODE_PATH"},

    {RES_CONSOLE_SERVER_IP, "CONSOLE_SERVER_IP"},
    {RES_CONSOLE_SERVER_PORT, "CONSOLE_SERVER_PORT"},

	{RES_CDN_CACHE_PATH, "CDN_CACHE_PATH"},
    {RES_SERVICE_PLUGIN_PATH, "SERVICE_PLUGIN_PATH"},
    {RES_RUNNING_IDENTITY, "RUNNING_IDENTITY"},
    {RES_BLOCK_INTERVAL_AUTHS, "BLOCK_INTERVAL_AUTHS"},
    {RES_CONFIG_FILE_PATH, "CONFIG_FILE_PATH"},
    {RES_DATA_FILE_PATH, "DATA_FILE_PATH"},
	{RES_LOGIN_SCRIPT_PATH, "LOGIN_SCRIPT_PATH"}
};

static POP3_ERROR_CODE g_default_pop3_error_code_table[] = {
    { 2170000, "+OK" },
    { 2170001, "-ERR time out" },
    { 2170002, "-ERR line too long" },
    { 2170003, "-ERR command unkown" },
    { 2170004, "-ERR command parameter error" },
    { 2170005, "-ERR input username first" },
    { 2170006, "-ERR too many failure, user will be blocked for a while" },
    { 2170007, "-ERR message not found" },
    { 2170008, "-ERR login first" },
    { 2170009, "-ERR fail to open message" },
    { 2170010, "+OK quit <host>" },
    { 2170011, "+OK <host> pop service ready" },
    { 2170012, "-ERR access deny by ip filter for <ip>" },
    { 2170013, "-ERR <host> pop service unavailable" },
    { 2170014, "-ERR login auth fail, because: <reason>" },
    { 2170015, "-ERR cannot get mailbox location from database" },
    { 2170016, "-ERR fail to open/read inbox index" },
    { 2170017, "-ERR access deny by user filter for <user>" },
    { 2170018, "-ERR error internal" },
    { 2170019, "-ERR fail to retrieve message" },
    { 2170020, "-ERR cannot relogin under login stat" },
	{ 2170021, "-ERR midb read/write error" },
	{ 2170022, "-ERR fail to excute command in midb"},
	{ 2170023, "-ERR fail to initialize TLS"},
	{ 2170024, "+OK begin TLS negotiation"},
	{ 2170025, "-ERR TLS negotiation only begin in AUTHORIZATION state"},
	{ 2170026,  "-ERR must issue a STLS command first"}
};

/* private global variables */
static char g_cfg_filename[MAX_FILE_NAME_LEN];
static CONFIG_FILE* g_config_file = NULL;

static POP3_ERROR_CODE* g_error_code_table  = NULL;
static POP3_ERROR_CODE* g_def_code_table    = NULL;
static pthread_rwlock_t g_error_table_lock;

static int resource_find_pop3_code_index(int native_code);

static int resource_construct_pop3_table(POP3_ERROR_CODE **pptable);

static int resource_parse_pop3_line(char* dest, char* src_str, int len);


void resource_init(char* cfg_filename)
{
    strcpy(g_cfg_filename, cfg_filename);
    pthread_rwlock_init(&g_error_table_lock, NULL);
}

void resource_free()
{   
    /* to avoid memory leak because of not stop */
    pthread_rwlock_destroy(&g_error_table_lock);
    if (NULL != g_config_file) {
        config_file_free(g_config_file);
        g_config_file = NULL;
    }
}

int resource_run()
{
    int i;

    g_def_code_table = malloc(sizeof(POP3_ERROR_CODE) * POP3_CODE_COUNT);
    if (NULL == g_def_code_table) {
        printf("[resource]: fail to allocate default code table\n" );
        return -1;
    }
    g_config_file = config_file_init(g_cfg_filename);

    if (NULL == g_config_file) {
        printf("[resource]: fail to init config file\n");
        free(g_def_code_table);
        return -2;
    }

    if (FALSE == resource_refresh_pop3_code_table()) {
        printf("[resource]: fail to load pop3 code\n");
    }
    for (i = 0; i < POP3_CODE_COUNT; i++) {
        g_def_code_table[i].code =
                    g_default_pop3_error_code_table[i].code;

        resource_parse_pop3_line(g_def_code_table[i].comment, 
            g_default_pop3_error_code_table[i].comment, 
            strlen(g_default_pop3_error_code_table[i].comment));
    }

    
    return 0;
}

int resource_stop()
{
    if (NULL != g_config_file) {
        config_file_free(g_config_file);
        g_config_file = NULL;
    }

    if (NULL != g_def_code_table) {
        free(g_def_code_table);
        g_def_code_table = NULL;
    }
    return 0;
}

BOOL resource_save()
{
	if (NULL == g_config_file) {
        debug_info("[resource]: error!!! config file not init or init fail but"
                    " it is now being used");
		return FALSE;
	}
	return config_file_save(g_config_file);
}

/*
 *  get a specified integer value that match the key
 *
 *  @param
 *      key             key that describe the integer value
 *      value [out]     pointer to the integer value
 *
 *  @return
 *      TRUE        success
 *      FALSE       fail
 */
BOOL resource_get_integer(int key, int* value)
{
    char *pvalue    = NULL;     /* string value of the mapped key */

    if ((key < 0 || key > MAX_RES_CONFG_VAR_NUM) && NULL != value) {
        debug_info("[resource]: invalid param resource_get_integer");
        return FALSE;
    }

    if (NULL == g_config_file) {
        debug_info("[resource]: error!!! config file not init or init fail but"
                    " it is now being used");
        return FALSE;
    }
    pvalue = config_file_get_value(g_config_file, 
        g_string_table[key].name);

    if (NULL == pvalue) {
        debug_info("[resource]: no value map to the key in "
                    "resource_get_integer");
        return FALSE;
    }
    *value = atoi(pvalue);
    return TRUE;
}

/*
 *  set the specified integer that match the key
 *
 *  @param
 *      key             key that describe the integer value
 *      value           the new value
 *
 *  @return
 *      TRUE        success
 *      FALSE       fail
 */

BOOL resource_set_integer(int key, int value)
{
    char m_buf[32];             /* buffer to hold the int string  */

    if ((key < 0 || key > MAX_RES_CONFG_VAR_NUM)) {
        debug_info("[resource]: invalid param in resource_set_integer");
        return FALSE;
    }

    if (NULL == g_config_file) {
        debug_info("[resource]: error!!! config file not init or init fail but"
                    " it is now being used");
        return FALSE;
    }
    itoa(value, m_buf, 10);
    return config_file_set_value(g_config_file, 
				g_string_table[key].name, m_buf);
}

/*
 *  set the specified string that match the key
 *
 *  @param
 *      key             key that describe the string value
 *      value [out]     the string value
 *
 *  @return
 *      TRUE        success
 *      FALSE       fail
 */
BOOL resource_set_string(int key, char* value)
{

    if (key < 0 || key > MAX_RES_CONFG_VAR_NUM || NULL == value) {
        debug_info("[resource]: invalid param in resource_set_string");
        return FALSE;
    }

    if (NULL == g_config_file) {
        debug_info("[resource]: error!!! config file not init or init fail but"
                    " it is now being used");
        return FALSE;
    }

    return config_file_set_value(g_config_file,
				g_string_table[key].name, value);
}

/*
 *  get a specified string value that match the key
 *
 *  @param
 *      key             key that describe the string value
 *      value [out]     pointer to the string value
 *
 *  @return
 *      TRUE        success
 *      FALSE       fail
 */

const char* resource_get_string(int key)
{
    const char *pvalue  = NULL;     /* string value of the mapped key */

    if ((key < 0 || key > MAX_RES_CONFG_VAR_NUM) && NULL != pvalue) {
        debug_info("[resource]: invalid param in resource_get_string");
        return NULL;
    }

    if (NULL == g_config_file) {
        debug_info("[resource]: error!!! config file not init or init fail but"
                    " it is now being used");
        return NULL;
    }

    pvalue = config_file_get_value(g_config_file, g_string_table[key].name);

    if (NULL == pvalue) {
        debug_info("[resource]: no value map to the key in "
                    "resource_get_string");
        return NULL;
    }
    return pvalue;
}

/*
 *  construct a pop3 error code table, which is as the below table
 *
 *      number              comments
 *      2175018             550 Access denied to you
 *      2175019             550 Access to Mailbox 
 *                              <email_addr>  is denied
 *  @param
 *      pptable [in/out]        pointer to the newly created table
 *
 *  @return
 *      0           success
 *      <>0         fail
 */
static int resource_construct_pop3_table(POP3_ERROR_CODE **pptable)
{
    char line[MAX_FILE_LINE_LEN], buf[MAX_FILE_LINE_LEN];
    char *filename, *pbackup, *ptr, code[32];
    POP3_ERROR_CODE *code_table;
    FILE *file_ptr = NULL;

    int total, index, native_code, len;

    filename = (char*)resource_get_string(RES_POP3_RETURN_CODE_PATH);
	if (NULL == filename) {
		return -1;
	}
    if (NULL == (file_ptr = fopen(filename, "r"))) {
        printf("[resource]: can not open pop3 error table file  %s\n",
                filename);
        return -1;
    }

    code_table = malloc(sizeof(POP3_ERROR_CODE) * POP3_CODE_COUNT);

    if (NULL == code_table) {
        printf("[resource]: fail to allocate memory for pop3 return code"
                " table\n");
        fclose(file_ptr);
        return -1;
    }

    for (total = 0; total < POP3_CODE_COUNT; total++) {
        code_table[total].code              = -1;
        memset(code_table[total].comment, 0, 512);
    }

    for (total = 0; fgets(line, MAX_FILE_LINE_LEN, file_ptr); total++) {

        if (line[0] == '\r' || line[0] == '\n' || line[0] == '#') {
            /* skip empty line or comments */
            continue;
        }

        ptr = (pbackup = line);

        len = 0;
        while (*ptr && *ptr != '\r' && *ptr != '\n') {
            if (*ptr == ' ' || *ptr == '\t') {
                break;
            }
            len++;
            ptr++;
        }

        if (len <= 0 || len > sizeof(code) - 1) {
            printf("[resource]: invalid native code format, file: %s line: "
                    "%d, %s\n", filename, total + 1, line);

            continue;
        }

        memcpy(code, pbackup, len);
        code[len]   = '\0';

        if ((native_code = atoi(code)) <= 0) {
            printf("[resource]: invalid native code, file: %s line: %d, %s\n", 
                filename, total + 1, line);
            continue;
        }

        while (*ptr && (*ptr == ' ' || *ptr == '\t')) {
            ptr++;
        }

        pbackup = ptr;
        len     = 0;
        while (*ptr && *ptr != '\r' && *ptr != '\n') {
            len++;
            ptr++;
        }

        while (len > 0 && (*ptr == ' ' || *ptr == '\t')) {
            len--;
            ptr--;
        }

        if (len <= 0 || len > MAX_FILE_LINE_LEN - 1) {
            printf("[resource]: invalid native comment, file: %s line: %d, "
                    "%s\n", filename, total + 1, line);
            continue;
        }
        memcpy(buf, pbackup, len);
        buf[len]    = '\0';

        if ((index = resource_find_pop3_code_index(native_code)) < 0) {
            printf("[resource]: no such native code, file: %s line: %d, %s\n", 
                filename, total + 1, line);
            continue;
        }

        if (-1 != code_table[index].code) {
            printf("[resource]: the error code has already been defined, file:"
                " %s line: %d, %s\n", filename, total + 1, line);
            continue;

        }

        if (resource_parse_pop3_line(code_table[index].comment, buf, len) < 0) {
            printf("[resource]: invalid pop3 code format, file: %s line: %d,"
                    " %s", filename, total + 1, line);
            continue;
        }
        code_table[index].code  = native_code;
    }

    *pptable = code_table;
    fclose(file_ptr);
    return 0;
}

/*
 *  take a description line and construct it into what we need.
 *  for example, if the source string is "hello", we just copy
 *  it to the destination with a its length at the beginning of 
 *  the destination.    8 h e l l o \r \n \0    , the length 
 *  includes the CRLF and null terminator. if the source string
 *  is like "hi <who>, give", the destination format will be:       
 *      4 h i space \0 9 , space g i v e \r \n \0
 *
 *  @param
 *      dest    [in]        where we will copy the source string to
 *      src_str [in]        the source description string
 *      len                 the length of the source string
 *
 *  @return
 *      0       success
 *      <0      fail
 */
static int resource_parse_pop3_line(char* dest, char* src_str, int len)
{
    char *ptr = NULL, *end_ptr = NULL, sub_len = 0;

    if (NULL == (ptr = strchr(src_str, '<')) || ptr == src_str) {
        dest[0] = (char)(len + 3);
        strncpy(dest + 1, src_str, len);
        strncpy(dest + len + 1, "\r\n", 2);
        dest[len + 3] = '\0';
    } else {
        sub_len = (char)(ptr - src_str);
        dest[0] = sub_len + 1;          /* include null terminator */
        strncpy(dest + 1, src_str, sub_len);
        dest[sub_len + 1] = '\0';

        if (NULL == (ptr = strchr(ptr, '>'))) {
            return -1;
        }

        end_ptr = src_str + len;
        dest[sub_len + 2] = (char)(end_ptr - ptr + 2);
        end_ptr = ptr + 1;
        ptr     = dest + sub_len + 3;
        sub_len = dest[sub_len + 2];

        strncpy(ptr, end_ptr, sub_len);
        strncpy(ptr + sub_len - 3, "\r\n", 2);
        *(ptr + sub_len - 1) = '\0';
    }
    return 0;

}

char* resource_get_pop3_code(int code_type, int n, int *len)
{
    POP3_ERROR_CODE *pitem = NULL;
    char *ret_ptr = NULL;
    int   ret_len = 0;
#define FIRST_PART      1
#define SECOND_PART     2

    pthread_rwlock_rdlock(&g_error_table_lock);
    if (NULL == g_error_code_table || g_error_code_table[code_type].code
        == -1) {
        pitem = &g_def_code_table[code_type];
    } else {
        pitem = &g_error_code_table[code_type];
    }
    ret_len = pitem->comment[0];
    ret_ptr = &(pitem->comment[1]);
    if (FIRST_PART == n)    {
        *len = ret_len - 1;
        pthread_rwlock_unlock(&g_error_table_lock);
        return ret_ptr;
    }
    if (SECOND_PART == n)   {
        ret_ptr = ret_ptr + ret_len + 1;
        ret_len = pitem->comment[ret_len + 1];

        if (ret_len > 0) {
            *len = ret_len - 1;
            pthread_rwlock_unlock(&g_error_table_lock);
            return ret_ptr;
        }
    }
    pthread_rwlock_unlock(&g_error_table_lock);
    debug_info("[resource]: not exits nth in resource_get_pop3_code");
    return NULL;
}

BOOL resource_refresh_pop3_code_table()
{
    POP3_ERROR_CODE *pnew_table = NULL;

    if (0 != resource_construct_pop3_table(
        &pnew_table)) {
        return FALSE;
    }

    pthread_rwlock_wrlock(&g_error_table_lock);
    if (NULL != g_error_code_table) {
        free(g_error_code_table);
    }
    g_error_code_table = pnew_table;
    pthread_rwlock_unlock(&g_error_table_lock);
    return TRUE;
}

static int resource_find_pop3_code_index(int native_code)
{
    int i;

    for (i = 0; i < POP3_CODE_COUNT; i++) {
        if (g_default_pop3_error_code_table[i].code == native_code) {
            return i;
        }
    }
    return -1;
}

