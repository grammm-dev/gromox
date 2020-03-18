#ifndef _H_AB_TYPES_
#define _H_AB_TYPES_
#include "mapi_types.h"

typedef struct _ADDRESSBOOK_TAPROPVAL {
	uint16_t type;
	uint16_t propid;
	void *pvalue;
} ADDRESSBOOK_TAPROPVAL;

typedef struct _ADDRESSBOOK_PROPLIST {
	uint32_t count;
	ADDRESSBOOK_TAPROPVAL *ppropval;
} ADDRESSBOOK_PROPLIST;

typedef struct _ADDRESSBOOK_TYPROPVAL {
	uint16_t type;
	void *pvalue;
} ADDRESSBOOK_TYPROPVAL;

#define ADDRESSBOOK_FLAG_AVAILABLE		0x0
#define ADDRESSBOOK_FLAG_UNAVAILABLE	0x1
#define ADDRESSBOOK_FLAG_ERROR			0xA

typedef struct _ADDRESSBOOK_FPROPVAL {
	uint8_t flag;
	void *pvalue;
} ADDRESSBOOK_FPROPVAL;

typedef struct _ADDRESSBOOK_TFPROPVAL {
	uint16_t type;
	uint8_t flag;	/*	ADDRESSBOOK_FLAG_AVAILABLE
						ADDRESSBOOK_FLAG_UNAVAILABLE
						ADDRESSBOOK_FLAG_ERROR */
	void *pvalue;
} ADDRESSBOOK_TFPROPVAL;

#define PROPROW_FLAG_NORMAL				0
#define PROPROW_FLAG_FLAGGED			1

typedef struct _ADDRESSBOOK_PROPROW {
	uint8_t flag;
	void **ppvalue;
} ADDRESSBOOK_PROPROW;

typedef struct _LPROPTAG_ARRAY {
	uint32_t count;
	uint32_t *pproptag;
} LPROPTAG_ARRAY;

typedef struct _MID_ARRAY {
	uint32_t count;
	uint32_t *pmid;
} MID_ARRAY;

typedef struct _STAT {
	uint32_t sort_type;
	uint32_t container_id;
	uint32_t cur_rec;
	int32_t delta;
	uint32_t num_pos;
	uint32_t total_rec;
	uint32_t codepage;
	uint32_t template_locale;
	uint32_t sort_locale;
} STAT;

typedef struct _ADDRESSBOOK_PROPNAME {
	GUID guid;
	uint32_t id;
} ADDRESSBOOK_PROPNAME;

typedef struct _ADDRESSBOOK_COLROW {
	LPROPTAG_ARRAY columns;
	uint32_t row_count;
	ADDRESSBOOK_PROPROW *prows;
} ADDRESSBOOK_COLROW;

typedef struct _NSP_ENTRYID {
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
} NSP_ENTRYID;

typedef struct _NSP_ENTRYIDS {
	uint32_t count;
	NSP_ENTRYID *pentryid;
} NSP_ENTRYIDS;

typedef struct _BIND_REQUEST {
	uint32_t flags;
	STAT *pstat;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} BIND_REQUEST;

typedef struct _BIND_RESPONSE {
	uint32_t status;
	uint32_t result;
	GUID server_guid;
} BIND_RESPONSE;

typedef struct _UNBIND_REQUEST {
	uint32_t reserved;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} UNBIND_REQUEST;

typedef struct _UNBIND_RESPONSE {
	uint32_t status;
	uint32_t result;
} UNBIND_RESPONSE;

typedef struct _COMPAREMIDS_REQUEST {
	uint32_t reserved;
	STAT *pstat;
	uint32_t mid1;
	uint32_t mid2;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} COMPAREMIDS_REQUEST;

typedef struct _COMPAREMIDS_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t result1;
} COMPAREMIDS_RESPONSE;

typedef struct _DNTOMID_REQUEST {
	uint32_t reserved;
	STRING_ARRAY *pnames;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} DNTOMID_REQUEST;

typedef struct _DNTOMID_RESPONSE {
	uint32_t status;
	uint32_t result;
	MID_ARRAY *poutmids;
} DNTOMID_RESPONSE;

typedef struct _GETMATCHES_REQUEST {
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
} GETMATCHES_REQUEST;

typedef struct _GETMATCHES_RESPONSE {
	uint32_t status;
	uint32_t result;
	STAT *pstat;
	MID_ARRAY *pmids;
	ADDRESSBOOK_COLROW column_rows;
} GETMATCHES_RESPONSE;

typedef struct _GETPROPLIST_REQUEST {
	uint32_t flags;
	uint32_t mid;
	uint32_t codepage;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} GETPROPLIST_REQUEST;

typedef struct _GETPROPLIST_RESPONSE {
	uint32_t status;
	uint32_t result;
	LPROPTAG_ARRAY *pproptags;
} GETPROPLIST_RESPONSE;

typedef struct _GETPROPS_REQUEST {
	uint32_t flags;
	STAT *pstat;
	LPROPTAG_ARRAY *pproptags;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} GETPROPS_REQUEST;

typedef struct _GETPROPS_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t codepage;
	ADDRESSBOOK_PROPLIST *prow;
} GETPROPS_RESPONSE;

typedef struct _GETSPECIALTABLE_REQUEST {
	uint32_t flags;
	STAT *pstat;
	uint32_t *pversion;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} GETSPECIALTABLE_REQUEST;

typedef struct _GETSPECIALTABLE_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t codepage;
	uint32_t *pversion;
	uint32_t count;
	ADDRESSBOOK_PROPLIST *prow;
} GETSPECIALTABLE_RESPONSE;

typedef struct _GETTEMPLATEINFO_REQUEST {
	uint32_t flags;
	uint32_t type;
	char *pdn;
	uint32_t codepage;
	uint32_t locale_id;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} GETTEMPLATEINFO_REQUEST;

typedef struct _GETTEMPLATEINFO_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t codepage;
	ADDRESSBOOK_PROPLIST *prow;
} GETTEMPLATEINFO_RESPONSE;

typedef struct _MODLINKATT_REQUEST {
	uint32_t flags;
	uint32_t proptag;
	uint32_t mid;
	NSP_ENTRYIDS entryids;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} MODLINKATT_REQUEST;

typedef struct _MODLINKATT_RESPONSE {
	uint32_t status;
	uint32_t result;
} MODLINKATT_RESPONSE;

typedef struct _MODPROPS_REQUEST {
	uint32_t reserved;
	STAT *pstat;
	LPROPTAG_ARRAY *pproptags;
	ADDRESSBOOK_PROPLIST *pvalues;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} MODPROPS_REQUEST;

typedef struct _MODPROPS_RESPONSE {
	uint32_t status;
	uint32_t result;
} MODPROPS_RESPONSE;

typedef struct _QUERYROWS_REQUEST {
	uint32_t flags;
	STAT *pstat;
	MID_ARRAY explicit_table;
	uint32_t count;
	LPROPTAG_ARRAY *pcolumns;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} QUERYROWS_REQUEST;

typedef struct _QUERYROWS_RESPONSE {
	uint32_t status;
	uint32_t result;
	STAT *pstat;
	ADDRESSBOOK_COLROW column_rows;
} QUERYROWS_RESPONSE;

typedef struct _QUERYCOLUMNS_REQUEST {
	uint32_t reserved;
	uint32_t flags;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} QUERYCOLUMNS_REQUEST;

typedef struct _QUERYCOLUMNS_RESPONSE {
	uint32_t status;
	uint32_t result;
	LPROPTAG_ARRAY *pcolumns;
} QUERYCOLUMNS_RESPONSE;

typedef struct _RESOLVENAMES_REQUEST {
	uint32_t reserved;
	STAT *pstat;
	LPROPTAG_ARRAY *pproptags;
	STRING_ARRAY *pnames;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} RESOLVENAMES_REQUEST;

typedef struct _RESOLVENAMES_RESPONSE {
	uint32_t status;
	uint32_t result;
	uint32_t codepage;
	MID_ARRAY *pmids;
	ADDRESSBOOK_COLROW column_rows;
} RESOLVENAMES_RESPONSE;

typedef struct _RESORTRESTRICTION_REQUEST {
	uint32_t reserved;
	STAT *pstat;
	MID_ARRAY *pinmids;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} RESORTRESTRICTION_REQUEST;

typedef struct _RESORTRESTRICTION_RESPONSE {
	uint32_t status;
	uint32_t result;
	STAT *pstat;
	MID_ARRAY *poutmids;
} RESORTRESTRICTION_RESPONSE;

typedef struct _SEEKENTRIES_REQUEST {
	uint32_t reserved;
	STAT *pstat;
	ADDRESSBOOK_TAPROPVAL *ptarget;
	MID_ARRAY *pexplicit_table;
	LPROPTAG_ARRAY *pcolumns;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} SEEKENTRIES_REQUEST;

typedef struct _SEEKENTRIES_RESPONSE {
	uint32_t status;
	uint32_t result;
	STAT *pstat;
	ADDRESSBOOK_COLROW column_rows;
} SEEKENTRIES_RESPONSE;

typedef struct _UPDATESTAT_REQUEST {
	uint32_t reserved;
	STAT *pstat;
	uint8_t delta_requested;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} UPDATESTAT_REQUEST;

typedef struct _UPDATESTAT_RESPONSE {
	uint32_t status;
	uint32_t result;
	STAT *pstat;
	int32_t *pdelta;
} UPDATESTAT_RESPONSE;

typedef struct _GETMAILBOXURL_REQUEST {
	uint32_t flags;
	char *puser_dn;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} GETMAILBOXURL_REQUEST;

typedef struct _GETMAILBOXURL_RESPONSE {
	uint32_t status;
	uint32_t result;
	char server_url[1024];
} GETMAILBOXURL_RESPONSE;

typedef struct _GETADDRESSBOOKURL_REQUEST {
	uint32_t flags;
	char *puser_dn;
	uint32_t cb_auxin;
	uint8_t *pauxin;
} GETADDRESSBOOKURL_REQUEST;

typedef struct _GETADDRESSBOOKURL_RESPONSE {
	uint32_t status;
	uint32_t result;
	char server_url[1024];
} GETADDRESSBOOKURL_RESPONSE;

typedef union _NSP_REQUEST {
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
} NSP_REQUEST;

typedef union _NSP_RESPONSE {
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
} NSP_RESPONSE;

/****************** definition from exchange_nsp ******************/

typedef struct _FLATUID {
	uint8_t ab[16];
} FLATUID;

typedef struct _NSP_PROPNAME {
	FLATUID *pguid;
	uint32_t reserved;
	uint32_t id;
} NSP_PROPNAME;

typedef struct _FILETIME {
	uint32_t low_datetime;
	uint32_t high_datetime;
} FILETIME;

typedef struct _FLATUID_ARRAY {
	uint32_t cvalues;
	FLATUID **ppguid;
} FLATUID_ARRAY;

typedef struct _FILETIME_ARRAY {
	uint32_t cvalues;
	FILETIME *pftime;
} FILETIME_ARRAY;

typedef union _PROP_VAL_UNION {
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
} PROP_VAL_UNION;

typedef struct _PROPERTY_VALUE {
	uint32_t proptag;
	uint32_t reserved;
	PROP_VAL_UNION value; /* type is proptag&0xFFFF */
} PROPERTY_VALUE;

typedef struct _NSP_PROPROW {
	uint32_t reserved;
	uint32_t cvalues;
	PROPERTY_VALUE *pprops;
} NSP_PROPROW;

typedef struct _NSP_ROWSET {
	uint32_t crows;
	NSP_PROPROW *prows;
} NSP_ROWSET;

typedef struct _NSPRES_AND_OR {
	uint32_t cres;
	struct _NSPRES *pres;
} NSPRES_AND_OR;

typedef struct _NSPRES_NOT {
	struct _NSPRES *pres;
} NSPRES_NOT;

typedef struct _NSPRES_CONTENT {
	uint32_t fuzzy_level;
	uint32_t proptag;
	PROPERTY_VALUE *pprop;
} NSPRES_CONTENT;

typedef struct _NSPRES_PROPERTY {
	uint32_t relop;
	uint32_t proptag;
	PROPERTY_VALUE *pprop;
} NSPRES_PROPERTY;

typedef struct _NSPRES_PROPCOMPARE {
	uint32_t relop;
	uint32_t proptag1;
	uint32_t proptag2;
} NSPRES_PROPCOMPARE;

typedef struct _NSPRES_BITMASK {
	uint32_t rel_mbr;
	uint32_t proptag;
	uint32_t mask;
} NSPRES_BITMASK;

typedef struct _NSPRES_SIZE {
	uint32_t relop;
	uint32_t proptag;
	uint32_t cb;
} NSPRES_SIZE;

typedef struct _NSPRES_EXIST {
	uint32_t reserved1;
	uint32_t proptag;
	uint32_t reserved2;
} NSPRES_EXIST;

typedef struct _NSPRES_SUB {
	uint32_t subobject;
	struct _NSPRES *pres;
} NSPRES_SUB;

typedef union _NSPRES_UNION {
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
} NSPRES_UNION;

typedef struct _NSPRES {
	uint32_t res_type;
	NSPRES_UNION res;
} NSPRES;

#endif /* _H_AB_TYPES_ */
