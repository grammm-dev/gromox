#ifndef _H_AB_TYPES_
#define _H_AB_TYPES_
#include <gromox/mapi_types.hpp>

struct ADDRESSBOOK_TAPROPVAL {
	uint16_t type;
	uint16_t propid;
	void *pvalue;
};

struct ADDRESSBOOK_PROPLIST {
	uint32_t count;
	ADDRESSBOOK_TAPROPVAL *ppropval;
};

struct ADDRESSBOOK_TYPROPVAL {
	uint16_t type;
	void *pvalue;
};

#define ADDRESSBOOK_FLAG_AVAILABLE		0x0
#define ADDRESSBOOK_FLAG_UNAVAILABLE	0x1
#define ADDRESSBOOK_FLAG_ERROR			0xA

struct ADDRESSBOOK_FPROPVAL {
	uint8_t flag;
	void *pvalue;
};

struct ADDRESSBOOK_TFPROPVAL {
	uint16_t type;
	uint8_t flag;	/*	ADDRESSBOOK_FLAG_AVAILABLE
						ADDRESSBOOK_FLAG_UNAVAILABLE
						ADDRESSBOOK_FLAG_ERROR */
	void *pvalue;
};

#define PROPROW_FLAG_NORMAL				0
#define PROPROW_FLAG_FLAGGED			1

struct ADDRESSBOOK_PROPROW {
	uint8_t flag;
	void **ppvalue;
};

struct LPROPTAG_ARRAY {
	uint32_t cvalues;
	uint32_t *pproptag;
};
using MID_ARRAY = LPROPTAG_ARRAY;

struct STAT {
	uint32_t sort_type;
	uint32_t container_id;
	uint32_t cur_rec;
	int32_t delta;
	uint32_t num_pos;
	uint32_t total_rec;
	uint32_t codepage;
	uint32_t template_locale;
	uint32_t sort_locale;
};

struct ADDRESSBOOK_PROPNAME {
	GUID guid;
	uint32_t id;
};

struct ADDRESSBOOK_COLROW {
	LPROPTAG_ARRAY columns;
	uint32_t row_count;
	ADDRESSBOOK_PROPROW *prows;
};

struct NSP_ENTRYID {
	uint8_t id_type;		/* constant: 0x87, 0x00 */
	uint8_t r1;				/* reserved: 0x0        */
	uint8_t r2;				/* reserved: 0x0        */
	uint8_t r3;				/* reserved: 0x0        */
	GUID provider_uid;		/* NSPI server GUID     */
	uint32_t r4;			/* constant: 0x1        */
	uint32_t display_type;	/* must match one of the existing display type value */
	union {
		char *pdn;				/* DN string representing the object GUID */
		uint32_t mid;			/* mid of this object   */
	} payload;
};

struct NSP_ENTRYIDS {
	uint32_t count;
	NSP_ENTRYID *pentryid;
};

struct BIND_REQUEST {
	uint32_t flags;
	STAT *pstat;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct BIND_RESPONSE {
	uint32_t status;
	uint32_t result;
	GUID server_guid;
};

struct UNBIND_REQUEST {
	uint32_t reserved;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct UNBIND_RESPONSE {
	uint32_t status;
	uint32_t result;
};

struct COMPAREMIDS_REQUEST {
	uint32_t reserved;
	STAT *pstat;
	uint32_t mid1;
	uint32_t mid2;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct COMPAREMIDS_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t result1;
};

struct DNTOMID_REQUEST {
	uint32_t reserved;
	STRING_ARRAY *pnames;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct DNTOMID_RESPONSE {
	uint32_t status;
	uint32_t result;
	MID_ARRAY *poutmids;
};

struct GETMATCHES_REQUEST {
	uint32_t reserved1;
	STAT *pstat;
	MID_ARRAY *pinmids;
	uint32_t reserved2;
	RESTRICTION *pfilter;
	ADDRESSBOOK_PROPNAME *ppropname;
	uint32_t row_count;
	LPROPTAG_ARRAY *pcolumns;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct GETMATCHES_RESPONSE {
	uint32_t status;
	uint32_t result;
	STAT *pstat;
	MID_ARRAY *pmids;
	ADDRESSBOOK_COLROW column_rows;
};

struct GETPROPLIST_REQUEST {
	uint32_t flags;
	uint32_t mid;
	uint32_t codepage;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct GETPROPLIST_RESPONSE {
	uint32_t status;
	uint32_t result;
	LPROPTAG_ARRAY *pproptags;
};

struct GETPROPS_REQUEST {
	uint32_t flags;
	STAT *pstat;
	LPROPTAG_ARRAY *pproptags;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct GETPROPS_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t codepage;
	ADDRESSBOOK_PROPLIST *prow;
};

struct GETSPECIALTABLE_REQUEST {
	uint32_t flags;
	STAT *pstat;
	uint32_t *pversion;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct GETSPECIALTABLE_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t codepage;
	uint32_t *pversion;
	uint32_t count;
	ADDRESSBOOK_PROPLIST *prow;
};

struct GETTEMPLATEINFO_REQUEST {
	uint32_t flags;
	uint32_t type;
	char *pdn;
	uint32_t codepage;
	uint32_t locale_id;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct GETTEMPLATEINFO_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t codepage;
	ADDRESSBOOK_PROPLIST *prow;
};

struct MODLINKATT_REQUEST {
	uint32_t flags;
	uint32_t proptag;
	uint32_t mid;
	NSP_ENTRYIDS entryids;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct MODLINKATT_RESPONSE {
	uint32_t status;
	uint32_t result;
};

struct MODPROPS_REQUEST {
	uint32_t reserved;
	STAT *pstat;
	LPROPTAG_ARRAY *pproptags;
	ADDRESSBOOK_PROPLIST *pvalues;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct MODPROPS_RESPONSE {
	uint32_t status;
	uint32_t result;
};

struct QUERYROWS_REQUEST {
	uint32_t flags;
	STAT *pstat;
	MID_ARRAY explicit_table;
	uint32_t count;
	LPROPTAG_ARRAY *pcolumns;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct QUERYROWS_RESPONSE {
	uint32_t status;
	uint32_t result;
	STAT *pstat;
	ADDRESSBOOK_COLROW column_rows;
};

struct QUERYCOLUMNS_REQUEST {
	uint32_t reserved;
	uint32_t flags;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct QUERYCOLUMNS_RESPONSE {
	uint32_t status;
	uint32_t result;
	LPROPTAG_ARRAY *pcolumns;
};

struct RESOLVENAMES_REQUEST {
	uint32_t reserved;
	STAT *pstat;
	LPROPTAG_ARRAY *pproptags;
	STRING_ARRAY *pnames;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct RESOLVENAMES_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t codepage;
	MID_ARRAY *pmids;
	ADDRESSBOOK_COLROW column_rows;
};

struct RESORTRESTRICTION_REQUEST {
	uint32_t reserved;
	STAT *pstat;
	MID_ARRAY *pinmids;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct RESORTRESTRICTION_RESPONSE {
	uint32_t status;
	uint32_t result;
	STAT *pstat;
	MID_ARRAY *poutmids;
};

struct SEEKENTRIES_REQUEST {
	uint32_t reserved;
	STAT *pstat;
	ADDRESSBOOK_TAPROPVAL *ptarget;
	MID_ARRAY *pexplicit_table;
	LPROPTAG_ARRAY *pcolumns;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct SEEKENTRIES_RESPONSE {
	uint32_t status;
	uint32_t result;
	STAT *pstat;
	ADDRESSBOOK_COLROW column_rows;
};

struct UPDATESTAT_REQUEST {
	uint32_t reserved;
	STAT *pstat;
	uint8_t delta_requested;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct UPDATESTAT_RESPONSE {
	uint32_t status;
	uint32_t result;
	STAT *pstat;
	int32_t *pdelta;
};

struct GETMAILBOXURL_REQUEST {
	uint32_t flags;
	char *puser_dn;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct GETMAILBOXURL_RESPONSE {
	uint32_t status;
	uint32_t result;
	char server_url[1024];
};

struct GETADDRESSBOOKURL_REQUEST {
	uint32_t flags;
	char *puser_dn;
	uint32_t cb_auxin;
	uint8_t *pauxin;
};

struct GETADDRESSBOOKURL_RESPONSE {
	uint32_t status;
	uint32_t result;
	char server_url[1024];
};

union NSP_REQUEST {
	BIND_REQUEST bind;
	UNBIND_REQUEST unbind;
	COMPAREMIDS_REQUEST comparemids;
	DNTOMID_REQUEST dntomid;
	GETMATCHES_REQUEST getmatches;
	GETPROPLIST_REQUEST getproplist;
	GETPROPS_REQUEST getprops;
	GETSPECIALTABLE_REQUEST getspecialtable;
	GETTEMPLATEINFO_REQUEST gettemplateinfo;
	MODLINKATT_REQUEST modlinkatt;
	MODPROPS_REQUEST modprops;
	QUERYCOLUMNS_REQUEST querycolumns;
	QUERYROWS_REQUEST queryrows;
	RESOLVENAMES_REQUEST resolvenames;
	RESORTRESTRICTION_REQUEST resortrestriction;
	SEEKENTRIES_REQUEST seekentries;
	UPDATESTAT_REQUEST updatestat;
	GETMAILBOXURL_REQUEST getmailboxurl;
	GETADDRESSBOOKURL_REQUEST getaddressbookurl;
};

union NSP_RESPONSE {
	BIND_RESPONSE bind;
	UNBIND_RESPONSE unbind;
	COMPAREMIDS_RESPONSE comparemids;
	DNTOMID_RESPONSE dntomid;
	GETMATCHES_RESPONSE getmatches;
	GETPROPLIST_RESPONSE getproplist;
	GETPROPS_RESPONSE getprops;
	GETSPECIALTABLE_RESPONSE getspecialtable;
	GETTEMPLATEINFO_RESPONSE gettemplateinfo;
	MODLINKATT_RESPONSE modlinkatt;
	MODPROPS_RESPONSE modprops;
	QUERYCOLUMNS_RESPONSE querycolumns;
	QUERYROWS_RESPONSE queryrows;
	RESOLVENAMES_RESPONSE resolvenames;
	RESORTRESTRICTION_RESPONSE resortrestriction;
	SEEKENTRIES_RESPONSE seekentries;
	UPDATESTAT_RESPONSE updatestat;
	GETMAILBOXURL_RESPONSE getmailboxurl;
	GETADDRESSBOOKURL_RESPONSE getaddressbookurl;
};

/****************** definition from exchange_nsp ******************/

struct FLATUID {
	uint8_t ab[16];
};

struct NSP_PROPNAME {
	FLATUID *pguid;
	uint32_t reserved;
	uint32_t id;
};

struct FILETIME {
	uint32_t low_datetime;
	uint32_t high_datetime;
};

struct FLATUID_ARRAY {
	uint32_t cvalues;
	FLATUID **ppguid;
};

struct FILETIME_ARRAY {
	uint32_t cvalues;
	FILETIME *pftime;
};

union PROP_VAL_UNION {
	uint16_t s;
	uint32_t l;
	uint8_t b;
	char *pstr;
	BINARY bin;
	FLATUID *pguid;
	FILETIME ftime;
	uint32_t err;
	SHORT_ARRAY short_array;
	LONG_ARRAY long_array;
	STRING_ARRAY string_array;
	BINARY_ARRAY bin_array;
	FLATUID_ARRAY guid_array;
	uint32_t reserved;
};

struct PROPERTY_VALUE {
	uint32_t proptag;
	uint32_t reserved;
	PROP_VAL_UNION value; /* type is proptag&0xFFFF */
};

struct NSP_PROPROW {
	uint32_t reserved;
	uint32_t cvalues;
	PROPERTY_VALUE *pprops;
};

struct NSP_ROWSET {
	uint32_t crows;
	NSP_PROPROW *prows;
};

struct NSPRES_AND_OR {
	uint32_t cres;
	struct NSPRES *pres;
};

struct NSPRES_NOT {
	struct NSPRES *pres;
};

struct NSPRES_CONTENT {
	uint32_t fuzzy_level;
	uint32_t proptag;
	PROPERTY_VALUE *pprop;
};

struct NSPRES_PROPERTY {
	uint32_t relop;
	uint32_t proptag;
	PROPERTY_VALUE *pprop;
};

struct NSPRES_PROPCOMPARE {
	uint32_t relop;
	uint32_t proptag1;
	uint32_t proptag2;
};

struct NSPRES_BITMASK {
	uint32_t rel_mbr;
	uint32_t proptag;
	uint32_t mask;
};

struct NSPRES_SIZE {
	uint32_t relop;
	uint32_t proptag;
	uint32_t cb;
};

struct NSPRES_EXIST {
	uint32_t reserved1;
	uint32_t proptag;
	uint32_t reserved2;
};

struct NSPRES_SUB {
	uint32_t subobject;
	struct NSPRES *pres;
};

union NSPRES_UNION {
	NSPRES_AND_OR res_and;
	NSPRES_AND_OR res_or;
	NSPRES_NOT res_not;
	NSPRES_CONTENT res_content;
	NSPRES_PROPERTY res_property;
	NSPRES_PROPCOMPARE res_propcompare;
	NSPRES_BITMASK res_bitmask;
	NSPRES_SIZE res_size;
	NSPRES_EXIST res_exist;
	NSPRES_SUB res_sub;
};

struct NSPRES {
	uint32_t res_type;
	NSPRES_UNION res;
};

#endif /* _H_AB_TYPES_ */
