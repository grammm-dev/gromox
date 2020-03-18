#include "adac_client.h"
#include "rpc_ext.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#define RESPONSE_CODE_SUCCESS      			0x00

#define CS_PATH								"/var/grid-agent/token/adac"

static int adac_client_connect()
{
	int sockd, len;
	struct sockaddr_un un;
	
	sockd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockd < 0) {
		return -1;
	}
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, CS_PATH);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);
	if (connect(sockd, (struct sockaddr*)&un, len) < 0) {
		close(sockd);
		return -2;
	}
	return sockd;
}

static zend_bool adac_client_read_socket(int sockd, BINARY *pbin)
{
	int read_len;
	uint32_t offset;
	uint8_t resp_buff[5];
	
	pbin->pb = NULL;
	while (1) {
		if (NULL == pbin->pb) {
			read_len = read(sockd, resp_buff, 5);
			if (1 == read_len) {
				pbin->cb = 1;
				pbin->pb = emalloc(1);
				if (NULL == pbin->pb) {
					return 0;
				}
				*(uint8_t*)pbin->pb = resp_buff[0];
				return 1;
			} else if (5 == read_len) {
				pbin->cb = *(uint32_t*)(resp_buff + 1) + 5;
				pbin->pb = emalloc(pbin->cb);
				if (NULL == pbin->pb) {
					return 0;
				}
				memcpy(pbin->pb, resp_buff, 5);
				offset = 5;
				if (offset == pbin->cb) {
					return 1;
				}
				continue;
			} else {
				return 0;
			}
		}
		read_len = read(sockd, pbin->pb + offset, pbin->cb - offset);
		if (read_len <= 0) {
			return 0;
		}
		offset += read_len;
		if (offset == pbin->cb) {
			return 1;
		}
	}
}

static zend_bool adac_client_write_socket(int sockd, const BINARY *pbin)
{
	int written_len;
	uint32_t offset;
	
	offset = 0;
	while (1) {
		written_len = write(sockd, pbin->pb + offset, pbin->cb - offset);
		if (written_len <= 0) {
			return 0;
		}
		offset += written_len;
		if (offset == pbin->cb) {
			return 1;
		}
	}
}

static zend_bool adac_client_do_rpc(
	const RPC_REQUEST *prequest, RPC_RESPONSE *presponse)
{
	int sockd;
	BINARY tmp_bin;
	
	if (!rpc_ext_push_request(prequest, &tmp_bin)) {
		return 0;
	}
	sockd = adac_client_connect();
	if (sockd < 0) {
		efree(tmp_bin.pb);
		return 0;
	}
	if (!adac_client_write_socket(sockd, &tmp_bin)) {
		efree(tmp_bin.pb);
		close(sockd);
		return 0;
	}
	efree(tmp_bin.pb);
	if (!adac_client_read_socket(sockd, &tmp_bin)) {
		close(sockd);
		return 0;
	}
	close(sockd);
	if (tmp_bin.cb < 5 || RESPONSE_CODE_SUCCESS != tmp_bin.pb[0]) {
		if (NULL != tmp_bin.pb) {
			efree(tmp_bin.pb);
		}
		return 0;
	}
	presponse->call_id = prequest->call_id;
	tmp_bin.cb -= 5;
	tmp_bin.pb += 5;
	if (!rpc_ext_pull_response(&tmp_bin, presponse)) {
		efree(tmp_bin.pb - 5);
		return 0;
	}
	efree(tmp_bin.pb - 5);
	return 1;
}

uint32_t adac_client_getaddressbook(
	const char *address, char *ab_address)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_GETADDRESSBOOK;
	request.address = address;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		strcpy(ab_address, response.payload.getaddressbook.pab_address);
	}
	return response.result;
}

uint32_t adac_client_getabhierarchy(
	const char *ab_address, DAC_VSET *pset)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_GETABHIERARCHY;
	request.address = ab_address;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pset = response.payload.getabhierarchy.set;
	}
	return response.result;
}

uint32_t adac_client_getabcontent(const char *folder_address,
	uint32_t start_pos, uint16_t count, DAC_VSET *pset)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_GETABCONTENT;
	request.address = folder_address;
	request.payload.getabcontent.start_pos = start_pos;
	request.payload.getabcontent.count = count;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pset = response.payload.getabcontent.set;
	}
	return response.result;
}

uint32_t adac_client_restrictabcontent(const char *folder_address,
	const DAC_RES *prestrict, uint16_t max_count, DAC_VSET *pset)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_RESTRICTABCONTENT;
	request.address = folder_address;
	request.payload.restrictabcontent.prestrict = (void*)prestrict;
	request.payload.restrictabcontent.max_count = max_count;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pset = response.payload.restrictabcontent.set;
	}
	return response.result;
}

uint32_t adac_client_getexaddress(
	const char *address, char *ex_address)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_GETEXADDRESS;
	request.address = address;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		strcpy(ex_address, response.payload.getexaddress.pex_address);
	}
	return response.result;
}

uint32_t adac_client_getexaddresstype(
	const char *address, uint8_t *paddress_type)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_GETEXADDRESSTYPE;
	request.address = address;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*paddress_type = response.payload.getexaddresstype.address_type;
	}
	return response.result;
}

uint32_t adac_processor_checkmlistinclude(const char *mlist_address,
	const char *account_address, zend_bool *pb_include)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_CHECKMLISTINCLUDE;
	request.address = mlist_address;
	request.payload.checkmlistinclude.account_address =
								(void*)account_address;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pb_include = response.payload.checkmlistinclude.b_include;
	}
	return response.result;
}

uint32_t adac_client_allocdir(uint8_t type, char *dir)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_ALLOCDIR;
	request.address = NULL;
	request.payload.allocdir.type = type;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		strcpy(dir, response.payload.allocdir.pdir);
	}
	return response.result;
}

uint32_t adac_client_freedir(const char *address)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_FREEDIR;
	request.address = address;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t adac_client_setpropvals(const char *address,
	const char *pnamespace, const DAC_VARRAY *ppropvals)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_SETPROPVALS;
	request.address = address;
	request.payload.setpropvals.pnamespace = (void*)pnamespace;
	request.payload.setpropvals.propvals = *ppropvals;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t adac_client_getpropvals(const char *address,
	const char *pnamespace, const STRING_ARRAY *pnames,
	DAC_VARRAY *ppropvals)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_GETPROPVALS;
	request.address = address;
	request.payload.getpropvals.pnamespace = (void*)pnamespace;
	request.payload.getpropvals.names = *pnames;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*ppropvals = response.payload.getpropvals.propvals;
	}
	return response.result;
}

uint32_t adac_client_deletepropvals(const char *address,
	const char *pnamespace, const STRING_ARRAY *pnames)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_DELETEPROPVALS;
	request.address = address;
	request.payload.deletepropvals.pnamespace = (void*)pnamespace;
	request.payload.deletepropvals.names = *pnames;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t adac_client_opentable(const char *address,
	const char *pnamespace, const char *ptablename,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_OPENTABLE;
	request.address = address;
	request.payload.opentable.pnamespace = (void*)pnamespace;
	request.payload.opentable.ptablename = (void*)ptablename;
	request.payload.opentable.flags = flags;
	request.payload.opentable.puniquename = (void*)puniquename;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*ptable_id = response.payload.opentable.table_id;
	}
	return response.result;
}

uint32_t adac_client_opencelltable(const char *address,
	uint64_t row_instance, const char *pcolname,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_OPENCELLTABLE;
	request.address = address;
	request.payload.opencelltable.row_instance = row_instance;
	request.payload.opencelltable.pcolname = (void*)pcolname;
	request.payload.opencelltable.flags = flags;
	request.payload.opencelltable.puniquename = (void*)puniquename;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*ptable_id = response.payload.opentable.table_id;
	}
	return response.result;
}

uint32_t adac_client_restricttable(const char *address,
	uint32_t table_id, const DAC_RES *prestrict)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_RESTRICTTABLE;
	request.address = address;
	request.payload.restricttable.table_id = table_id;
	request.payload.restricttable.prestrict = (void*)prestrict;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t adac_client_updatecells(const char *address,
	uint64_t row_instance, const DAC_VARRAY *pcells)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_UPDATECELLS;
	request.address = address;
	request.payload.updatecells.row_instance = row_instance;
	request.payload.updatecells.cells = *pcells;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t adac_client_sumtable(const char *address,
	uint32_t table_id, uint32_t *pcount)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_SUMTABLE;
	request.address = address;
	request.payload.sumtable.table_id = table_id;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pcount = response.payload.sumtable.count;
	}
	return response.result;	
}

uint32_t adac_client_queryrows(const char *address,
	uint32_t table_id, uint32_t start_pos,
	uint32_t row_count, DAC_VSET *pset)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_QUERYROWS;
	request.address = address;
	request.payload.queryrows.table_id = table_id;
	request.payload.queryrows.start_pos = start_pos;
	request.payload.queryrows.row_count = row_count;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pset = response.payload.queryrows.set;
	}
	return response.result;
}

uint32_t adac_client_insertrows(const char *address,
	uint32_t table_id, uint32_t flags, const DAC_VSET *pset)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_INSERTROWS;
	request.address = address;
	request.payload.insertrows.table_id = table_id;
	request.payload.insertrows.flags = flags;
	request.payload.insertrows.set = *pset;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}
	
uint32_t adac_client_deleterow(const char *address, uint64_t row_instance)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_DELETEROW;
	request.address = address;
	request.payload.deleterow.row_instance = row_instance;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t adac_client_deletecells(const char *address,
	uint64_t row_instance, const STRING_ARRAY *pcolnames)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_DELETECELLS;
	request.address = address;
	request.payload.deletecells.row_instance = row_instance;
	request.payload.deletecells.colnames = *pcolnames;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t adac_client_deletetable(const char *address,
	const char *pnamespace, const char *ptablename)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_DELETETABLE;
	request.address = address;
	request.payload.deletetable.pnamespace = (void*)pnamespace;
	request.payload.deletetable.ptablename = (void*)ptablename;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t adac_client_closetable(const char *address, uint32_t table_id)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_CLOSETABLE;
	request.address = address;
	request.payload.closetable.table_id = table_id;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t adac_client_matchrow(const char *address,
	uint32_t table_id, uint16_t type,
	const void *pvalue, DAC_VARRAY *prow)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_MATCHROW;
	request.address = address;
	request.payload.matchrow.table_id = table_id;
	request.payload.matchrow.type = type;
	request.payload.matchrow.pvalue = (void*)pvalue;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*prow = response.payload.matchrow.row;
	}
	return response.result;
}

uint32_t adac_client_getnamespaces(const char *address,
	STRING_ARRAY *pnamespaces)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_GETNAMESPACES;
	request.address = address;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pnamespaces = response.payload.getnamespaces.namespaces;
	}
	return response.result;
}

uint32_t adac_client_getpropnames(const char *address,
	const char *pnamespace, STRING_ARRAY *ppropnames)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_GETPROPNAMES;
	request.address = address;
	request.payload.getpropnames.pnamespace = (void*)pnamespace;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*ppropnames = response.payload.getpropnames.propnames;
	}
	return response.result;
}

uint32_t adac_client_gettablenames(const char *address,
	const char *pnamespace, STRING_ARRAY *ptablenames)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_GETTABLENAMES;
	request.address = address;
	request.payload.gettablenames.pnamespace = (void*)pnamespace;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*ptablenames = response.payload.gettablenames.tablenames;
	}
	return response.result;
}

uint32_t adac_client_readfile(const char *address, const char *path,
	const char *file_name, uint64_t offset, uint32_t length,
	BINARY *pcontent_bin)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_READFILE;
	request.address = address;
	request.payload.readfile.path = (void*)path;
	request.payload.readfile.file_name = (void*)file_name;
	request.payload.readfile.offset = offset;
	request.payload.readfile.length = length;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pcontent_bin = response.payload.readfile.content_bin;
	}
	return response.result;
}

uint32_t adac_client_appendfile(const char *address,
	const char *path, const char *file_name,
	const BINARY *pcontent_bin)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_APPENDFILE;
	request.address = address;
	request.payload.appendfile.path = (void*)path;
	request.payload.appendfile.file_name = (void*)file_name;
	request.payload.appendfile.content_bin = *pcontent_bin;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t adac_client_removefile(const char *address,
	const char *path, const char *file_name)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_REMOVEFILE;
	request.address = address;
	request.payload.removefile.path = (void*)path;
	request.payload.removefile.file_name = (void*)file_name;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	return response.result;
}

uint32_t adac_client_statfile(const char *address,
	const char *path, const char *file_name,
	uint64_t *psize)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_STATFILE;
	request.address = address;
	request.payload.statfile.path = (void*)path;
	request.payload.statfile.file_name = (void*)file_name;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*psize = response.payload.statfile.size;
	}
	return response.result;
}

uint32_t adac_client_phprpc(const char *address,
	const char *script_path, const BINARY *pbin_in,
	BINARY *pbin_out)
{
	RPC_REQUEST request;
	RPC_RESPONSE response;
	
	request.call_id = CALL_ID_PHPRPC;
	request.address = address;
	request.payload.phprpc.script_path = (void*)script_path;
	request.payload.phprpc.bin_in = *pbin_in;
	if (!adac_client_do_rpc(&request, &response)) {
		return EC_RPC_FAIL;
	}
	if (EC_SUCCESS == response.result) {
		*pbin_out = response.payload.phprpc.bin_out;
	}
	return response.result;
}
