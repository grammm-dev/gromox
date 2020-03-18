#include "ext_pack.h"
#include "rpc_ext.h"

static zend_bool rpc_ext_push_allocdir_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	return ext_pack_push_uint8(pctx, ppayload->allocdir.type);
}

static zend_bool rpc_ext_pull_allocdir_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_string(pctx, &ppayload->allocdir.pdir);
}

static zend_bool rpc_ext_push_setpropvals_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_string(pctx,
		ppayload->setpropvals.pnamespace)) {
		return 0;
	}
	return ext_pack_push_dac_varray(
		pctx, &ppayload->setpropvals.propvals);
}

static zend_bool rpc_ext_push_getpropvals_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (NULL == ppayload->setpropvals.pnamespace) {
		if (!ext_pack_push_uint8(pctx, 0)) {
			return 0;
		}
	} else {
		if (!ext_pack_push_uint8(pctx, 1)) {
			return 0;
		}
		if (!ext_pack_push_string(pctx,
			ppayload->setpropvals.pnamespace)) {
			return 0;
		}
	}
	return ext_pack_push_string_array(pctx,
			&ppayload->getpropvals.names);
}

static zend_bool rpc_ext_pull_getpropvals_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_dac_varray(pctx,
		&ppayload->getpropvals.propvals);
}

static zend_bool rpc_ext_push_deletepropvals_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_string(
		pctx, ppayload->setpropvals.pnamespace)) {
		return 0;	
	}
	return ext_pack_push_string_array(
		pctx, &ppayload->getpropvals.names);
}

static zend_bool rpc_ext_push_opentable_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{	
	if (!ext_pack_push_string(pctx,
		ppayload->setpropvals.pnamespace)) {
		return 0;	
	}
	if (!ext_pack_push_string(pctx, ppayload->opentable.ptablename)) {
		return 0;	
	}
	if (!ext_pack_push_uint32(pctx, ppayload->opentable.flags)) {
		return 0;	
	}
	if (NULL == ppayload->opentable.puniquename) {
		return ext_pack_push_uint8(pctx, 0);
	} else {
		if (!ext_pack_push_uint8(pctx, 1)) {
			return 0;
		}
		return ext_pack_push_string(pctx,
			ppayload->opentable.puniquename);
	}
}

static zend_bool rpc_ext_pull_opentable_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_uint32(pctx, &ppayload->opentable.table_id);
}

static zend_bool rpc_ext_push_opencelltable_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_uint64(pctx,
		ppayload->opencelltable.row_instance)) {
		return 0;
	}
	if (!ext_pack_push_string(pctx,
		ppayload->opencelltable.pcolname)) {
		return 0;
	}
	if (!ext_pack_push_uint32(pctx, ppayload->opencelltable.flags)) {
		return 0;
	}
	if (NULL == ppayload->opencelltable.puniquename) {
		return ext_pack_push_uint8(pctx, 0);
	} else {
		if (!ext_pack_push_uint8(pctx, 1)) {
			return 0;
		}
		return ext_pack_push_string(pctx,
			ppayload->opencelltable.puniquename);
	}
}

static zend_bool rpc_ext_pull_opencelltable_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_uint32(pctx, &ppayload->opentable.table_id);
}

static zend_bool rpc_ext_push_restricttable_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_uint32(pctx,
		ppayload->restricttable.table_id)) {
		return 0;	
	}
	return ext_pack_push_dac_res(pctx,
		ppayload->restricttable.prestrict);
}

static zend_bool rpc_ext_push_updatecells_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_uint64(pctx,
		ppayload->updatecells.row_instance)) {
		return 0;	
	}
	return ext_pack_push_dac_varray(pctx,
			&ppayload->updatecells.cells);
}

static zend_bool rpc_ext_push_sumtable_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	return ext_pack_push_uint32(pctx, ppayload->sumtable.table_id);
}

static zend_bool rpc_ext_pull_sumtable_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_uint32(pctx, &ppayload->sumtable.count);
}

static zend_bool rpc_ext_push_queryrows_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_uint32(pctx, ppayload->queryrows.table_id)) {
		return 0;
	}
	if (!ext_pack_push_uint32(pctx, ppayload->queryrows.start_pos)) {
		return 0;	
	}
	return ext_pack_push_uint32(pctx, ppayload->queryrows.row_count);
}

static zend_bool rpc_ext_pull_queryrows_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_dac_vset(pctx, &ppayload->queryrows.set);
}

static zend_bool rpc_ext_push_insertrows_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_uint32(
		pctx, ppayload->insertrows.table_id)) {
		return 0;
	}
	if (!ext_pack_push_uint32(
		pctx, ppayload->insertrows.flags)) {
		return 0;	
	}
	return ext_pack_push_dac_vset(pctx,
			&ppayload->insertrows.set);
}

static zend_bool rpc_ext_push_deleterow_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	return ext_pack_push_uint64(pctx,
		ppayload->deleterow.row_instance);
}

static zend_bool rpc_ext_push_deletecells_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_uint64(
		pctx, ppayload->deletecells.row_instance)) {
		return 0;
	}
	return ext_pack_push_string_array(pctx,
			&ppayload->deletecells.colnames);
}

static zend_bool rpc_ext_push_deletetable_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_string(pctx,
		ppayload->deletetable.pnamespace)) {
		return 0;	
	}
	return ext_pack_push_string(pctx,
		ppayload->deletetable.ptablename);
}

static zend_bool rpc_ext_push_closetable_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	return ext_pack_push_uint32(pctx,
		ppayload->closetable.table_id);
}

static zend_bool rpc_ext_push_matchrow_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_uint32(
		pctx, ppayload->matchrow.table_id)) {
		return 0;	
	}
	if (!ext_pack_push_uint16(
		pctx, ppayload->matchrow.type)) {
		return 0;	
	}
	return ext_pack_push_propval(
		pctx, ppayload->matchrow.type,
		ppayload->matchrow.pvalue);
}

static zend_bool rpc_ext_pull_matchrow_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_dac_varray(pctx,
			&ppayload->matchrow.row);
}

static zend_bool rpc_ext_pull_getnamespaces_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_string_array(pctx,
		&ppayload->getnamespaces.namespaces);
}

static zend_bool rpc_ext_push_getpropnames_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	return ext_pack_push_string(pctx,
		ppayload->getpropnames.pnamespace);
}

static zend_bool rpc_ext_pull_getpropnames_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_string_array(pctx,
		&ppayload->getpropnames.propnames);
}

static zend_bool rpc_ext_push_gettablenames_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	return ext_pack_push_string(pctx,
		ppayload->gettablenames.pnamespace);
}

static zend_bool rpc_ext_pull_gettablenames_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_string_array(pctx,
		&ppayload->gettablenames.tablenames);
}

static zend_bool rpc_ext_push_readfile_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_string(pctx, ppayload->readfile.path)) {
		return 0;	
	}
	if (!ext_pack_push_string(pctx,
		ppayload->readfile.file_name)) {
		return 0;	
	}
	if (!ext_pack_push_uint64(
		pctx, ppayload->readfile.offset)) {
		return 0;	
	}
	return ext_pack_push_uint32(pctx, ppayload->readfile.length);
}

static zend_bool rpc_ext_pull_readfile_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_binary(pctx,
		&ppayload->readfile.content_bin);
}

static zend_bool rpc_ext_push_appendfile_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_string(pctx, ppayload->appendfile.path)) {
		return 0;	
	}
	if (!ext_pack_push_string(pctx, ppayload->appendfile.file_name)) {
		return 0;	
	}
	return ext_pack_push_binary(pctx,
		&ppayload->appendfile.content_bin);
}

static zend_bool rpc_ext_push_removefile_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_string(
		pctx, ppayload->removefile.path)) {
		return 0;	
	}
	return ext_pack_push_string(pctx, ppayload->removefile.file_name);
}

static zend_bool rpc_ext_push_statfile_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_string(
		pctx, ppayload->statfile.path)) {
		return 0;	
	}
	return ext_pack_push_string(pctx, ppayload->statfile.file_name);
}

static zend_bool rpc_ext_pull_statfile_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_uint64(pctx, &ppayload->statfile.size);
}

static zend_bool rpc_ext_pull_getaddressbook_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_string(pctx,
		&ppayload->getaddressbook.pab_address);
}

static zend_bool rpc_ext_pull_getabhierarchy_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_dac_vset(pctx, &ppayload->getabhierarchy.set);
}

static zend_bool rpc_ext_push_getabcontent_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_uint32(pctx, ppayload->getabcontent.start_pos)) {
		return 0;	
	}
	return ext_pack_push_uint16(pctx, ppayload->getabcontent.count);
}

static zend_bool rpc_ext_pull_getabcontent_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_dac_vset(pctx, &ppayload->getabcontent.set);
}

static zend_bool rpc_ext_push_restrictabcontent_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_dac_res(pctx,
		ppayload->restrictabcontent.prestrict)) {
		return 0;	
	}
	return ext_pack_push_uint16(pctx, ppayload->restrictabcontent.max_count);
}

static zend_bool rpc_ext_pull_restrictabcontent_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_dac_vset(pctx, &ppayload->restrictabcontent.set);
}

static zend_bool rpc_ext_pull_getexaddress_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_string(pctx,
		&ppayload->getexaddress.pex_address);
}

static zend_bool rpc_ext_pull_getexaddresstype_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_uint8(pctx,
		&ppayload->getexaddresstype.address_type);
}

static zend_bool rpc_ext_push_checkmlistinclude_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	return ext_pack_push_string(pctx,
		ppayload->checkmlistinclude.account_address);
}

static zend_bool rpc_ext_pull_checkmlistinclude_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_bool(pctx,
		&ppayload->checkmlistinclude.b_include);
}

static zend_bool rpc_ext_push_phprpc_request(
	PUSH_CTX *pctx, const REQUEST_PAYLOAD *ppayload)
{
	if (!ext_pack_push_string(pctx, ppayload->phprpc.script_path)) {
		return 0;
	}
	return ext_pack_push_binary(pctx, &ppayload->phprpc.bin_in);
}

static zend_bool rpc_ext_pull_phprpc_response(
	PULL_CTX *pctx, RESPONSE_PAYLOAD *ppayload)
{
	return ext_pack_pull_binary(pctx, &ppayload->phprpc.bin_out);
}

zend_bool rpc_ext_push_request(const RPC_REQUEST *prequest, BINARY *pbin_out)
{
	PUSH_CTX ext_push;
	zend_bool b_result;
	
	if (!ext_pack_push_init(&ext_push)) {
		return 0;	
	}
	if (!ext_pack_push_advance(&ext_push, sizeof(uint32_t))) {
		ext_pack_push_free(&ext_push);
		return 0;
	}
	if (!ext_pack_push_uint8(&ext_push, prequest->call_id)) {
		ext_pack_push_free(&ext_push);
		return 0;
	}
	if (CALL_ID_ALLOCDIR != prequest->call_id) {
		if (!ext_pack_push_string(&ext_push, prequest->address)) {
			ext_pack_push_free(&ext_push);
			return 0;
		}
	}
	switch (prequest->call_id) {
	case CALL_ID_INFOSTORAGE:
		b_result = 1;
		break;
	case CALL_ID_ALLOCDIR:
		b_result = rpc_ext_push_allocdir_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_FREEDIR:
		b_result = 1;
		break;
	case CALL_ID_SETPROPVALS:
		b_result = rpc_ext_push_setpropvals_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_GETPROPVALS:
		b_result = rpc_ext_push_getpropvals_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_DELETEPROPVALS:
		b_result = rpc_ext_push_deletepropvals_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_OPENTABLE:
		b_result = rpc_ext_push_opentable_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_OPENCELLTABLE:
		b_result = rpc_ext_push_opencelltable_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_RESTRICTTABLE:
		b_result = rpc_ext_push_restricttable_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_UPDATECELLS:
		b_result = rpc_ext_push_updatecells_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_SUMTABLE:
		b_result = rpc_ext_push_sumtable_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_QUERYROWS:
		b_result = rpc_ext_push_queryrows_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_INSERTROWS:
		b_result = rpc_ext_push_insertrows_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_DELETEROW:
		b_result = rpc_ext_push_deleterow_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_DELETECELLS:
		b_result = rpc_ext_push_deletecells_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_DELETETABLE:
		b_result = rpc_ext_push_deletetable_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_CLOSETABLE:
		b_result = rpc_ext_push_closetable_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_MATCHROW:
		b_result = rpc_ext_push_matchrow_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_GETNAMESPACES:
		b_result = 1;
		break;
	case CALL_ID_GETPROPNAMES:
		b_result = rpc_ext_push_getpropnames_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_GETTABLENAMES:
		b_result = rpc_ext_push_gettablenames_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_READFILE:
		b_result = rpc_ext_push_readfile_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_APPENDFILE:
		b_result = rpc_ext_push_appendfile_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_REMOVEFILE:
		b_result = rpc_ext_push_removefile_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_STATFILE:
		b_result = rpc_ext_push_statfile_request(
					&ext_push, &prequest->payload);
		break;
	case CALL_ID_GETADDRESSBOOK:
		b_result = 1;
		break;
	case CALL_ID_GETABHIERARCHY:
		b_result = 1;
		break;
	case CALL_ID_GETABCONTENT:
		b_result = rpc_ext_push_getabcontent_request(
						&ext_push, &prequest->payload);
		break;
	case CALL_ID_RESTRICTABCONTENT:
		b_result = rpc_ext_push_restrictabcontent_request(
							&ext_push, &prequest->payload);
		break;
	case CALL_ID_GETEXADDRESS:
		b_result = 1;
		break;
	case CALL_ID_GETEXADDRESSTYPE:
		b_result = 1;
		break;
	case CALL_ID_CHECKMLISTINCLUDE:
		b_result = rpc_ext_push_checkmlistinclude_request(
							&ext_push, &prequest->payload);
		break;
	case CALL_ID_PHPRPC:
		b_result = rpc_ext_push_phprpc_request(
				&ext_push, &prequest->payload);
		break;
	default:
		b_result = 0;
		break;
	}
	if (0 == b_result) {
		ext_pack_push_free(&ext_push);
		return 0;
	}
	pbin_out->cb = ext_push.offset;
	pbin_out->pb = ext_push.data;
	ext_push.offset = 0;
	ext_pack_push_uint32(&ext_push,
		pbin_out->cb - sizeof(uint32_t));
	return 1;
}

zend_bool rpc_ext_pull_response(const BINARY *pbin_in,
	RPC_RESPONSE *presponse)
{
	PULL_CTX ext_pull;

	ext_pack_pull_init(&ext_pull, pbin_in->pb, pbin_in->cb);
	if (!ext_pack_pull_uint32(&ext_pull, &presponse->result)) {
		return 0;	
	}
	if (EC_SUCCESS != presponse->result) {
		return 1;
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
		return 1;
	case CALL_ID_ALLOCDIR:
		return rpc_ext_pull_allocdir_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_GETPROPVALS:
		return rpc_ext_pull_getpropvals_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_OPENTABLE:
		return rpc_ext_pull_opentable_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_OPENCELLTABLE:
		return rpc_ext_pull_opencelltable_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_SUMTABLE:
		return rpc_ext_pull_sumtable_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_QUERYROWS:
		return rpc_ext_pull_queryrows_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_MATCHROW:
		return rpc_ext_pull_matchrow_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_GETNAMESPACES:
		return rpc_ext_pull_getnamespaces_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_GETPROPNAMES:
		return rpc_ext_pull_getpropnames_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_GETTABLENAMES:
		return rpc_ext_pull_gettablenames_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_READFILE:
		return rpc_ext_pull_readfile_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_STATFILE:
		return rpc_ext_pull_statfile_response(
				&ext_pull, &presponse->payload);
	case CALL_ID_GETADDRESSBOOK:
		return rpc_ext_pull_getaddressbook_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_GETABHIERARCHY:
		return rpc_ext_pull_getabhierarchy_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_GETABCONTENT:
		return rpc_ext_pull_getabcontent_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_RESTRICTABCONTENT:
		return rpc_ext_pull_restrictabcontent_response(
						&ext_pull, &presponse->payload);
	case CALL_ID_GETEXADDRESS:
		return rpc_ext_pull_getexaddress_response(
					&ext_pull, &presponse->payload);
	case CALL_ID_GETEXADDRESSTYPE:
		return rpc_ext_pull_getexaddresstype_response(
						&ext_pull, &presponse->payload);
	case CALL_ID_CHECKMLISTINCLUDE:
		return rpc_ext_pull_checkmlistinclude_response(
						&ext_pull, &presponse->payload);
	case CALL_ID_PHPRPC:
		return rpc_ext_pull_phprpc_response(
			&ext_pull, &presponse->payload);
	default:
		return 0;
	}
}
