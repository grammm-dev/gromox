#ifndef _H_RESOURCE_
#define _H_RESOURCE_
#include "common_types.h"

enum {
    RES_LISTEN_PORT = 0,
	RES_LISTEN_SSL_PORT,
    RES_HOST_ID,
	RES_DEFAULT_DOMAIN,
	RES_DOMAIN_LIST_VALID,

    RES_CONTEXT_NUM,
    RES_CONTEXT_AVERAGE_MEM,
    RES_CONTEXT_MAX_MEM,

    RES_SMTP_MAX_MAIL_NUM,
    RES_SMTP_RUNNING_MODE,
    RES_SMTP_NEED_AUTH,
    RES_SMTP_AUTH_TIMES,
    RES_SMTP_CONN_TIMEOUT,
    RES_SMTP_SUPPORT_PIPELINE,
	RES_SMTP_SUPPORT_STARTTLS,
	RES_SMTP_CERTIFICATE_PATH,
	RES_SMTP_CERTIFICATE_PASSWD,
	RES_SMTP_PRIVATE_KEY_PATH,
	RES_SMTP_FORCE_STARTTLS,

    RES_THREAD_INIT_NUM,
    RES_THREAD_CHARGE_NUM,

    RES_MAIL_MAX_LENGTH,

    RES_ANTI_SPAMMING_INIT_PATH,
    RES_SMTP_RETURN_CODE_PATH,

    RES_CONSOLE_SERVER_IP,
    RES_CONSOLE_SERVER_PORT,

    RES_SERVICE_PLUGIN_PATH,
    RES_FLUSHER_PLUGIN_PATH,
    
    RES_RUNNING_IDENTITY,
    RES_BLOCK_INTERVAL_AUTHS,
    RES_BLOCK_INTERVAL_SESSIONS,
    RES_CONFIG_FILE_PATH,
    RES_DATA_FILE_PATH,
	RES_LOGIN_SCRIPT_PATH,
    MAX_RES_CONFG_VAR_NUM
};

typedef struct _SMTP_ERROR_CODE {
    int     code;
    char    comment[512];
} SMTP_ERROR_CODE;

enum {
    SMTP_CODE_2172001 = 0,
    SMTP_CODE_2172002,
    SMTP_CODE_2172003,
    SMTP_CODE_2172004,
    SMTP_CODE_2172005,
    SMTP_CODE_2172006,
    SMTP_CODE_2172007,
    SMTP_CODE_2172008,
    SMTP_CODE_2172009,
    SMTP_CODE_2172010,

    SMTP_CODE_2173001,
    SMTP_CODE_2173002,
    SMTP_CODE_2173003,
    SMTP_CODE_2173004,

    SMTP_CODE_2174001,
    SMTP_CODE_2174002,
    SMTP_CODE_2174003,
    SMTP_CODE_2174004,
    SMTP_CODE_2174005, 
    SMTP_CODE_2174006,
    SMTP_CODE_2174007, 
    SMTP_CODE_2174008, 
    SMTP_CODE_2174009, 
    SMTP_CODE_2174010, 
    SMTP_CODE_2174011, 
    SMTP_CODE_2174012, 
    SMTP_CODE_2174013, 
    SMTP_CODE_2174014, 
    SMTP_CODE_2174015, 
    SMTP_CODE_2174016, 
    SMTP_CODE_2174017, 
    SMTP_CODE_2174018, 
    SMTP_CODE_2174019, 
    SMTP_CODE_2174020,

    SMTP_CODE_2175001,
    SMTP_CODE_2175002,
    SMTP_CODE_2175003,
    SMTP_CODE_2175004,
    SMTP_CODE_2175005,
    SMTP_CODE_2175006,
    SMTP_CODE_2175007,
    SMTP_CODE_2175008,
    SMTP_CODE_2175009,
    SMTP_CODE_2175010,
    SMTP_CODE_2175011,
    SMTP_CODE_2175012,
    SMTP_CODE_2175013,
    SMTP_CODE_2175014,
    SMTP_CODE_2175015,
    SMTP_CODE_2175016,
    SMTP_CODE_2175017,
    SMTP_CODE_2175018,
    SMTP_CODE_2175019,
    SMTP_CODE_2175020,
    SMTP_CODE_2175021,
    SMTP_CODE_2175022,
    SMTP_CODE_2175023,
    SMTP_CODE_2175024,
    SMTP_CODE_2175025,
    SMTP_CODE_2175026,
    SMTP_CODE_2175027,
    SMTP_CODE_2175028,
    SMTP_CODE_2175029,
    SMTP_CODE_2175030,
    SMTP_CODE_2175031,
    SMTP_CODE_2175032,
    SMTP_CODE_2175033, 
    SMTP_CODE_2175034, 
    SMTP_CODE_2175035,
    SMTP_CODE_2175036,
    SMTP_CODE_COUNT
};

void resource_init(char* cfg_filename);

void resource_free();

int resource_run();

int resource_stop();

BOOL resource_save();

BOOL resource_get_integer(int key, int* value);

const char* resource_get_string(int key);

BOOL resource_set_integer(int key, int value);

BOOL resource_set_string(int key, char* value);

char* resource_get_smtp_code(int code_type, int n, int *len);

BOOL resource_refresh_smtp_code_table();


#endif /* _H_RESOURCE_ */
