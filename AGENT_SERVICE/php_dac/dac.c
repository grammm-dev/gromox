#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"
#include "php_dac.h"
#include "ext_pack.h"
#include "adac_client.h"
#include "type_conversion.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef struct _DAC_TABLE {
	char *address;
	uint32_t table_id;
} DAC_TABLE;

static zend_function_entry dac_functions[] = {
	ZEND_FE(dac_last_hresult, NULL)
	ZEND_FE(dac_get_addressbook, NULL)
	ZEND_FE(dac_get_abhierarchy, NULL)
	ZEND_FE(dac_get_abcontent, NULL)
	ZEND_FE(dac_restrict_abcontent, NULL)
	ZEND_FE(dac_get_exaddress, NULL)
	ZEND_FE(dac_get_exaddresstype, NULL)
	ZEND_FE(dac_check_mlistinclude, NULL)
	ZEND_FE(dac_alloc_dir, NULL)
	ZEND_FE(dac_free_dir, NULL)
	ZEND_FE(dac_set_propvals, NULL)
	ZEND_FE(dac_get_propvals, NULL)
	ZEND_FE(dac_delete_propvals, NULL)
	ZEND_FE(dac_open_table, NULL)
	ZEND_FE(dac_open_celltable, NULL)
	ZEND_FE(dac_restrict_table, NULL)
	ZEND_FE(dac_update_cells, NULL)
	ZEND_FE(dac_sum_table, NULL)
	ZEND_FE(dac_query_rows, NULL)
	ZEND_FE(dac_insert_rows, NULL)
	ZEND_FE(dac_delete_row, NULL)
	ZEND_FE(dac_delete_cells, NULL)
	ZEND_FE(dac_delete_table, NULL)
	ZEND_FE(dac_close_table, NULL)
	ZEND_FE(dac_match_row, NULL)
	ZEND_FE(dac_get_namespaces, NULL)
	ZEND_FE(dac_get_propnames, NULL)
	ZEND_FE(dac_get_tablenames, NULL)
	ZEND_FE(dac_read_file, NULL)
	ZEND_FE(dac_append_file, NULL)
	ZEND_FE(dac_remove_file, NULL)
	ZEND_FE(dac_stat_file, NULL)
	ZEND_FE(dac_php_rpc, NULL)
	{NULL, NULL, NULL}
};

ZEND_DECLARE_MODULE_GLOBALS(dac)

static void php_dac_init_globals(zend_dac_globals *dac_globals)
{
	/* nothing to do */
}

zend_module_entry dac_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_DAC_EXTNAME,
    dac_functions,
    PHP_MINIT(dac),
    PHP_MSHUTDOWN(dac),
    PHP_RINIT(dac),
	PHP_RSHUTDOWN(dac),
    PHP_MINFO(dac),
    PHP_DAC_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_DAC
ZEND_GET_MODULE(dac)
#endif


static char name_dac_table[] = "DAC Table";

static int le_dac_table;


static void dac_table_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	DAC_TABLE *ptable;

	if (NULL == rsrc->ptr) {
		return;
	}
	ptable = (DAC_TABLE*)rsrc->ptr;
	adac_client_closetable(ptable->address, ptable->table_id);
	efree(ptable->address);
	efree(ptable);
}

PHP_MINIT_FUNCTION(dac)
{
	le_dac_table = zend_register_list_destructors_ex(
		dac_table_dtor, NULL, name_dac_table, module_number);
	ZEND_INIT_MODULE_GLOBALS(dac, php_dac_init_globals, NULL);
	return SUCCESS;
}

PHP_MINFO_FUNCTION(dac)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "DAC Support", "enabled");
	php_info_print_table_row(2, "Version", "1.0");
	php_info_print_table_end();
}

PHP_MSHUTDOWN_FUNCTION(dac)
{
	return SUCCESS;
}

PHP_RINIT_FUNCTION(dac)
{
	DAC_G(hr) = 0;
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(dac)
{
	return SUCCESS;
}

/*---------------------------------------------------------------------------*/

ZEND_FUNCTION(dac_last_hresult)
{
	RETURN_LONG(DAC_G(hr));
}

ZEND_FUNCTION(dac_get_addressbook)
{
	char *address;
	int address_len;
	uint32_t result;
	char ab_address[1024];
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &address, &address_len) == FAILURE ||
		NULL == address || 0 == address_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_getaddressbook(address, ab_address);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_STRINGL(ab_address, strlen(ab_address), 1);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_get_abhierarchy)
{
	zval *pzset;
	char *address;
	int address_len;
	uint32_t result;
	DAC_VSET tmp_set;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &address, &address_len) == FAILURE ||
		NULL == address || 0 == address_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_getabhierarchy(address, &tmp_set);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	if (!dac_vset_to_php(&tmp_set, &pzset TSRMLS_CC)) {
		DAC_G(hr) = EC_ERROR;
		RETVAL_FALSE;
		return;
	}
	RETVAL_ZVAL(pzset, 0, 0);
	FREE_ZVAL(pzset);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_get_abcontent)
{
	long count;
	zval *pzset;
	char *address;
	long start_pos;
	int address_len;
	uint32_t result;
	DAC_VSET tmp_set;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll",
		&address, &address_len, &start_pos, &count) == FAILURE
		|| NULL == address || 0 == address_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	if (count > 0xFFFF) {
		count = 0xFFFF;
	}
	result = adac_client_getabcontent(
		address, start_pos, count, &tmp_set);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	if (!dac_vset_to_php(&tmp_set, &pzset TSRMLS_CC)) {
		DAC_G(hr) = EC_ERROR;
		RETVAL_FALSE;
		return;
	}
	RETVAL_ZVAL(pzset, 0, 0);
	FREE_ZVAL(pzset);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_restrict_abcontent)
{
	long count;
	zval *pzset;
	char *address;
	long start_pos;
	int address_len;
	uint32_t result;
	DAC_VSET tmp_set;
	DAC_RES restriction;
	zval *pzrestrictarray;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sal",
		&address, &address_len, &pzrestrictarray, &count) ==
		FAILURE || NULL == address || 0 == address_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	if (count > 0xFFFF) {
		count = 0xFFFF;
	}
	if (!php_to_dac_res(pzrestrictarray, &restriction TSRMLS_CC)) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_restrictabcontent(
		address, &restriction, count, &tmp_set);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	if (!dac_vset_to_php(&tmp_set, &pzset TSRMLS_CC)) {
		DAC_G(hr) = EC_ERROR;
		RETVAL_FALSE;
		return;
	}
	RETVAL_ZVAL(pzset, 0, 0);
	FREE_ZVAL(pzset);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_get_exaddress)
{
	char *address;
	int address_len;
	uint32_t result;
	char ex_address[1024];
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &address, &address_len) == FAILURE ||
		NULL == address || 0 == address_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_getexaddress(address, ex_address);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_STRINGL(ex_address, strlen(ex_address), 1);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_get_exaddresstype)
{
	char *address;
	int address_len;
	uint32_t result;
	uint8_t address_type;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &address, &address_len) == FAILURE ||
		NULL == address || 0 == address_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_getexaddresstype(address, &address_type);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_LONG(address_type);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_check_mlistinclude)
{
	int mlist_len;
	int account_len;
	uint32_t result;
	zend_bool b_include;
	char *mlist_address;
	char *account_address;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss",
		&mlist_address, &mlist_len, &account_address, &account_len)
		== FAILURE || NULL == mlist_address || 0 == mlist_len ||
		NULL == account_address || 0 == account_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_processor_checkmlistinclude(
		mlist_address, account_address, &b_include);
	DAC_G(hr) = EC_SUCCESS;
	if (EC_SUCCESS != result) {
		RETVAL_FALSE;
	} else {
		if (0 == b_include) {
			RETVAL_FALSE;
		} else {
			RETVAL_TRUE;
		}
	}
}

ZEND_FUNCTION(dac_alloc_dir)
{
	long type;
	char dir[256];
	uint32_t result;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS()
		TSRMLS_CC, "l", &type) == FAILURE) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	if (DAC_DIR_TYPE_SIMPLE != type &&
		DAC_DIR_TYPE_PRIVATE != type &&
		DAC_DIR_TYPE_PUBLIC != type) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;	
	}
	result = adac_client_allocdir(type, dir);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_STRINGL(dir, strlen(dir), 1);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_free_dir)
{
	char *dir;
	int dir_len;
	uint32_t result;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS()
		TSRMLS_CC, "s", &dir, &dir_len) == FAILURE) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_freedir(dir);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_TRUE;
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_set_propvals)
{
	char *address;
	uint32_t result;
	int address_len;
	char *pnamespace;
	zval *pzpropvals;
	int namespace_len;
	DAC_VARRAY propvals;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &address,
		&address_len, &pnamespace, &namespace_len, &pzpropvals) == FAILURE
		|| NULL == address || 0 == address_len || NULL == pnamespace ||
		0 == namespace_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	if (!php_to_dac_varray(pzpropvals, &propvals TSRMLS_CC)) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_setpropvals(address, pnamespace, &propvals);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_TRUE;
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_get_propvals)
{
	char *address;
	zval *pznames;
	uint32_t result;
	int address_len;
	char *pnamespace;
	zval *pzpropvals;
	int namespace_len;
	STRING_ARRAY names;
	DAC_VARRAY propvals;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &address,
		&address_len, &pnamespace, &namespace_len, &pznames) == FAILURE
		|| NULL == address || 0 == address_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	if (NULL != pnamespace && '\0' == pnamespace[0]) {
		pnamespace = NULL;
	}
	if (!php_to_string_array(pznames, &names TSRMLS_CC)) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_getpropvals(address,
			pnamespace, &names, &propvals);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	if (!dac_varray_to_php(&propvals, &pzpropvals TSRMLS_CC)) {
		DAC_G(hr) = EC_ERROR;
		RETVAL_FALSE;
		return;
	}
	RETVAL_ZVAL(pzpropvals, 0, 0);
	FREE_ZVAL(pzpropvals);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_delete_propvals)
{
	char *address;
	zval *pznames;
	uint32_t result;
	int address_len;
	char *pnamespace;
	int namespace_len;
	STRING_ARRAY names;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &address,
		&address_len, &pnamespace, &namespace_len, &pznames) == FAILURE
		|| NULL == address || 0 == address_len || NULL == pnamespace ||
		0 == namespace_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	if (!php_to_string_array(pznames, &names TSRMLS_CC)) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_deletepropvals(address, pnamespace, &names);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_TRUE;
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_open_table)
{
	long flags;
	char *address;
	int address_len;
	uint32_t result;
	char *pnamespace;
	char *ptablename;
	char *puniquename;
	uint32_t table_id;
	int namespace_len;
	int tablename_len;
	DAC_TABLE *ptable;
	int uniquename_len;
	
	flags = 0;
	puniquename = NULL;
	uniquename_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"sss|sl", &address, &address_len, &pnamespace,
		&namespace_len, &ptablename, &tablename_len,
		&puniquename, &uniquename_len, &flags) == FAILURE
		|| NULL == address || 0 == address_len || NULL
		== pnamespace || 0 == namespace_len || NULL ==
		ptablename || 0 == tablename_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	if (0 == uniquename_len) {
		puniquename = NULL;
	}
	result = adac_client_opentable(address, pnamespace,
			ptablename, flags, puniquename, &table_id);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	ptable = emalloc(sizeof(DAC_TABLE));
	if (NULL == ptable) {
		DAC_G(hr) = EC_OUT_OF_MEMORY;
		RETVAL_FALSE;
		return;
	}
	ptable->address = emalloc(address_len + 1);
	if (NULL == ptable->address) {
		DAC_G(hr) = EC_OUT_OF_MEMORY;
		RETVAL_FALSE;
		return;
	}
	strcpy(ptable->address, address);
	ptable->table_id = table_id;
	ZEND_REGISTER_RESOURCE(return_value, ptable, le_dac_table);
	DAC_G(hr) = EC_SUCCESS;
	return;
}

ZEND_FUNCTION(dac_open_celltable)
{
	long flags;
	char *address;
	char *pcolname;
	int colname_len;
	int address_len;
	uint32_t result;
	DAC_TABLE *ptable;
	long row_instance;
	char *puniquename;
	uint32_t table_id;
	int uniquename_len;
	
	flags = 0;
	puniquename = NULL;
	uniquename_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sls|sl",
		&address, &address_len, &row_instance, &pcolname, &colname_len,
		&puniquename, &uniquename_len, &flags)== FAILURE || NULL == address
		|| 0 == address_len || NULL == pcolname || 0 == colname_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	if (0 == uniquename_len) {
		puniquename = NULL;
	}
	result = adac_client_opencelltable(address, row_instance,
					pcolname, flags, puniquename, &table_id);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	ptable = emalloc(sizeof(DAC_TABLE));
	if (NULL == ptable) {
		DAC_G(hr) = EC_OUT_OF_MEMORY;
		RETVAL_FALSE;
		return;
	}
	ptable->address = emalloc(address_len + 1);
	if (NULL == ptable->address) {
		DAC_G(hr) = EC_OUT_OF_MEMORY;
		RETVAL_FALSE;
		return;
	}
	strcpy(ptable->address, address);
	ptable->table_id = table_id;
	ZEND_REGISTER_RESOURCE(return_value, ptable, le_dac_table);
	DAC_G(hr) = EC_SUCCESS;
	return;
}

ZEND_FUNCTION(dac_restrict_table)
{
	zval *pztable;
	uint32_t result;
	DAC_TABLE *ptable;
	DAC_RES restriction;
	zval *pzrestrictarray;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ra",
		&pztable, &pzrestrictarray) == FAILURE || NULL == pztable
		|| NULL == pzrestrictarray) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	ZEND_FETCH_RESOURCE(ptable, DAC_TABLE*,
		&pztable, -1, name_dac_table, le_dac_table);
	if (!php_to_dac_res(pzrestrictarray, &restriction TSRMLS_CC)) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_restricttable(ptable->address,
						ptable->table_id, &restriction);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	DAC_G(hr) = EC_SUCCESS;
	RETVAL_TRUE;
}

ZEND_FUNCTION(dac_update_cells)
{
	zval *pzcells;
	char *address;
	int address_len;
	uint32_t result;
	DAC_VARRAY cells;
	long row_instance;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sla",
		&address, &address_len, &row_instance, &pzcells) == FAILURE
		|| NULL == address || 0 == address_len || NULL == pzcells) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	if (!php_to_dac_varray(pzcells, &cells TSRMLS_CC)) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_updatecells(address, row_instance, &cells);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_TRUE;
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_sum_table)
{
	zval *pztable;
	uint32_t count;
	uint32_t result;
	DAC_TABLE *ptable;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"r", &pztable) == FAILURE || NULL == pztable) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	ZEND_FETCH_RESOURCE(ptable, DAC_TABLE*,
		&pztable, -1, name_dac_table, le_dac_table);
	result = adac_client_sumtable(ptable->address,
						ptable->table_id, &count);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_LONG(count);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_query_rows)
{
	zval *pzset;
	zval *pztable;
	long row_count;
	long start_pos;
	uint32_t result;
	DAC_VSET tmp_set;
	DAC_TABLE *ptable;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"rll", &pztable, &start_pos, &row_count) == FAILURE
		|| NULL == pztable || start_pos > 0xFFFFFFFF ||
		row_count > 0xFFFFFFFF) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	ZEND_FETCH_RESOURCE(ptable, DAC_TABLE*,
		&pztable, -1, name_dac_table, le_dac_table);
	result = adac_client_queryrows(ptable->address,
		ptable->table_id, start_pos, row_count, &tmp_set);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	if (!dac_vset_to_php(&tmp_set, &pzset TSRMLS_CC)) {
		DAC_G(hr) = EC_ERROR;
		RETVAL_FALSE;
		return;
	}
	RETVAL_ZVAL(pzset, 0, 0);
	FREE_ZVAL(pzset);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_insert_rows)
{
	long flags;
	zval *pzset;
	zval *pztable;
	uint32_t result;
	DAC_VSET tmp_set;
	DAC_TABLE *ptable;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"rla", &pztable, &flags, &pzset) == FAILURE
		|| NULL == pztable || NULL == pzset) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	ZEND_FETCH_RESOURCE(ptable, DAC_TABLE*,
		&pztable, -1, name_dac_table, le_dac_table);
	if (!php_to_dac_vset(pzset, &tmp_set TSRMLS_CC)) {
		DAC_G(hr) = EC_ERROR;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_insertrows(ptable->address,
				ptable->table_id, flags, &tmp_set);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_TRUE;
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_delete_row)
{
	char *address;
	int address_len;
	uint32_t result;
	long row_instance;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl",
		&address, &address_len, &row_instance) == FAILURE ||
		NULL == address || 0 == address_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_deleterow(address, row_instance);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_TRUE;
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_delete_cells)
{
	char *address;
	zval *pznames;
	int address_len;
	uint32_t result;
	long row_instance;
	STRING_ARRAY names;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sla",
		&address, &address_len, &row_instance, &pznames) ==
		FAILURE || NULL == address || 0 == address_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	if (!php_to_string_array(pznames, &names TSRMLS_CC)) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_deletecells(address, row_instance, &names);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_TRUE;
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_delete_table)
{
	char *address;
	uint32_t result;
	int address_len;
	char *ptablename;
	char *pnamespace;
	int tablename_len;
	int namespace_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss",
		&address, &address_len, &pnamespace, &namespace_len,
		&ptablename, &tablename_len) == FAILURE || NULL ==
		address || 0 == address_len || NULL == pnamespace ||
		0 == namespace_len || NULL == ptablename || 0 ==
		tablename_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_deletetable(address, pnamespace, ptablename);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_TRUE;
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_close_table)
{
	zval *pztable;
	uint32_t result;
	DAC_TABLE *ptable;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"r", &pztable) == FAILURE || NULL == pztable) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	ZEND_FETCH_RESOURCE(ptable, DAC_TABLE*,
		&pztable, -1, name_dac_table, le_dac_table);
	result = adac_client_closetable(
		ptable->address, ptable->table_id);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_TRUE;
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_match_row)
{
	double dval;
	char *ptype;
	zval *pzval;
	zval *pzrow;
	uint8_t bval;
	void *pvalue;
	int type_len;
	uint64_t lval;
	uint16_t type;
	zval *pztable;
	uint32_t result;
	DAC_TABLE *ptable;
	DAC_VARRAY tmp_row;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"rsz", &pztable, &ptype, &type_len, &pzval) ==
		FAILURE || NULL == pztable || NULL == ptype ||
		0 == type_len ||NULL == pzval) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	ZEND_FETCH_RESOURCE(ptable, DAC_TABLE*,
		&pztable, -1, name_dac_table, le_dac_table);
	if (0 == strcasecmp(ptype, "bool")) {
		convert_to_boolean(pzval);
		bval = pzval->value.lval;
		pvalue = &bval;
		type = PROPVAL_TYPE_BYTE;
	} else if (0 == strcasecmp(ptype, "int")) {
		convert_to_long(pzval);
		lval = pzval->value.lval;
		pvalue = &lval;
		type = PROPVAL_TYPE_LONGLONG;
	} else if (0 == strcasecmp(ptype, "double")) {
		convert_to_double(pzval);
		dval = pzval->value.dval;
		pvalue = &dval;
		type = PROPVAL_TYPE_DOUBLE;
	} else if (0 == strcasecmp(ptype, "text")) {
		convert_to_string(pzval);
		pvalue = emalloc(pzval->value.str.len + 1);
		if (NULL == pvalue) {
			DAC_G(hr) = EC_OUT_OF_MEMORY;
			RETVAL_FALSE;
			return;
		}
		memcpy(pvalue, pzval->value.str.val,
			pzval->value.str.len);
		((char*)pvalue)[pzval->value.str.len] = '\0';
		type = PROPVAL_TYPE_STRING;
	} else if (0 == strcasecmp(ptype, "guid")) {
		convert_to_string(pzval);
		if (pzval->value.str.len != sizeof(GUID)) {
			DAC_G(hr) = EC_INVALID_PARAMETER;
			RETVAL_FALSE;
			return;
		}
		pvalue = emalloc(sizeof(GUID));
		if (NULL == pvalue) {
			DAC_G(hr) = EC_OUT_OF_MEMORY;
			RETVAL_FALSE;
			return;
		}
		memcpy(pvalue, pzval->value.str.val, sizeof(GUID));
		type = PROPVAL_TYPE_GUID;
	} else if (0 == strcasecmp(ptype, "blob")) {
		convert_to_string(pzval);
		pvalue = emalloc(sizeof(BINARY));
		if (NULL == pvalue) {
			DAC_G(hr) = EC_OUT_OF_MEMORY;
			RETVAL_FALSE;
			return;
		}
		((BINARY*)pvalue)->cb = pzval->value.str.len;
		if (0 == pzval->value.str.len) {
			((BINARY*)pvalue)->pb = NULL;
		} else {
			((BINARY*)pvalue)->pb = emalloc(pzval->value.str.len);
			if (NULL == ((BINARY*)pvalue)->pb) {
				DAC_G(hr) = EC_OUT_OF_MEMORY;
				RETVAL_FALSE;
				return;
			}
			memcpy(((BINARY*)pvalue)->pb,
					pzval->value.str.val,
					pzval->value.str.len);
		}
		type = PROPVAL_TYPE_BINARY;
	} else if (0 == strcasecmp(ptype, "texts")) {
		pvalue = emalloc(sizeof(STRING_ARRAY));
		if (NULL == pvalue) {
			DAC_G(hr) = EC_OUT_OF_MEMORY;
			RETVAL_FALSE;
			return;
		}
		if (!php_to_string_array(pzval, pvalue TSRMLS_CC)) {
			DAC_G(hr) = EC_INVALID_PARAMETER;
			RETVAL_FALSE;
			return;
		}
		type = PROPVAL_TYPE_STRING_ARRAY;
	} else if (0 == strcasecmp(ptype, "ints")) {
		pvalue = emalloc(sizeof(LONGLONG_ARRAY));
		if (NULL == pvalue) {
			DAC_G(hr) = EC_OUT_OF_MEMORY;
			RETVAL_FALSE;
			return;
		}
		if (!php_to_longlong_array(pzval, pvalue TSRMLS_CC)) {
			DAC_G(hr) = EC_INVALID_PARAMETER;
			RETVAL_FALSE;
			return;
		}
		type = PROPVAL_TYPE_LONGLONG_ARRAY;
	} else {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_matchrow(ptable->address,
		ptable->table_id, type, pvalue, &tmp_row);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	if (!dac_varray_to_php(&tmp_row, &pzrow TSRMLS_CC)) {
		DAC_G(hr) = EC_ERROR;
		RETVAL_FALSE;
		return;
	}
	RETVAL_ZVAL(pzrow, 0, 0);
	FREE_ZVAL(pzrow);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_get_namespaces)
{
	zval *pznames;
	char *address;
	int address_len;
	uint32_t result;
	STRING_ARRAY tmp_names;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"s", &address, &address_len) == FAILURE ||
		NULL == address || 0 == address_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_getnamespaces(address, &tmp_names);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	if (!string_array_to_php(&tmp_names, &pznames TSRMLS_CC)) {
		DAC_G(hr) = EC_ERROR;
		RETVAL_FALSE;
		return;
	}
	RETVAL_ZVAL(pznames, 0, 0);
	FREE_ZVAL(pznames);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_get_propnames)
{
	zval *pznames;
	char *address;
	int address_len;
	uint32_t result;
	char *pnamespace;
	int namespace_len;
	STRING_ARRAY tmp_names;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss",
		&address, &address_len, &pnamespace, &namespace_len)
		== FAILURE || NULL == address || 0 == address_len ||
		NULL == pnamespace || 0 == namespace_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_getpropnames(address, pnamespace, &tmp_names);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	if (!string_array_to_php(&tmp_names, &pznames TSRMLS_CC)) {
		DAC_G(hr) = EC_ERROR;
		RETVAL_FALSE;
		return;
	}
	RETVAL_ZVAL(pznames, 0, 0);
	FREE_ZVAL(pznames);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_get_tablenames)
{
	zval *pznames;
	char *address;
	int address_len;
	uint32_t result;
	char *pnamespace;
	int namespace_len;
	STRING_ARRAY tmp_names;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss",
		&address, &address_len, &pnamespace, &namespace_len)
		== FAILURE || NULL == address || 0 == address_len ||
		NULL == pnamespace || 0 == namespace_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_gettablenames(address, pnamespace, &tmp_names);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	if (!string_array_to_php(&tmp_names, &pznames TSRMLS_CC)) {
		DAC_G(hr) = EC_ERROR;
		RETVAL_FALSE;
		return;
	}
	RETVAL_ZVAL(pznames, 0, 0);
	FREE_ZVAL(pznames);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_read_file)
{
	char *path;
	long offset;
	long length;
	int path_len;
	char *address;
	char *filename;
	BINARY tmp_bin;
	uint32_t result;
	int address_len;
	int filename_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sssll", &address,
		&address_len, &path, &path_len, &filename, &filename_len, &offset,
		&length) == FAILURE || NULL == address || 0 == address_len ||
		NULL == path || 0 == path || NULL == filename || 0 == filename_len
		|| offset < 0 || length < 0 || length > 0xFFFFFFFF) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_readfile(address, path,
			filename, offset, length, &tmp_bin);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_STRINGL(tmp_bin.pb, tmp_bin.cb, 0);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_append_file)
{
	char *path;
	int path_len;
	char *address;
	char *filename;
	BINARY tmp_bin;
	char *pcontent;
	int content_len;
	uint32_t result;
	int address_len;
	int filename_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssss", &address,
		&address_len, &path, &path_len, &filename, &filename_len, &pcontent,
		&content_len) == FAILURE || NULL == address || 0 == address_len ||
		NULL == path || 0 == path || NULL == filename || 0 == filename_len
		|| NULL == pcontent || 0 == content_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	tmp_bin.pb = pcontent;
	tmp_bin.cb = content_len;
	result = adac_client_appendfile(address, path, filename, &tmp_bin);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_TRUE;
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_remove_file)
{
	char *path;
	int path_len;
	char *address;
	char *filename;
	uint32_t result;
	int address_len;
	int filename_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &address,
		&address_len, &path, &path_len, &filename, &filename_len) == FAILURE
		|| NULL == address || 0 == address_len || NULL == path || 0 == path
		|| NULL == filename || 0 == filename_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_removefile(address, path, filename);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_TRUE;
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_stat_file)
{
	char *path;
	int path_len;
	char *address;
	uint64_t size;
	char *filename;
	uint32_t result;
	int address_len;
	int filename_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &address,
		&address_len, &path, &path_len, &filename, &filename_len) == FAILURE
		|| NULL == address || 0 == address_len || NULL == path ||
		0 == path_len || NULL == filename || 0 == filename_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	result = adac_client_statfile(address, path, filename, &size);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_LONG(size);
	DAC_G(hr) = EC_SUCCESS;
}

ZEND_FUNCTION(dac_php_rpc)
{
	int path_len;
	char *address;
	char *in_buff;
	BINARY in_bin;
	BINARY out_bin;
	int inbuff_len;
	uint32_t result;
	int address_len;
	char *script_path;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &address,
		&address_len, &script_path, &path_len, &in_buff, &inbuff_len)
		== FAILURE || NULL == address || 0 == address_len || NULL ==
		script_path || 0 == path_len || NULL == in_buff || 0 == inbuff_len) {
		DAC_G(hr) = EC_INVALID_PARAMETER;
		RETVAL_FALSE;
		return;
	}
	in_bin.pb = in_buff;
	in_bin.cb = inbuff_len;
	result = adac_client_phprpc(address, script_path, &in_bin, &out_bin);
	if (EC_SUCCESS != result) {
		DAC_G(hr) = result;
		RETVAL_FALSE;
		return;
	}
	RETVAL_STRINGL(out_bin.pb, out_bin.cb, 0);
	DAC_G(hr) = EC_SUCCESS;
}
