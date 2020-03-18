#ifndef PHP_DAC_H
#define PHP_DAC_H 1

#ifdef ZTS
#include "TSRM.h"
#endif

ZEND_BEGIN_MODULE_GLOBALS(dac)
// this is the global hresult value, used in *every* php dac function
	unsigned long hr;
ZEND_END_MODULE_GLOBALS(dac)

#ifdef ZTS
#define DAC_G(v) TSRMG(dac_globals_id, zend_dac_globals *, v)
#else
#define DAC_G(v) (dac_globals.v)
#endif

#define PHP_DAC_VERSION "1.0"
#define PHP_DAC_EXTNAME "dac"

/* All the functions that will be exported (available) must be declared */
PHP_MINIT_FUNCTION(dac);
PHP_MINFO_FUNCTION(dac);
PHP_MSHUTDOWN_FUNCTION(dac);
PHP_RINIT_FUNCTION(dac);
PHP_RSHUTDOWN_FUNCTION(dac);

ZEND_FUNCTION(dac_last_hresult);

ZEND_FUNCTION(dac_get_addressbook);
ZEND_FUNCTION(dac_get_abhierarchy);
ZEND_FUNCTION(dac_get_abcontent);
ZEND_FUNCTION(dac_restrict_abcontent);
ZEND_FUNCTION(dac_get_exaddress);
ZEND_FUNCTION(dac_get_exaddresstype);
ZEND_FUNCTION(dac_check_mlistinclude);
ZEND_FUNCTION(dac_alloc_dir);
ZEND_FUNCTION(dac_free_dir);
ZEND_FUNCTION(dac_set_propvals);
ZEND_FUNCTION(dac_get_propvals);
ZEND_FUNCTION(dac_delete_propvals);
ZEND_FUNCTION(dac_open_table);
ZEND_FUNCTION(dac_open_celltable);
ZEND_FUNCTION(dac_restrict_table);
ZEND_FUNCTION(dac_update_cells);
ZEND_FUNCTION(dac_sum_table);
ZEND_FUNCTION(dac_query_rows);
ZEND_FUNCTION(dac_insert_rows);
ZEND_FUNCTION(dac_delete_row);
ZEND_FUNCTION(dac_delete_cells);
ZEND_FUNCTION(dac_delete_table);
ZEND_FUNCTION(dac_close_table);
ZEND_FUNCTION(dac_match_row);
ZEND_FUNCTION(dac_get_namespaces);
ZEND_FUNCTION(dac_get_propnames);
ZEND_FUNCTION(dac_get_tablenames);
ZEND_FUNCTION(dac_read_file);
ZEND_FUNCTION(dac_append_file);
ZEND_FUNCTION(dac_remove_file);
ZEND_FUNCTION(dac_stat_file);
ZEND_FUNCTION(dac_php_rpc);

extern zend_module_entry dac_module_entry;
#define phpext_dac_ptr &dac_module_entry

#endif
