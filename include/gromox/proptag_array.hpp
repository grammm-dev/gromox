#pragma once
#include <cstdint>
#include <gromox/defs.h>
#include <gromox/mapi_types.hpp>

extern PROPTAG_ARRAY *proptag_array_init();
void proptag_array_free_internal(PROPTAG_ARRAY *pproptags);

void proptag_array_free(PROPTAG_ARRAY *pproptags);

void proptag_array_clear(PROPTAG_ARRAY *pproptags);
extern GX_EXPORT bool proptag_array_append(PROPTAG_ARRAY *, uint32_t tag);
void proptag_array_remove(PROPTAG_ARRAY *pproptags, uint32_t proptag);
extern GX_EXPORT bool proptag_array_check(const PROPTAG_ARRAY *, uint32_t tag);
PROPTAG_ARRAY* proptag_array_dup(const PROPTAG_ARRAY *pproptags);
