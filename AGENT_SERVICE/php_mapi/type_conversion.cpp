#include "type_conversion.h"
#include <iconv.h>
#include "ext.hpp"
#define TIME_FIXUP_CONSTANT_INT				11644473600LL

uint64_t unix_to_nttime(time_t unix_time)
{
	uint64_t nt_time; 

	nt_time = unix_time;
	nt_time += TIME_FIXUP_CONSTANT_INT;
	nt_time *= 10000000;
	return nt_time;
}

time_t nttime_to_unix(uint64_t nt_time)
{
	uint64_t unix_time;

	unix_time = nt_time;
	unix_time /= 10000000;
	unix_time -= TIME_FIXUP_CONSTANT_INT;
	return (time_t)unix_time;
}

/* in php-mapi the PROPVAL_TYPE_STRING means utf-8
	string we don't user PROPVAL_TYPE_WSTRING,
	there's no definition for ansi string */
uint32_t proptag_to_phptag(uint32_t proptag)
{
	uint32_t proptag1;
	
	proptag1 = proptag;
	switch (proptag & 0xFFFF) {
	case PROPVAL_TYPE_WSTRING:
		proptag1 &= 0xFFFF0000;
		proptag1 |= PROPVAL_TYPE_STRING;
		break;
	case PROPVAL_TYPE_WSTRING_ARRAY:
		proptag1 &= 0xFFFF0000;
		proptag1 |= PROPVAL_TYPE_STRING_ARRAY;
		break;
	}
	return proptag1;
}

uint32_t phptag_to_proptag(uint32_t proptag)
{
	uint32_t proptag1;
	
	proptag1 = proptag;
	switch (proptag & 0xFFFF) {
	case PROPVAL_TYPE_STRING:
		proptag1 &= 0xFFFF0000;
		proptag1 |= PROPVAL_TYPE_WSTRING;
		break;
	case PROPVAL_TYPE_STRING_ARRAY:
		proptag1 &= 0xFFFF0000;
		proptag1 |= PROPVAL_TYPE_WSTRING_ARRAY;
		break;
	}
	return proptag1;
}

zend_bool php_to_binary_array(zval *pzval,
	BINARY_ARRAY *pbins TSRMLS_DC)
{
	HashTable *ptarget_hash;
	
	if (NULL == pzval) {
		return 0;
	}
	ptarget_hash = HASH_OF(pzval);
	if (NULL == ptarget_hash) {
		return 0;
	}
	pbins->count = zend_hash_num_elements(Z_ARRVAL_P(pzval));
	if (0 == pbins->count) {
		pbins->pbin = NULL;
		return 1;
	}
	pbins->pbin = sta_malloc<BINARY>(pbins->count);
	if (NULL == pbins->pbin) {
		return 0;
	}

	size_t i = 0;
	zval *entry;
	ZEND_HASH_FOREACH_VAL(ptarget_hash, entry) {
		zstrplus str(zval_get_string(entry));
		pbins->pbin[i].cb = str->len;
		if (str->len == 0) {
			pbins->pbin[i].pb = NULL;
		} else {
			pbins->pbin[i].pb = sta_malloc<uint8_t>(pbins->pbin[i].cb);
			if (NULL == pbins->pbin[i].pb) {
				return 0;
			}
			memcpy(pbins->pbin[i].pb, str->val, str->len);
		}
		++i;
	} ZEND_HASH_FOREACH_END();
	return 1;
}

zend_bool binary_array_to_php(const BINARY_ARRAY *pbins,
	zval *pzval TSRMLS_DC)
{
	int i;
	
	array_init(pzval);
	for (i=0; i<pbins->count; i++) {
		add_next_index_stringl(
			pzval, reinterpret_cast<const char *>(pbins->pbin[i].pb),
			pbins->pbin[i].cb);
	}
	return 1;
}

zend_bool php_to_sortorder_set(zval *pzval,
	SORTORDER_SET *pset TSRMLS_DC)
{
	unsigned long idx;
	HashTable *ptarget_hash;
	
	if (NULL == pzval) {
		return 0;
	}
	ptarget_hash = HASH_OF(pzval);
	if (NULL == ptarget_hash) {
		return 0;
	}
	pset->count = zend_hash_num_elements(Z_ARRVAL_P(pzval));
	pset->ccategories = 0;
	pset->cexpanded = 0;
	if (0 == pset->count) {
		pset->psort = NULL;
		return 1;
	}
	pset->psort = sta_malloc<SORT_ORDER>(pset->count);
	if (NULL == pset->psort) {
		return 0;
	}

	zend_string *key;
	zval *entry;
	size_t i = 0;
	ZEND_HASH_FOREACH_KEY_VAL(ptarget_hash, idx, key, entry) {
		uint32_t proptag = phptag_to_proptag(key != nullptr ? atoi(key->val) : idx);
		pset->psort[i].propid = proptag >> 16;
		pset->psort[i].type = proptag & 0xFFFF;
		pset->psort[i].table_sort = zval_get_long(entry);
		++i;
	} ZEND_HASH_FOREACH_END();
	return 1;
}

zend_bool php_to_proptag_array(zval *pzval,
	PROPTAG_ARRAY *pproptags TSRMLS_DC)
{
	HashTable *ptarget_hash;
	
	if (NULL == pzval) {
		return 0;
	}
	ptarget_hash = HASH_OF(pzval);
	if (NULL == ptarget_hash) {
		return 0;
	}
	pproptags->count = zend_hash_num_elements(ptarget_hash);
	if (0 == pproptags->count) {
		pproptags->pproptag = NULL;
		return 1;
	}
	pproptags->pproptag = sta_malloc<uint32_t>(pproptags->count);

	size_t i = 0;
	zval *entry;
	ZEND_HASH_FOREACH_VAL(ptarget_hash, entry) {
		pproptags->pproptag[i++] = phptag_to_proptag(zval_get_long(entry));
	} ZEND_HASH_FOREACH_END();
	return 1;
}

static void *php_to_propval(zval *entry, uint16_t proptype)
{
	int j, k;
	void *pvalue;
	char *pstring;
	zval *data_entry;
	ACTION_BLOCK *pblock;
	HashTable *pdata_hash;
	HashTable *paction_hash;
	RESTRICTION	prestriction;
	HashTable *precipient_hash;
	TPROPVAL_ARRAY tmp_propvals;
	RECIPIENT_BLOCK *prcpt_block;
	zstrplus str_action(zend_string_init("action", sizeof("action") - 1, 0));
	zstrplus str_flags(zend_string_init("flags", sizeof("flags") - 1, 0));
	zstrplus str_flavor(zend_string_init("flavor", sizeof("flavor") - 1, 0));
	zstrplus str_storeentryid(zend_string_init("storeentryid", sizeof("storeentryid") - 1, 0));
	zstrplus str_folderentryid(zend_string_init("folderentryid", sizeof("folderentryid") - 1, 0));
	zstrplus str_replyentryid(zend_string_init("replyentryid", sizeof("replyentryid") - 1, 0));
	zstrplus str_replyguid(zend_string_init("replyguid", sizeof("replyguid") - 1, 0));
	zstrplus str_dam(zend_string_init("dam", sizeof("dam") - 1, 0));
	zstrplus str_code(zend_string_init("code", sizeof("code") - 1, 0));
	zstrplus str_adrlist(zend_string_init("adrlist", sizeof("adrlist") - 1, 0));
	zstrplus str_proptag(zend_string_init("proptag", sizeof("proptag") - 1, 0));
	
	switch(proptype)	{
	case PROPVAL_TYPE_SHORT:
		pvalue = emalloc(sizeof(uint16_t));
		if (NULL == pvalue) {
			return NULL;
		}
		*static_cast<uint16_t *>(pvalue) = zval_get_long(entry);
		break;
	case PROPVAL_TYPE_LONG:
	case PROPVAL_TYPE_ERROR:
		pvalue = emalloc(sizeof(uint32_t));
		if (NULL == pvalue) {
			return NULL;
		}
		*static_cast<uint32_t *>(pvalue) = zval_get_long(entry);
		break;
	case PROPVAL_TYPE_FLOAT:
		pvalue = emalloc(sizeof(float));
		if (NULL == pvalue) {
			return NULL;
		}
		*static_cast<float *>(pvalue) = zval_get_double(entry);
		break;
	case PROPVAL_TYPE_DOUBLE:
	case PROPVAL_TYPE_FLOATINGTIME:
		pvalue = emalloc(sizeof(double));
		if (NULL == pvalue) {
			return NULL;
		}
		*static_cast<double *>(pvalue) = zval_get_double(entry);
		break;
	case PROPVAL_TYPE_LONGLONG:
		pvalue = emalloc(sizeof(uint64_t));
		if (NULL == pvalue) {
			return NULL;
		}
		*static_cast<uint64_t *>(pvalue) = zval_get_double(entry);
		break;
	case PROPVAL_TYPE_BYTE:
		pvalue = emalloc(sizeof(uint8_t));
		if (NULL == pvalue) {
			return NULL;
		}
		*static_cast<uint8_t *>(pvalue) = zval_is_true(entry);
		break;
	case PROPVAL_TYPE_FILETIME:
		/* convert unix timestamp to nt timestamp */
		pvalue = emalloc(sizeof(uint64_t));
		if (NULL == pvalue) {
			return NULL;
		}
		*static_cast<uint64_t *>(pvalue) = unix_to_nttime(zval_get_long(entry));
		break;
	case PROPVAL_TYPE_BINARY: {
		zstrplus str(zval_get_string(entry));
		pvalue = emalloc(sizeof(BINARY));
		if (NULL == pvalue) {
			return NULL;
		}
		static_cast<BINARY *>(pvalue)->cb = str->len;
		if (str->len == 0) {
			((BINARY*)pvalue)->pb = NULL;
		} else {
			static_cast<BINARY *>(pvalue)->pb = sta_malloc<uint8_t>(str->len);
			if (NULL == ((BINARY*)pvalue)->pb) {
				return NULL;
			}
			memcpy(static_cast<BINARY *>(pvalue)->pb, str->val, str->len);
		}
		break;
	}
	case PROPVAL_TYPE_STRING:
	case PROPVAL_TYPE_WSTRING: {
		zstrplus str(zval_get_string(entry));
		pvalue = emalloc(str->len + 1);
		if (NULL == pvalue) {
			return NULL;
		}
		memcpy(pvalue, str->val, str->len);
		static_cast<char *>(pvalue)[str->len] = '\0';
		break;
	}
	case PROPVAL_TYPE_GUID: {
		zstrplus str(zval_get_string(entry));
		if (str->len != sizeof(GUID))
			return NULL;
		pvalue = emalloc(sizeof(GUID));
		if (NULL == pvalue) {
			return NULL;
		}
		memcpy(pvalue, str->val, sizeof(GUID));
		break;
	}
	case PROPVAL_TYPE_SHORT_ARRAY:
		pdata_hash = HASH_OF(entry);
		if (NULL == pdata_hash) {
			return NULL;
		}
		pvalue = emalloc(sizeof(SHORT_ARRAY));
		if (NULL == pvalue) {
			return NULL;
		}
		((SHORT_ARRAY*)pvalue)->count =
			zend_hash_num_elements(pdata_hash);
		if (0 == ((SHORT_ARRAY*)pvalue)->count) {
			((SHORT_ARRAY*)pvalue)->ps = NULL;
			break;
		}
		((SHORT_ARRAY*)pvalue)->ps =
			sta_malloc<uint16_t>(static_cast<SHORT_ARRAY *>(pvalue)->count);
		if (NULL == ((SHORT_ARRAY*)pvalue)->ps) {
			return NULL;
		}
		ZEND_HASH_FOREACH_VAL(pdata_hash, data_entry) {
			static_cast<SHORT_ARRAY *>(pvalue)->ps[j++] = zval_get_long(entry);
		} ZEND_HASH_FOREACH_END();
		break;
	case PROPVAL_TYPE_LONG_ARRAY:
		pdata_hash = HASH_OF(entry);
		if (NULL == pdata_hash) {
			return NULL;
		}
		pvalue = emalloc(sizeof(LONG_ARRAY));
		if (NULL == pvalue) {
			return NULL;
		}
		((LONG_ARRAY*)pvalue)->count =
			zend_hash_num_elements(pdata_hash);
		if (0 == ((LONG_ARRAY*)pvalue)->count) {
			((LONG_ARRAY*)pvalue)->pl = NULL;
			break;
		}
		((LONG_ARRAY*)pvalue)->pl =
			sta_malloc<uint32_t>(static_cast<LONG_ARRAY *>(pvalue)->count);
		if (NULL == ((LONG_ARRAY*)pvalue)->pl) {
			return NULL;
		}
		ZEND_HASH_FOREACH_VAL(pdata_hash, data_entry) {
			static_cast<LONG_ARRAY *>(pvalue)->pl[j++] = zval_get_long(entry);
		} ZEND_HASH_FOREACH_END();
		break;
	case PROPVAL_TYPE_LONGLONG_ARRAY:
		pdata_hash = HASH_OF(entry);
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
			sta_malloc<uint64_t>(static_cast<LONGLONG_ARRAY *>(pvalue)->count);
		if (NULL == ((LONGLONG_ARRAY*)pvalue)->pll) {
			return NULL;
		}
		ZEND_HASH_FOREACH_VAL(pdata_hash, data_entry) {
			static_cast<LONGLONG_ARRAY *>(pvalue)->pll[j++] = zval_get_double(data_entry);
		} ZEND_HASH_FOREACH_END();
		break;
	case PROPVAL_TYPE_STRING_ARRAY:
	case PROPVAL_TYPE_WSTRING_ARRAY:
		pdata_hash = HASH_OF(entry);
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
		static_cast<STRING_ARRAY *>(pvalue)->ppstr = sta_malloc<char *>(static_cast<STRING_ARRAY *>(pvalue)->count);
		if (NULL == ((STRING_ARRAY*)pvalue)->ppstr) {
			return NULL;
		}
		ZEND_HASH_FOREACH_VAL(pdata_hash, data_entry) {
			zstrplus str(zval_get_string(data_entry));
			pstring = sta_malloc<char>(str->len + 1);
			if (NULL == pstring) {
				return NULL;
			}
			static_cast<STRING_ARRAY *>(pvalue)->ppstr[j++] = pstring;
			memcpy(pstring, str->val, str->len);
			pstring[str->len] = '\0';
		} ZEND_HASH_FOREACH_END();
		break;
	case PROPVAL_TYPE_BINARY_ARRAY:
		pdata_hash = HASH_OF(entry);
		if (NULL == pdata_hash) {
			return NULL;
		}
		pvalue = emalloc(sizeof(BINARY_ARRAY));
		if (NULL == pvalue) {
			return NULL;
		}
		((BINARY_ARRAY*)pvalue)->count =
			zend_hash_num_elements(pdata_hash);
		if (0 == ((BINARY_ARRAY*)pvalue)->count) {
			((BINARY_ARRAY*)pvalue)->pbin = NULL;
			break;
		}
		static_cast<BINARY_ARRAY *>(pvalue)->pbin = sta_malloc<BINARY>(static_cast<BINARY_ARRAY *>(pvalue)->count);
		if (NULL == ((BINARY_ARRAY*)pvalue)->pbin) {
			return NULL;
		}
		ZEND_HASH_FOREACH_VAL(pdata_hash, data_entry) {
			zstrplus str(zval_get_string(data_entry));
			static_cast<BINARY_ARRAY *>(pvalue)->pbin[j].cb = str->len;
			if (str->len == 0) {
				((BINARY_ARRAY*)pvalue)->pbin[j].pb = NULL;
			} else {
				static_cast<BINARY_ARRAY *>(pvalue)->pbin[j].pb = sta_malloc<uint8_t>(str->len);
				if (NULL == ((BINARY_ARRAY*)pvalue)->pbin[j].pb) {
					return NULL;
				}
				memcpy(static_cast<BINARY_ARRAY *>(pvalue)->pbin[j].pb, str->val, str->len);
			}
			++j;
		} ZEND_HASH_FOREACH_END();
		break;
	case PROPVAL_TYPE_GUID_ARRAY:
		pdata_hash = HASH_OF(entry);
		if (NULL == pdata_hash) {
			return NULL;
		}
		pvalue = emalloc(sizeof(GUID_ARRAY));
		if (NULL == pvalue) {
			return NULL;
		}
		((GUID_ARRAY*)pvalue)->count =
			zend_hash_num_elements(pdata_hash);
		if (0 == ((GUID_ARRAY*)pvalue)->count) {
			((GUID_ARRAY*)pvalue)->pguid = NULL;
			break;
		}
		static_cast<GUID_ARRAY *>(pvalue)->pguid = sta_malloc<GUID>(static_cast<GUID_ARRAY *>(pvalue)->count);
		if (NULL == ((GUID_ARRAY*)pvalue)->pguid) {
			return NULL;
		}
		ZEND_HASH_FOREACH_VAL(pdata_hash, data_entry) {
			zstrplus str(zval_get_string(data_entry));
			if (str->len != sizeof(GUID))
				return NULL;
			memcpy(&((GUID_ARRAY*)pvalue)->pguid[j],
				Z_STRVAL_P(data_entry), sizeof(GUID));
		} ZEND_HASH_FOREACH_END();
		break;
	case PROPVAL_TYPE_RULE:
		pvalue = emalloc(sizeof(RULE_ACTIONS));
		if (NULL == pvalue) {
			return NULL;
		}
		pdata_hash = HASH_OF(entry);
		if (NULL == pdata_hash) {
			((RULE_ACTIONS*)pvalue)->count = 0;
			((RULE_ACTIONS*)pvalue)->pblock = NULL;
			break;
		}
		((RULE_ACTIONS*)pvalue)->count =
			zend_hash_num_elements(pdata_hash);
		if (0 == ((RULE_ACTIONS*)pvalue)->count) {
			((RULE_ACTIONS*)pvalue)->pblock = NULL;
			break;
		}
		static_cast<RULE_ACTIONS *>(pvalue)->pblock = sta_malloc<ACTION_BLOCK>(static_cast<RULE_ACTIONS *>(pvalue)->count);
		if (NULL == ((RULE_ACTIONS*)pvalue)->pblock) {
			return NULL;
		}
		ZEND_HASH_FOREACH_VAL(pdata_hash, data_entry) {
			ZVAL_DEREF(data_entry);
			paction_hash = HASH_OF(data_entry);
			if (NULL == paction_hash) {
				return NULL;
			}
			data_entry = zend_hash_find(paction_hash, str_action.get());
			if (data_entry == nullptr)
				return NULL;
			pblock = ((RULE_ACTIONS*)pvalue)->pblock + j;
			pblock->type = zval_get_long(data_entry);
			/* option field user defined flags, default 0 */
			data_entry = zend_hash_find(paction_hash, str_flags.get());
			if (data_entry != nullptr) {
				pblock->flags = zval_get_long(data_entry);
			} else {
				pblock->flags = 0;
			}
			/* option field used with OP_REPLAY and OP_FORWARD, default 0 */
			data_entry = zend_hash_find(paction_hash, str_flavor.get());
			if (data_entry != nullptr) {
				pblock->flavor = zval_get_long(data_entry);
			} else {
				pblock->flavor = 0;
			}
			switch (pblock->type) {
			case ACTION_TYPE_OP_MOVE:
			case ACTION_TYPE_OP_COPY: {
				pblock->pdata = emalloc(sizeof(MOVECOPY_ACTION));
				if (NULL == pblock->pdata) {
					return NULL;
				}

				data_entry = zend_hash_find(paction_hash, str_storeentryid.get());
				if (data_entry == nullptr)
					return NULL;
				zstrplus str1(zval_get_string(data_entry));
				((MOVECOPY_ACTION*)pblock->pdata)->store_eid.cb =
									str1->len;
				((MOVECOPY_ACTION*)pblock->pdata)->store_eid.pb =
							sta_malloc<uint8_t>(str1->len);
				if (NULL == ((MOVECOPY_ACTION*)pblock->pdata)->store_eid.pb) {
					return NULL;
				}
				memcpy(((MOVECOPY_ACTION*)
					pblock->pdata)->store_eid.pb,
					str1->val, str1->len);

				data_entry = zend_hash_find(paction_hash, str_folderentryid.get());
				if (data_entry == nullptr)
					return NULL;
				zstrplus str2(zval_get_string(data_entry));
				((MOVECOPY_ACTION*)pblock->pdata)->folder_eid.cb =
					str2->len;
				((MOVECOPY_ACTION*)pblock->pdata)->folder_eid.pb =
					sta_malloc<uint8_t>(str2->len);
				if (NULL == ((MOVECOPY_ACTION*)pblock->pdata)->folder_eid.pb) {
					return NULL;
				}
				memcpy(((MOVECOPY_ACTION*)
					pblock->pdata)->folder_eid.pb,
					str2->val, str2->len);
				break;
			}
			case ACTION_TYPE_OP_REPLY:
			case ACTION_TYPE_OP_OOF_REPLY: {
				data_entry = zend_hash_find(paction_hash, str_replyentryid.get());
				if (data_entry == nullptr)
					return NULL;
				zstrplus str1(zval_get_string(data_entry));
				pblock->pdata = emalloc(sizeof(REPLY_ACTION));
				if (NULL == pblock->pdata) {
					return NULL;
				}
				((REPLY_ACTION*)pblock->pdata)->message_eid.cb =
					str1->len;
				((REPLY_ACTION*)pblock->pdata)->message_eid.pb =
					sta_malloc<uint8_t>(str1->len);
				if (NULL == ((REPLY_ACTION*)pblock->pdata)->message_eid.pb) {
					return NULL;
				}
				memcpy(((REPLY_ACTION*)
					pblock->pdata)->message_eid.pb,
					str1->val, str1->len);

				data_entry = zend_hash_find(paction_hash, str_replyguid.get());
				if (data_entry != nullptr) {
					zstrplus str2(zval_get_string(data_entry));
					if (str2->len != sizeof(GUID))
						return NULL;
					memcpy(&((REPLY_ACTION*)pblock->pdata)->template_guid,
						str2->val, sizeof(GUID));
				} else {
					memset(&((REPLY_ACTION*)
						pblock->pdata)->template_guid, 0, sizeof(GUID));
				}
				break;
			}
			case ACTION_TYPE_OP_DEFER_ACTION: {
				data_entry = zend_hash_find(paction_hash, str_dam.get());
				if (data_entry == nullptr)
					return NULL;
				zstrplus str1(zval_get_string(data_entry));
				if (str1->len == 0)
					return NULL;
				pblock->length = str1->len + sizeof(uint8_t) + 2 * sizeof(uint32_t);
				pblock->pdata = emalloc(str1->len);
				if (NULL == pblock->pdata) {
					return NULL;
				}
				memcpy(pblock->pdata, str1->val, str1->len);
				break;
			}
			case ACTION_TYPE_OP_BOUNCE:
				data_entry = zend_hash_find(paction_hash, str_code.get());
				if (data_entry == nullptr)
					return NULL;
				pblock->pdata = emalloc(sizeof(uint32_t));
				if (NULL == pblock->pdata) {
					return NULL;
				}
				*static_cast<uint32_t *>(pblock->pdata) = zval_get_long(data_entry);
				break;
			case ACTION_TYPE_OP_FORWARD:
			case ACTION_TYPE_OP_DELEGATE:
				data_entry = zend_hash_find(paction_hash, str_adrlist.get());
				if (data_entry == nullptr || Z_TYPE_P(data_entry) != IS_ARRAY)
					return NULL;
				pblock->pdata = emalloc(sizeof(FORWARDDELEGATE_ACTION));
				if (NULL == pblock->pdata) {
					return NULL;
				}
				ZVAL_DEREF(data_entry);
				precipient_hash = HASH_OF(data_entry);
				((FORWARDDELEGATE_ACTION*)pblock->pdata)->count =
						zend_hash_num_elements(precipient_hash);
				if (0 == ((FORWARDDELEGATE_ACTION*)pblock->pdata)->count) {
					return NULL;
				}
				((FORWARDDELEGATE_ACTION*)pblock->pdata)->pblock =
					sta_malloc<RECIPIENT_BLOCK>(
					((FORWARDDELEGATE_ACTION*)pblock->pdata)->count);
				if (NULL == ((FORWARDDELEGATE_ACTION*)
					pblock->pdata)->pblock) {
					return NULL;
				}
				ZEND_HASH_FOREACH_VAL(precipient_hash, data_entry) {
					if (!php_to_tpropval_array(data_entry,
						&tmp_propvals TSRMLS_CC)) {
						return NULL;
					}
					prcpt_block = ((FORWARDDELEGATE_ACTION*)
								pblock->pdata)->pblock + k;
					prcpt_block->reserved = 0;
					prcpt_block->count = tmp_propvals.count;
					prcpt_block->ppropval = tmp_propvals.ppropval;
					++k;
				} ZEND_HASH_FOREACH_END();
				break;
			case ACTION_TYPE_OP_TAG:
				data_entry = zend_hash_find(paction_hash, str_proptag.get());
				if (data_entry == nullptr)
					return NULL;
				if (!php_to_tpropval_array(data_entry, &tmp_propvals TSRMLS_CC))
					return NULL;
				if (1 != tmp_propvals.count) {
					return NULL;
				}
				pblock->pdata = tmp_propvals.ppropval;
				break;
			case ACTION_TYPE_OP_DELETE:
			case ACTION_TYPE_OP_MARK_AS_READ:
				pblock->pdata = NULL;
				break;
			default:
				return NULL;
			}
			++j;
		} ZEND_HASH_FOREACH_END();
		break;
	case PROPVAL_TYPE_RESTRICTION:
		pvalue = emalloc(sizeof(RESTRICTION));
		if (NULL == pvalue) {
			return NULL;
		}
		if (!php_to_restriction(entry, static_cast<RESTRICTION *>(pvalue) TSRMLS_CC))
			return NULL;
		break;
	default:
		return NULL;
	}
	return pvalue;
}

zend_bool php_to_tpropval_array(zval *pzval,
	TPROPVAL_ARRAY *ppropvals TSRMLS_DC)
{
	zend_string *pstring;
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
	ppropvals->ppropval = sta_malloc<TAGGED_PROPVAL>(ppropvals->count);
	if (NULL == ppropvals->ppropval) {
		return 0;
	}

	zval *entry;
	size_t i = 0;
	ZEND_HASH_FOREACH_KEY_VAL(ptarget_hash, idx, pstring, entry) {
		ppropvals->ppropval[i].proptag = phptag_to_proptag(idx);
		ppropvals->ppropval[i].pvalue =
			php_to_propval(entry, idx & 0xFFFF);
		if (NULL == ppropvals->ppropval[i].pvalue) {
			return 0;
		}
		++i;
	} ZEND_HASH_FOREACH_END();
	return 1;
}

zend_bool php_to_tarray_set(zval *pzval, TARRAY_SET *pset TSRMLS_DC) 
{
	HashTable *ptarget_hash;
	
	if (NULL == pzval) {
		return 0;
	}
	if (Z_TYPE_P(pzval) != IS_ARRAY)
		return 0;
	ptarget_hash = HASH_OF(pzval);
	if (NULL == ptarget_hash) {
		return 0;
	}
	pset->count = zend_hash_num_elements(ptarget_hash);
	if (0 == pset->count) {
		pset->pparray = NULL;
		return 1;
	}
	pset->pparray = sta_malloc<TPROPVAL_ARRAY *>(pset->count);
	if (NULL == pset->pparray) {
		return 0;
	}

	zval *entry;
	size_t i = 0;
	ZEND_HASH_FOREACH_VAL(ptarget_hash, entry) {
		if (Z_TYPE_P(entry) != IS_ARRAY)
			return 0;
		pset->pparray[i] = st_malloc<TPROPVAL_ARRAY>();
		if (NULL == pset->pparray[i]) {
			return 0;
		}
		if (!php_to_tpropval_array(entry, pset->pparray[i] TSRMLS_CC))
			return 0;
		++i;
	} ZEND_HASH_FOREACH_END();
	return 1;
}

zend_bool php_to_rule_list(zval *pzval, RULE_LIST *plist TSRMLS_DC)
{
	zstrplus str_properties(zend_string_init("properties", sizeof("properties") - 1, 0));
	zstrplus str_rowflags(zend_string_init("rowflags", sizeof("rowflags") - 1, 0));
	HashTable *ptarget_hash;
	
	if (NULL == pzval) {
		return 0;
	}
	if (Z_TYPE_P(pzval) != IS_ARRAY)
		return 0;
	ptarget_hash = HASH_OF(pzval);
	if (NULL == ptarget_hash) {
		return 0;
	}
	plist->count = zend_hash_num_elements(ptarget_hash);
	if (0 == plist->count) {
		plist->prule = NULL;
		return 1;
	}
	plist->prule = sta_malloc<RULE_DATA>(plist->count);
	if (NULL == plist->prule) {
		return 0;
	}

	zval *entry;
	size_t i = 0;
	ZEND_HASH_FOREACH_VAL(ptarget_hash, entry) {
		if (Z_TYPE_P(entry) != IS_ARRAY)
			return 0;
		auto data = zend_hash_find(HASH_OF(entry), str_properties.get());
		if (data == nullptr)
			return 0;	
		if (!php_to_tpropval_array(data, &plist->prule[i].propvals TSRMLS_CC))
			return 0;
		data = zend_hash_find(HASH_OF(entry), str_rowflags.get());
		if (data == nullptr)
			return 0;	
		plist->prule[i].flags = zval_get_long(data);
		++i;
	} ZEND_HASH_FOREACH_END();
	return 1;
}

#define IDX_VALUE									0
#define IDX_RELOP									1
#define IDX_FUZZYLEVEL								2
#define IDX_SIZE									3
#define IDX_TYPE									4
#define IDX_MASK									5
#define IDX_PROPTAG									6
#define IDX_PROPTAG1								7
#define IDX_PROPTAG2								8
#define IDX_PROPVALS								9
#define IDX_RESTRICTION								10

zend_bool php_to_restriction(zval *pzval, RESTRICTION *pres TSRMLS_DC)
{
	int i;
	HashTable *pres_hash;
	HashTable *pdata_hash;
	TPROPVAL_ARRAY tmp_propvals;
	
	pres_hash = HASH_OF(pzval);
	if (NULL == pres_hash || zend_hash_num_elements(pres_hash) != 2) {
		return 0;
	}

	HashPosition hpos;
	zend_hash_internal_pointer_reset_ex(pres_hash, &hpos);
	/* 0=>type, 1=>value array */
	auto type_entry = zend_hash_get_current_data_ex(pres_hash, &hpos);
	zend_hash_move_forward_ex(pres_hash, &hpos);
	auto value_entry = zend_hash_get_current_data_ex(pres_hash, &hpos);
	pres->rt = zval_get_long(type_entry);
	ZVAL_DEREF(value_entry);
	pdata_hash = HASH_OF(value_entry);
	if (NULL == pdata_hash) {
		return 0;
	}
	switch(pres->rt) {
	case RESTRICTION_TYPE_AND:
	case RESTRICTION_TYPE_OR:
		pres->pres = emalloc(sizeof(RESTRICTION_AND_OR));
		if (NULL == pres->pres) {
			return 0;
		}
		((RESTRICTION_AND_OR*)pres->pres)->count =
				zend_hash_num_elements(pdata_hash);
		static_cast<RESTRICTION_AND_OR *>(pres->pres)->pres = sta_malloc<RESTRICTION>(static_cast<RESTRICTION_AND_OR *>(pres->pres)->count);
		if (NULL == ((RESTRICTION_AND_OR*)pres->pres)->pres) {
			return 0;
		}
		i = 0;
		ZEND_HASH_FOREACH_VAL(pdata_hash, value_entry) {
			if (!php_to_restriction(value_entry,
			    &static_cast<RESTRICTION_AND_OR *>(pres->pres)->pres[i++] TSRMLS_CC)) {
				return 0;
			}
		} ZEND_HASH_FOREACH_END();
		break;
	case RESTRICTION_TYPE_NOT: {
		pres->pres = emalloc(sizeof(RESTRICTION_NOT));
		if (NULL == pres->pres) {
			return 0;
		}
		i = 0;
		HashPosition hpos;
		zend_hash_internal_pointer_reset_ex(pdata_hash, &hpos);
		value_entry = zend_hash_get_current_data_ex(pdata_hash, &hpos);
		if (!php_to_restriction(value_entry,
			&((RESTRICTION_NOT*)pres->pres)->res TSRMLS_CC)) {
			return 0;
		}
		break;
	}
	case RESTRICTION_TYPE_SUBOBJ:
		pres->pres = emalloc(sizeof(RESTRICTION_SUBOBJ));
		if (NULL == pres->pres) {
			return 0;
		}
		value_entry = zend_hash_index_find(pdata_hash, IDX_PROPTAG);
		if (value_entry == nullptr)
			return 0;
		((RESTRICTION_SUBOBJ*)pres->pres)->subobject =
			phptag_to_proptag(zval_get_long(value_entry));
		value_entry = zend_hash_index_find(pdata_hash, IDX_RESTRICTION);
		if (value_entry == nullptr)
			return 0;
		if (!php_to_restriction(value_entry,
			&((RESTRICTION_SUBOBJ*)pres->pres)->res TSRMLS_CC)) {
			return 0;	
		}
		break;
	case RESTRICTION_TYPE_COMMENT:
		pres->pres = emalloc(sizeof(RESTRICTION_COMMENT));
		if (NULL == pres->pres) {
			return 0;
		}
		((RESTRICTION_COMMENT*)pres->pres)->pres =
			st_malloc<RESTRICTION>();
		if (NULL == ((RESTRICTION_COMMENT*)pres->pres)->pres) {
			/* memory leak */
			return 0;
		}
		value_entry = zend_hash_index_find(pdata_hash, IDX_RESTRICTION);
		if (value_entry == nullptr)
			return 0;
		if (!php_to_restriction(value_entry,
			((RESTRICTION_COMMENT*)pres->pres)->pres TSRMLS_CC)) {
			return 0;
		}
		value_entry = zend_hash_index_find(pdata_hash, IDX_PROPVALS);
		if (value_entry == nullptr)
			return 0;
		if (!php_to_tpropval_array(
			value_entry, &tmp_propvals TSRMLS_CC)) {
			return 0;
		}
		((RESTRICTION_COMMENT*)pres->pres)->count = tmp_propvals.count;
		((RESTRICTION_COMMENT*)pres->pres)->ppropval = tmp_propvals.ppropval;
		break;
	case RESTRICTION_TYPE_CONTENT:
		pres->pres = emalloc(sizeof(RESTRICTION_CONTENT));
		if (NULL == pres->pres) {
			return 0;
		}
		value_entry = zend_hash_index_find(pdata_hash, IDX_PROPTAG);
		if (value_entry == nullptr)
			return 0;
		((RESTRICTION_CONTENT*)pres->pres)->proptag =
			phptag_to_proptag(zval_get_long(value_entry));
		value_entry = zend_hash_index_find(pdata_hash, IDX_FUZZYLEVEL);
		if (value_entry == nullptr)
			return 0;
		((RESTRICTION_CONTENT*)pres->pres)->fuzzy_level =
			zval_get_long(value_entry);
		value_entry = zend_hash_index_find(pdata_hash, IDX_VALUE);
		if (value_entry == nullptr)
			return 0;
		if (Z_TYPE_P(value_entry) == IS_ARRAY) {
			if (!php_to_tpropval_array(value_entry, &tmp_propvals TSRMLS_CC))
				return 0;
			if (1 != tmp_propvals.count) {
				return 0;
			}
			((RESTRICTION_CONTENT*)pres->pres)->propval =
									*tmp_propvals.ppropval;
		} else {
			((RESTRICTION_CONTENT*)pres->pres)->propval.proptag =
				((RESTRICTION_CONTENT*)pres->pres)->proptag;
			((RESTRICTION_CONTENT*)pres->pres)->propval.pvalue =
				php_to_propval(value_entry,
				((RESTRICTION_CONTENT*)pres->pres)->proptag&0xFFFF);
			if (NULL == ((RESTRICTION_CONTENT*)pres->pres)->propval.pvalue) {
				return 0;
			}
		}
		break;
	case RESTRICTION_TYPE_PROPERTY:
		pres->pres = emalloc(sizeof(RESTRICTION_PROPERTY));
		if (NULL == pres->pres) {
			return 0;
		}
		value_entry = zend_hash_index_find(pdata_hash, IDX_PROPTAG);
		if (value_entry == nullptr)
			return 0;
		((RESTRICTION_PROPERTY*)pres->pres)->proptag =
			phptag_to_proptag(zval_get_long(value_entry));
		value_entry = zend_hash_index_find(pdata_hash, IDX_RELOP);
		if (value_entry == nullptr)
			return 0;
		((RESTRICTION_PROPERTY*)pres->pres)->relop =
			zval_get_long(value_entry);
		value_entry = zend_hash_index_find(pdata_hash, IDX_VALUE);
		if (value_entry == nullptr)
			return 0;
		if (Z_TYPE_P(value_entry) == IS_ARRAY) {
			if (!php_to_tpropval_array(value_entry, &tmp_propvals TSRMLS_CC))
				return 0;
			if (1 != tmp_propvals.count) {
				return 0;
			}
			((RESTRICTION_PROPERTY*)pres->pres)->propval =
									*tmp_propvals.ppropval;
		} else {
			((RESTRICTION_PROPERTY*)pres->pres)->propval.proptag =
				((RESTRICTION_PROPERTY*)pres->pres)->proptag;
			((RESTRICTION_PROPERTY*)pres->pres)->propval.pvalue =
				php_to_propval(value_entry,
				((RESTRICTION_PROPERTY*)pres->pres)->proptag&0xFFFF);
			if (NULL == ((RESTRICTION_PROPERTY*)pres->pres)->propval.pvalue) {
				return 0;
			}
		}
		break;
	case RESTRICTION_TYPE_PROPCOMPARE:
		pres->pres = emalloc(sizeof(RESTRICTION_PROPCOMPARE));
		if (NULL == pres->pres) {
			return 0;
		}
		value_entry = zend_hash_index_find(pdata_hash, IDX_RELOP);
		if (value_entry == nullptr)
			/* memory leak */
			return 0;
		((RESTRICTION_PROPCOMPARE*)pres->pres)->relop =
			zval_get_long(value_entry);
		value_entry = zend_hash_index_find(pdata_hash, IDX_PROPTAG1);
		if (value_entry == nullptr)
			return 0;
		((RESTRICTION_PROPCOMPARE*)pres->pres)->proptag1 =
			zval_get_long(value_entry);
		value_entry = zend_hash_index_find(pdata_hash, IDX_PROPTAG2);
		if (value_entry == nullptr)
			return 0;
		((RESTRICTION_PROPCOMPARE*)pres->pres)->proptag2 =
			zval_get_long(value_entry);
		break;
	case RESTRICTION_TYPE_BITMASK:
		pres->pres = emalloc(sizeof(RESTRICTION_BITMASK));
		if (NULL == pres->pres) {
			return 0;
		}
		value_entry = zend_hash_index_find(pdata_hash, IDX_TYPE);
		if (value_entry == nullptr)
			/* memory leak */
			return 0;
		((RESTRICTION_BITMASK*)pres->pres)->bitmask_relop =
			zval_get_long(value_entry);
		value_entry = zend_hash_index_find(pdata_hash, IDX_MASK);
		if (value_entry == nullptr)
			return 0;
		((RESTRICTION_BITMASK*)pres->pres)->mask =
			zval_get_long(value_entry);
		value_entry = zend_hash_index_find(pdata_hash, IDX_PROPTAG);
		if (value_entry == nullptr)
			return 0;
		((RESTRICTION_BITMASK*)pres->pres)->proptag =
			phptag_to_proptag(zval_get_long(value_entry));
		break;
	case RESTRICTION_TYPE_SIZE:
		pres->pres = emalloc(sizeof(RESTRICTION_SIZE));
		if (NULL == pres->pres) {
			return 0;
		}
		value_entry = zend_hash_index_find(pdata_hash, IDX_SIZE);
		if (value_entry == nullptr)
			/* memory leak */
			return 0;
		((RESTRICTION_SIZE*)pres->pres)->size =
			zval_get_long(value_entry);
		value_entry = zend_hash_index_find(pdata_hash, IDX_RELOP);
		if (value_entry == nullptr)
			return 0;
		((RESTRICTION_SIZE*)pres->pres)->relop =
			zval_get_long(value_entry);
		value_entry = zend_hash_index_find(pdata_hash, IDX_PROPTAG);
		if (value_entry == nullptr)
			return 0;
		((RESTRICTION_SIZE*)pres->pres)->proptag =
			phptag_to_proptag(zval_get_long(value_entry));
		break;
	case RESTRICTION_TYPE_EXIST:
		pres->pres = emalloc(sizeof(RESTRICTION_EXIST));
		if (NULL == pres->pres) {
			return 0;
		}
		value_entry = zend_hash_index_find(pdata_hash, IDX_PROPTAG);
		if (value_entry == nullptr)
			return 0;
		((RESTRICTION_EXIST*)pres->pres)->proptag =
			phptag_to_proptag(zval_get_long(value_entry));
		break;
	default:
		return 0;
	}
	return 1;
}

zend_bool restriction_to_php(const RESTRICTION *pres,
	zval *pzret TSRMLS_DC)
{
	int i;
	char key[16];
	zval pzrops, pzentry, pzarray, pzrestriction;
	TPROPVAL_ARRAY tmp_propvals;
	
	array_init(pzret);
	switch (pres->rt) {
	case RESTRICTION_TYPE_AND:
	case RESTRICTION_TYPE_OR:
		array_init(&pzarray);
		for (i=0; i<((RESTRICTION_AND_OR*)pres->pres)->count; i++) {
			sprintf(key, "%i", i);
			if (!restriction_to_php(
				&((RESTRICTION_AND_OR*)pres->pres)->pres[i],
				&pzentry TSRMLS_CC)) {
				return 0;
			}
			add_assoc_zval(&pzarray, key, &pzentry);
		}
		break;
	case RESTRICTION_TYPE_NOT:
		array_init(&pzarray);
		if (!restriction_to_php(
			&((RESTRICTION_NOT*)pres->pres)->res,
			&pzentry TSRMLS_CC)) {
			return 0;	
		}
		add_assoc_zval(&pzarray, "0", &pzentry);
		break;
	case RESTRICTION_TYPE_CONTENT:
		tmp_propvals.count = 1;
		tmp_propvals.ppropval = &((RESTRICTION_CONTENT*)pres->pres)->propval;
		if (!tpropval_array_to_php(&tmp_propvals, &pzrops TSRMLS_CC)) {
			return 0;
		}
		array_init(&pzarray);
		sprintf(key, "%i", IDX_VALUE);
		add_assoc_zval(&pzarray, key, &pzrops);		
		sprintf(key, "%i", IDX_PROPTAG);
		add_assoc_long(&pzarray, key, proptag_to_phptag(
			((RESTRICTION_CONTENT*)pres->pres)->proptag));
		sprintf(key, "%i", IDX_FUZZYLEVEL);
		add_assoc_long(&pzarray, key,
			((RESTRICTION_CONTENT*)pres->pres)->fuzzy_level);
		break;
	case RESTRICTION_TYPE_PROPERTY:
		tmp_propvals.count = 1;
		tmp_propvals.ppropval = &((RESTRICTION_CONTENT*)pres->pres)->propval;
		if (!tpropval_array_to_php(&tmp_propvals, &pzrops TSRMLS_CC)) {
			return 0;
		}
		array_init(&pzarray);
		sprintf(key, "%i", IDX_VALUE);
		add_assoc_zval(&pzarray, key, &pzrops);
		sprintf(key, "%i", IDX_RELOP);
		add_assoc_long(&pzarray, key,
			((RESTRICTION_PROPERTY*)pres->pres)->relop);
		sprintf(key, "%i", IDX_PROPTAG);
		add_assoc_long(&pzarray, key, proptag_to_phptag(
			((RESTRICTION_PROPERTY*)pres->pres)->proptag));
		break;
	case RESTRICTION_TYPE_PROPCOMPARE:
		array_init(&pzarray);
		sprintf(key, "%i", IDX_RELOP);
		add_assoc_long(&pzarray, key,
			((RESTRICTION_PROPCOMPARE*)pres->pres)->relop);
		sprintf(key, "%i", IDX_PROPTAG1);
		add_assoc_long(&pzarray, key, proptag_to_phptag(
			((RESTRICTION_PROPCOMPARE*)pres->pres)->proptag1));
		sprintf(key, "%i", IDX_PROPTAG2);
		add_assoc_long(&pzarray, key, proptag_to_phptag(
			((RESTRICTION_PROPCOMPARE*)pres->pres)->proptag2));
		break;
	case RESTRICTION_TYPE_BITMASK:
		array_init(&pzarray);
		sprintf(key, "%i", IDX_TYPE);
		add_assoc_long(&pzarray, key,
			((RESTRICTION_BITMASK*)pres->pres)->bitmask_relop);
		sprintf(key, "%i", IDX_PROPTAG);
		add_assoc_long(&pzarray, key, proptag_to_phptag(
			((RESTRICTION_BITMASK*)pres->pres)->proptag));
		sprintf(key, "%i", IDX_MASK);
		add_assoc_long(&pzarray, key,
			((RESTRICTION_BITMASK*)pres->pres)->mask);
		break;
	case RESTRICTION_TYPE_SIZE:
		array_init(&pzarray);
		sprintf(key, "%i", IDX_RELOP);
		add_assoc_long(&pzarray, key,
			((RESTRICTION_SIZE*)pres->pres)->relop);
		sprintf(key, "%i", IDX_PROPTAG);
		add_assoc_long(&pzarray, key, proptag_to_phptag(
			((RESTRICTION_SIZE*)pres->pres)->proptag));
		sprintf(key, "%i", IDX_SIZE);
		add_assoc_long(&pzarray, key,
			((RESTRICTION_SIZE*)pres->pres)->size);
		break;
	case RESTRICTION_TYPE_EXIST:
		array_init(&pzarray);
		sprintf(key, "%i", IDX_PROPTAG);
		add_assoc_long(&pzarray, key, proptag_to_phptag(
			((RESTRICTION_EXIST*)pres->pres)->proptag));
		break;
	case RESTRICTION_TYPE_SUBOBJ:
		if (!restriction_to_php(
			&((RESTRICTION_SUBOBJ*)pres->pres)->res,
			&pzrestriction TSRMLS_CC)) {
			return 0;	
		}
		array_init(&pzarray);
		sprintf(key, "%i", IDX_PROPTAG);
		add_assoc_long(&pzarray, key, proptag_to_phptag(
			((RESTRICTION_SUBOBJ*)pres->pres)->subobject));
		sprintf(key, "%i", IDX_RESTRICTION);
		add_assoc_zval(&pzarray, key, &pzrestriction);
		break;
	case RESTRICTION_TYPE_COMMENT:
		tmp_propvals.count = ((RESTRICTION_COMMENT*)pres->pres)->count;
		tmp_propvals.ppropval = ((RESTRICTION_COMMENT*)pres->pres)->ppropval;
		if (!tpropval_array_to_php(&tmp_propvals, &pzrops TSRMLS_CC)) {
			return 0;
		}
		if (!restriction_to_php(((RESTRICTION_COMMENT*)
			pres->pres)->pres, &pzrestriction TSRMLS_CC)) {
			return 0;	
		}
		array_init(&pzarray);
		sprintf(key, "%i", IDX_PROPVALS);
		add_assoc_zval(&pzarray, key, &pzrops);
		sprintf(key, "%i", IDX_RESTRICTION);
		add_assoc_zval(&pzarray, key, &pzrestriction);
		break;
	default:
		return 0;
	}
	add_assoc_long(pzret, "0", pres->rt);
	add_assoc_zval(pzret, "1", &pzarray);
	return 1;
}

zend_bool proptag_array_to_php(const PROPTAG_ARRAY *pproptags,
	zval *pzret TSRMLS_DC)
{
	int i;
	
	array_init(pzret);
	for (i=0; i<pproptags->count; i++) {
		add_next_index_long(pzret,
			proptag_to_phptag(pproptags->pproptag[i]));
	}
	return 1;
}

zend_bool tpropval_array_to_php(const TPROPVAL_ARRAY *ppropvals,
	zval *pzret TSRMLS_DC)
{
	int i, j, k;
	char key[16];
	zval pzmval, pzalist, pzactval, pzpropval, pzactarray;
	RULE_ACTIONS *prule;
	char proptag_string[16];
	TAGGED_PROPVAL *ppropval;
	TPROPVAL_ARRAY tmp_propvals;
	
	array_init(pzret);
	for (i=0; i<ppropvals->count; i++) {
		ppropval = &ppropvals->ppropval[i];
		/*
		* PHP wants a string as array key. PHP will transform this to zval integer when possible.
		* Because MAPI works with ULONGS, some properties (namedproperties) are bigger than LONG_MAX
		* and they will be stored as a zval string.
		* To prevent this we cast the ULONG to a signed long. The number will look a bit weird but it
		* will work.
		*/
		sprintf(proptag_string, "%u", proptag_to_phptag(ppropval->proptag));
		switch (ppropval->proptag & 0xFFFF) {
		case PROPVAL_TYPE_LONG:
		case PROPVAL_TYPE_ERROR:
			add_assoc_long(pzret, proptag_string, *(uint32_t*)ppropval->pvalue);
			break;
		case PROPVAL_TYPE_SHORT:
			add_assoc_long(pzret, proptag_string, *(uint16_t*)ppropval->pvalue);
			break;
		case PROPVAL_TYPE_DOUBLE:
		case PROPVAL_TYPE_FLOATINGTIME:
			add_assoc_double(pzret, proptag_string, *(double*)ppropval->pvalue);
			break;
		case PROPVAL_TYPE_LONGLONG:
 			add_assoc_double(pzret, proptag_string, *(uint64_t*)ppropval->pvalue);
			break;
		case PROPVAL_TYPE_FLOAT:
			add_assoc_double(pzret, proptag_string, *(float*)ppropval->pvalue);
			break;
		case PROPVAL_TYPE_BYTE:
			add_assoc_bool(pzret, proptag_string, *(uint8_t*)ppropval->pvalue);
			break;
		case PROPVAL_TYPE_STRING:
		case PROPVAL_TYPE_WSTRING:
			add_assoc_string(pzret, proptag_string, static_cast<const char *>(ppropval->pvalue));
			break;
		case PROPVAL_TYPE_BINARY:
			add_assoc_stringl(pzret, proptag_string,
				reinterpret_cast<const char *>(static_cast<BINARY *>(ppropval->pvalue)->pb),
				static_cast<BINARY *>(ppropval->pvalue)->cb);
			break;
		case PROPVAL_TYPE_FILETIME:
			add_assoc_long(pzret, proptag_string,
				nttime_to_unix(*(uint64_t*)ppropval->pvalue));
			break;
		case PROPVAL_TYPE_GUID:
			add_assoc_stringl(pzret, proptag_string,
				static_cast<const char *>(ppropval->pvalue), sizeof(GUID));
			break;
		case PROPVAL_TYPE_SHORT_ARRAY:
			array_init(&pzmval);
			for (j=0; j<((SHORT_ARRAY*)ppropval->pvalue)->count; j++) {
				sprintf(key, "%i", j);
				add_assoc_long(&pzmval, key,
					((SHORT_ARRAY*)ppropval->pvalue)->ps[j]);
			}
			add_assoc_zval(pzret, proptag_string, &pzmval);
			break;
		case PROPVAL_TYPE_LONG_ARRAY:
			array_init(&pzmval);
			for (j=0; j<((LONG_ARRAY*)ppropval->pvalue)->count; j++) {
				sprintf(key, "%i", j);
				add_assoc_long(&pzmval, key,
					((LONG_ARRAY*)ppropval->pvalue)->pl[j]);
			}
			add_assoc_zval(pzret, proptag_string, &pzmval);
			break;
		case PROPVAL_TYPE_BINARY_ARRAY:
			array_init(&pzmval);
			for (j=0; j<((BINARY_ARRAY*)ppropval->pvalue)->count; j++) {
				sprintf(key, "%i", j);
				add_assoc_stringl(&pzmval, key,
					reinterpret_cast<const char *>(static_cast<BINARY_ARRAY *>(ppropval->pvalue)->pbin[j].pb),
					static_cast<BINARY_ARRAY *>(ppropval->pvalue)->pbin[j].cb);
			}
			add_assoc_zval(pzret, proptag_string, &pzmval);
			break;
		case PROPVAL_TYPE_STRING_ARRAY:
		case PROPVAL_TYPE_WSTRING_ARRAY:
			array_init(&pzmval);
			for (j=0; j<((STRING_ARRAY*)ppropval->pvalue)->count; j++) {
				sprintf(key, "%i", j);
				add_assoc_string(&pzmval, key,
					static_cast<STRING_ARRAY *>(ppropval->pvalue)->ppstr[j]);
			}
			add_assoc_zval(pzret, proptag_string, &pzmval);
			break;
		case PROPVAL_TYPE_GUID_ARRAY:
			array_init(&pzmval);
			for (j=0; j<((GUID_ARRAY*)ppropval->pvalue)->count; j++) {
				sprintf(key, "%i", j);
				add_assoc_stringl(&pzmval, key,
					(char*)&((GUID_ARRAY*)ppropval->pvalue)->pguid[j],
					sizeof(GUID));
			}
			add_assoc_zval(pzret, proptag_string, &pzmval);
			break;
		case PROPVAL_TYPE_RULE:
			prule = (RULE_ACTIONS*)ppropval->pvalue;
			array_init(&pzactarray);
			for (j=0; j<prule->count; j++) {
				array_init(&pzactval);
				add_assoc_long(&pzactval, "action", prule->pblock[j].type);
				add_assoc_long(&pzactval, "flags", prule->pblock[j].flags);
				add_assoc_long(&pzactval, "flavor", prule->pblock[j].flavor);
				switch (prule->pblock[j].type) {
				case ACTION_TYPE_OP_MOVE:
				case ACTION_TYPE_OP_COPY:
					add_assoc_stringl(&pzactval, "storeentryid",
						reinterpret_cast<const char *>(static_cast<MOVECOPY_ACTION *>(prule->pblock[j].pdata)->store_eid.pb),
						((MOVECOPY_ACTION*)
						prule->pblock[j].pdata)->store_eid.cb);
					add_assoc_stringl(&pzactval, "folderentryid",
						reinterpret_cast<const char *>(static_cast<MOVECOPY_ACTION *>(prule->pblock[j].pdata)->folder_eid.pb),
						((MOVECOPY_ACTION*)
						prule->pblock[j].pdata)->folder_eid.cb);
					break;
				case ACTION_TYPE_OP_REPLY:
				case ACTION_TYPE_OP_OOF_REPLY:
					add_assoc_stringl(&pzactval, "replyentryid",
						reinterpret_cast<const char *>(static_cast<REPLY_ACTION *>(prule->pblock[j].pdata)->message_eid.pb),
						((REPLY_ACTION*)
						prule->pblock[j].pdata)->message_eid.cb);
					add_assoc_stringl(
						&pzactval, "replyguid",
						(char*)&((REPLY_ACTION*)
						prule->pblock[j].pdata)->template_guid,
						sizeof(GUID));
					break;
				case ACTION_TYPE_OP_DEFER_ACTION:
					add_assoc_stringl(&pzactval, "dam",
						static_cast<const char *>(prule->pblock[j].pdata), prule->pblock[j].length
						- sizeof(uint8_t) - 2*sizeof(uint32_t));
					break;
				case ACTION_TYPE_OP_BOUNCE:
					add_assoc_long(&pzactval, "code",
						*(uint32_t*)prule->pblock[j].pdata);
					break;
				case ACTION_TYPE_OP_FORWARD:
				case ACTION_TYPE_OP_DELEGATE:
					array_init(&pzalist);
					for (k=0; k<((FORWARDDELEGATE_ACTION*)
						prule->pblock[j].pdata)->count; k++) {
						tmp_propvals.count = ((FORWARDDELEGATE_ACTION*)
								prule->pblock[j].pdata)->pblock[k].count;
						tmp_propvals.ppropval = ((FORWARDDELEGATE_ACTION*)
								prule->pblock[j].pdata)->pblock[k].ppropval;
						if (!tpropval_array_to_php(&tmp_propvals,
							&pzpropval TSRMLS_CC)) {
							return 0;
						}
						zend_hash_next_index_insert(HASH_OF(&pzalist),
									&pzpropval);
					}
					add_assoc_zval(&pzactval, "adrlist", &pzalist);
					break;
				case ACTION_TYPE_OP_TAG:
					tmp_propvals.count = 1;
					tmp_propvals.ppropval = static_cast<TAGGED_PROPVAL *>(prule->pblock[j].pdata);
					if (!tpropval_array_to_php(&tmp_propvals,
						&pzalist TSRMLS_CC)) {
						return 0;
					}
					add_assoc_zval(&pzactval, "proptag", &pzalist);
					break;
				case ACTION_TYPE_OP_DELETE:
				case ACTION_TYPE_OP_MARK_AS_READ:
					break;
				default:
					return 0;
				};
				sprintf(key, "%i", j);
				add_assoc_zval(&pzactarray, key, &pzactval);
			}
			add_assoc_zval(pzret, proptag_string, &pzactarray);
			break;
		case PROPVAL_TYPE_RESTRICTION:
			if (!restriction_to_php(static_cast<RESTRICTION *>(ppropval->pvalue),
				&pzactval TSRMLS_CC)) {
				return 0;
			}
			add_assoc_zval(pzret, proptag_string, &pzactval);
			break;
		}
	}
	return 1;
}

zend_bool tarray_set_to_php(const TARRAY_SET *pset,
	zval *pret TSRMLS_DC)
{
	int i;
	zval pzpropval;
	
	array_init(pret);
	for (i=0; i<pset->count; i++) {
		tpropval_array_to_php(pset->pparray[i],
						&pzpropval TSRMLS_CC);
		zend_hash_next_index_insert(HASH_OF(pret), &pzpropval);
	}
	return 1;
}

zend_bool state_array_to_php(const STATE_ARRAY *pstates,
	zval *pzret TSRMLS_DC)
{
	int i;
	zval pzval;
	
	array_init(pzret);
	for (i=0; i<pstates->count; i++) {
		array_init(&pzval);
		add_assoc_stringl(&pzval, "sourcekey",
			reinterpret_cast<const char *>(pstates->pstate[i].source_key.pb),
			pstates->pstate[i].source_key.cb);
		add_assoc_long(&pzval, "flags",
			pstates->pstate[i].message_flags);
		add_next_index_zval(pzret, &pzval);
	}
	return 1;
}

zend_bool php_to_state_array(zval *pzval,
	STATE_ARRAY *pstates TSRMLS_DC)
{
	int i; 
	zval *pentry, *ppvalue_entry;
	HashTable *ptarget_hash;
	zstrplus str_sourcekey(zend_string_init("sourcekey", sizeof("sourcekey") - 1, 0));
	zstrplus str_flags(zend_string_init("flags", sizeof("flags") - 1, 0));
	
	if (NULL == pzval) {
		return 0;
	}
	ptarget_hash = HASH_OF(pzval);
	if (NULL == ptarget_hash) {
		return 0;
	}
	pstates->count = zend_hash_num_elements(Z_ARRVAL_P(pzval));
	if (0 == pstates->count) {
		pstates->pstate = NULL;
		return 1;
	}
	pstates->pstate = sta_malloc<MESSAGE_STATE>(pstates->count);
	if (NULL == pstates->pstate) {
		return 0;
	}
	i = 0;
	ZEND_HASH_FOREACH_VAL(ptarget_hash, pentry) {
		auto value_entry = zend_hash_find(HASH_OF(pentry), str_sourcekey.get());
		if (value_entry == nullptr)
			return 0;
		zstrplus str(zval_get_string(value_entry));
		pstates->pstate[i].source_key.cb = str->len;
		pstates->pstate[i].source_key.pb = sta_malloc<uint8_t>(str->len);
		if (NULL == pstates->pstate[i].source_key.pb) {
			return 0;
		}
		memcpy(pstates->pstate[i].source_key.pb, str->val, str->len);
		value_entry = zend_hash_find(HASH_OF(pentry), str_flags.get());
		if (value_entry == nullptr)
			return 0;
		pstates->pstate[i++].message_flags = zval_get_long(value_entry);
	} ZEND_HASH_FOREACH_END();
	return 1;
}

zend_bool znotification_array_to_php(
	ZNOTIFICATION_ARRAY *pnotifications, zval *pzret TSRMLS_DC)
{
	int i;
	zval pzvalprops, pzvalnotif;
	NEWMAIL_ZNOTIFICATION *pnew_notification;
	OBJECT_ZNOTIFICATION *pobject_notification;
	
	array_init(pzret);
	for (i=0; i<pnotifications->count; i++) {
		array_init(&pzvalnotif);
		add_assoc_long(&pzvalnotif, "eventtype",
			pnotifications->ppnotification[i]->event_type);
		switch(pnotifications->ppnotification[i]->event_type) {
		case EVENT_TYPE_NEWMAIL:
			pnew_notification =
				static_cast<NEWMAIL_ZNOTIFICATION *>(pnotifications->ppnotification[i]->pnotification_data);
			add_assoc_stringl(&pzvalnotif, "entryid",
				reinterpret_cast<const char *>(pnew_notification->entryid.pb),
				pnew_notification->entryid.cb);
			add_assoc_stringl(&pzvalnotif, "parentid",
				reinterpret_cast<const char *>(pnew_notification->parentid.pb),
				pnew_notification->parentid.cb);
			add_assoc_long(&pzvalnotif, "flags",
				pnew_notification->flags);
			add_assoc_string(&pzvalnotif, "messageclass",
				pnew_notification->message_class);
			add_assoc_long(&pzvalnotif, "messageflags",
				pnew_notification->message_flags);
			break;
		case EVENT_TYPE_OBJECTCREATED:
		case EVENT_TYPE_OBJECTDELETED:
		case EVENT_TYPE_OBJECTMODIFIED:
		case EVENT_TYPE_OBJECTMOVED:
		case EVENT_TYPE_OBJECTCOPIED:
		case EVENT_TYPE_SEARCHCOMPLETE:
			pobject_notification =
				static_cast<OBJECT_ZNOTIFICATION *>(pnotifications->ppnotification[i]->pnotification_data);
			if (NULL != pobject_notification->pentryid) {
				add_assoc_stringl(&pzvalnotif, "entryid",
					reinterpret_cast<const char *>(pobject_notification->pentryid->pb),
					pobject_notification->pentryid->cb);
			}
			add_assoc_long(&pzvalnotif, "objtype",
				pobject_notification->object_type);
			if (NULL != pobject_notification->pparentid) {
				add_assoc_stringl(&pzvalnotif, "parentid",
				reinterpret_cast<const char *>(pobject_notification->pparentid->pb),
				pobject_notification->pparentid->cb);
			}
			if (NULL != pobject_notification->pold_entryid) {
				add_assoc_stringl(&pzvalnotif, "oldid",
				reinterpret_cast<const char *>(pobject_notification->pold_entryid->pb),
				pobject_notification->pold_entryid->cb);
			}
			if (NULL != pobject_notification->pold_parentid) {
				add_assoc_stringl(&pzvalnotif, "oldparentid",
				reinterpret_cast<const char *>(pobject_notification->pold_parentid->pb),
				pobject_notification->pold_parentid->cb);
			}
			if (NULL != pobject_notification->pproptags) {
				if (!proptag_array_to_php(
					pobject_notification->pproptags,
					&pzvalprops TSRMLS_CC)) {
					return 0;
				}
				add_assoc_zval(&pzvalnotif, "proptagarray", &pzvalprops);
			}
			break;
		default:
			continue;
		}
		add_next_index_zval(pzret, &pzvalnotif);
	}
	return 1;
}

zend_bool php_to_propname_array(zval *pzval_names,
	zval *pzval_guids, PROPNAME_ARRAY *ppropnames TSRMLS_DC)
{
	int i;
	zval *guidentry;
	HashTable *pguidhash;
	HashTable *pnameshash;
	static GUID guid_appointment = {0x00062002, 0x0000, 0x0000,
			{0xC0, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
	
	pnameshash = Z_ARRVAL_P(pzval_names);
	if (NULL != pzval_guids) {
		pguidhash = Z_ARRVAL_P(pzval_guids);
	} else {
		pguidhash = NULL;
	}
	ppropnames->count = zend_hash_num_elements(pnameshash);
	if (NULL != pguidhash && ppropnames->count !=
		zend_hash_num_elements(pguidhash)) {
		return 0;
	}
	if (0 == ppropnames->count) {
		ppropnames->ppropname = NULL;
		return 1;
	}
	ppropnames->ppropname = sta_malloc<PROPERTY_NAME>(ppropnames->count);
	if (NULL == ppropnames->ppropname) {
		return 0;
	}
	zend_hash_internal_pointer_reset(pnameshash);
	if (NULL != pguidhash) {
		zend_hash_internal_pointer_reset(pguidhash);
	}
	HashPosition thpos, ghpos;
	zend_hash_internal_pointer_reset_ex(pnameshash, &thpos);
	if (pguidhash != nullptr)
		zend_hash_internal_pointer_reset_ex(pguidhash, &ghpos);
	for (i=0; i<ppropnames->count; i++) {
		auto entry = zend_hash_get_current_data_ex(pnameshash, &thpos);
		if (NULL != pguidhash) {
			guidentry = zend_hash_get_current_data_ex(pguidhash, &ghpos);
		}
		ppropnames->ppropname[i].guid = guid_appointment;
		if (NULL != pguidhash) {
			if (Z_TYPE_P(guidentry) == IS_STRING && Z_STRLEN_P(guidentry) == sizeof(GUID))
				memcpy(&ppropnames->ppropname[i].guid, Z_STRVAL_P(guidentry), sizeof(GUID));
		}
		switch (Z_TYPE_P(entry)) {
		case IS_LONG:
			ppropnames->ppropname[i].kind = KIND_LID;
			ppropnames->ppropname[i].plid = st_malloc<uint32_t>();
			if (NULL == ppropnames->ppropname[i].plid) {
				return 0;
			}
			*ppropnames->ppropname[i].plid = zval_get_long(entry);
			ppropnames->ppropname[i].pname = NULL;
			break;
		case IS_STRING:
			ppropnames->ppropname[i].kind = KIND_NAME;
			ppropnames->ppropname[i].plid = NULL;
			ppropnames->ppropname[i].pname = estrdup(Z_STRVAL_P(entry));
			if (NULL == ppropnames->ppropname[i].pname) {
				return 0;
			}
			break;
		case IS_DOUBLE:
			ppropnames->ppropname[i].kind = KIND_LID;
			ppropnames->ppropname[i].plid = st_malloc<uint32_t>();
			if (NULL == ppropnames->ppropname[i].plid) {
				return 0;
			}
			*ppropnames->ppropname[i].plid = zval_get_long(entry);
			ppropnames->ppropname[i].pname = NULL;
			break;
		default:
			return 0;
		}
		zend_hash_move_forward_ex(pnameshash, &thpos);
		if(NULL != pguidhash) {
			zend_hash_move_forward_ex(pguidhash, &ghpos);
		}
	}
	return 1;
}