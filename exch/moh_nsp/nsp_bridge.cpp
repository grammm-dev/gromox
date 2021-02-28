#include <gromox/rpc_types.hpp>
#include "nsp_bridge.h"
#include "common_util.h"
#include <string.h>
#define HANDLE_EXCHANGE_NSP								1

#define MAPI_E_SUCCESS 									0x00000000
#define MAPI_E_CALL_FAILED								0x80004005

#define FLAG_UNICODESTRINGS								0x00000004

using NSP_HANDLE = CONTEXT_HANDLE;

int (*nsp_interface_bind)(uint64_t hrpc, uint32_t flags,
	const STAT *pstat, FLATUID *pserver_guid, NSP_HANDLE *phandle);
uint32_t (*nsp_interface_unbind)(
	NSP_HANDLE *phandle, uint32_t reserved);
int (*nsp_interface_update_stat)(NSP_HANDLE handle,
	uint32_t reserved, STAT *pstat, int32_t *pdelta);
int (*nsp_interface_query_rows)(NSP_HANDLE handle, uint32_t flags,
	STAT *pstat, uint32_t table_count, uint32_t *ptable, uint32_t count,
	const LPROPTAG_ARRAY *pproptags, NSP_ROWSET **pprows);
int (*nsp_interface_seek_entries)(NSP_HANDLE handle,
	uint32_t reserved, STAT *pstat, PROPERTY_VALUE *ptarget,
	const MID_ARRAY *ptable, const LPROPTAG_ARRAY *pproptags,
	NSP_ROWSET **pprows);
decltype(nsp_interface_get_matches) nsp_interface_get_matches;
int (*nsp_interface_resort_restriction)(
	NSP_HANDLE handle, uint32_t reserved, STAT *pstat,
	const MID_ARRAY *pinmids, MID_ARRAY **ppoutmids);
int (*nsp_interface_dntomid)(NSP_HANDLE handle,
	uint32_t reserved, const STRING_ARRAY *pnames,
	MID_ARRAY **ppoutmids);
int (*nsp_interface_get_proplist)(NSP_HANDLE handle,
	uint32_t flags, uint32_t mid, uint32_t codepage,
	LPROPTAG_ARRAY **ppproptags);
int (*nsp_interface_get_props)(
	NSP_HANDLE handle, uint32_t flags, const STAT *pstat,
	const LPROPTAG_ARRAY *pproptags, NSP_PROPROW **pprows);
int (*nsp_interface_compare_mids)(
	NSP_HANDLE handle, uint32_t reserved, const STAT *pstat,
	uint32_t mid1, uint32_t mid2, uint32_t *presult);
int (*nsp_interface_mod_props)(
	NSP_HANDLE handle, uint32_t reserved, const STAT *pstat,
	const LPROPTAG_ARRAY *pproptags, const NSP_PROPROW *prow);
int (*nsp_interface_get_specialtable)(NSP_HANDLE handle,
	uint32_t flags, const STAT *pstat, uint32_t *pversion,
	NSP_ROWSET **pprows);
int (*nsp_interface_get_templateinfo)(NSP_HANDLE handle,
	uint32_t flags, uint32_t type, const char *pdn,
	uint32_t codepage, uint32_t locale_id, NSP_PROPROW **ppdata);
int (*nsp_interface_mod_linkatt)(NSP_HANDLE handle,
	uint32_t flags, uint32_t proptag, uint32_t mid,
	const BINARY_ARRAY *pentry_ids);
int (*nsp_interface_query_columns)(NSP_HANDLE handle,
	uint32_t reserved, uint32_t flags, LPROPTAG_ARRAY **ppcolumns);
int (*nsp_interface_resolve_namesw)(NSP_HANDLE handle,
	uint32_t reserved, const STAT *pstat, const LPROPTAG_ARRAY *pproptags,
	const STRING_ARRAY *pstrs, MID_ARRAY **ppmids, NSP_ROWSET **pprows);

uint32_t nsp_bridge_bind(uint32_t flags, const STAT *pstat,
	uint32_t cb_auxin, const uint8_t *pauxin, GUID *psession_guid,
	GUID *pserver_guid)
{
	uint32_t result;
	FLATUID server_flatuid;
	NSP_HANDLE session_handle;
	
	result = nsp_interface_bind(0, flags, pstat,
			&server_flatuid, &session_handle);
	if (result != ecSuccess) {
		memset(psession_guid, 0, sizeof(GUID));
		memset(pserver_guid, 0, sizeof(GUID));
		return result;
	}
	common_util_flatuid_to_guid(&server_flatuid, pserver_guid);
	*psession_guid = session_handle.guid;
	return ecSuccess;
}

uint32_t nsp_bridge_unbind(GUID session_guid, uint32_t reserved,
	uint32_t cb_auxin, const uint8_t *pauxin)
{
	NSP_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	return nsp_interface_unbind(&session_handle, reserved);
}

uint32_t nsp_bridge_comparemids(GUID session_guid, uint32_t reserved,
	const STAT *pstat, uint32_t mid1, uint32_t mid2, uint32_t cb_auxin,
	const uint8_t *pauxin, uint32_t *presult)
{
	NSP_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	return nsp_interface_compare_mids(session_handle,
				reserved, pstat, mid1, mid2, presult);
}

uint32_t nsp_bridge_dntomid(GUID session_guid,
	uint32_t reserved, const STRING_ARRAY *pnames,
	uint32_t cb_auxin, const uint8_t *pauxin,
	MID_ARRAY **ppoutmids)
{
	NSP_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	return nsp_interface_dntomid(session_handle,
					reserved, pnames, ppoutmids);
}

uint32_t nsp_bridge_getmatches(GUID session_guid, uint32_t reserved1,
	STAT *pstat, const MID_ARRAY *pinmids, uint32_t reserved2,
	const RESTRICTION *pfilter, const ADDRESSBOOK_PROPNAME *ppropname,
	uint32_t row_count, const LPROPTAG_ARRAY *pcolumns,
	uint32_t cb_auxin, const uint8_t *pauxin, MID_ARRAY **ppmids,
	ADDRESSBOOK_COLROW *pcolumn_rows)
{
	NSPRES *pnspres;
	uint32_t result;
	NSP_ROWSET *poutrows;
	NSP_PROPNAME *pnspname;
	NSP_HANDLE session_handle;
	
	memset(pcolumn_rows, 0, sizeof(ADDRESSBOOK_COLROW));
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	if (NULL == pfilter) {
		pnspres = NULL;
	} else {
		pnspres = cu_alloc<NSPRES>();
		if (NULL == pnspres) {
			*ppmids = NULL;
			return ecRpcFailed;
		}
		if (FALSE == common_util_restriction_to_nspres(
			pfilter, pnspres)) {
			*ppmids = NULL;
			return ecRpcFailed;
		}
	}
	if (NULL == ppropname) {
		pnspname = NULL;
	} else {
		pnspname = cu_alloc<NSP_PROPNAME>();
		if (NULL == pnspname) {
			*ppmids = NULL;
			return ecRpcFailed;
		}
		if (FALSE == common_util_addressbook_propname_to_nsp(
			ppropname, pnspname)) {
			*ppmids = NULL;
			return ecRpcFailed;
		}
	}
	result = nsp_interface_get_matches(
		session_handle, reserved1, pstat,
		pinmids, reserved2, pnspres, pnspname,
		row_count, ppmids, pcolumns, &poutrows);
	if (result != ecSuccess)
		return result;
	if (NULL != poutrows) {
		if (FALSE == common_util_nsp_rowset_to_addressbook_colrow(
			pcolumns, poutrows, pcolumn_rows)) {
			return ecRpcFailed;
		}
	}
	return ecSuccess;
}

uint32_t nsp_bridge_getproplist(GUID session_guid, uint32_t flags,
	uint32_t mid, uint32_t codepage, uint32_t cb_auxin,
	const uint8_t *pauxin, LPROPTAG_ARRAY **ppproptags)
{
	NSP_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	return nsp_interface_get_proplist(session_handle,
					flags, mid, codepage, ppproptags);
}

uint32_t nsp_bridge_getprops(GUID session_guid, uint32_t flags,
	const STAT *pstat, const LPROPTAG_ARRAY *pproptags,
	uint32_t cb_auxin, const uint8_t *pauxin, uint32_t *pcodepage,
	ADDRESSBOOK_PROPLIST **pprow)
{
	uint32_t result;
	NSP_PROPROW *prow;
	NSP_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	result = nsp_interface_get_props(session_handle,
					flags, pstat, pproptags, &prow);
	if (result != ecSuccess) {
		*pprow = NULL;
		return result;
	}
	if (NULL == prow) {
		*pprow = NULL;
	} else {
		*pprow = cu_alloc<ADDRESSBOOK_PROPLIST>();
		if (NULL == *pprow) {
			return ecRpcFailed;
		}
		if (FALSE == common_util_nsp_proprow_to_addressbook_proplist(
			prow, *pprow)) {
			*pprow = NULL;
			return ecRpcFailed;
		}
	}
	*pcodepage = pstat->codepage;
	return ecSuccess;
}

uint32_t nsp_bridge_getspecialtable(GUID session_guid, uint32_t flags,
	const STAT *pstat, uint32_t *pversion, uint32_t cb_auxin,
	const uint8_t *pauxin, uint32_t *pcodepage, uint32_t **ppversion,
	uint32_t *prow_count, ADDRESSBOOK_PROPLIST **pprows)
{
	int i;
	uint32_t result;
	uint32_t version;
	NSP_ROWSET *prows;
	NSP_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	if (NULL != pversion) {
		version = *pversion;
	}
	result = nsp_interface_get_specialtable(session_handle,
							flags, pstat, &version, &prows);
	if (result != ecSuccess) {
		*ppversion = NULL;
		*prow_count = 0;
		*pprows = NULL;
		return result;
	}
	if (NULL == pversion) {
		*ppversion = NULL;
	} else {
		*pversion = version;
		*ppversion = pversion;
	}
	if (NULL == prows) {
		*prow_count = 0;
		*pprows = NULL;
		return ecSuccess;
	}
	*prow_count = prows->crows;
	*pprows = cu_alloc<ADDRESSBOOK_PROPLIST>(prows->crows);
	if (NULL == *pprows) {
		return ecRpcFailed;
	}
	for (i=0; i<prows->crows; i++) {
		if (FALSE == common_util_nsp_proprow_to_addressbook_proplist(
			prows->prows + i, (*pprows) + i)) {
			*pprows = NULL;
			return ecRpcFailed;
		}
	}
	*pcodepage = pstat->codepage;
	return ecSuccess;
}

uint32_t nsp_bridge_gettemplateinfo(GUID session_guid, uint32_t flags,
	uint32_t type, const char *pdn, uint32_t codepage, uint32_t locale_id,
	uint32_t cb_auxin, const uint8_t *pauxin, uint32_t *pcodepage,
	ADDRESSBOOK_PROPLIST **pprow)
{
	uint32_t result;
	NSP_PROPROW *prow;
	NSP_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	result = nsp_interface_get_templateinfo(session_handle,
			flags, type, pdn, codepage, locale_id, &prow);
	if (result != ecSuccess) {
		*pprow = NULL;
		return result;
	}
	if (NULL == prow) {
		*pprow = NULL;
		return ecSuccess;
	}
	*pprow = cu_alloc<ADDRESSBOOK_PROPLIST>();
	if (NULL == *pprow) {
		return ecRpcFailed;
	}
	if (FALSE == common_util_nsp_proprow_to_addressbook_proplist(
		prow, *pprow)) {
		*pprow = NULL;
		return ecRpcFailed;
	}
	*pcodepage = codepage;
	return ecSuccess;
}

uint32_t nsp_bridge_modlinkatt(GUID session_guid, uint32_t flags,
	uint32_t proptag, uint32_t mid, const NSP_ENTRYIDS *pentryids,
	uint32_t cb_auxin, const uint8_t *pauxin)
{
	int i;
	NSP_HANDLE session_handle;
	BINARY_ARRAY entryid_array;
	
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	entryid_array.count = pentryids->count;
	if (0 == pentryids->count) {
		entryid_array.pbin = NULL;
	} else {
		entryid_array.pbin = cu_alloc<BINARY>(pentryids->count);
		if (NULL == entryid_array.pbin) {
			return ecRpcFailed;
		}
	}
	for (i=0; i<pentryids->count; i++) {
		if (FALSE == common_util_entryid_to_binary(
			pentryids->pentryid + i, entryid_array.pbin + i)) {
			return ecRpcFailed;
		}
	}
	return nsp_interface_mod_linkatt(session_handle,
			flags, proptag, mid, &entryid_array);
}

uint32_t nsp_bridge_modprops(GUID session_guid, uint32_t reserved,
	const STAT *pstat, const LPROPTAG_ARRAY *pproptags,
	const ADDRESSBOOK_PROPLIST *pvalues, uint32_t cb_auxin,
	const uint8_t *pauxin)
{
	uint32_t result;
	NSP_PROPROW *prow;
	NSP_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	if (NULL == pvalues) {
		prow = NULL;
	} else {
		prow = cu_alloc<NSP_PROPROW>();
		if (NULL == prow) {
			return ecRpcFailed;
		}
		if (FALSE == common_util_addressbook_proplist_to_nsp_proprow(
			pvalues, prow)) {
			return ecRpcFailed;
		}
	}
	return nsp_interface_mod_props(session_handle,
				reserved, pstat, pproptags, prow);
}

uint32_t nsp_bridge_queryrows(GUID session_guid, uint32_t flags,
    STAT *pstat, const LPROPTAG_ARRAY explicit_table, uint32_t count,
	const LPROPTAG_ARRAY *pcolumns, uint32_t cb_auxin,
	const uint8_t *pauxin, ADDRESSBOOK_COLROW *pcolumn_rows)
{
	uint32_t result;
	NSP_ROWSET *prows;
	NSP_HANDLE session_handle;
	
	memset(pcolumn_rows, 0, sizeof(ADDRESSBOOK_COLROW));
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	result = nsp_interface_query_rows(session_handle, flags,
	         pstat, explicit_table.cvalues, explicit_table.pproptag,
		count, pcolumns, &prows);
	if (result != ecSuccess)
		return result;
	if (NULL != prows) {
		if (FALSE == common_util_nsp_rowset_to_addressbook_colrow(
			pcolumns, prows, pcolumn_rows)) {
			return ecRpcFailed;
		}
	}
	return ecSuccess;
}

uint32_t nsp_bridge_querycolumns(GUID session_guid,
	uint32_t reserved, uint32_t flags, uint32_t cb_auxin,
	const uint8_t *pauxin, LPROPTAG_ARRAY **ppcolumns)
{
	NSP_ROWSET *prows;
	NSP_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	return nsp_interface_query_columns(session_handle,
						reserved, flags, ppcolumns);
}

uint32_t nsp_bridge_resolvenames(
	GUID session_guid, uint32_t reserved, const STAT *pstat,
	const LPROPTAG_ARRAY *pproptags, const STRING_ARRAY *pnames,
	uint32_t cb_auxin, const uint8_t *pauxin, uint32_t *pcodepage,
	MID_ARRAY **ppmids, ADDRESSBOOK_COLROW *pcolumn_rows)
{
	uint32_t result;
	NSP_ROWSET *prows;
	NSP_HANDLE session_handle;
	
	memset(pcolumn_rows, 0, sizeof(ADDRESSBOOK_COLROW));
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	*pcodepage = 1200;
	result = nsp_interface_resolve_namesw(session_handle,
		reserved, pstat, pproptags, pnames, ppmids, &prows);
	if (result != ecSuccess)
		return result;
	if (NULL != prows) {
		if (FALSE == common_util_nsp_rowset_to_addressbook_colrow(
			pproptags, prows, pcolumn_rows)) {
			return ecRpcFailed;
		}
	}
	return ecSuccess;
}

uint32_t nsp_bridge_resortrestriction(GUID session_guid, 
	uint32_t reserved, STAT *pstat, const MID_ARRAY *pinmids,
	uint32_t cb_auxin, const uint8_t *pauxin, MID_ARRAY **ppoutmids)
{
	NSP_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	return nsp_interface_resort_restriction(session_handle,
					reserved, pstat, pinmids, ppoutmids);
}

uint32_t nsp_bridge_seekentries(GUID session_guid,
	uint32_t reserved, STAT *pstat, const ADDRESSBOOK_TAPROPVAL *ptarget,
	const MID_ARRAY *pexplicit_table, const LPROPTAG_ARRAY *pcolumns,
	uint32_t cb_auxin, const uint8_t *pauxin,
	ADDRESSBOOK_COLROW *pcolumn_rows)
{
	uint32_t result;
	NSP_ROWSET *prows;
	NSP_HANDLE session_handle;
	PROPERTY_VALUE *ptarget_val;
	
	memset(pcolumn_rows, 0, sizeof(ADDRESSBOOK_COLROW));
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	if (NULL == ptarget) {
		ptarget_val = NULL;
	} else {
		ptarget_val = cu_alloc<PROPERTY_VALUE>();
		if (NULL == ptarget_val) {
			return ecRpcFailed;
		}
		if (FALSE == common_util_addressbook_tapropval_to_property_value(
			ptarget, ptarget_val)) {
			return ecRpcFailed;
		}
	}
	result = nsp_interface_seek_entries(session_handle,
		reserved, pstat, ptarget_val, pexplicit_table,
		pcolumns, &prows);
	if (result != ecSuccess)
		return result;
	if (NULL != prows) {
		if (FALSE == common_util_nsp_rowset_to_addressbook_colrow(
			pcolumns, prows, pcolumn_rows)) {
			return ecRpcFailed;
		}
	}
	return ecSuccess;
}

uint32_t nsp_bridge_updatestat(GUID session_guid, uint32_t reserved,
	STAT *pstat, uint8_t delta_requested, uint32_t cb_auxin,
	const uint8_t *pauxin, int32_t **ppdelta)
{
	int32_t delta;
	uint32_t result;
	NSP_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_NSP;
	session_handle.guid = session_guid;
	if (0 != delta_requested) {
		*ppdelta = cu_alloc<int32_t>();
		if (NULL == *ppdelta) {
			return ecRpcFailed;
		}
	} else {
		*ppdelta = NULL;
	}
	result = nsp_interface_update_stat(session_handle,
							reserved, pstat, &delta);
	if (0 != delta_requested) {
		**ppdelta = delta;
	}
	return result;
}

void nsp_bridge_touch_handle(GUID session_guid)
{
	/* currently do nothing because there's
	no need to toch nsp interface handle */
}
