#pragma once
#include <cstdint>
#include <gromox/mail_func.hpp>
#include <gromox/mapi_types.hpp>

struct USER_OBJECT {
	int base_id;
	uint32_t minid;
};

USER_OBJECT* user_object_create(int base_id, uint32_t minid);

void user_object_free(USER_OBJECT *puser);

BOOL user_object_check_valid(USER_OBJECT *puser);

BOOL user_object_get_properties(USER_OBJECT *puser,
	const PROPTAG_ARRAY *pproptags, TPROPVAL_ARRAY *ppropvals);
