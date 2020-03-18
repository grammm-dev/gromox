#include "dac_ext.h"
#include "ext_buffer.h"
#include "common_util.h"

static BOOL dac_ext_push_dac_res(EXT_PUSH *pext, const DAC_RES *r);

static BOOL dac_ext_pull_propval(
	EXT_PULL *pext, uint16_t type, void **ppval)
{
	switch (type) {
	case PROPVAL_TYPE_DOUBLE:
		*ppval = pext->alloc(sizeof(double));
		if (NULL == *ppval) {
			return FALSE;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_pull_double(
			pext, *ppval)) {
			return FALSE;
		}
		return TRUE;
	case PROPVAL_TYPE_BYTE:
		*ppval = pext->alloc(sizeof(uint8_t));
		if (NULL == *ppval) {
			return FALSE;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_pull_uint8(
			pext, *ppval)) {
			return FALSE;	
		}
		return TRUE;
	case PROPVAL_TYPE_LONGLONG:
		*ppval = pext->alloc(sizeof(uint64_t));
		if (NULL == *ppval) {
			return FALSE;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_pull_uint64(
			pext, *ppval)) {
			return FALSE;
		}
		return TRUE;
	case PROPVAL_TYPE_STRING:
		if (EXT_ERR_SUCCESS != ext_buffer_pull_string(
			pext, (char**)ppval)) {
			return FALSE;	
		}
		return TRUE;
	case PROPVAL_TYPE_BINARY:
		*ppval = pext->alloc(sizeof(BINARY));
		if (NULL == *ppval) {
			return FALSE;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_pull_binary(
			pext, *ppval)) {
			return FALSE;
		}
		return TRUE;
	case PROPVAL_TYPE_GUID:
		*ppval = pext->alloc(sizeof(GUID));
		if (NULL == *ppval) {
			return FALSE;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_pull_guid(
			pext, *ppval)) {
			return FALSE;
		}
		return TRUE;
	case PROPVAL_TYPE_LONGLONG_ARRAY:
		*ppval = pext->alloc(sizeof(LONGLONG_ARRAY));
		if (NULL == *ppval) {
			return FALSE;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_pull_longlong_array(
			pext, *ppval)) {
			return FALSE;	
		}
		return TRUE;
	case PROPVAL_TYPE_STRING_ARRAY:
		*ppval = pext->alloc(sizeof(STRING_ARRAY));
		if (NULL == *ppval) {
			return FALSE;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_pull_string_array(
			pext, *ppval)) {
			return FALSE;	
		}
		return TRUE;
	case PROPVAL_TYPE_OBJECT:
		/* no value for object */
		*ppval = NULL;
		return TRUE;
	default:
		return FALSE;
	}
}

static BOOL dac_ext_pull_dac_propval(
	EXT_PULL *pext, DAC_PROPVAL *r)
{	
	if (EXT_ERR_SUCCESS != ext_buffer_pull_string(
		pext, &r->pname)) {
		return FALSE;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint16(
		pext, &r->type)) {
		return FALSE;	
	}
	return dac_ext_pull_propval(pext, r->type, &r->pvalue);
}

static BOOL dac_ext_pull_dac_varray(
	EXT_PULL *pext, DAC_VARRAY *r)
{
	int i;
	
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint16(
		pext, &r->count)) {
		return FALSE;
	}
	if (0 == r->count) {
		r->ppropval = NULL;
		return TRUE;
	}
	r->ppropval = pext->alloc(sizeof(DAC_PROPVAL)*r->count);
	if (NULL == r->ppropval) {
		return FALSE;
	}
	for (i=0; i<r->count; i++) {
		if (TRUE != dac_ext_pull_dac_propval(
			pext, r->ppropval + i)) {
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL dac_ext_pull_dac_vset(
	EXT_PULL *pext, DAC_VSET *r)
{
	int i;

	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint16(
		pext, &r->count)) {
		return FALSE;
	}
	if (0 == r->count) {
		r->pparray = NULL;
		return TRUE;
	}
	r->pparray = pext->alloc(sizeof(DAC_VARRAY*)*r->count);
	if (NULL == r->pparray) {
		return FALSE;
	}
	for (i=0; i<r->count; i++) {
		r->pparray[i] = pext->alloc(sizeof(DAC_VARRAY));
		if (NULL == r->pparray[i]) {
			return FALSE;
		}
		if (FALSE == dac_ext_pull_dac_varray(
			pext, r->pparray[i])) {
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL dac_ext_push_propval(EXT_PUSH *pext,
	uint16_t type, const void *pval)
{
	switch (type) {
	case PROPVAL_TYPE_DOUBLE:
		if (EXT_ERR_SUCCESS != ext_buffer_push_double(
			pext, *(double*)pval)) {
			return FALSE;	
		}
		return TRUE;
	case PROPVAL_TYPE_BYTE:
		if (EXT_ERR_SUCCESS != ext_buffer_push_uint8(
			pext, *(uint8_t*)pval)) {
			return FALSE;	
		}
		return TRUE;
	case PROPVAL_TYPE_LONGLONG:
		if (EXT_ERR_SUCCESS != ext_buffer_push_uint64(
			pext, *(uint64_t*)pval)) {
			return FALSE;
		}
		return TRUE;
	case PROPVAL_TYPE_STRING:
		if (EXT_ERR_SUCCESS != ext_buffer_push_string(
			pext, pval)) {
			return FALSE;	
		}
		return TRUE;
	case PROPVAL_TYPE_BINARY:
		if (EXT_ERR_SUCCESS != ext_buffer_push_binary(
			pext, pval)) {
			return FALSE;	
		}
		return TRUE;
	case PROPVAL_TYPE_GUID:
		if (EXT_ERR_SUCCESS != ext_buffer_push_guid(
			pext, pval)) {
			return FALSE;	
		}
		return TRUE;
	case PROPVAL_TYPE_LONGLONG_ARRAY:
		if (EXT_ERR_SUCCESS != ext_buffer_push_longlong_array(
			pext, pval)) {
			return FALSE;	
		}
		return TRUE;
	case PROPVAL_TYPE_STRING_ARRAY:
		if (EXT_ERR_SUCCESS != ext_buffer_push_string_array(
			pext, pval)) {
			return FALSE;	
		}
		return TRUE;
	default:
		return FALSE;
	}
}

static BOOL dac_ext_push_dac_propval(
	EXT_PUSH *pext, const DAC_PROPVAL *r)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, r->pname)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint16(
		pext, r->type)) {
		return FALSE;	
	}
	return dac_ext_push_propval(pext, r->type, r->pvalue);
}

static BOOL dac_ext_push_dac_varray(
	EXT_PUSH *pext, const DAC_VARRAY *r)
{
	int i;
	
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint16(
		pext, r->count)) {
		return FALSE;
	}
	for (i=0; i<r->count; i++) {
		if (TRUE != dac_ext_push_dac_propval(
			pext, r->ppropval + i)) {
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL dac_ext_push_dac_vset(
	EXT_PUSH *pext, const DAC_VSET *r)
{
	int i;

	if (EXT_ERR_SUCCESS != ext_buffer_push_uint16(
		pext, r->count)) {
		return FALSE;
	}
	for (i=0; i<r->count; i++) {
		if (FALSE == dac_ext_push_dac_varray(
			pext, r->pparray[i])) {
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL dac_ext_push_dac_res_and_or(
	EXT_PUSH *pext, const DAC_RES_AND_OR *r)
{
	int i;
	
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(pext, r->count)) {
		return FALSE;
	}
	for (i=0; i<r->count; i++) {
		if (TRUE != dac_ext_push_dac_res(pext, r->pres + i)) {
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL dac_ext_push_dac_res_not(EXT_PUSH *pext, const DAC_RES_NOT *r)
{
	return dac_ext_push_dac_res(pext, &r->res);
}

static BOOL dac_ext_push_dac_res_content(
	EXT_PUSH *pext, const DAC_RES_CONTENT *r)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint8(pext, r->fl)) {
		return FALSE;
	}
	return dac_ext_push_dac_propval(pext, &r->propval);
}

static BOOL dac_ext_push_dac_res_property(
	EXT_PUSH *pext, const DAC_RES_PROPERTY *r)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint8(pext, r->relop)) {
		return FALSE;
	}
	return dac_ext_push_dac_propval(pext, &r->propval);
}

static BOOL dac_ext_push_dac_res_exist(
	EXT_PUSH *pext, const DAC_RES_EXIST *r)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(pext, r->pname)) {
		return FALSE;
	}
	return TRUE;
}

static BOOL dac_ext_push_dac_res_subobj(
	EXT_PUSH *pext, const DAC_RES_SUBOBJ *r)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(pext, r->pname)) {
		return FALSE;
	}
	return dac_ext_push_dac_res(pext, &r->res);
}

static BOOL dac_ext_push_dac_res(EXT_PUSH *pext, const DAC_RES *r)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint8(pext, r->rt)) {
		return FALSE;	
	}
	switch (r->rt) {
	case DAC_RES_TYPE_AND:
	case DAC_RES_TYPE_OR:
		return dac_ext_push_dac_res_and_or(pext, r->pres);
	case DAC_RES_TYPE_NOT:
		return dac_ext_push_dac_res_not(pext, r->pres);
	case DAC_RES_TYPE_CONTENT:
		return dac_ext_push_dac_res_content(pext, r->pres);
	case DAC_RES_TYPE_PROPERTY:
		return dac_ext_push_dac_res_property(pext, r->pres);
	case DAC_RES_TYPE_EXIST:
		return dac_ext_push_dac_res_exist(pext, r->pres);
	case DAC_RES_TYPE_SUBOBJ:
		return dac_ext_push_dac_res_subobj(pext, r->pres);
	default:
		return FALSE;
	}
}

static BOOL dac_ext_push_connect_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->connect.remote_id)) {
		return FALSE;
	}
	return TRUE;
}

static BOOL dac_ext_pull_infostorage_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint64(
		pext, &ppayload->infostorage.total_space)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint64(
		pext, &ppayload->infostorage.total_used)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint32(
		pext, &ppayload->infostorage.total_dir)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_allocdir_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint8(
		pext, ppayload->allocdir.type)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_pull_allocdir_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_pull_string(
		pext, &ppayload->allocdir.pdir)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_setpropvals_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->setpropvals.pnamespace)) {
		return FALSE;
	}
	return dac_ext_push_dac_varray(
		pext, ppayload->setpropvals.ppropvals);
}

static BOOL dac_ext_push_getpropvals_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->getpropvals.pnamespace)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string_array(
		pext, ppayload->getpropvals.pnames)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_pull_getpropvals_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	return dac_ext_pull_dac_varray(pext,
		&ppayload->getpropvals.propvals);
}

static BOOL dac_ext_push_deletepropvals_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->deletepropvals.pnamespace)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string_array(
		pext, ppayload->deletepropvals.pnames)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_opentable_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{	
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->opentable.pnamespace)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->opentable.ptablename)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		pext, ppayload->opentable.flags)) {
		return FALSE;	
	}
	if (NULL == ppayload->opentable.puniquename) {
		if (EXT_ERR_SUCCESS != ext_buffer_push_uint8(pext, 0)) {
			return FALSE;
		}
	} else {
		if (EXT_ERR_SUCCESS != ext_buffer_push_uint8(pext, 1)) {
			return FALSE;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_string(
			pext, ppayload->opentable.puniquename)) {
			return FALSE;	
		}
	}
	return TRUE;
}

static BOOL dac_ext_pull_opentable_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint32(
		pext, &ppayload->opentable.table_id)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_opencelltable_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint64(
		pext, ppayload->opencelltable.row_instance)) {
		return FALSE;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->opencelltable.pcolname)) {
		return FALSE;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		pext, ppayload->opencelltable.flags)) {
		return FALSE;
	}
	if (NULL == ppayload->opencelltable.puniquename) {
		if (EXT_ERR_SUCCESS != ext_buffer_push_uint8(pext, 0)) {
			return FALSE;
		}
	} else {
		if (EXT_ERR_SUCCESS != ext_buffer_push_uint8(pext, 1)) {
			return FALSE;
		}
		if (EXT_ERR_SUCCESS != ext_buffer_push_string(
			pext, ppayload->opencelltable.puniquename)) {
			return FALSE;	
		}
	}
	return TRUE;
}

static BOOL dac_ext_pull_opencelltable_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint32(
		pext, &ppayload->opentable.table_id)) {
		return FALSE;
	}
	return TRUE;
}

static BOOL dac_ext_push_restricttable_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		pext, ppayload->restricttable.table_id)) {
		return FALSE;	
	}
	return dac_ext_push_dac_res(pext,
		ppayload->restricttable.prestrict);
}

static BOOL dac_ext_push_updatecells_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint64(
		pext, ppayload->updatecells.row_instance)) {
		return FALSE;	
	}
	return dac_ext_push_dac_varray(pext,
		ppayload->updatecells.pcells);
}

static BOOL dac_ext_push_sumtable_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		pext, ppayload->sumtable.table_id)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_pull_sumtable_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint32(
		pext, &ppayload->sumtable.count)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_queryrows_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		pext, ppayload->queryrows.table_id)) {
		return FALSE;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		pext, ppayload->queryrows.start_pos)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		pext, ppayload->queryrows.row_count)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_pull_queryrows_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	return dac_ext_pull_dac_vset(pext, &ppayload->queryrows.set);
}

static BOOL dac_ext_push_insertrows_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		pext, ppayload->insertrows.table_id)) {
		return FALSE;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		pext, ppayload->insertrows.flags)) {
		return FALSE;	
	}
	return dac_ext_push_dac_vset(pext,
			ppayload->insertrows.pset);
}

static BOOL dac_ext_push_deleterow_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint64(
		pext, ppayload->deleterow.row_instance)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_deletecells_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint64(
		pext, ppayload->deletecells.row_instance)) {
		return FALSE;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string_array(
		pext, ppayload->deletecells.pcolnames)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_deletetable_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->deletetable.pnamespace)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->deletetable.ptablename)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_closetable_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		pext, ppayload->closetable.table_id)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_matchrow_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		pext, ppayload->matchrow.table_id)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint16(
		pext, ppayload->matchrow.type)) {
		return FALSE;	
	}
	return dac_ext_push_propval(
		pext, ppayload->matchrow.type,
		ppayload->matchrow.pvalue);
}

static BOOL dac_ext_pull_matchrow_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	return dac_ext_pull_dac_varray(pext,
			&ppayload->matchrow.row);
}

static BOOL dac_ext_pull_getnamespaces_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_pull_string_array(
		pext, &ppayload->getnamespaces.namespaces)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_getpropnames_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->getpropnames.pnamespace)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_pull_getpropnames_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_pull_string_array(
		pext, &ppayload->getpropnames.propnames)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_gettablenames_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->gettablenames.pnamespace)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_pull_gettablenames_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_pull_string_array(
		pext, &ppayload->gettablenames.tablenames)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_readfile_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->readfile.path)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->readfile.file_name)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint64(
		pext, ppayload->readfile.offset)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(
		pext, ppayload->readfile.length)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_pull_readfile_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_pull_binary(
		pext, &ppayload->readfile.content_bin)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_appendfile_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->appendfile.path)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->appendfile.file_name)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_binary(
		pext, ppayload->appendfile.pcontent_bin)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_removefile_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->removefile.path)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->removefile.file_name)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_statfile_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->statfile.path)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->statfile.file_name)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_pull_statfile_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint64(
		pext, &ppayload->statfile.size)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_push_checkrow_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->checkrow.pnamespace)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->checkrow.ptablename)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint16(
		pext, ppayload->checkrow.type)) {
		return FALSE;	
	}
	return dac_ext_push_propval(
		pext, ppayload->checkrow.type,
		ppayload->checkrow.pvalue);
}

static BOOL dac_ext_push_phprpc_request(
	EXT_PUSH *pext, const REQUEST_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->phprpc.account)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_string(
		pext, ppayload->phprpc.script_path)) {
		return FALSE;	
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_binary(
		pext, ppayload->phprpc.pbin_in)) {
		return FALSE;	
	}
	return TRUE;
}

static BOOL dac_ext_pull_phprpc_response(
	EXT_PULL *pext, RESPONSE_PAYLOAD *ppayload)
{
	if (EXT_ERR_SUCCESS != ext_buffer_pull_binary(
		pext, &ppayload->phprpc.bin_out)) {
		return FALSE;	
	}
	return TRUE;
}

BOOL dac_ext_push_request(const RPC_REQUEST *prequest, BINARY *pbin_out)
{
	int status;
	BOOL b_result;
	EXT_PUSH ext_push;
	
	if (FALSE == ext_buffer_push_init(&ext_push,
		NULL, 0, EXT_FLAG_WCOUNT)) {
		return FALSE;	
	}
	status = ext_buffer_push_advance(&ext_push, sizeof(uint32_t));
	if (EXT_ERR_SUCCESS != status) {
		ext_buffer_push_free(&ext_push);
		return FALSE;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint8(
		&ext_push, prequest->call_id)) {
		ext_buffer_push_free(&ext_push);
		return FALSE;
	}
	if (CALL_ID_CONNECT != prequest->call_id &&
		CALL_ID_ALLOCDIR != prequest->call_id &&
		CALL_ID_INFOSTORAGE != prequest->call_id) {
		if (EXT_ERR_SUCCESS != ext_buffer_push_string(
			&ext_push, prequest->dir)) {
			ext_buffer_push_free(&ext_push);
			return FALSE;
		}
	}
	switch (prequest->call_id) {
	case CALL_ID_CONNECT:
		b_result = dac_ext_push_connect_request(
				&ext_push, &prequest->payload);
		break;
	case CALL_ID_INFOSTORAGE:
		b_result = TRUE;
		break;
	case CALL_ID_ALLOCDIR:
		b_result = dac_ext_push_allocdir_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_FREEDIR:
		b_result = TRUE;
		break;
	case CALL_ID_SETPROPVALS:
		b_result = dac_ext_push_setpropvals_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_GETPROPVALS:
		b_result = dac_ext_push_getpropvals_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_DELETEPROPVALS:
		b_result = dac_ext_push_deletepropvals_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_OPENTABLE:
		b_result = dac_ext_push_opentable_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_OPENCELLTABLE:
		b_result = dac_ext_push_opencelltable_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_RESTRICTTABLE:
		b_result = dac_ext_push_restricttable_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_UPDATECELLS:
		b_result = dac_ext_push_updatecells_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_SUMTABLE:
		b_result = dac_ext_push_sumtable_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_QUERYROWS:
		b_result = dac_ext_push_queryrows_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_INSERTROWS:
		b_result = dac_ext_push_insertrows_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_DELETEROW:
		b_result = dac_ext_push_deleterow_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_DELETECELLS:
		b_result = dac_ext_push_deletecells_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_DELETETABLE:
		b_result = dac_ext_push_deletetable_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_CLOSETABLE:
		b_result = dac_ext_push_closetable_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_MATCHROW:
		b_result = dac_ext_push_matchrow_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_GETNAMESPACES:
		b_result = TRUE;
		break;
	case CALL_ID_GETPROPNAMES:
		b_result = dac_ext_push_getpropnames_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_GETTABLENAMES:
		b_result = dac_ext_push_gettablenames_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_READFILE:
		b_result = dac_ext_push_readfile_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_APPENDFILE:
		b_result = dac_ext_push_appendfile_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_REMOVEFILE:
		b_result = dac_ext_push_removefile_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_STATFILE:
		b_result = dac_ext_push_statfile_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_CHECKROW:
		b_result = dac_ext_push_checkrow_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_PHPRPC:
		b_result = dac_ext_push_phprpc_request(
				&ext_push, &prequest->payload);
		break;
	default:
		b_result = FALSE;
		break;
	}
	if (FALSE == b_result) {
		ext_buffer_push_free(&ext_push);
		return FALSE;
	}
	pbin_out->cb = ext_push.offset;
	pbin_out->pb = ext_push.data;
	ext_push.offset = 0;
	ext_buffer_push_uint32(&ext_push,
		pbin_out->cb - sizeof(uint32_t));
	/* memory referneced by ext_push.data will be freed outside */
	return TRUE;
}

/* CALL_ID_CONNECT  not included */
BOOL dac_ext_pull_response(const BINARY *pbin_in, RPC_RESPONSE *presponse)
{
	EXT_PULL ext_pull;

	ext_buffer_pull_init(&ext_pull, pbin_in->pb,
		pbin_in->cb, common_util_alloc, EXT_FLAG_WCOUNT);
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint32(
		&ext_pull, &presponse->result)) {
		return FALSE;	
	}
	if (EC_SUCCESS != presponse->result) {
		return TRUE;
	}
	switch (presponse->call_id) {
	case CALL_ID_FREEDIR:
	case CALL_ID_SETPROPVALS:
	case CALL_ID_DELETEPROPVALS:
	case CALL_ID_RESTRICTTABLE:
	case CALL_ID_UPDATECELLS:
	case CALL_ID_INSERTROWS:
	case CALL_ID_DELETEROW:
	case CALL_ID_DELETECELLS:
	case CALL_ID_DELETETABLE:
	case CALL_ID_CLOSETABLE:
	case CALL_ID_APPENDFILE:
	case CALL_ID_REMOVEFILE:
	case CALL_ID_CHECKROW:
		return TRUE;
	case CALL_ID_INFOSTORAGE:
		return dac_ext_pull_infostorage_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_ALLOCDIR:
		return dac_ext_pull_allocdir_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_GETPROPVALS:
		return dac_ext_pull_getpropvals_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_OPENTABLE:
		return dac_ext_pull_opentable_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_OPENCELLTABLE:
		return dac_ext_pull_opencelltable_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_SUMTABLE:
		return dac_ext_pull_sumtable_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_QUERYROWS:
		return dac_ext_pull_queryrows_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_MATCHROW:
		return dac_ext_pull_matchrow_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_GETNAMESPACES:
		return dac_ext_pull_getnamespaces_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_GETPROPNAMES:
		return dac_ext_pull_getpropnames_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_GETTABLENAMES:
		return dac_ext_pull_gettablenames_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_READFILE:
		return dac_ext_pull_readfile_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_STATFILE:
		return dac_ext_pull_statfile_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_PHPRPC:
		return dac_ext_pull_phprpc_response(
			&ext_pull, &presponse->payload);
	default:
		return FALSE;
	}
}
