#ifndef _H_url_wildcard_
#define _H_url_wildcard_
#include "common_types.h"

void url_wildcard_init(const char *list_path);

int url_wildcard_run();

int url_wildcard_stop();

void url_wildcard_free();

BOOL url_wildcard_query(const char *url);

BOOL url_wildcard_refresh();

#endif /* _H_url_wildcard_ */