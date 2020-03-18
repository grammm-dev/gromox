#ifndef _H_MOD_CACHE_
#define _H_MOD_CACHE_
#include "common_types.h"

struct _HTTP_CONTEXT;

void mod_cache_init(int context_num, const char *list_path);

int mod_cache_run();

int mod_cache_stop();

void mod_cache_free();

BOOL mod_cache_check_caching(struct _HTTP_CONTEXT *phttp);

BOOL mod_cache_get_context(struct _HTTP_CONTEXT *phttp);

void mod_cache_put_context(struct _HTTP_CONTEXT *phttp);

BOOL mod_cache_check_responded(struct _HTTP_CONTEXT *phttp);

BOOL mod_cache_read_response(struct _HTTP_CONTEXT *phttp);

#endif /* _H_MOD_CACHE_ */

