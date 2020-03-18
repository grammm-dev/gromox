#include "util.h"
#include "ab_tree.h"
#include "str_hash.h"
#include "list_file.h"
#include "ext_buffer.h"
#include "common_util.h"
#include "alloc_context.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <strings.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

typedef struct _ENVIRONMENT_CONTEXT {
	ALLOC_CONTEXT allocator;
} ENVIRONMENT_CONTEXT;

static char g_org_name[256];
static pthread_key_t g_env_key;

void common_util_init(const char *org_name)
{
	strcpy(g_org_name, org_name);
	pthread_key_create(&g_env_key, NULL);
}

int common_util_run()
{
	return 0;
}

int common_util_stop()
{
	return 0;
}

void common_util_free()
{
	pthread_key_delete(g_env_key);
}

BOOL common_util_build_environment()
{
	ENVIRONMENT_CONTEXT *pctx;
	
	pctx = malloc(sizeof(ENVIRONMENT_CONTEXT));
	if (NULL == pctx) {
		return FALSE;
	}
	alloc_context_init(&pctx->allocator);
	pthread_setspecific(g_env_key, pctx);
}

void common_util_free_environment()
{
	ENVIRONMENT_CONTEXT *pctx;

	pctx = pthread_getspecific(g_env_key);
	pthread_setspecific(g_env_key, NULL);
	alloc_context_free(&pctx->allocator);
	free(pctx);
}

void* common_util_alloc(size_t size)
{
	ENVIRONMENT_CONTEXT *pctx;
	
	pctx = pthread_getspecific(g_env_key);
	return alloc_context_alloc(&pctx->allocator, size);
}

char* common_util_dup(const char *pstr)
{
	int len;
	char *pstr1;

	len = strlen(pstr) + 1;
	pstr1 = common_util_alloc(len);
	if (NULL == pstr1) {
		return NULL;
	}
	memcpy(pstr1, pstr, len);
	return pstr1;
}

int common_util_get_essdn_type(const char *essdn)
{
	int tmp_len;
	char *ptoken;
	char tmp_essdn[1024];

	tmp_len = sprintf(tmp_essdn,
		"/o=%s/ou=Exchange Administrative Group"
		" (FYDIBOHF23SPDLT)/cn=", g_org_name);
	if (0 != strncasecmp(essdn, tmp_essdn, tmp_len)) {
		return ESSDN_TYPE_ERROR;
	}
	if (0 == strncasecmp(essdn + tmp_len, "Share/", 6)) {
		ptoken = strchr(essdn + tmp_len + 6, '/');
		if (NULL == ptoken) {
			return ESSDN_TYPE_DOMAIN;
		}
		if (0 == strcasecmp(ptoken, "/cn=GAL")) {
			return ESSDN_TYPE_GAL;
		}
	} else if (0 == strncasecmp(essdn + tmp_len,
		"Configuration/cn=Groups/cn=", 27)) {
		ptoken = strchr(essdn + tmp_len + 27, '/');
		if (NULL == ptoken) {
			return ESSDN_TYPE_GROUP;
		}
	} else if (0 == strncasecmp(essdn + tmp_len,
		"Configuration/cn=Classes/cn=", 28)) {
		ptoken = strchr(essdn + tmp_len + 28, '/');
		if (NULL == ptoken) {
			return ESSDN_TYPE_CLASS;
		}
	} else if (0 == strncasecmp(essdn + tmp_len, "Recipients/", 11)) {
		ptoken = strchr(essdn + tmp_len + 11, '/');
		if (NULL == ptoken) {
			return ESSDN_TYPE_USER;
		}
	} else if (0 == strncasecmp(essdn + tmp_len, "Aggregation/", 12)) {
		ptoken = strchr(essdn + tmp_len + 12, '/');
		if (NULL == ptoken) {
			return ESSDN_TYPE_ORG;
		}
		if (0 == strcasecmp(ptoken, "/cn=GAL")) {
			return ESSDN_TYPE_GAL;
		}
	}
	return ESSDN_TYPE_UNKNOWN;
}

BOOL common_util_essdn_to_user_ids(const char *pessdn,
	int *pdomain_id, int *puser_id)
{
	int tmp_len;
	char tmp_essdn[1024];

	tmp_len = sprintf(tmp_essdn,
		"/o=%s/ou=Exchange Administrative Group "
		"(FYDIBOHF23SPDLT)/cn=Recipients/cn=",
		g_org_name);
	if (0 != strncasecmp(pessdn, tmp_essdn, tmp_len)) {
		return FALSE;
	}
	if ('-' != pessdn[tmp_len + 16]) {
		return FALSE;
	}
	*pdomain_id = decode_hex_int(pessdn + tmp_len);
	*puser_id = decode_hex_int(pessdn + tmp_len + 8);
	return TRUE;
}

BOOL common_util_essdn_to_base_id(
	const char *pessdn, int *pbase_id)
{
	int tmp_len;
	char tmp_essdn[1024];

	tmp_len = sprintf(tmp_essdn,
		"/o=%s/ou=Exchange Administrative Group"
		" (FYDIBOHF23SPDLT)/cn=", g_org_name);
	if (0 != strncasecmp(pessdn, tmp_essdn, tmp_len)) {
		return FALSE;
	}
	if (0 == strncasecmp(pessdn + tmp_len, "Share/cn=", 9)) {
		*pbase_id = decode_hex_int(pessdn + tmp_len + 9);
		*pbase_id *= -1;
		return TRUE;
	} else if (0 == strncasecmp(pessdn +
		tmp_len, "Aggregation/cn=", 15)) {
		*pbase_id = decode_hex_int(pessdn + tmp_len + 15);
		return TRUE;
	}
	return FALSE;
}

BOOL common_util_essdn_to_domain_id(
	const char *pessdn, int *pdomain_id)
{
	int tmp_len;
	char tmp_essdn[1024];

	tmp_len = sprintf(tmp_essdn,
		"/o=%s/ou=Exchange Administrative Group"
		" (FYDIBOHF23SPDLT)/cn=", g_org_name);
	if (0 != strncasecmp(pessdn, tmp_essdn, tmp_len)) {
		return FALSE;
	}
	if (0 == strncasecmp(pessdn + tmp_len, "Share/cn=", 9)) {
		*pdomain_id = decode_hex_int(pessdn + tmp_len + 9);
		return TRUE;
	}
	return FALSE;
}

BOOL common_util_essdn_to_group_id(const char *pessdn,
	int *pdomain_id, int *pgroup_id)
{
	int tmp_len;
	char tmp_essdn[1024];

	tmp_len = sprintf(tmp_essdn,
		"/o=%s/ou=Exchange Administrative Group"
		" (FYDIBOHF23SPDLT)/cn=", g_org_name);
	if (0 != strncasecmp(pessdn, tmp_essdn, tmp_len)) {
		return FALSE;
	}
	if (0 == strncasecmp(pessdn + tmp_len,
		"Configuration/cn=Groups/cn=", 27)) {
		*pdomain_id = decode_hex_int(pessdn + tmp_len + 27);
		*pgroup_id = decode_hex_int(pessdn + tmp_len + 35);
		return TRUE;
	}
	return FALSE;
}

BOOL common_util_essdn_to_class_id(const char *pessdn,
	int *pdomain_id, int *pclass_id)
{
	int tmp_len;
	char tmp_essdn[1024];

	tmp_len = sprintf(tmp_essdn,
		"/o=%s/ou=Exchange Administrative Group"
		" (FYDIBOHF23SPDLT)/cn=", g_org_name);
	if (0 != strncasecmp(pessdn, tmp_essdn, tmp_len)) {
		return FALSE;
	}
	if (0 == strncasecmp(pessdn + tmp_len,
		"Configuration/cn=Classes/cn=", 28)) {
		*pdomain_id = decode_hex_int(pessdn + tmp_len + 28);
		*pclass_id = decode_hex_int(pessdn + tmp_len + 36);
		return TRUE;
	}
	return FALSE;
}

BOOL common_util_get_abhierarchy_from_node(
	SIMPLE_TREE_NODE *pnode, DAC_VSET *pset)
{
	uint32_t count;
	DAC_VARRAY **pparray;
	static char* tmp_strs[] = {
		"display_name",
		"node_type",
		"has_sub",
		"ex_address",
		"parent_address",
		"content_count"};
	static STRING_ARRAY names = {6, tmp_strs};
	
	count = (pset->count / 100 + 1) * 100;
	if (pset->count + 1 >= count) {
		count += 100;
		pparray = common_util_alloc(count*sizeof(DAC_VARRAY*));
		if (NULL == pparray) {
			return FALSE;
		}
		memcpy(pparray, pset->pparray,
			pset->count*sizeof(DAC_VARRAY*));
		pset->pparray = pparray;
	}
	pset->pparray[pset->count] =
		common_util_alloc(sizeof(DAC_VARRAY));
	if (NULL == pset->pparray[pset->count]) {
		return FALSE;
	}
	if (FALSE == ab_tree_fetch_node_properties(pnode,
		&names, pset->pparray[pset->count])) {
		return FALSE;	
	}
	pset->count ++;
	if (TRUE == ab_tree_has_child(pnode)) {
		pnode = simple_tree_node_get_child(pnode);
		do {
			if (ab_tree_get_node_type(pnode) < 0x80) {
				continue;
			}
			if (FALSE == common_util_get_abhierarchy_from_node(
				pnode, pset)) {
				return FALSE;	
			}
		} while (pnode=simple_tree_node_get_slibling(pnode));
	}
	return TRUE;
}


BOOL common_util_get_dir(const char *address, char *dir)
{
	int base_id;
	int user_id;
	int domain_id;
	int essdn_type;
	AB_BASE *pbase;
	uint32_t minid;
	int address_type;
	SIMPLE_TREE_NODE *pnode;
	
	if (NULL != strchr(address, '@')) {
		if (FALSE == mysql_adaptor_get_user_ids(address,
			&user_id, &domain_id, &address_type)) {
			return FALSE;
		}
		if (ADDRESS_TYPE_VIRTUAL == address_type) {
			return FALSE;
		}
		base_id = (-1)*domain_id;
		pbase = ab_tree_get_base(base_id);
		if (NULL == pbase) {
			return FALSE;
		}
		minid = ab_tree_make_minid(MINID_TYPE_ADDRESS, user_id);
		pnode = ab_tree_minid_to_node(pbase, minid);
	} else {
		essdn_type = common_util_get_essdn_type(address);
		switch (essdn_type) {
		case ESSDN_TYPE_DOMAIN:
			if (FALSE == common_util_essdn_to_domain_id(address, &domain_id)) {
				return FALSE;
			}
			base_id = (-1)*domain_id;
			pbase = ab_tree_get_base(base_id);
			if (NULL == pbase) {
				return FALSE;
			}
			minid = ab_tree_make_minid(MINID_TYPE_DOMAIN, domain_id);
			pnode = ab_tree_minid_to_node(pbase, minid);
			break;
		case ESSDN_TYPE_GROUP:
		case ESSDN_TYPE_CLASS:
			/* TODO support simple dac node later */
			return FALSE;
		case ESSDN_TYPE_USER:
			if (FALSE == common_util_essdn_to_user_ids(
				address, &domain_id,  &user_id)) {
				return FALSE;	
			}
			base_id = (-1)*domain_id;
			pbase = ab_tree_get_base(base_id);
			if (NULL == pbase) {
				return FALSE;
			}
			minid = ab_tree_make_minid(MINID_TYPE_ADDRESS, user_id);
			pnode = ab_tree_minid_to_node(pbase, minid);
			break;
		default:
			return FALSE;
		}
	}
	if (NULL == pnode || FALSE == ab_tree_get_node_dir(pnode, dir)) {
		ab_tree_put_base(pbase);
		return FALSE;
	}
	ab_tree_put_base(pbase);
	return TRUE;
}

const char* common_util_get_org_name()
{
	return g_org_name;
}
