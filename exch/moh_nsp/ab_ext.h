#ifndef _H_AB_EXT_
#define _H_AB_EXT_
#include "ab_types.h"
#include <gromox/ext_buffer.hpp>

int ab_ext_pull_bind_request(EXT_PULL *pext, BIND_REQUEST *prequest);

int ab_ext_pull_unbind_request(EXT_PULL *pext, UNBIND_REQUEST *prequest);

int ab_ext_pull_comparemids_request(EXT_PULL *pext,
	COMPAREMIDS_REQUEST *prequest);

int ab_ext_pull_dntomid_request(EXT_PULL *pext, DNTOMID_REQUEST *prequest);

int ab_ext_pull_getmatches_request(
	EXT_PULL *pext, GETMATCHES_REQUEST *prequest);

int ab_ext_pull_getproplist_request(EXT_PULL *pext,
	GETPROPLIST_REQUEST *prequest);

int ab_ext_pull_getprops_request(EXT_PULL *pext, GETPROPS_REQUEST *prequest);

int ab_ext_pull_getspecialtable_request(EXT_PULL *pext,
	GETSPECIALTABLE_REQUEST *prequest);

int ab_ext_pull_gettemplateinfo_request(EXT_PULL *pext,
	GETTEMPLATEINFO_REQUEST *prequest);

int ab_ext_pull_modlinkatt_request(EXT_PULL *pext,
	MODLINKATT_REQUEST *prequest);

int ab_ext_pull_modprops_request(EXT_PULL *pext, MODPROPS_REQUEST *prequest);

int ab_ext_pull_queryrows_request(EXT_PULL *pext,
	QUERYROWS_REQUEST *prequest);

int ab_ext_pull_querycolumns_request(EXT_PULL *pext,
	QUERYCOLUMNS_REQUEST *prequest);

int ab_ext_pull_resolvenames_request(
	EXT_PULL *pext, RESOLVENAMES_REQUEST *prequest);

int ab_ext_pull_resortrestriction_request(EXT_PULL *pext,
	RESORTRESTRICTION_REQUEST *prequest);

int ab_ext_pull_seekentries_request(EXT_PULL *pext,
	SEEKENTRIES_REQUEST *prequest);

int ab_ext_pull_updatestat_request(EXT_PULL *pext,
	UPDATESTAT_REQUEST *prequest);

int ab_ext_pull_getmailboxurl_request(EXT_PULL *pext,
	GETMAILBOXURL_REQUEST *prequest);

int ab_ext_pull_getaddressbookurl_request(EXT_PULL *pext,
	GETADDRESSBOOKURL_REQUEST *prequest);

int ab_ext_push_bind_response(EXT_PUSH *pext,
	const BIND_RESPONSE *presponse);

int ab_ext_push_unbind_response(EXT_PUSH *pext,
	const UNBIND_RESPONSE *presponse);

int ab_ext_push_comparemids_response(EXT_PUSH *pext,
	const COMPAREMIDS_RESPONSE *presponse);
	
int ab_ext_push_dntomid_response(EXT_PUSH *pext,
	const DNTOMID_RESPONSE *presponse);

int ab_ext_push_getmatches_response(EXT_PUSH *pext,
	const GETMATCHES_RESPONSE *presponse);
	
int ab_ext_push_getproplist_response(EXT_PUSH *pext,
	const GETPROPLIST_RESPONSE *presponse);

int ab_ext_push_getprops_response(EXT_PUSH *pext,
	const GETPROPS_RESPONSE *presponse);

int ab_ext_push_getspecialtable_response(EXT_PUSH *pext,
	const GETSPECIALTABLE_RESPONSE *presponse);

int ab_ext_push_gettemplateinfo_response(EXT_PUSH *pext,
	const GETTEMPLATEINFO_RESPONSE *presponse);

int ab_ext_push_modlinkatt_response(EXT_PUSH *pext,
	const MODLINKATT_RESPONSE *presponse);

int ab_ext_push_modprops_response(EXT_PUSH *pext,
	const MODPROPS_RESPONSE *presponse);

int ab_ext_push_queryrows_response(EXT_PUSH *pext,
	const QUERYROWS_RESPONSE *presponse);

int ab_ext_push_querycolumns_response(EXT_PUSH *pext,
	const QUERYCOLUMNS_RESPONSE *presponse);

int ab_ext_push_resolvenames_response(EXT_PUSH *pext,
	const RESOLVENAMES_RESPONSE *presponse);

int ab_ext_push_resortrestriction_response(EXT_PUSH *pext,
	const RESORTRESTRICTION_RESPONSE *presponse);

int ab_ext_push_seekentries_response(EXT_PUSH *pext,
	const SEEKENTRIES_RESPONSE *presponse);

int ab_ext_push_updatestat_response(EXT_PUSH *pext,
	const UPDATESTAT_RESPONSE *presponse);

int ab_ext_push_getaddressbookurl_response(EXT_PUSH *pext,
	const GETADDRESSBOOKURL_RESPONSE *presponse);

int ab_ext_push_failure_response(EXT_PUSH *pext, uint32_t status_code);
extern int ab_ext_push_getmailboxurl_response(EXT_PUSH *, const GETMAILBOXURL_RESPONSE *);

#endif /* _H_AB_EXT_ */
