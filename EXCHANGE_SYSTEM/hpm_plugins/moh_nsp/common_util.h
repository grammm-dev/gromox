#ifndef _H_COMMON_UTIL_
#define _H_COMMON_UTIL_
#include "ab_types.h"

void* common_util_alloc(size_t size);

void common_util_guid_to_flatuid(const GUID *pguid, FLATUID *pflatuid);

void common_util_flatuid_to_guid(const FLATUID *pflatuid, GUID *pguid);

BOOL common_util_addressbook_propname_to_nsp(
	const ADDRESSBOOK_PROPNAME *pabname, NSP_PROPNAME *ppropname);

BOOL common_util_addressbook_tapropval_to_property_value(
	const ADDRESSBOOK_TAPROPVAL *ptval, PROPERTY_VALUE *pvalue);
	
BOOL common_util_nsp_proprow_to_addressbook_proplist(
	const NSP_PROPROW *prow, ADDRESSBOOK_PROPLIST *pproplist);

BOOL common_util_addressbook_proplist_to_nsp_proprow(
	const ADDRESSBOOK_PROPLIST *pproplist, NSP_PROPROW *prow);

BOOL common_util_nsp_rowset_to_addressbook_colrow(
	const LPROPTAG_ARRAY *pcolumns, const NSP_ROWSET *pset,
	ADDRESSBOOK_COLROW *pcolrow);

BOOL common_util_restriction_to_nspres(
	const RESTRICTION *pres, NSPRES *pnspres);

BOOL common_util_entryid_to_binary(
	const NSP_ENTRYID *pentryid, BINARY *pbin);

#endif /* _H_COMMON_UTIL_ */
