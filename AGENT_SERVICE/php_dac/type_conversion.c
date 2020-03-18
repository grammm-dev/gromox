#include "type_conversion.h"

zend_bool php_to_string_array(zval *pzval,
	STRING_ARRAY *pnames TSRMLS_DC)
{
	int i;
	char *pstring;
	zval **ppentry;
	HashTable *ptarget_hash;

	if (NULL == pzval) {
		return 0;
	}
	ptarget_hash = HASH_OF(pzval);
	if (NULL == ptarget_hash) {
		return 0;
	}
	pnames->count = zend_hash_num_elements(ptarget_hash);
	if (0 == pnames->count) {
		pnames->ppstr = NULL;
		return 1;
	}
	pnames->ppstr = emalloc(sizeof(void*)*pnames->count);
	zend_hash_internal_pointer_reset(ptarget_hash);
	for (i=0; i<pnames->count; i++) {
		zend_hash_get_current_data(ptarget_hash, (void**)&ppentry);
		convert_to_string_ex(ppentry);
		pstring = emalloc(ppentry[0]->value.str.len + 1);
		if (NULL == pstring) {
			return 0;
		}
		pnames->ppstr[i] = pstring;
		memcpy(pstring, ppentry[0]->value.str.val,
						ppentry[0]->value.str.len);
		pstring[ppentry[0]->value.str.len] = '\0';
		pnames->ppstr[i] = pstring;
		zend_hash_move_forward(ptarget_hash);
	}
	return 1;
}

zend_bool php_to_longlong_array(zval *pzval,
	LONGLONG_ARRAY *plonglongs TSRMLS_DC)
{
	int i;
	zval **ppentry;
	HashTable *ptarget_hash;

	if (NULL == pzval) {
		return 0;
	}
	ptarget_hash = HASH_OF(pzval);
	if (NULL == ptarget_hash) {
		return 0;
	}
	plonglongs->count = zend_hash_num_elements(ptarget_hash);
	if (0 == plonglongs->count) {
		plonglongs->pll = NULL;
		return 1;
	}
	plonglongs->pll = emalloc(sizeof(uint64_t)*plonglongs->count);
	zend_hash_internal_pointer_reset(ptarget_hash);
	for (i=0; i<plonglongs->count; i++) {
		zend_hash_get_current_data(ptarget_hash, (void**)&ppentry);
		convert_to_long_ex(ppentry);
		plonglongs->pll[i] = ppentry[0]->value.lval;
		zend_hash_move_forward(ptarget_hash);
	}
	return 1;
}

zend_bool string_array_to_php(const STRING_ARRAY *pnames,
	zval **ppret TSRMLS_DC)
{
	int i;
	zval *pzret;

	MAKE_STD_ZVAL(pzret);
	array_init(pzret);
	for (i=0; i<pnames->count; i++) {
		add_next_index_string(pzret, pnames->ppstr[i], 1);
	}
	*ppret = pzret;
	return 1;
}

static void* php_to_propval(zval **ppentry, uint16_t proptype)
{
	int i;
	void *pvalue;
	char *pstring;
	zval **ppdata_entry;
	HashTable *pdata_hash;
	
	switch(proptype) {
	case PROPVAL_TYPE_DOUBLE:
		convert_to_double_ex(ppentry);
		pvalue = emalloc(sizeof(double));
		if (NULL == pvalue) {
			return NULL;
		}
		*(double*)pvalue = ppentry[0]->value.dval;
		break;
	case PROPVAL_TYPE_LONGLONG:
		convert_to_long_ex(ppentry);
		pvalue = emalloc(sizeof(uint64_t));
		if (NULL == pvalue) {
			return NULL;
		}
		*(uint64_t*)pvalue = (uint64_t)ppentry[0]->value.lval;
		break;
	case PROPVAL_TYPE_BYTE:
		convert_to_boolean_ex(ppentry);
		pvalue = emalloc(sizeof(uint8_t));
		if (NULL == pvalue) {
			return NULL;
		}
		*(uint8_t*)pvalue = ppentry[0]->value.lval;
		break;
	case PROPVAL_TYPE_BINARY:
		convert_to_string_ex(ppentry);
		pvalue = emalloc(sizeof(BINARY));
		if (NULL == pvalue) {
			return NULL;
		}
		((BINARY*)pvalue)->cb = ppentry[0]->value.str.len;
		if (0 == ppentry[0]->value.str.len) {
			((BINARY*)pvalue)->pb = NULL;
		} else {
			((BINARY*)pvalue)->pb = emalloc(ppentry[0]->value.str.len);
			if (NULL == ((BINARY*)pvalue)->pb) {
				return NULL;
			}
			memcpy(((BINARY*)pvalue)->pb,
				ppentry[0]->value.str.val,
				ppentry[0]->value.str.len);
		}
		break;
	case PROPVAL_TYPE_STRING:
		convert_to_string_ex(ppentry);
		pvalue = emalloc(ppentry[0]->value.str.len + 1);
		if (NULL == pvalue) {
			return NULL;
		}
		memcpy(pvalue, ppentry[0]->value.str.val,
			ppentry[0]->value.str.len);
		((char*)pvalue)[ppentry[0]->value.str.len] = '\0';
		break;
	case PROPVAL_TYPE_GUID:
		convert_to_string_ex(ppentry);
		if (ppentry[0]->value.str.len != sizeof(GUID)) {
			return NULL;
		}
		pvalue = emalloc(sizeof(GUID));
		if (NULL == pvalue) {
			return NULL;
		}
		memcpy(pvalue, ppentry[0]->value.str.val, sizeof(GUID));
		break;
	case PROPVAL_TYPE_LONGLONG_ARRAY:
		pdata_hash = HASH_OF(ppentry[0]);
		if (NULL == pdata_hash) {
			return NULL;
		}
		pvalue = emalloc(sizeof(LONGLONG_ARRAY));
		if (NULL == pvalue) {
			return NULL;
		}
		((LONGLONG_ARRAY*)pvalue)->count =
			zend_hash_num_elements(pdata_hash);
		if (0 == ((LONGLONG_ARRAY*)pvalue)->count) {
			((LONGLONG_ARRAY*)pvalue)->pll = NULL;
			break;
		}
		((LONGLONG_ARRAY*)pvalue)->pll =
			emalloc(sizeof(uint64_t)*
			((LONGLONG_ARRAY*)pvalue)->count);
		if (NULL == ((LONGLONG_ARRAY*)pvalue)->pll) {
			return NULL;
		}
		zend_hash_internal_pointer_reset(pdata_hash);
		for (i=0; i<((LONGLONG_ARRAY*)pvalue)->count; i++) {
			zend_hash_get_current_data(
				pdata_hash, (void**)&ppdata_entry);
			convert_to_long_ex(ppdata_entry);
			((LONGLONG_ARRAY*)pvalue)->pll[i] =
					ppdata_entry[0]->value.lval;
			zend_hash_move_forward(pdata_hash);
		}
		break;
	case PROPVAL_TYPE_STRING_ARRAY:
		pdata_hash = HASH_OF(ppentry[0]);
		if (NULL == pdata_hash) {
			return NULL;
		}
		pvalue = emalloc(sizeof(STRING_ARRAY));
		if (NULL == pvalue) {
			return NULL;
		}
		((STRING_ARRAY*)pvalue)->count =
			zend_hash_num_elements(pdata_hash);
		if (0 == ((STRING_ARRAY*)pvalue)->count) {
			((STRING_ARRAY*)pvalue)->ppstr = NULL;
			break;
		}
		((STRING_ARRAY*)pvalue)->ppstr = emalloc(
			sizeof(char*)*((STRING_ARRAY*)pvalue)->count);
		if (NULL == ((STRING_ARRAY*)pvalue)->ppstr) {
			return NULL;
		}
		zend_hash_internal_pointer_reset(pdata_hash);
		for (i=0; i<((STRING_ARRAY*)pvalue)->count; i++) {
			zend_hash_get_current_data(pdata_hash, (void**)&ppdata_entry);
			convert_to_string_ex(ppdata_entry);
			pstring = emalloc(ppdata_entry[0]->value.str.len + 1);
			if (NULL == pstring) {
				return NULL;
			}
			((STRING_ARRAY*)pvalue)->ppstr[i] = pstring;
			memcpy(pstring, ppdata_entry[0]->value.str.val,
							ppdata_entry[0]->value.str.len);
			pstring[ppdata_entry[0]->value.str.len] = '\0';
			zend_hash_move_forward(pdata_hash);
		}
		break;
	default:
		return NULL;
	}
	return pvalue;
}

zend_bool php_to_dac_varray(zval *pzval,
	DAC_VARRAY *ppropvals TSRMLS_DC)
{
	int i;
	char *ptoken;
	char *pstring;
	zval **ppentry;
	unsigned long idx;
	HashTable *ptarget_hash;
	
	ptarget_hash = HASH_OF(pzval);
	if (NULL == ptarget_hash) {
		return 0;
	}
	ppropvals->count = zend_hash_num_elements(ptarget_hash);
	if (0 == ppropvals->count) {
	   ppropvals->ppropval = NULL;
	   return 1;
	}
	ppropvals->ppropval = emalloc(sizeof(
			DAC_PROPVAL)*ppropvals->count);
	if (NULL == ppropvals->ppropval) {
		return 0;
	}
	zend_hash_internal_pointer_reset(ptarget_hash);
	for (i=0; i<ppropvals->count; i++) {
		zend_hash_get_current_data(ptarget_hash, (void**)&ppentry);
		zend_hash_get_current_key(ptarget_hash, &pstring, &idx, 0);
		if (NULL == pstring || NULL == (ptoken = strchr(
			pstring, ':')) || ptoken == pstring) {
			return 0;
		}
		ptoken ++;
		if (0 == strcasecmp(ptoken, "bool")) {
			ppropvals->ppropval[i].type = PROPVAL_TYPE_BYTE;
		} else if (0 == strcasecmp(ptoken, "int")) {
			ppropvals->ppropval[i].type = PROPVAL_TYPE_LONGLONG;
		} else if (0 == strcasecmp(ptoken, "double")) {
			ppropvals->ppropval[i].type = PROPVAL_TYPE_DOUBLE;
		} else if (0 == strcasecmp(ptoken, "text")) {
			ppropvals->ppropval[i].type = PROPVAL_TYPE_STRING;
		} else if (0 == strcasecmp(ptoken, "guid")) {
			ppropvals->ppropval[i].type = PROPVAL_TYPE_GUID;
		} else if (0 == strcasecmp(ptoken, "blob")) {
			ppropvals->ppropval[i].type = PROPVAL_TYPE_BINARY;
		} else if (0 == strcasecmp(ptoken, "texts")) {
			ppropvals->ppropval[i].type = PROPVAL_TYPE_STRING_ARRAY;
		} else if (0 == strcasecmp(ptoken, "ints")) {
			ppropvals->ppropval[i].type = PROPVAL_TYPE_LONGLONG_ARRAY;
		} else {
			return 0;
		}
		ppropvals->ppropval[i].pname = emalloc(ptoken - pstring);
		if (NULL == ppropvals->ppropval[i].pname) {
			return 0;
		}
		memcpy(ppropvals->ppropval[i].pname,
			pstring, ptoken - pstring - 1);
		ppropvals->ppropval[i].pname[ptoken - pstring - 1] = '\0';
		ppropvals->ppropval[i].pvalue = php_to_propval(
				ppentry, ppropvals->ppropval[i].type);
		if (NULL == ppropvals->ppropval[i].pvalue) {
			return 0;
		}
		zend_hash_move_forward(ptarget_hash);
	}
	return 1;
}

zend_bool php_to_dac_vset(zval *pzval, DAC_VSET *pset TSRMLS_DC)
{
	int i;
	zval **ppentry;
	HashTable *ptarget_hash;
	
	if (NULL == pzval) {
		return 0;
	}
	if (pzval->type != IS_ARRAY) {
		return 0;
	}
	ptarget_hash = HASH_OF(pzval);
	if (NULL == ptarget_hash) {
		return 0;
	}
	pset->count = zend_hash_num_elements(ptarget_hash);
	if (0 == pset->count) {
		pset->pparray = NULL;
		return 1;
	}
	pset->pparray = emalloc(sizeof(DAC_VARRAY*)*pset->count);
	if (NULL == pset->pparray) {
		return 0;
	}
	zend_hash_internal_pointer_reset(ptarget_hash);
	for (i=0; i<pset->count; i++) {
		zend_hash_get_current_data(ptarget_hash, (void**)&ppentry);
		if (ppentry[0]->type != IS_ARRAY) {
			return 0;
		}
		pset->pparray[i] = emalloc(sizeof(DAC_VARRAY));
		if (NULL == pset->pparray[i]) {
			return 0;
		}
		if (!php_to_dac_varray(ppentry[0], pset->pparray[i] TSRMLS_CC)) {
			return 0;
		}
		zend_hash_move_forward(ptarget_hash);
	}
	return 1;
}

zend_bool php_to_dac_res(zval *pzval, DAC_RES *pres TSRMLS_DC)
{
	int i;
	zval **pptype_entry;
	zval **ppvalue_entry;
	HashTable *pres_hash;
	HashTable *pdata_hash;
	DAC_VARRAY tmp_propvals;
	
	pres_hash = HASH_OF(pzval);
	if (NULL == pres_hash || zend_hash_num_elements(pres_hash) != 2) {
		return 0;
	}
	zend_hash_internal_pointer_reset(pres_hash);
	/* 0=>type, 1=>value array */
	zend_hash_get_current_data(pres_hash, (void**)&pptype_entry);
	zend_hash_move_forward(pres_hash);
	zend_hash_get_current_data(pres_hash, (void**)&ppvalue_entry);
	pres->rt = pptype_entry[0]->value.lval;
	pdata_hash = HASH_OF(ppvalue_entry[0]);
	if (NULL == pdata_hash) {
		return 0;
	}
	zend_hash_internal_pointer_reset(pdata_hash);
	switch(pres->rt) {
	case DAC_RES_TYPE_AND:
	case DAC_RES_TYPE_OR:
		pres->pres = emalloc(sizeof(DAC_RES_AND_OR));
		if (NULL == pres->pres) {
			return 0;
		}
		((DAC_RES_AND_OR*)pres->pres)->count =
				zend_hash_num_elements(pdata_hash);
		((DAC_RES_AND_OR*)pres->pres)->pres = emalloc(sizeof(
			DAC_RES)*((DAC_RES_AND_OR*)pres->pres)->count);
		if (NULL == ((DAC_RES_AND_OR*)pres->pres)->pres) {
			return 0;
		}
		for (i=0; i<((DAC_RES_AND_OR*)pres->pres)->count; i++) {
			zend_hash_get_current_data(pdata_hash, (void**)&ppvalue_entry);
			if (!php_to_dac_res(ppvalue_entry[0],
				&((DAC_RES_AND_OR*)pres->pres)->pres[i] TSRMLS_CC)) {
				return 0;
			}
			zend_hash_move_forward(pdata_hash);
		}
		break;
	case DAC_RES_TYPE_NOT:
		pres->pres = emalloc(sizeof(DAC_RES_NOT));
		if (NULL == pres->pres) {
			return 0;
		}
		zend_hash_get_current_data(pdata_hash, (void**)&ppvalue_entry);
		if (!php_to_dac_res(ppvalue_entry[0],
			&((DAC_RES_NOT*)pres->pres)->res TSRMLS_CC)) {
			return 0;
		}
		break;
	case DAC_RES_TYPE_SUBOBJ:
		pres->pres = emalloc(sizeof(DAC_RES_SUBOBJ));
		if (NULL == pres->pres) {
			return 0;
		}
		if (zend_hash_find(pdata_hash, "name", 5,
			(void**)&ppvalue_entry) == FAILURE) {
			return 0;
		}
		convert_to_string_ex(ppvalue_entry);
		((DAC_RES_SUBOBJ*)pres->pres)->pname =
			emalloc(ppvalue_entry[0]->value.str.len + 1);
		if (NULL == ((DAC_RES_SUBOBJ*)pres->pres)->pname) {
			return 0;
		}
		memcpy(((DAC_RES_SUBOBJ*)pres->pres)->pname,
					ppvalue_entry[0]->value.str.val,
					ppvalue_entry[0]->value.str.len);
		((DAC_RES_SUBOBJ*)pres->pres)->pname[
			ppvalue_entry[0]->value.str.len] = '\0';
		if (zend_hash_find(pdata_hash, "restriction",
			12, (void**)&ppvalue_entry) == FAILURE) {
			return 0;
		}
		if (!php_to_dac_res(ppvalue_entry[0],
			&((DAC_RES_SUBOBJ*)pres->pres)->res TSRMLS_CC)) {
			return 0;	
		}
		break;
	case DAC_RES_TYPE_CONTENT:
		pres->pres = emalloc(sizeof(DAC_RES_CONTENT));
		if (NULL == pres->pres) {
			return 0;
		}
		if (zend_hash_find(pdata_hash, "fl", 3,
			(void**)&ppvalue_entry) == FAILURE) {
			return 0;
		}
		convert_to_long_ex(ppvalue_entry);
		((DAC_RES_CONTENT*)pres->pres)->fl =
				ppvalue_entry[0]->value.lval;
		if (zend_hash_find(pdata_hash, "propval",
			8, (void**)&ppvalue_entry) == FAILURE) {
			return 0;
		}
		if (ppvalue_entry[0]->type != IS_ARRAY) {
			return 0;
		}
		if (!php_to_dac_varray(ppvalue_entry[0],
			&tmp_propvals TSRMLS_CC)) {
			return 0;
		}
		if (1 != tmp_propvals.count) {
			return 0;
		}
		((DAC_RES_CONTENT*)pres->pres)->propval =
							*tmp_propvals.ppropval;
		break;
	case DAC_RES_TYPE_PROPERTY:
		pres->pres = emalloc(sizeof(DAC_RES_PROPERTY));
		if (NULL == pres->pres) {
			return 0;
		}
		if (zend_hash_find(pdata_hash, "relop", 6,
			(void**)&ppvalue_entry) == FAILURE) {
			return 0;
		}
		convert_to_long_ex(ppvalue_entry);
		((DAC_RES_PROPERTY*)pres->pres)->relop =
					ppvalue_entry[0]->value.lval;
		if (zend_hash_find(pdata_hash, "propval",
			8, (void**)&ppvalue_entry) == FAILURE) {
			return 0;
		}
		if (ppvalue_entry[0]->type != IS_ARRAY) {
			return 0;
		}
		if (!php_to_dac_varray(ppvalue_entry[0],
			&tmp_propvals TSRMLS_CC)) {
			return 0;
		}
		if (1 != tmp_propvals.count) {
			return 0;
		}
		((DAC_RES_PROPERTY*)pres->pres)->propval =
							*tmp_propvals.ppropval;
		break;
		
		break;
	case DAC_RES_TYPE_EXIST:
		pres->pres = emalloc(sizeof(DAC_RES_EXIST));
		if (NULL == pres->pres) {
			return 0;
		}
		if (zend_hash_find(pdata_hash, "name", 5,
			(void**)&ppvalue_entry) == FAILURE) {
			return 0;
		}
		convert_to_string_ex(ppvalue_entry);
		((DAC_RES_EXIST*)pres->pres)->pname =
			emalloc(ppvalue_entry[0]->value.str.len + 1);
		if (NULL == ((DAC_RES_EXIST*)pres->pres)->pname) {
			return 0;
		}
		memcpy(((DAC_RES_EXIST*)pres->pres)->pname,
					ppvalue_entry[0]->value.str.val,
					ppvalue_entry[0]->value.str.len);
		((DAC_RES_EXIST*)pres->pres)->pname[
			ppvalue_entry[0]->value.str.len] = '\0';
		break;
	default:
		return 0;
	}
	return 1;
}

zend_bool dac_varray_to_php(const DAC_VARRAY *ppropvals,
	zval **ppret TSRMLS_DC)
{
	int i, j;
	zval *pzret;
	zval *pzmval;
	DAC_PROPVAL *ppropval;
	char name_string[1024];
	
	MAKE_STD_ZVAL(pzret);
	array_init(pzret);
	for (i=0; i<ppropvals->count; i++) {
		ppropval = &ppropvals->ppropval[i];
		switch (ppropval->type) {
		case PROPVAL_TYPE_DOUBLE:
			snprintf(name_string, sizeof(name_string),
						"%s:double", ppropval->pname);
			add_assoc_double(pzret, name_string,
					*(double*)ppropval->pvalue);
			break;
		case PROPVAL_TYPE_LONGLONG:
			snprintf(name_string, sizeof(name_string),
						"%s:int", ppropval->pname);
 			add_assoc_long(pzret, name_string,
				*(uint64_t*)ppropval->pvalue);
			break;
		case PROPVAL_TYPE_BYTE:
			snprintf(name_string, sizeof(name_string),
						"%s:bool", ppropval->pname);
			add_assoc_bool(pzret, name_string,
				*(uint8_t*)ppropval->pvalue);
			break;
		case PROPVAL_TYPE_STRING:
			snprintf(name_string, sizeof(name_string),
						"%s:text", ppropval->pname);
			add_assoc_string(pzret, name_string,
							ppropval->pvalue, 1);
			break;
		case PROPVAL_TYPE_BINARY:
			snprintf(name_string, sizeof(name_string),
						"%s:blob", ppropval->pname);
			add_assoc_stringl(pzret, name_string,
				((BINARY*)ppropval->pvalue)->pb,
				((BINARY*)ppropval->pvalue)->cb, 1);
			break;
		case PROPVAL_TYPE_GUID:
			snprintf(name_string, sizeof(name_string),
						"%s:guid", ppropval->pname);
			add_assoc_stringl(pzret, name_string,
				ppropval->pvalue, sizeof(GUID), 1);
			break;
		case PROPVAL_TYPE_LONGLONG_ARRAY:
			snprintf(name_string, sizeof(name_string),
						"%s:ints", ppropval->pname);
			MAKE_STD_ZVAL(pzmval);
			array_init(pzmval);
			for (j=0; j<((LONGLONG_ARRAY*)ppropval->pvalue)->count; j++) {
				add_next_index_long(pzmval,
					((LONGLONG_ARRAY*)ppropval->pvalue)->pll[j]);
			}
			add_assoc_zval(pzret, name_string, pzmval);
			break;
		case PROPVAL_TYPE_STRING_ARRAY:
			snprintf(name_string, sizeof(name_string),
						"%s:texts", ppropval->pname);
			MAKE_STD_ZVAL(pzmval);
			array_init(pzmval);
			for (j=0; j<((STRING_ARRAY*)ppropval->pvalue)->count; j++) {
				add_next_index_string(pzmval,
					((STRING_ARRAY*)ppropval->pvalue)->ppstr[j], 1);
			}
			add_assoc_zval(pzret, name_string, pzmval);
			break;
		case PROPVAL_TYPE_OBJECT:
			snprintf(name_string, sizeof(name_string),
						"%s:table", ppropval->pname);
			add_assoc_null(pzret, name_string);
			break;
		}
	}
	*ppret = pzret;
	return 1;
}

zend_bool dac_vset_to_php(const DAC_VSET *pset, zval **ppret TSRMLS_DC)
{
	int i;
	zval *pret;
	zval *pzpropval;
	
	MAKE_STD_ZVAL(pret);
	array_init(pret);
	for (i=0; i<pset->count; i++) {
		dac_varray_to_php(pset->pparray[i], &pzpropval TSRMLS_CC);
		zend_hash_next_index_insert(HASH_OF(pret),
				&pzpropval, sizeof(zval*), NULL);
	}
	*ppret = pret;
	return 1;
}
