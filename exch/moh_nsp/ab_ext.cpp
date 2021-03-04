#include <gromox/mapidefs.h>
#include "ab_ext.h"
#define TRY(expr) do { int v = (expr); if (v != EXT_ERR_SUCCESS) return v; } while (false)

static int ab_ext_pull_string_array(EXT_PULL *pext, STRING_ARRAY *r)
{
	int i;
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &r->count));
	if (0 == r->count) {
		r->ppstr = NULL;
		return EXT_ERR_SUCCESS;
	}
	r->ppstr = pext->anew<char *>(r->count);
	if (NULL == r->ppstr) {
		return EXT_ERR_ALLOC;
	}
	for (i=0; i<r->count; i++) {
		TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
		if (0 == tmp_byte) {
			r->ppstr[i] = NULL;
		} else {
			TRY(ext_buffer_pull_string(pext, &r->ppstr[i]));
		}
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_pull_wstring_array(EXT_PULL *pext, STRING_ARRAY *r)
{
	int i;
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &r->count));
	if (0 == r->count) {
		r->ppstr = NULL;
		return EXT_ERR_SUCCESS;
	}
	r->ppstr = pext->anew<char *>(r->count);
	if (NULL == r->ppstr) {
		return EXT_ERR_ALLOC;
	}
	for (i=0; i<r->count; i++) {
		TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
		if (0 == tmp_byte) {
			r->ppstr[i] = NULL;
		} else {
			TRY(ext_buffer_pull_wstring(pext, &r->ppstr[i]));
		}
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_pull_binary_array(EXT_PULL *pext, BINARY_ARRAY *r)
{
	int i;
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &r->count));
	if (0 == r->count) {
		r->pbin = NULL;
		return EXT_ERR_SUCCESS;
	}
	r->pbin = pext->anew<BINARY>(r->count);
	if (NULL == r->pbin) {
		return EXT_ERR_ALLOC;
	}
	for (i=0; i<r->count; i++) {
		TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
		if (0 == tmp_byte) {
			r->pbin[i].cb = 0;
			r->pbin[i].pb = NULL;
		} else {
			TRY(ext_buffer_pull_binary(pext, &r->pbin[i]));
		}
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_pull_multiple_val(
	EXT_PULL *pext, uint16_t type, void **ppval)
{
	switch (type) {
	case PT_MV_SHORT:
		*ppval = pext->anew<SHORT_ARRAY>();
		if (NULL == (*ppval)) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_short_array(pext, static_cast<SHORT_ARRAY *>(*ppval));
	case PT_MV_LONG:
		*ppval = pext->anew<LONG_ARRAY>();
		if (NULL == (*ppval)) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_long_array(pext, static_cast<LONG_ARRAY *>(*ppval));
	case PT_MV_I8:
		*ppval = pext->anew<LONGLONG_ARRAY>();
		if (NULL == (*ppval)) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_longlong_array(pext, static_cast<LONGLONG_ARRAY *>(*ppval));
	case PT_MV_STRING8:
		*ppval = pext->anew<STRING_ARRAY>();
		if (NULL == (*ppval)) {
			return EXT_ERR_ALLOC;
		}
		return ab_ext_pull_string_array(pext, static_cast<STRING_ARRAY *>(*ppval));
	case PT_MV_UNICODE:
		*ppval = pext->anew<STRING_ARRAY>();
		if (NULL == (*ppval)) {
			return EXT_ERR_ALLOC;
		}
		return ab_ext_pull_wstring_array(pext, static_cast<STRING_ARRAY *>(*ppval));
	case PT_MV_CLSID:
		*ppval = pext->anew<GUID_ARRAY>();
		if (NULL == (*ppval)) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_guid_array(pext, static_cast<GUID_ARRAY *>(*ppval));
	case PT_MV_BINARY:
		*ppval = pext->anew<BINARY_ARRAY>();
		if (NULL == (*ppval)) {
			return EXT_ERR_ALLOC;
		}
		return ab_ext_pull_binary_array(pext, static_cast<BINARY_ARRAY *>(*ppval));
	}
	return EXT_ERR_FORMAT;
}

static int ab_ext_pull_addressbook_propval(
	EXT_PULL *pext, uint16_t type, void **ppval)
{
	uint8_t tmp_byte;
	
	if (type == PT_STRING8 || type == PT_UNICODE ||
	    type == PT_BINARY || (type & MV_FLAG)) {
		TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
		if (0xFF == tmp_byte) {
			if (type & MV_FLAG)
				return ab_ext_pull_multiple_val(pext, type, ppval);
			else
				return ext_buffer_pull_propval(pext, type, ppval);
		} else if (0 == tmp_byte) {
			*ppval = NULL;
			return EXT_ERR_SUCCESS;
		} else {
			return EXT_ERR_FORMAT;
		}
	}
	return ext_buffer_pull_propval(pext, type, ppval);
}

static int ab_ext_pull_addressbook_tapropval(
	EXT_PULL *pext, ADDRESSBOOK_TAPROPVAL *ppropval)
{
	TRY(ext_buffer_pull_uint16(pext, &ppropval->type));
	TRY(ext_buffer_pull_uint16(pext, &ppropval->propid));
	return ab_ext_pull_addressbook_propval(pext,
			ppropval->type, &ppropval->pvalue);	
}

static int ab_ext_pull_addressbook_proplist(
	EXT_PULL *pext, ADDRESSBOOK_PROPLIST *pproplist)
{
	int i;
	
	TRY(ext_buffer_pull_uint32(pext, &pproplist->count));
	if (0 == pproplist->count) {
		pproplist->ppropval = NULL;
		return EXT_ERR_SUCCESS;
	}
	pproplist->ppropval = pext->anew<ADDRESSBOOK_TAPROPVAL>(pproplist->count);
	if (NULL == pproplist->ppropval) {
		return EXT_ERR_ALLOC;
	}
	for (i=0; i<pproplist->count; i++) {
		TRY(ab_ext_pull_addressbook_tapropval(
					pext, pproplist->ppropval + i));
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_pull_addressbook_typropval(
	EXT_PULL *pext, ADDRESSBOOK_TYPROPVAL *ppropval)
{
	TRY(ext_buffer_pull_uint16(pext, &ppropval->type));
	return ab_ext_pull_addressbook_propval(pext,
			ppropval->type, &ppropval->pvalue);	
}

static int ab_ext_pull_addressbook_fpropval(EXT_PULL *pext,
	uint16_t type, ADDRESSBOOK_FPROPVAL *ppropval)
{
	TRY(ext_buffer_pull_uint8(pext, &ppropval->flag));
	switch (ppropval->flag) {
	case ADDRESSBOOK_FLAG_AVAILABLE:
		return ab_ext_pull_addressbook_propval(
				pext, type, &ppropval->pvalue);
	case ADDRESSBOOK_FLAG_UNAVAILABLE:
		ppropval->pvalue = NULL;
		return EXT_ERR_SUCCESS;
	case ADDRESSBOOK_FLAG_ERROR:
		ppropval->pvalue = pext->anew<uint32_t>();
		if (NULL == ppropval->pvalue) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_uint32(pext, static_cast<uint32_t *>(ppropval->pvalue));
	}
	return EXT_ERR_FORMAT;
}

static int ab_ext_pull_addressbook_tfpropval(EXT_PULL *pext,
	ADDRESSBOOK_TFPROPVAL *ppropval)
{
	TRY(ext_buffer_pull_uint16(pext, &ppropval->type));
	TRY(ext_buffer_pull_uint8(pext, &ppropval->flag));
	switch (ppropval->flag) {
	case ADDRESSBOOK_FLAG_AVAILABLE:
		return ab_ext_pull_addressbook_propval(pext,
				ppropval->type, &ppropval->pvalue);
	case ADDRESSBOOK_FLAG_UNAVAILABLE:
		ppropval->pvalue = NULL;
		return EXT_ERR_SUCCESS;
	case ADDRESSBOOK_FLAG_ERROR:
		ppropval->pvalue = pext->anew<uint32_t>();
		if (NULL == ppropval->pvalue) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_uint32(pext, static_cast<uint32_t *>(ppropval->pvalue));
	}
	return EXT_ERR_FORMAT;
}

static int ab_ext_pull_addressbook_proprow(EXT_PULL *pext,
	const LPROPTAG_ARRAY *pcolumns, ADDRESSBOOK_PROPROW *prow)
{
	TRY(ext_buffer_pull_uint8(pext, &prow->flag));
	prow->ppvalue = pext->anew<void *>(pcolumns->cvalues);
	if (NULL == prow->ppvalue) {
		return EXT_ERR_ALLOC;
	}
	if (PROPROW_FLAG_NORMAL == prow->flag) {
		for (unsigned int i = 0; i < pcolumns->cvalues; ++i) {
			if (PROP_TYPE(pcolumns->pproptag[i]) == PT_UNSPECIFIED) {
				prow->ppvalue[i] = pext->anew<ADDRESSBOOK_TYPROPVAL>();
				if (NULL == prow->ppvalue[i]) {
					return EXT_ERR_ALLOC;
				}
				TRY(ab_ext_pull_addressbook_typropval(pext,
				    static_cast<ADDRESSBOOK_TYPROPVAL *>(prow->ppvalue[i])));
			} else {
				TRY(ab_ext_pull_addressbook_propval(pext,
				    PROP_TYPE(pcolumns->pproptag[i]), prow->ppvalue + i));
			}
		}
	} else if (PROPROW_FLAG_FLAGGED == prow->flag) {
		for (unsigned int i = 0; i < pcolumns->cvalues; ++i) {
			if (PROP_TYPE(pcolumns->pproptag[i]) == PT_UNSPECIFIED) {
				prow->ppvalue[i] = pext->anew<ADDRESSBOOK_TFPROPVAL>();
				if (NULL == prow->ppvalue[i]) {
					return EXT_ERR_ALLOC;
				}
				TRY(ab_ext_pull_addressbook_tfpropval(pext,
				    static_cast<ADDRESSBOOK_TFPROPVAL *>(prow->ppvalue[i])));
			} else {
				prow->ppvalue[i] = pext->alloc(
					sizeof(ADDRESSBOOK_FPROPVAL));
				if (NULL == prow->ppvalue[i]) {
					return EXT_ERR_ALLOC;
				}
				TRY(ab_ext_pull_addressbook_fpropval(pext,
				    PROP_TYPE(pcolumns->pproptag[i]),
				    static_cast<ADDRESSBOOK_FPROPVAL *>(prow->ppvalue[i])));
			}
		}
	} else {
		return EXT_ERR_FORMAT;
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_pull_lproptag_array(EXT_PULL *pext,
	LPROPTAG_ARRAY *pproptags)
{
	TRY(ext_buffer_pull_uint32(pext, &pproptags->cvalues));
	pproptags->pproptag = pext->anew<uint32_t>(pproptags->cvalues);
	if (NULL == pproptags->pproptag) {
		return EXT_ERR_ALLOC;
	}
	for (unsigned int i = 0; i < pproptags->cvalues; ++i) {
		TRY(ext_buffer_pull_uint32(pext, pproptags->pproptag + i));
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_pull_mid_array(EXT_PULL *pext,
	MID_ARRAY *pmids)
{
	TRY(ext_buffer_pull_uint32(pext, &pmids->cvalues));
	if (pmids->cvalues == 0) {
		pmids->pproptag = nullptr;
	} else {
		pmids->pproptag = pext->anew<uint32_t>(pmids->cvalues);
		if (pmids->pproptag == nullptr)
			return EXT_ERR_ALLOC;
	}
	for (unsigned int i = 0; i < pmids->cvalues; ++i) {
		TRY(ext_buffer_pull_uint32(pext, &pmids->pproptag[i]));
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_pull_stat(EXT_PULL *pext, STAT *pstat)
{
	TRY(ext_buffer_pull_uint32(pext, &pstat->sort_type));
	TRY(ext_buffer_pull_uint32(pext, &pstat->container_id));
	TRY(ext_buffer_pull_uint32(pext, &pstat->cur_rec));
	TRY(ext_buffer_pull_int32(pext, &pstat->delta));
	TRY(ext_buffer_pull_uint32(pext, &pstat->num_pos));
	TRY(ext_buffer_pull_uint32(pext, &pstat->total_rec));
	TRY(ext_buffer_pull_uint32(pext, &pstat->codepage));
	TRY(ext_buffer_pull_uint32(pext, &pstat->template_locale));
	return ext_buffer_pull_uint32(pext, &pstat->sort_locale);
}

static int ab_ext_pull_addressbook_propname(
	EXT_PULL *pext, ADDRESSBOOK_PROPNAME *ppropname)
{
	TRY(ext_buffer_pull_guid(pext, &ppropname->guid));
	return ext_buffer_pull_uint32(pext, &ppropname->id);
}

static int ab_ext_pull_addressbook_colrow(
	EXT_PULL *pext, ADDRESSBOOK_COLROW *pcolrow)
{
	int i;
	
	TRY(ab_ext_pull_lproptag_array(pext, &pcolrow->columns));
	TRY(ext_buffer_pull_uint32(pext, &pcolrow->row_count));
	pcolrow->prows = static_cast<ADDRESSBOOK_PROPROW *>(pext->alloc(pcolrow->row_count * sizeof(ADDRESSBOOK_PROPROW)));
	if (NULL == pcolrow->prows) {
		return EXT_ERR_ALLOC;
	}
	for (i=0; i<pcolrow->row_count; i++) {
		TRY(ab_ext_pull_addressbook_proprow(pext,
				&pcolrow->columns, pcolrow->prows + i));
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_pull_nsp_entryid(EXT_PULL *pext, NSP_ENTRYID *pentryid)
{
	TRY(ext_buffer_pull_uint8(pext, &pentryid->id_type));
	if (0x87 != pentryid->id_type && 0x0 != pentryid->id_type) {
		return EXT_ERR_FORMAT;
	}
	TRY(ext_buffer_pull_uint8(pext, &pentryid->r1));
	TRY(ext_buffer_pull_uint8(pext, &pentryid->r2));
	TRY(ext_buffer_pull_uint8(pext, &pentryid->r3));
	TRY(ext_buffer_pull_guid(pext, &pentryid->provider_uid));
	TRY(ext_buffer_pull_uint32(pext, &pentryid->display_type));
	if (0 == pentryid->id_type) {
		return ext_buffer_pull_string(pext, &pentryid->payload.pdn);
	} else {
		return ext_buffer_pull_uint32(pext, &pentryid->payload.mid);
	}
}

static int ab_ext_pull_nsp_entryids(EXT_PULL *pext, NSP_ENTRYIDS *pentryids)
{
	int i;
	
	TRY(ext_buffer_pull_uint32(pext, &pentryids->count));
	pentryids->pentryid = pext->anew<NSP_ENTRYID>(pentryids->count);
	if (NULL == pentryids->pentryid) {
		return EXT_ERR_ALLOC;
	}
	for (i=0; i<pentryids->count; i++) {
		TRY(ab_ext_pull_nsp_entryid(pext, pentryids->pentryid + i));
	}
	return EXT_ERR_SUCCESS;
}

int ab_ext_pull_bind_request(EXT_PULL *pext, BIND_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->flags));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pstat = NULL;
	} else {
		prequest->pstat = pext->anew<STAT>();
		if (NULL == prequest->pstat) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_stat(pext, prequest->pstat));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_unbind_request(EXT_PULL *pext, UNBIND_REQUEST *prequest)
{
	TRY(ext_buffer_pull_uint32(pext, &prequest->reserved));
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_comparemids_request(EXT_PULL *pext,
	COMPAREMIDS_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->reserved));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pstat = NULL;
	} else {
		prequest->pstat = pext->anew<STAT>();
		if (NULL == prequest->pstat) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_stat(pext, prequest->pstat));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->mid1));
	TRY(ext_buffer_pull_uint32(pext, &prequest->mid2));
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_dntomid_request(EXT_PULL *pext, DNTOMID_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->reserved));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pnames = NULL;
	} else {
		prequest->pnames = pext->anew<STRING_ARRAY>();
		if (NULL == prequest->pnames) {
			return EXT_ERR_ALLOC;
		}
		TRY(ext_buffer_pull_string_array(pext, prequest->pnames));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_getmatches_request(
	EXT_PULL *pext, GETMATCHES_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->reserved1));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pstat = NULL;
	} else {
		prequest->pstat = pext->anew<STAT>();
		if (NULL == prequest->pstat) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_stat(pext, prequest->pstat));
	}
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pinmids = NULL;
	} else {
		prequest->pinmids = pext->anew<MID_ARRAY>();
		if (NULL == prequest->pinmids) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_mid_array(pext, prequest->pinmids));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->reserved2));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pfilter = NULL;
	} else {
		prequest->pfilter = pext->anew<RESTRICTION>();
		if (NULL == prequest->pfilter) {
			return EXT_ERR_ALLOC;
		}
		TRY(ext_buffer_pull_restriction(pext, prequest->pfilter));
	}
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->ppropname = NULL;
	} else {
		prequest->ppropname = pext->anew<ADDRESSBOOK_PROPNAME>();
		if (NULL == prequest->ppropname) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_addressbook_propname(
						pext, prequest->ppropname));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->row_count));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pcolumns = NULL;
	} else {
		prequest->pcolumns = pext->anew<LPROPTAG_ARRAY>();
		if (NULL == prequest->pcolumns) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_lproptag_array(pext, prequest->pcolumns));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_getproplist_request(EXT_PULL *pext,
	GETPROPLIST_REQUEST *prequest)
{
	TRY(ext_buffer_pull_uint32(pext, &prequest->flags));
	TRY(ext_buffer_pull_uint32(pext, &prequest->mid));
	TRY(ext_buffer_pull_uint32(pext, &prequest->codepage));
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_getprops_request(EXT_PULL *pext, GETPROPS_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->flags));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pstat = NULL;
	} else {
		prequest->pstat = pext->anew<STAT>();
		if (NULL == prequest->pstat) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_stat(pext, prequest->pstat));
	}
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pproptags = NULL;
	} else {
		prequest->pproptags = pext->anew<LPROPTAG_ARRAY>();
		if (NULL == prequest->pproptags) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_lproptag_array(pext, prequest->pproptags));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_getspecialtable_request(EXT_PULL *pext,
	GETSPECIALTABLE_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->flags));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pstat = NULL;
	} else {
		prequest->pstat = pext->anew<STAT>();
		if (NULL == prequest->pstat) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_stat(pext, prequest->pstat));
	}
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pversion = NULL;
	} else {
		prequest->pversion = pext->anew<uint32_t>();
		if (NULL == prequest->pversion) {
			return EXT_ERR_ALLOC;
		}
		TRY(ext_buffer_pull_uint32(pext, prequest->pversion));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_gettemplateinfo_request(EXT_PULL *pext,
	GETTEMPLATEINFO_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->flags));
	TRY(ext_buffer_pull_uint32(pext, &prequest->type));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pdn = NULL;
	} else {
		TRY(ext_buffer_pull_string(pext, &prequest->pdn));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->codepage));
	TRY(ext_buffer_pull_uint32(pext, &prequest->locale_id));
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_modlinkatt_request(EXT_PULL *pext,
	MODLINKATT_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->flags));
	TRY(ext_buffer_pull_uint32(pext, &prequest->proptag));
	TRY(ext_buffer_pull_uint32(pext, &prequest->mid));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->entryids.count = 0;
		prequest->entryids.pentryid = NULL;
	} else {
		TRY(ab_ext_pull_nsp_entryids(pext, &prequest->entryids));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_modprops_request(EXT_PULL *pext, MODPROPS_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->reserved));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pstat = NULL;
	} else {
		prequest->pstat = pext->anew<STAT>();
		if (NULL == prequest->pstat) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_stat(pext, prequest->pstat));
	}
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pproptags = NULL;
	} else {
		prequest->pproptags = pext->anew<LPROPTAG_ARRAY>();
		if (NULL == prequest->pproptags) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_lproptag_array(pext, prequest->pproptags));
	}
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pvalues = NULL;
	} else {
		prequest->pvalues = pext->anew<ADDRESSBOOK_PROPLIST>();
		if (NULL == prequest->pvalues) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_addressbook_proplist(pext, prequest->pvalues));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_queryrows_request(EXT_PULL *pext,
	QUERYROWS_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->flags));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pstat = NULL;
	} else {
		prequest->pstat = pext->anew<STAT>();
		if (NULL == prequest->pstat) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_stat(pext, prequest->pstat));
	}
	TRY(ab_ext_pull_mid_array(pext, &prequest->explicit_table));
	TRY(ext_buffer_pull_uint32(pext, &prequest->count));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pcolumns = NULL;
	} else {
		prequest->pcolumns = pext->anew<LPROPTAG_ARRAY>();
		if (NULL == prequest->pcolumns) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_lproptag_array(pext, prequest->pcolumns));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_querycolumns_request(EXT_PULL *pext,
	QUERYCOLUMNS_REQUEST *prequest)
{
	TRY(ext_buffer_pull_uint32(pext, &prequest->reserved));
	TRY(ext_buffer_pull_uint32(pext, &prequest->flags));
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_resolvenames_request(
	EXT_PULL *pext, RESOLVENAMES_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->reserved));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pstat = NULL;
	} else {
		prequest->pstat = pext->anew<STAT>();
		if (NULL == prequest->pstat) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_stat(pext, prequest->pstat));
	}
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pproptags = NULL;
	} else {
		prequest->pproptags = pext->anew<LPROPTAG_ARRAY>();
		if (NULL == prequest->pproptags) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_lproptag_array(pext, prequest->pproptags));
	}
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pnames = NULL;
	} else {
		prequest->pnames = pext->anew<STRING_ARRAY>();
		if (NULL == prequest->pnames) {
			return EXT_ERR_ALLOC;
		}
		TRY(ext_buffer_pull_wstring_array(pext, prequest->pnames));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_resortrestriction_request(EXT_PULL *pext,
	RESORTRESTRICTION_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->reserved));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pstat = NULL;
	} else {
		prequest->pstat = pext->anew<STAT>();
		if (NULL == prequest->pstat) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_stat(pext, prequest->pstat));
	}
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pinmids = NULL;
	} else {
		prequest->pinmids = pext->anew<MID_ARRAY>();
		if (NULL == prequest->pinmids) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_mid_array(pext, prequest->pinmids));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_seekentries_request(EXT_PULL *pext,
	SEEKENTRIES_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->reserved));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pstat = NULL;
	} else {
		prequest->pstat = pext->anew<STAT>();
		if (NULL == prequest->pstat) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_stat(pext, prequest->pstat));
	}
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->ptarget = NULL;
	} else {
		prequest->ptarget = pext->anew<ADDRESSBOOK_TAPROPVAL>();
		if (NULL == prequest->ptarget) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_addressbook_tapropval(pext, prequest->ptarget));
	}
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pexplicit_table = NULL;
	} else {
		prequest->pexplicit_table = pext->anew<MID_ARRAY>();
		if (NULL == prequest->pexplicit_table) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_mid_array(pext, prequest->pexplicit_table));
	}
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pcolumns = NULL;
	} else {
		prequest->pcolumns = pext->anew<LPROPTAG_ARRAY>();
		if (NULL == prequest->pcolumns) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_lproptag_array(pext, prequest->pcolumns));
	}
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_updatestat_request(EXT_PULL *pext,
	UPDATESTAT_REQUEST *prequest)
{
	uint8_t tmp_byte;
	
	TRY(ext_buffer_pull_uint32(pext, &prequest->reserved));
	TRY(ext_buffer_pull_uint8(pext, &tmp_byte));
	if (0 == tmp_byte) {
		prequest->pstat = NULL;
	} else {
		prequest->pstat = pext->anew<STAT>();
		if (NULL == prequest->pstat) {
			return EXT_ERR_ALLOC;
		}
		TRY(ab_ext_pull_stat(pext, prequest->pstat));
	}
	TRY(ext_buffer_pull_uint8(pext, &prequest->delta_requested));
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_getmailboxurl_request(EXT_PULL *pext,
	GETMAILBOXURL_REQUEST *prequest)
{
	TRY(ext_buffer_pull_uint32(pext, &prequest->flags));
	TRY(ext_buffer_pull_wstring(pext, &prequest->puser_dn));
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int ab_ext_pull_getaddressbookurl_request(EXT_PULL *pext,
	GETADDRESSBOOKURL_REQUEST *prequest)
{
	TRY(ext_buffer_pull_uint32(pext, &prequest->flags));
	TRY(ext_buffer_pull_wstring(pext, &prequest->puser_dn));
	TRY(ext_buffer_pull_uint32(pext, &prequest->cb_auxin));
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = static_cast<uint8_t *>(pext->alloc(prequest->cb_auxin));
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

/*---------------------------------------------------------------------------*/

static int ab_ext_push_string_array(EXT_PUSH *pext, const STRING_ARRAY *r)
{
	int i;
	
	TRY(ext_buffer_push_uint32(pext, r->count));
	for (i=0; i<r->count; i++) {
		if (NULL == r->ppstr[i]) {
			TRY(ext_buffer_push_uint8(pext, 0));
		} else {
			TRY(ext_buffer_push_uint8(pext, 0xFF));
			TRY(ext_buffer_push_string(pext, r->ppstr[i]));
		}
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_push_wstring_array(EXT_PUSH *pext, const STRING_ARRAY *r)
{
	int i;
	
	TRY(ext_buffer_push_uint32(pext, r->count));
	for (i=0; i<r->count; i++) {
		if (NULL == r->ppstr[i]) {
			TRY(ext_buffer_push_uint8(pext, 0));
		} else {
			TRY(ext_buffer_push_uint8(pext, 0xFF));
			TRY(ext_buffer_push_wstring(pext, r->ppstr[i]));
		}
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_push_binary_array(EXT_PUSH *pext, const BINARY_ARRAY *r)
{
	int i;
	
	TRY(ext_buffer_push_uint32(pext, r->count));
	for (i=0; i<r->count; i++) {
		if (0 == r->pbin[i].cb) {
			TRY(ext_buffer_push_uint8(pext, 0));
		} else {
			TRY(ext_buffer_push_uint8(pext, 0xFF));
			TRY(ext_buffer_push_binary(pext, &r->pbin[i]));
		}
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_push_multiple_val(EXT_PUSH *pext,
	uint16_t type, const void *pval)
{
	switch(type) {
	case PT_MV_SHORT:
		return ext_buffer_push_short_array(pext, static_cast<const SHORT_ARRAY *>(pval));
	case PT_MV_LONG:
		return ext_buffer_push_long_array(pext, static_cast<const LONG_ARRAY *>(pval));
	case PT_MV_I8:
		return ext_buffer_push_longlong_array(pext, static_cast<const LONGLONG_ARRAY *>(pval));
	case PT_MV_STRING8:
		return ab_ext_push_string_array(pext, static_cast<const STRING_ARRAY *>(pval));
	case PT_MV_UNICODE:
		return ab_ext_push_wstring_array(pext, static_cast<const STRING_ARRAY *>(pval));
	case PT_MV_CLSID:
		return ext_buffer_push_guid_array(pext, static_cast<const GUID_ARRAY *>(pval));
	case PT_MV_BINARY:
		return ab_ext_push_binary_array(pext, static_cast<const BINARY_ARRAY *>(pval));
	}
	return EXT_ERR_FORMAT;
}

static int ab_ext_push_addressbook_propval(
	EXT_PUSH *pext, uint16_t type, const void *pval)
{
	if (type == PT_STRING8 || type == PT_UNICODE || type == PT_BINARY ||
	    (type & MV_FLAG)) {
		if (NULL == pval) {
			return ext_buffer_push_uint8(pext, 0);
		}
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		if (type & MV_FLAG)
			return ab_ext_push_multiple_val(pext, type, pval);
	}
	return ext_buffer_push_propval(pext, type, pval);
}

static int ab_ext_push_addressbook_tapropval(
	EXT_PUSH *pext, const ADDRESSBOOK_TAPROPVAL *ppropval)
{
	TRY(ext_buffer_push_uint16(pext, ppropval->type));
	TRY(ext_buffer_push_uint16(pext, ppropval->propid));
	return ab_ext_push_addressbook_propval(pext,
			ppropval->type, ppropval->pvalue);	
}

static int ab_ext_push_addressbook_proplist(
	EXT_PUSH *pext, const ADDRESSBOOK_PROPLIST *pproplist)
{
	int i;
	
	TRY(ext_buffer_push_uint32(pext, pproplist->count));
	for (i=0; i<pproplist->count; i++) {
		TRY(ab_ext_push_addressbook_tapropval(
					pext, pproplist->ppropval + i));
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_push_addressbook_typropval(
	EXT_PUSH *pext, const ADDRESSBOOK_TYPROPVAL *ppropval)
{
	TRY(ext_buffer_push_uint16(pext, ppropval->type));
	return ab_ext_push_addressbook_propval(pext,
			ppropval->type, ppropval->pvalue);	
}

static int ab_ext_push_addressbook_fpropval(EXT_PUSH *pext,
	uint16_t type, const ADDRESSBOOK_FPROPVAL *ppropval)
{
	TRY(ext_buffer_push_uint8(pext, ppropval->flag));
	switch (ppropval->flag) {
	case ADDRESSBOOK_FLAG_AVAILABLE:
		return ab_ext_push_addressbook_propval(
				pext, type, ppropval->pvalue);
	case ADDRESSBOOK_FLAG_UNAVAILABLE:
		return EXT_ERR_SUCCESS;
	case ADDRESSBOOK_FLAG_ERROR:
		return ext_buffer_push_uint32(pext, *(uint32_t*)ppropval->pvalue);
	}
	return EXT_ERR_FORMAT;
}

static int ab_ext_push_addressbook_tfpropval(EXT_PUSH *pext,
	const ADDRESSBOOK_TFPROPVAL *ppropval)
{
	TRY(ext_buffer_push_uint16(pext, ppropval->type));
	TRY(ext_buffer_push_uint8(pext, ppropval->flag));
	switch (ppropval->flag) {
	case ADDRESSBOOK_FLAG_AVAILABLE:
		return ab_ext_push_addressbook_propval(pext,
				ppropval->type, ppropval->pvalue);
	case ADDRESSBOOK_FLAG_UNAVAILABLE:
		return EXT_ERR_SUCCESS;
	case ADDRESSBOOK_FLAG_ERROR:
		return ext_buffer_push_uint32(pext, *(uint32_t*)ppropval->pvalue);
	}
	return EXT_ERR_FORMAT;
}

static int ab_ext_push_addressbook_proprow(EXT_PUSH *pext,
	const LPROPTAG_ARRAY *pcolumns, const ADDRESSBOOK_PROPROW *prow)
{
	TRY(ext_buffer_push_uint8(pext, prow->flag));
	if (PROPROW_FLAG_NORMAL == prow->flag) {
		for (unsigned int i = 0; i < pcolumns->cvalues; ++i) {
			if (PROP_TYPE(pcolumns->pproptag[i]) == PT_UNSPECIFIED)
				TRY(ab_ext_push_addressbook_typropval(pext,
				    static_cast<ADDRESSBOOK_TYPROPVAL *>(prow->ppvalue[i])));
			else
				TRY(ab_ext_push_addressbook_propval(pext,
				         PROP_TYPE(pcolumns->pproptag[i]), prow->ppvalue[i]));
		}
	} else if (PROPROW_FLAG_FLAGGED == prow->flag) {
		for (unsigned int i = 0; i < pcolumns->cvalues; ++i) {
			if (PROP_TYPE(pcolumns->pproptag[i]) == PT_UNSPECIFIED)
				TRY(ab_ext_push_addressbook_tfpropval(pext,
				    static_cast<ADDRESSBOOK_TFPROPVAL *>(prow->ppvalue[i])));
			else
				TRY(ab_ext_push_addressbook_fpropval(pext,
				         PROP_TYPE(pcolumns->pproptag[i]),
				         static_cast<ADDRESSBOOK_FPROPVAL *>(prow->ppvalue[i])));
		}
	} else {
		return EXT_ERR_FORMAT;
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_push_lproptag_array(EXT_PUSH *pext,
	const LPROPTAG_ARRAY *pproptags)
{
	TRY(ext_buffer_push_uint32(pext, pproptags->cvalues));
	for (unsigned int i = 0; i < pproptags->cvalues; ++i) {
		TRY(ext_buffer_push_uint32(pext, pproptags->pproptag[i]));
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_push_mid_array(EXT_PUSH *pext,
	const MID_ARRAY *pmids)
{
	TRY(ext_buffer_push_uint32(pext, pmids->cvalues));
	for (unsigned int i = 0; i < pmids->cvalues; ++i) {
		TRY(ext_buffer_push_uint32(pext, pmids->pproptag[i]));
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_push_stat(EXT_PUSH *pext, const STAT *pstat)
{
	TRY(ext_buffer_push_uint32(pext, pstat->sort_type));
	TRY(ext_buffer_push_uint32(pext, pstat->container_id));
	TRY(ext_buffer_push_uint32(pext, pstat->cur_rec));
	TRY(ext_buffer_push_int32(pext, pstat->delta));
	TRY(ext_buffer_push_uint32(pext, pstat->num_pos));
	TRY(ext_buffer_push_uint32(pext, pstat->total_rec));
	TRY(ext_buffer_push_uint32(pext, pstat->codepage));
	TRY(ext_buffer_push_uint32(pext, pstat->template_locale));
	return ext_buffer_push_uint32(pext, pstat->sort_locale);
}

static int ab_ext_push_addressbook_propname(
	EXT_PUSH *pext, const ADDRESSBOOK_PROPNAME *ppropname)
{
	TRY(ext_buffer_push_guid(pext, &ppropname->guid));
	return ext_buffer_push_uint32(pext, ppropname->id);
}

static int ab_ext_push_addressbook_colrow(
	EXT_PUSH *pext, const ADDRESSBOOK_COLROW *pcolrow)
{
	int i;
	
	TRY(ab_ext_push_lproptag_array(pext, &pcolrow->columns));
	TRY(ext_buffer_push_uint32(pext, pcolrow->row_count));
	for (i=0; i<pcolrow->row_count; i++) {
		TRY(ab_ext_push_addressbook_proprow(pext,
				&pcolrow->columns, pcolrow->prows + i));
	}
	return EXT_ERR_SUCCESS;
}

static int ab_ext_push_nsp_entryid(
	EXT_PUSH *pext, const NSP_ENTRYID *pentryid)
{
	if (0x87 != pentryid->id_type && 0x0 != pentryid->id_type) {
		return EXT_ERR_FORMAT;
	}
	TRY(ext_buffer_push_uint8(pext, pentryid->id_type));
	TRY(ext_buffer_push_uint8(pext, pentryid->r1));
	TRY(ext_buffer_push_uint8(pext, pentryid->r2));
	TRY(ext_buffer_push_uint8(pext, pentryid->r3));
	TRY(ext_buffer_push_guid(pext, &pentryid->provider_uid));
	TRY(ext_buffer_push_uint32(pext, pentryid->display_type));
	if (0 == pentryid->id_type) {
		return ext_buffer_push_string(pext, pentryid->payload.pdn);
	} else {
		return ext_buffer_push_uint32(pext, pentryid->payload.mid);
	}
}

int ab_ext_push_bind_response(EXT_PUSH *pext,
	const BIND_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	TRY(ext_buffer_push_guid(pext, &presponse->server_guid));
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_unbind_response(EXT_PUSH *pext,
	const UNBIND_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_comparemids_response(EXT_PUSH *pext,
	const COMPAREMIDS_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	TRY(ext_buffer_push_uint32(pext, presponse->result1));
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_dntomid_response(EXT_PUSH *pext,
	const DNTOMID_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	if (NULL == presponse->poutmids) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_mid_array(pext, presponse->poutmids));
	}
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_getmatches_response(EXT_PUSH *pext,
	const GETMATCHES_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	if (NULL == presponse->pstat) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_stat(pext, presponse->pstat));
	}
	if (NULL == presponse->pmids) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_mid_array(pext, presponse->pmids));
	}
	if (presponse->result != ecSuccess) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_addressbook_colrow(
				pext, &presponse->column_rows));
	}
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_getproplist_response(EXT_PUSH *pext,
	const GETPROPLIST_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	if (NULL == presponse->pproptags) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_lproptag_array(pext, presponse->pproptags));
	}
	return ext_buffer_push_uint32(pext, 0);
} 

int ab_ext_push_getprops_response(EXT_PUSH *pext,
	const GETPROPS_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	TRY(ext_buffer_push_uint32(pext, presponse->codepage));
	if (NULL == presponse->prow) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_addressbook_proplist(pext, presponse->prow));
	}
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_getspecialtable_response(EXT_PUSH *pext,
	const GETSPECIALTABLE_RESPONSE *presponse)
{
	int i;
	
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	TRY(ext_buffer_push_uint32(pext, presponse->codepage));
	if (NULL == presponse->pversion) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ext_buffer_push_uint32(pext, *presponse->pversion));
	}
	if (0 == presponse->count) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ext_buffer_push_uint32(pext, presponse->count));
		for (i=0; i<presponse->count; i++) {
			TRY(ab_ext_push_addressbook_proplist(
					pext, presponse->prow + i));
		}
	}
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_gettemplateinfo_response(EXT_PUSH *pext,
	const GETTEMPLATEINFO_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	TRY(ext_buffer_push_uint32(pext, presponse->codepage));
	if (NULL == presponse->prow) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_addressbook_proplist(pext, presponse->prow));
	}
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_modlinkatt_response(EXT_PUSH *pext,
	const MODLINKATT_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_modprops_response(EXT_PUSH *pext,
	const MODPROPS_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_queryrows_response(EXT_PUSH *pext,
	const QUERYROWS_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	if (NULL == presponse->pstat) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_stat(pext, presponse->pstat));
	}
	if (presponse->result != ecSuccess) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_addressbook_colrow(
				pext, &presponse->column_rows));
	}
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_querycolumns_response(EXT_PUSH *pext,
	const QUERYCOLUMNS_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	if (NULL == presponse->pcolumns) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_lproptag_array(pext, presponse->pcolumns));
	}
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_resolvenames_response(EXT_PUSH *pext,
	const RESOLVENAMES_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	TRY(ext_buffer_push_uint32(pext, presponse->codepage));
	if (NULL != presponse->pmids) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_mid_array(pext, presponse->pmids));
	}
	if (presponse->result != ecSuccess) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_addressbook_colrow(
				pext, &presponse->column_rows));
	}
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_resortrestriction_response(EXT_PUSH *pext,
	const RESORTRESTRICTION_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	if (NULL == presponse->pstat) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_stat(pext, presponse->pstat));
	}
	if (NULL != presponse->poutmids) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_mid_array(pext, presponse->poutmids));
	}
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_seekentries_response(EXT_PUSH *pext,
	const SEEKENTRIES_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	if (NULL == presponse->pstat) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_stat(pext, presponse->pstat));
	}
	if (presponse->result != ecSuccess) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_addressbook_colrow(
				pext, &presponse->column_rows));
	}
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_updatestat_response(EXT_PUSH *pext,
	const UPDATESTAT_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	if (NULL == presponse->pstat) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ab_ext_push_stat(pext, presponse->pstat));
	}
	if (NULL == presponse->pdelta) {
		TRY(ext_buffer_push_uint8(pext, 0));
	} else {
		TRY(ext_buffer_push_uint8(pext, 0xFF));
		TRY(ext_buffer_push_int32(pext, *presponse->pdelta));
	}
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_getmailboxurl_response(EXT_PUSH *pext,
	const GETMAILBOXURL_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	TRY(ext_buffer_push_wstring(pext, presponse->server_url));
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_getaddressbookurl_response(EXT_PUSH *pext,
	const GETADDRESSBOOKURL_RESPONSE *presponse)
{
	TRY(ext_buffer_push_uint32(pext, presponse->status));
	TRY(ext_buffer_push_uint32(pext, presponse->result));
	TRY(ext_buffer_push_wstring(pext, presponse->server_url));
	return ext_buffer_push_uint32(pext, 0);
}

int ab_ext_push_failure_response(EXT_PUSH *pext, uint32_t status_code)
{
	TRY(ext_buffer_push_uint32(pext, status_code));
	return ext_buffer_push_uint32(pext, 0);
}
