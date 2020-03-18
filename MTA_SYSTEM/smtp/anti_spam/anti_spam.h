#ifndef _H_ANTI_SPAMMING_
#define _H_ANTI_SPAMMING_

#include "common_types.h"
#include "smtp_parser.h"
#include "plugin.h"

/* enumeration for indicating the reslut of filter */
enum{
    MESSAGE_ACCEPT,   /* this message is not spam */
    MESSAGE_REJECT,   /* reject this spam with answer string 550 ... */
    MESSAGE_RETRYING, /* reject this mail with answer string 450 ... */
};

/* enumeration for indicating the callback type of filter function */
enum{
    ACTION_BLOCK_NEW,         /* a new block is now available */
    ACTION_BLOCK_PROCESSING,  /* processing a block */
    ACTION_BLOCK_FREE,        /* a block is now free */
};

/* struct only for anti-spam auditor */
typedef struct _MAIL_ENTITY{
    ENVELOP_INFO    *penvelop;
    MAIL_HEAD       *phead;
} MAIL_ENTITY;

typedef struct _MAIL_WHOLE{
    ENVELOP_INFO    *penvelop;
    MAIL_HEAD       *phead;
    MAIL_BODY       *pbody;
} MAIL_WHOLE;
/* struct only for anti-spam filter */
typedef struct _MAIL_BLOCK{
    int        block_ID;
    MEM_FILE   *fp_mime_info;
    BOOL       is_parsed;
    char       *original_buff;
    size_t     original_length;
    char       *parsed_buff;
    size_t     parsed_length;
} MAIL_BLOCK;

typedef int (*JUDGE_FUNCTION)(int, ENVELOP_INFO*, CONNECTION*, char*, int);
typedef int (*AUDITOR_FUNCTION)(int, MAIL_ENTITY*, CONNECTION*, char*, int);
typedef int (*FILTER_FUNCTION)(int, int, MAIL_BLOCK*, char*, int);
typedef int (*STATISTIC_FUNCTION)(int, MAIL_WHOLE*, CONNECTION*, char*, int);

void anti_spam_init(const char *path);

int anti_spam_run();

int anti_spam_unload_library(const char* path);

int anti_spam_load_library(const char* path);

int anti_spam_reload_library(const char* path);

int anti_spam_pass_judges(SMTP_CONTEXT* pcontext, char *reason,
    int length);

int anti_spam_pass_auditors(SMTP_CONTEXT* pcontext, char *reason,
    int length);

void anti_spam_inform_filters(const char *type, SMTP_CONTEXT *pcontext,
    int action, int block_ID);

int anti_spam_pass_filters(const char *type, SMTP_CONTEXT* pcontext,
    MAIL_BLOCK *pblock, char *reason, int length);

int anti_spam_pass_statistics(SMTP_CONTEXT* pcontext, char *reason,
    int length);

int anti_spam_console_talk(int argc, char **argv, char *result,int length);

void anti_spam_enum_plugins(ENUM_PLUGINS enum_func);

void anti_spam_threads_event_proc(int action);

int anti_spam_stop();

void anti_spam_free();

#endif

