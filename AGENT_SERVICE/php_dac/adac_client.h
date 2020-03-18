#ifndef _H_ADAC_CLIENT_
#define _H_ADAC_CLIENT_
#include "php.h"
#include "types.h"

uint32_t adac_client_getaddressbook(
	const char *address, char *ab_address);

uint32_t adac_client_getabhierarchy(
	const char *ab_address, DAC_VSET *pset);

uint32_t adac_client_getabcontent(const char *folder_address,
	uint32_t start_pos, uint16_t count, DAC_VSET *pset);

uint32_t adac_client_restrictabcontent(const char *folder_address,
	const DAC_RES *prestrict, uint16_t max_count, DAC_VSET *pset);

uint32_t adac_client_getexaddress(
	const char *address, char *ex_address);

uint32_t adac_client_getexaddresstype(
	const char *address, uint8_t *paddress_type);

uint32_t adac_processor_checkmlistinclude(const char *mlist_address,
	const char *account_address, zend_bool *pb_include);

uint32_t adac_client_allocdir(uint8_t type, char *dir);

uint32_t adac_client_freedir(const char *address);

uint32_t adac_client_setpropvals(const char *address,
	const char *pnamespace, const DAC_VARRAY *ppropvals);

uint32_t adac_client_getpropvals(const char *address,
	const char *pnamespace, const STRING_ARRAY *pnames,
	DAC_VARRAY *ppropvals);

uint32_t adac_client_deletepropvals(const char *address,
	const char *pnamespace, const STRING_ARRAY *pnames);

uint32_t adac_client_opentable(const char *address,
	const char *pnamespace, const char *ptablename,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id);

uint32_t adac_client_opencelltable(const char *address,
	uint64_t row_instance, const char *pcolname,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id);

uint32_t adac_client_restricttable(const char *address,
	uint32_t table_id, const DAC_RES *prestrict);

uint32_t adac_client_updatecells(const char *address,
	uint64_t row_instance, const DAC_VARRAY *pcells);

uint32_t adac_client_sumtable(const char *address,
	uint32_t table_id, uint32_t *pcount);

uint32_t adac_client_queryrows(const char *address,
	uint32_t table_id, uint32_t start_pos,
	uint32_t row_count, DAC_VSET *pset);

uint32_t adac_client_insertrows(const char *address,
	uint32_t table_id, uint32_t flags, const DAC_VSET *pset);
	
uint32_t adac_client_deleterow(const char *address, uint64_t row_instance);

uint32_t adac_client_deletecells(const char *address,
	uint64_t row_instance, const STRING_ARRAY *pcolnames);

uint32_t adac_client_deletetable(const char *address,
	const char *pnamespace, const char *ptablename);

uint32_t adac_client_closetable(const char *address, uint32_t table_id);

uint32_t adac_client_matchrow(const char *address,
	uint32_t table_id, uint16_t type,
	const void *pvalue, DAC_VARRAY *prow);

uint32_t adac_client_getnamespaces(const char *address,
	STRING_ARRAY *pnamespaces);

uint32_t adac_client_getpropnames(const char *address,
	const char *pnamespace, STRING_ARRAY *ppropnames);

uint32_t adac_client_gettablenames(const char *address,
	const char *pnamespace, STRING_ARRAY *ptablenames);

uint32_t adac_client_readfile(const char *address, const char *path,
	const char *file_name, uint64_t offset, uint32_t length,
	BINARY *pcontent_bin);

uint32_t adac_client_appendfile(const char *address,
	const char *path, const char *file_name,
	const BINARY *pcontent_bin);

uint32_t adac_client_removefile(const char *address,
	const char *path, const char *file_name);

uint32_t adac_client_statfile(const char *address,
	const char *path, const char *file_name,
	uint64_t *psize);
	
uint32_t adac_client_phprpc(const char *address,
	const char *script_path, const BINARY *pbin_in,
	BINARY *pbin_out);

#endif /* _H_ADAC_CLIENT_ */
