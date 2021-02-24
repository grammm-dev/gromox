#include "common_util.h"
#include <gromox/hpm_common.h>
#define PROPVAL_TYPE_UNSPECIFIED PT_UNSPECIFIED
#define PROPVAL_TYPE_BYTE PT_BOOLEAN
#define PROPVAL_TYPE_SHORT PT_SHORT
#define PROPVAL_TYPE_LONG PT_LONG
#define PROPVAL_TYPE_STRING PT_STRING8
#define PROPVAL_TYPE_WSTRING PT_UNICODE
#define PROPVAL_TYPE_BINARY PT_BINARY
#define PROPVAL_TYPE_GUID PT_CLSID
#define PROPVAL_TYPE_FILETIME PT_SYSTIME
#define PROPVAL_TYPE_ERROR PT_ERROR
#define PROPVAL_TYPE_SHORT_ARRAY PT_MV_SHORT
#define PROPVAL_TYPE_LONG_ARRAY PT_MV_LONG
#define PROPVAL_TYPE_LONGLONG_ARRAY PT_MV_I8
#define PROPVAL_TYPE_STRING_ARRAY PT_MV_STRING8
#define PROPVAL_TYPE_WSTRING_ARRAY PT_MV_UNICODE
#define PROPVAL_TYPE_BINARY_ARRAY PT_MV_BINARY
#define PROPVAL_TYPE_GUID_ARRAY PT_MV_CLSID
#define RESTRICTION_TYPE_AND RES_AND
#define RESTRICTION_TYPE_OR RES_OR
#define RESTRICTION_TYPE_NOT RES_NOT
#define RESTRICTION_TYPE_CONTENT RES_CONTENT
#define RESTRICTION_TYPE_PROPERTY RES_PROPERTY
#define RESTRICTION_TYPE_PROPCOMPARE RES_PROPCOMPARE
#define RESTRICTION_TYPE_BITMASK RES_BITMASK
#define RESTRICTION_TYPE_SIZE RES_SIZE
#define RESTRICTION_TYPE_EXIST RES_EXIST
#define RESTRICTION_TYPE_SUBOBJ RES_SUBRESTRICTION
#define EC_NOT_FOUND ecNotFound

void* common_util_alloc(size_t size)
{
	return ndr_stack_alloc(NDR_STACK_IN, size);
}

static void common_util_nttime_to_filetime(
	uint64_t nttime, FILETIME *pftime)
{
	pftime->low_datetime = nttime & 0xFFFFFFFF;
	pftime->high_datetime = nttime >> 32;
}

static void common_util_filetime_to_nttime(
	const FILETIME *pftime, uint64_t *pnttime)
{
	*pnttime = pftime->high_datetime;
	(*pnttime) <<= 32;
	*pnttime |= pftime->low_datetime;
}

void common_util_guid_to_flatuid(const GUID *pguid, FLATUID *pflatuid)
{
	pflatuid->ab[0] = pguid->time_low & 0XFF;
	pflatuid->ab[1] = (pguid->time_low >> 8) & 0XFF;
	pflatuid->ab[2] = (pguid->time_low >> 16) & 0XFF;
	pflatuid->ab[3] = (pguid->time_low >> 24) & 0XFF;
	pflatuid->ab[4] = pguid->time_mid & 0XFF;
	pflatuid->ab[5] = (pguid->time_mid >> 8) & 0XFF;
	pflatuid->ab[6] = pguid->time_hi_and_version & 0XFF;
	pflatuid->ab[7] = (pguid->time_hi_and_version >> 8) & 0XFF;
	memcpy(pflatuid->ab + 8,  pguid->clock_seq, sizeof(uint8_t) * 2);
	memcpy(pflatuid->ab + 10, pguid->node, sizeof(uint8_t) * 6);
}

void common_util_flatuid_to_guid(const FLATUID *pflatuid, GUID *pguid)
{
	pguid->time_low = ((uint32_t)pflatuid->ab[3]) << 24;
	pguid->time_low |= ((uint32_t)pflatuid->ab[2]) << 16;
	pguid->time_low |= ((uint32_t)pflatuid->ab[1]) << 8;
	pguid->time_low |= pflatuid->ab[0];
	pguid->time_mid = ((uint32_t)pflatuid->ab[5]) << 8;
	pguid->time_mid |= pflatuid->ab[4];
	pguid->time_hi_and_version = ((uint32_t)pflatuid->ab[7]) << 8;
	pguid->time_hi_and_version |= pflatuid->ab[6];
	memcpy(pguid->clock_seq, pflatuid->ab + 8, sizeof(uint8_t) * 2);
	memcpy(pguid->node, pflatuid->ab + 10, sizeof(uint8_t) * 6);
}

static BOOL common_util_guid_array_to_flatuid_array(
	const GUID_ARRAY *pguids, FLATUID_ARRAY *pflatuids)
{
	int i;
	
	pflatuids->cvalues = pguids->count;
	pflatuids->ppguid = cu_alloc<FLATUID *>(pguids->count);
	if (NULL == pflatuids->ppguid) {
		return FALSE;
	}
	for (i=0; i<pguids->count; i++) {
		pflatuids->ppguid[i] = cu_alloc<FLATUID>();
		if (NULL == pflatuids->ppguid[i]) {
			return FALSE;
		}
		common_util_guid_to_flatuid(pguids->pguid + i, pflatuids->ppguid[i]);
	}
	return TRUE;
}

static BOOL common_util_flatuid_array_to_guid_array(
	const FLATUID_ARRAY *pflatuids, GUID_ARRAY *pguids)
{
	int i;
	
	pguids->count = pflatuids->cvalues;
	pguids->pguid = cu_alloc<GUID>(pflatuids->cvalues);
	if (NULL == pguids->pguid) {
		return FALSE;
	}
	for (i=0; i<pguids->count; i++) {
		common_util_flatuid_to_guid(pflatuids->ppguid[i], pguids->pguid + i);
	}
	return TRUE;
}

BOOL common_util_addressbook_propname_to_nsp(
	const ADDRESSBOOK_PROPNAME *pabname, NSP_PROPNAME *ppropname)
{
	ppropname->pguid = cu_alloc<FLATUID>();
	if (NULL == ppropname->pguid) {
		return FALSE;
	}
	common_util_guid_to_flatuid(&pabname->guid, ppropname->pguid);
	ppropname->reserved = 0;
	ppropname->id = pabname->id;
	return TRUE;
}

static BOOL common_util_propval_to_valunion(uint16_t type,
	const void *pvalue, PROP_VAL_UNION *punion)
{
	switch (type) {
	case PROPVAL_TYPE_SHORT:
		punion->s = *(uint16_t*)pvalue;
		return TRUE;
	case PROPVAL_TYPE_LONG:
		punion->l = *(uint32_t*)pvalue;
		return TRUE;
	case PROPVAL_TYPE_BYTE:
		punion->b = *(uint8_t*)pvalue;
		return TRUE;
	case PROPVAL_TYPE_STRING:
	case PROPVAL_TYPE_WSTRING:
		punion->pstr = static_cast<char *>(deconst(pvalue));
		return TRUE;
	case PROPVAL_TYPE_BINARY:
		if (NULL == pvalue) {
			punion->bin.cb = 0;
			punion->bin.pb = NULL;
		} else {
			punion->bin = *(BINARY*)pvalue;
		}
		return TRUE;
	case PROPVAL_TYPE_GUID:
		punion->pguid = cu_alloc<FLATUID>();
		if (NULL == punion->pguid) {
			return FALSE;
		}
		common_util_guid_to_flatuid(static_cast<const GUID *>(pvalue), reinterpret_cast<FLATUID *>(punion->pguid));
		return TRUE;
	case PROPVAL_TYPE_FILETIME:
		common_util_nttime_to_filetime(*(uint64_t*)pvalue, &punion->ftime);
		return TRUE;
	case PROPVAL_TYPE_ERROR:
		punion->err = *(uint32_t*)pvalue;
		return TRUE;
	case PROPVAL_TYPE_SHORT_ARRAY:
		if (NULL == pvalue) {
			punion->short_array.count = 0;
			punion->short_array.ps = NULL;
		} else {
			punion->short_array = *(SHORT_ARRAY*)pvalue;
		}
		return TRUE;
	case PROPVAL_TYPE_LONG_ARRAY:
		if (NULL == pvalue) {
			punion->long_array.count = 0;
			punion->long_array.pl = NULL;
		} else {
			punion->long_array = *(LONG_ARRAY*)pvalue;
		}
		return TRUE;
	case PROPVAL_TYPE_STRING_ARRAY:
	case PROPVAL_TYPE_WSTRING_ARRAY:
		if (NULL == pvalue) {
			punion->string_array.count = 0;
			punion->string_array.ppstr = NULL;
		} else {
			punion->string_array = *(STRING_ARRAY*)pvalue;
		}
		return TRUE;
	case PROPVAL_TYPE_BINARY_ARRAY:
		if (NULL == pvalue) {
			punion->bin_array.count = 0;
			punion->bin_array.pbin = NULL;
		} else {
			punion->bin_array = *(BINARY_ARRAY*)pvalue;
		}
		return TRUE;
	case PROPVAL_TYPE_GUID_ARRAY:
		if (NULL == pvalue) {
			punion->guid_array.cvalues = 0;
			punion->guid_array.ppguid = NULL;
			return TRUE;
		}
		return common_util_guid_array_to_flatuid_array(
		       static_cast<const GUID_ARRAY *>(pvalue), &punion->guid_array);
	}
	return FALSE;
}

static BOOL common_util_valunion_to_propval(uint16_t type,
	const PROP_VAL_UNION *punion, void **ppvalue)
{
	void *pvalue;
	
	switch (type) {
	case PROPVAL_TYPE_SHORT:
		pvalue = (void*)&punion->s;
		break;
	case PROPVAL_TYPE_LONG:
		pvalue = (void*)&punion->l;
		break;
	case PROPVAL_TYPE_BYTE:
		pvalue = (void*)&punion->b;
		break;
	case PROPVAL_TYPE_STRING:
	case PROPVAL_TYPE_WSTRING:
		pvalue = punion->pstr;
		break;
	case PROPVAL_TYPE_BINARY:
		if (0 == punion->bin.cb) {
			pvalue = NULL;
		} else {
			pvalue = (void*)&punion->bin;
		}
		break;
	case PROPVAL_TYPE_GUID:
		pvalue = cu_alloc<GUID>();
		if (NULL == pvalue) {
			return FALSE;
		}
		common_util_flatuid_to_guid(punion->pguid, static_cast<GUID *>(pvalue));
		break;
	case PROPVAL_TYPE_FILETIME:
		pvalue = cu_alloc<uint64_t>();
		if (NULL == pvalue) {
			return FALSE;
		}
		common_util_filetime_to_nttime(&punion->ftime, static_cast<uint64_t *>(pvalue));
		break;
	case PROPVAL_TYPE_ERROR:
		pvalue = (void*)&punion->err;
		break;
	case PROPVAL_TYPE_SHORT_ARRAY:
		if (0 == punion->short_array.count) {
			pvalue = NULL;
		} else {
			pvalue = (void*)&punion->short_array;
		}
		break;
	case PROPVAL_TYPE_LONG_ARRAY:
		if (0 == punion->long_array.count) {
			pvalue = NULL;
		} else {
			pvalue = (void*)&punion->long_array;
		}
		break;
	case PROPVAL_TYPE_STRING_ARRAY:
	case PROPVAL_TYPE_WSTRING_ARRAY:
		if (0 == punion->string_array.count) {
			pvalue = NULL;
		} else {
			pvalue = (void*)&punion->string_array;
		}
		break;
	case PROPVAL_TYPE_BINARY_ARRAY:
		if (0 == punion->bin_array.count) {
			pvalue = NULL;
		} else {
			pvalue = (void*)&punion->bin_array;
		}
		break;
	case PROPVAL_TYPE_GUID_ARRAY:
		if (0 == punion->guid_array.cvalues) {
			pvalue = NULL;
		} else {
			pvalue = cu_alloc<GUID_ARRAY>();
			if (NULL == pvalue) {
				return FALSE;
			}
			if (FALSE == common_util_flatuid_array_to_guid_array(
			    &punion->guid_array, static_cast<GUID_ARRAY *>(pvalue)))
				return FALSE;	
		}
		break;
	default:
		return FALSE;
	}
	*ppvalue = pvalue;
	return TRUE;
}

BOOL common_util_addressbook_tapropval_to_property_value(
	const ADDRESSBOOK_TAPROPVAL *ptval, PROPERTY_VALUE *pvalue)
{
	pvalue->proptag = ptval->type | (((uint32_t)ptval->propid) << 16);
	pvalue->reserved = 0;
	return common_util_propval_to_valunion(
		ptval->type, &ptval->pvalue, &pvalue->value);
}

BOOL common_util_nsp_proprow_to_addressbook_proplist(
	const NSP_PROPROW *prow, ADDRESSBOOK_PROPLIST *pproplist)
{
	int i;
	
	pproplist->count = prow->cvalues;
	pproplist->ppropval = cu_alloc<ADDRESSBOOK_TAPROPVAL>(prow->cvalues);
	if (NULL == pproplist->ppropval) {
		return FALSE;
	}
	for (i=0; i<prow->cvalues; i++) {
		pproplist->ppropval[i].type = prow->pprops[i].proptag & 0xFFFF;
		pproplist->ppropval[i].propid = prow->pprops[i].proptag >> 16;
		if (FALSE == common_util_valunion_to_propval(
			pproplist->ppropval[i].type, &prow->pprops[i].value,
			&pproplist->ppropval[i].pvalue)) {
			return FALSE;	
		}
	}
	return TRUE;
}

BOOL common_util_addressbook_proplist_to_nsp_proprow(
	const ADDRESSBOOK_PROPLIST *pproplist, NSP_PROPROW *prow)
{
	int i;
	
	prow->reserved = 0;
	prow->cvalues = pproplist->count;
	prow->pprops = cu_alloc<PROPERTY_VALUE>(pproplist->count);
	if (NULL == prow->pprops) {
		return FALSE;
	}
	for (i=0; i<pproplist->count; i++) {
		prow->pprops[i].proptag = pproplist->ppropval[i].type |
			(((uint32_t)pproplist->ppropval[i].propid) << 16);
		prow->pprops[i].reserved = 0;
		if (FALSE == common_util_propval_to_valunion(
			pproplist->ppropval[i].type,
			&pproplist->ppropval[i].pvalue,
			&prow->pprops[i].value)) {
			return FALSE;	
		}
	}
	return TRUE;	
}

static BOOL common_util_nsp_proprow_to_addressbook_proprow(
	const LPROPTAG_ARRAY *pcolumns, const NSP_PROPROW *pnsprow,
	ADDRESSBOOK_PROPROW *pabrow)
{
	int i;
	
	if (0 == pnsprow->cvalues) {
		pabrow->ppvalue = NULL;
	} else {
		pabrow->ppvalue = cu_alloc<void *>(pnsprow->cvalues);
		if (NULL == pabrow->ppvalue) {
			return FALSE;
		}
	}
	for (i=0; i<pnsprow->cvalues; i++) {
		if (PROPVAL_TYPE_ERROR == (pnsprow->pprops[i].proptag & 0xFFFF)) {
			break;
		}
	}
	if (i < pnsprow->cvalues) {
		pabrow->flag = PROPROW_FLAG_FLAGGED;
	} else {
		pabrow->flag = PROPROW_FLAG_NORMAL;
	}
	for (i=0; i<pnsprow->cvalues; i++) {
		if (PROPVAL_TYPE_ERROR == (pnsprow->pprops[i].proptag & 0xFFFF)) {
			pabrow->ppvalue[i] = cu_alloc<ADDRESSBOOK_FPROPVAL>();
			if (NULL == pabrow->ppvalue[i]) {
				return FALSE;
			}
			if (EC_NOT_FOUND == pnsprow->pprops[i].value.err) {
				((ADDRESSBOOK_FPROPVAL*)pabrow->ppvalue[i])->flag =
										ADDRESSBOOK_FLAG_UNAVAILABLE;
				((ADDRESSBOOK_FPROPVAL*)pabrow->ppvalue[i])->pvalue = NULL;
			} else {
				((ADDRESSBOOK_FPROPVAL*)pabrow->ppvalue[i])->flag =
											ADDRESSBOOK_FLAG_ERROR;
				((ADDRESSBOOK_FPROPVAL*)pabrow->ppvalue[i])->pvalue =
								cu_alloc<uint32_t>();
				if (NULL == ((ADDRESSBOOK_FPROPVAL*)
					pabrow->ppvalue[i])->pvalue) {
					return FALSE;	
				}
				*(uint32_t*)(((ADDRESSBOOK_FPROPVAL*)
					pabrow->ppvalue[i])->pvalue) =
					pnsprow->pprops[i].value.err;
			}
		} else {
			if (PROPROW_FLAG_NORMAL == pabrow->flag) {
				if (PROPVAL_TYPE_UNSPECIFIED ==
					(pcolumns->pproptag[i] & 0xFFFF)) {
					pabrow->ppvalue[i] = cu_alloc<ADDRESSBOOK_TYPROPVAL>();
					if (NULL == pabrow->ppvalue[i]) {
						return FALSE;
					}
					((ADDRESSBOOK_TYPROPVAL*)pabrow->ppvalue[i])->type =
									pnsprow->pprops[i].proptag & 0xFFFF;
					if (FALSE == common_util_valunion_to_propval(
						pnsprow->pprops[i].proptag & 0xFFFF,
						&pnsprow->pprops[i].value,
						&((ADDRESSBOOK_TYPROPVAL*)
						pabrow->ppvalue[i])->pvalue)) {
						return FALSE;						
					}
				} else {
					if (FALSE == common_util_valunion_to_propval(
						pnsprow->pprops[i].proptag & 0xFFFF,
						&pnsprow->pprops[i].value,
						&pabrow->ppvalue[i])) {
						return FALSE;						
					}
				}
			} else {
				if (PROPVAL_TYPE_UNSPECIFIED ==
					(pcolumns->pproptag[i] & 0xFFFF)) {
					pabrow->ppvalue[i] = cu_alloc<ADDRESSBOOK_TFPROPVAL>();
					if (NULL == pabrow->ppvalue[i]) {
						return FALSE;
					}
					((ADDRESSBOOK_TFPROPVAL*)pabrow->ppvalue[i])->flag =
											ADDRESSBOOK_FLAG_AVAILABLE;
					((ADDRESSBOOK_TFPROPVAL*)pabrow->ppvalue[i])->type =
									pnsprow->pprops[i].proptag & 0xFFFF;
					if (FALSE == common_util_valunion_to_propval(
						pnsprow->pprops[i].proptag & 0xFFFF,
						&pnsprow->pprops[i].value,
						&((ADDRESSBOOK_TFPROPVAL*)
						pabrow->ppvalue[i])->pvalue)) {
						return FALSE;						
					}
				} else {
					pabrow->ppvalue[i] = cu_alloc<ADDRESSBOOK_FPROPVAL>();
					if (NULL == pabrow->ppvalue[i]) {
						return FALSE;
					}
					((ADDRESSBOOK_FPROPVAL*)pabrow->ppvalue[i])->flag =
											ADDRESSBOOK_FLAG_AVAILABLE;
					if (FALSE == common_util_valunion_to_propval(
						pnsprow->pprops[i].proptag & 0xFFFF,
						&pnsprow->pprops[i].value,
						&((ADDRESSBOOK_FPROPVAL*)
						pabrow->ppvalue[i])->pvalue)) {
						return FALSE;						
					}
				}
			}
		}
	}
	return TRUE;
}

BOOL common_util_nsp_rowset_to_addressbook_colrow(
	const LPROPTAG_ARRAY *pcolumns, const NSP_ROWSET *pset,
	ADDRESSBOOK_COLROW *pcolrow)
{
	int i;
	
	pcolrow->columns = *pcolumns;
	pcolrow->row_count = pset->crows;
	pcolrow->prows = cu_alloc<ADDRESSBOOK_PROPROW>(pset->crows);
	if (NULL == pcolrow->prows) {
		return FALSE;
	}
	for (i=0; i<pset->crows; i++) {
		if (FALSE == common_util_nsp_proprow_to_addressbook_proprow(
			pcolumns, pset->prows + i, pcolrow->prows + i)) {
			return FALSE;	
		}
	}
	return TRUE;
}

static BOOL common_util_to_nspres_and_or(
	const RESTRICTION_AND_OR *pres, NSPRES_AND_OR *pnspres)
{
	int i;
	
	pnspres->cres = pres->count;
	pnspres->pres = cu_alloc<NSPRES>(pres->count);
	if (NULL == pnspres->pres) {
		return FALSE;
	}
	for (i=0; i<pres->count; i++) {
		if (FALSE == common_util_restriction_to_nspres(
			pres->pres + i, pnspres->pres + i)) {
			return FALSE;	
		}
	}
	return TRUE;
}

static BOOL common_util_to_nspres_not(
	const RESTRICTION_NOT *pres, NSPRES_NOT *pnspres)
{
	pnspres->pres = cu_alloc<NSPRES>();
	if (NULL == pnspres->pres) {
		return FALSE;
	}
	return common_util_restriction_to_nspres(
					&pres->res, pnspres->pres);
}

static BOOL common_util_to_nspres_content(
	const RESTRICTION_CONTENT *pres, NSPRES_CONTENT *pnspres)
{
	pnspres->fuzzy_level = pres->fuzzy_level;
	pnspres->proptag = pres->proptag;
	pnspres->pprop = cu_alloc<PROPERTY_VALUE>();
	if (NULL == pnspres->pprop) {
		return FALSE;
	}
	pnspres->pprop->proptag = pres->propval.proptag;
	pnspres->pprop->reserved = 0;
	return common_util_propval_to_valunion(
			pres->propval.proptag & 0xFFFF,
			pres->propval.pvalue,
			&pnspres->pprop->value);
}

static BOOL common_util_to_nspres_property(
	const RESTRICTION_PROPERTY *pres, NSPRES_PROPERTY *pnspres)
{
	pnspres->relop = pres->relop;
	pnspres->proptag = pres->proptag;
	pnspres->pprop = cu_alloc<PROPERTY_VALUE>();
	if (NULL == pnspres->pprop) {
		return FALSE;
	}
	pnspres->pprop->proptag = pres->propval.proptag;
	pnspres->pprop->reserved = 0;
	return common_util_propval_to_valunion(
			pres->propval.proptag & 0xFFFF,
			pres->propval.pvalue,
			&pnspres->pprop->value);
}

static void common_util_to_nspres_propcompare(
	const RESTRICTION_PROPCOMPARE *pres,
	NSPRES_PROPCOMPARE *pnspres)
{
	pnspres->relop = pres->relop;
	pnspres->proptag1 = pres->proptag1;
	pnspres->proptag2 = pres->proptag2;
}

static void common_util_to_nspres_bitmask(
	const RESTRICTION_BITMASK *pres,
	NSPRES_BITMASK *pnspres)
{
	pnspres->rel_mbr = pres->bitmask_relop;
	pnspres->proptag = pres->proptag;
	pnspres->mask = pres->mask;
}

static void common_util_to_nspres_size(
	const RESTRICTION_SIZE *pres,
	NSPRES_SIZE *pnspres)
{
	pnspres->relop = pres->relop;
	pnspres->proptag = pres->proptag;
	pnspres->cb = pres->size;
}

static void common_util_to_nspres_exist(
	const RESTRICTION_EXIST *pres,
	NSPRES_EXIST *pnspres)
{
	pnspres->proptag = pres->proptag;
}

static BOOL common_util_to_nspres_subobj(
	const RESTRICTION_SUBOBJ *pres, NSPRES_SUB *pnspres)
{
	pnspres->subobject = pres->subobject;
	pnspres->pres = cu_alloc<NSPRES>();
	if (NULL == pnspres->pres) {
		return FALSE;
	}
	return common_util_restriction_to_nspres(
					&pres->res, pnspres->pres);
}

BOOL common_util_restriction_to_nspres(
	const RESTRICTION *pres, NSPRES *pnspres)
{
	pnspres->res_type = pres->rt;
	switch (pres->rt) {
	case RESTRICTION_TYPE_AND:
		return common_util_to_nspres_and_or(
		       static_cast<const RESTRICTION_AND_OR *>(pres->pres), &pnspres->res.res_and);
	case RESTRICTION_TYPE_OR:
		return common_util_to_nspres_and_or(
		       static_cast<const RESTRICTION_AND_OR *>(pres->pres), &pnspres->res.res_or);
	case RESTRICTION_TYPE_NOT:
		return common_util_to_nspres_not(
		       static_cast<RESTRICTION_NOT *>(pres->pres), &pnspres->res.res_not);
	case RESTRICTION_TYPE_CONTENT:
		return common_util_to_nspres_content(
		       static_cast<RESTRICTION_CONTENT *>(pres->pres), &pnspres->res.res_content);
	case RESTRICTION_TYPE_PROPERTY:
		return common_util_to_nspres_property(
		       static_cast<RESTRICTION_PROPERTY *>(pres->pres), &pnspres->res.res_property);
	case RESTRICTION_TYPE_PROPCOMPARE:
		common_util_to_nspres_propcompare(
			static_cast<RESTRICTION_PROPCOMPARE *>(pres->pres), &pnspres->res.res_propcompare);
		return TRUE;
	case RESTRICTION_TYPE_BITMASK:
		common_util_to_nspres_bitmask(
			static_cast<RESTRICTION_BITMASK *>(pres->pres), &pnspres->res.res_bitmask);
		return TRUE;
	case RESTRICTION_TYPE_SIZE:
		common_util_to_nspres_size(
			static_cast<RESTRICTION_SIZE *>(pres->pres), &pnspres->res.res_size);
		return TRUE;
	case RESTRICTION_TYPE_EXIST:
		common_util_to_nspres_exist(
			static_cast<RESTRICTION_EXIST *>(pres->pres), &pnspres->res.res_exist);
		return TRUE;
	case RESTRICTION_TYPE_SUBOBJ:
		return common_util_to_nspres_subobj(
			static_cast<RESTRICTION_SUBOBJ *>(pres->pres), &pnspres->res.res_sub);
	}
	return FALSE;
}

BOOL common_util_entryid_to_binary(
	const NSP_ENTRYID *pentryid, BINARY *pbin)
{
	int len;
	FLATUID provider_uid;
	
	common_util_guid_to_flatuid(&pentryid->provider_uid, &provider_uid);
	if (0x87 == pentryid->id_type) {
		pbin->cb = 32;
		pbin->pb = cu_alloc<uint8_t>(pbin->cb);
		if (NULL == pbin->pb) {
			return FALSE;
		}
		memset(pbin->pb, 0, pbin->cb);
		pbin->pb[0] = pentryid->id_type;
		pbin->pb[1] = pentryid->r1;
		pbin->pb[2] = pentryid->r2;
		pbin->pb[3] = pentryid->r3;
		memcpy(pbin->pb + 4, provider_uid.ab, 16);
		pbin->pb[20] = pentryid->r4 & 0xFF;
		pbin->pb[21] = (pentryid->r4 >> 8)  & 0xFF;
		pbin->pb[22] = (pentryid->r4 >> 16) & 0xFF;
		pbin->pb[23] = (pentryid->r4 >> 24) & 0xFF;
		pbin->pb[24] = pentryid->display_type & 0xFF;
		pbin->pb[25] = (pentryid->display_type >> 8)  & 0xFF;
		pbin->pb[26] = (pentryid->display_type >> 16) & 0xFF;
		pbin->pb[27] = (pentryid->display_type >> 24) & 0xFF;
		pbin->pb[28] = pentryid->payload.mid & 0xFF;
		pbin->pb[29] = (pentryid->payload.mid >> 8)  & 0xFF;
		pbin->pb[30] = (pentryid->payload.mid >> 16) & 0xFF;
		pbin->pb[31] = (pentryid->payload.mid >> 24) & 0xFF;
	} else {
		len = strlen(pentryid->payload.pdn) + 1;
		pbin->cb = 28 + len;
		pbin->pb = cu_alloc<uint8_t>(pbin->cb);
		if (NULL == pbin->pb) {
			return FALSE;
		}
		memset(pbin->pb, 0, pbin->cb);
		pbin->pb[0] = pentryid->id_type;
		pbin->pb[1] = pentryid->r1;
		pbin->pb[2] = pentryid->r2;
		pbin->pb[3] = pentryid->r3;
		memcpy(pbin->pb + 4, provider_uid.ab, 16);
		pbin->pb[20] = (pentryid->r4 & 0xFF);
		pbin->pb[21] = ((pentryid->r4 >> 8)  & 0xFF);
		pbin->pb[22] = ((pentryid->r4 >> 16) & 0xFF);
		pbin->pb[23] = ((pentryid->r4 >> 24) & 0xFF);
		pbin->pb[24] = (pentryid->display_type & 0xFF);
		pbin->pb[25] = ((pentryid->display_type >> 8)  & 0xFF);
		pbin->pb[26] = ((pentryid->display_type >> 16) & 0xFF);
		pbin->pb[27] = ((pentryid->display_type >> 24) & 0xFF);
		memcpy(pbin->pb + 28, pentryid->payload.pdn, len);
	}
	return TRUE;
}
