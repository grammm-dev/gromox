#ifndef _H_TYPE_CONVERSION_
#define _H_TYPE_CONVERSION_
#include "php.h"
#include "types.h"

zend_bool php_to_string_array(zval *pzval,
	STRING_ARRAY *pnames TSRMLS_DC);

zend_bool php_to_longlong_array(zval *pzval,
	LONGLONG_ARRAY *plonglongs TSRMLS_DC);

zend_bool string_array_to_php(const STRING_ARRAY *pnames,
	zval **ppret TSRMLS_DC);

zend_bool php_to_dac_varray(zval *pzval,
	DAC_VARRAY *ppropvals TSRMLS_DC);

zend_bool php_to_dac_vset(zval *pzval,
	DAC_VSET *pset TSRMLS_DC);

zend_bool php_to_dac_res(zval *pzval, DAC_RES *pres TSRMLS_DC);

zend_bool dac_varray_to_php(const DAC_VARRAY *ppropvals,
	zval **ppret TSRMLS_DC);
	
zend_bool dac_vset_to_php(const DAC_VSET *pset,
	zval **ppret TSRMLS_DC);

#endif /* _H_TYPE_CONVERSION_ */
