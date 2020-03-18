#ifndef _H_NSP_BRIDGE_
#define _H_NSP_BRIDGE_
#include "ab_types.h"

uint32_t nsp_bridge_bind(uint32_t flags, const STAT *pstat,
	uint32_t cb_auxin, const uint8_t *pauxin, GUID *psession_guid,
	GUID *pserver_guid);

uint32_t nsp_bridge_unbind(GUID session_guid, uint32_t reserved,
	uint32_t cb_auxin, const uint8_t *pauxin);

uint32_t nsp_bridge_comparemids(GUID session_guid, uint32_t reserved,
	const STAT *pstat, uint32_t mid1, uint32_t mid2, uint32_t cb_auxin,
	const uint8_t *pauxin, uint32_t *presult);

uint32_t nsp_bridge_dntomid(GUID session_guid,
	uint32_t reserved, const STRING_ARRAY *pnames,
	uint32_t cb_auxin, const uint8_t *pauxin,
	MID_ARRAY **ppoutmids);

uint32_t nsp_bridge_getmatches(GUID session_guid, uint32_t reserved1,
	STAT *pstat, const MID_ARRAY *pinmids, uint32_t reserved2,
	const RESTRICTION *pfilter, const ADDRESSBOOK_PROPNAME *ppropname,
	uint32_t row_count, const LPROPTAG_ARRAY *pcolumns,
	uint32_t cb_auxin, const uint8_t *pauxin, MID_ARRAY **ppmids,
	ADDRESSBOOK_COLROW *pcolumn_rows);

uint32_t nsp_bridge_getproplist(GUID session_guid, uint32_t flags,
	uint32_t mid, uint32_t codepage, uint32_t cb_auxin,
	const uint8_t *pauxin, LPROPTAG_ARRAY **ppproptags);

uint32_t nsp_bridge_getprops(GUID session_guid, uint32_t flags,
	const STAT *pstat, const LPROPTAG_ARRAY *pproptags,
	uint32_t cb_auxin, const uint8_t *pauxin, uint32_t *pcodepage,
	ADDRESSBOOK_PROPLIST **pprow);

uint32_t nsp_bridge_getspecialtable(GUID session_guid, uint32_t flags,
	const STAT *pstat, uint32_t *pversion, uint32_t cb_auxin,
	const uint8_t *pauxin, uint32_t *pcodepage, uint32_t **ppversion,
	uint32_t *prow_count, ADDRESSBOOK_PROPLIST **pprows);

uint32_t nsp_bridge_gettemplateinfo(GUID session_guid, uint32_t flags,
	uint32_t type, const char *pdn, uint32_t codepage, uint32_t locale_id,
	uint32_t cb_auxin, const uint8_t *pauxin, uint32_t *pcodepage,
	ADDRESSBOOK_PROPLIST **pprow);

uint32_t nsp_bridge_modlinkatt(GUID session_guid, uint32_t flags,
	uint32_t proptag, uint32_t mid, const NSP_ENTRYIDS *pentryids,
	uint32_t cb_auxin, const uint8_t *pauxin);

uint32_t nsp_bridge_modprops(GUID session_guid, uint32_t reserved,
	const STAT *pstat, const LPROPTAG_ARRAY *pproptags,
	const ADDRESSBOOK_PROPLIST *pvalues, uint32_t cb_auxin,
	const uint8_t *pauxin);

uint32_t nsp_bridge_queryrows(GUID session_guid, uint32_t flags,
	STAT *pstat, const MID_ARRAY explicit_table, uint32_t count,
	const LPROPTAG_ARRAY *pcolumns, uint32_t cb_auxin,
	const uint8_t *pauxin, ADDRESSBOOK_COLROW *pcolumn_rows);

uint32_t nsp_bridge_querycolumns(GUID session_guid,
	uint32_t reserved, uint32_t flags, uint32_t cb_auxin,
	const uint8_t *pauxin, LPROPTAG_ARRAY **ppcolumns);

uint32_t nsp_bridge_resolvenames(
	GUID session_guid, uint32_t reserved, const STAT *pstat,
	const LPROPTAG_ARRAY *pproptags, const STRING_ARRAY *pnames,
	uint32_t cb_auxin, const uint8_t *pauxin, uint32_t *pcodepage,
	MID_ARRAY **ppmids, ADDRESSBOOK_COLROW *pcolumn_rows);

uint32_t nsp_bridge_resortrestriction(GUID session_guid, 
	uint32_t reserved, STAT *pstat, const MID_ARRAY *pinmids,
	uint32_t cb_auxin, const uint8_t *pauxin, MID_ARRAY **ppoutmids);

uint32_t nsp_bridge_seekentries(GUID session_guid,
	uint32_t reserved, STAT *pstat, const ADDRESSBOOK_TAPROPVAL *ptarget,
	const MID_ARRAY *pexplicit_table, const LPROPTAG_ARRAY *pcolumns,
	uint32_t cb_auxin, const uint8_t *pauxin,
	ADDRESSBOOK_COLROW *pcolumn_rows);

uint32_t nsp_bridge_updatestat(GUID session_guid, uint32_t reserved,
	STAT *pstat, uint8_t delta_requested, uint32_t cb_auxin,
	const uint8_t *pauxin, int32_t **ppdelta);

void nsp_bridge_touch_handle(GUID session_guid);

#endif /* _H_NSP_BRIDGE_ */
