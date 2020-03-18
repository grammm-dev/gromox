#ifndef _H_ADAC_PROCESSOR_
#define _H_ADAC_PROCESSOR_
#include "common_util.h"

#define OPEN_TABLE_FLAG_CREATE					0x00000001

#define INSERT_FLAG_REPLACE						0x00000001

void adac_processor_init();

int adac_processor_run();

int adac_processor_stop();

void adac_processor_free();

uint32_t adac_processor_getaddressbook(
	const char *address, char *ab_address);

uint32_t adac_processor_getabhierarchy(
	const char *ab_address, DAC_VSET *pset);

uint32_t adac_processor_getabcontent(const char *folder_address,
	uint32_t start_pos, uint16_t count, DAC_VSET *pset);

uint32_t adac_processor_restrictabcontent(const char *folder_address,
	const DAC_RES *prestrict, uint16_t max_count, DAC_VSET *pset);

uint32_t adac_processor_getexaddress(
	const char *address, char *ex_address);

uint32_t adac_processor_getexaddresstype(
	const char *address, uint8_t *paddress_type);

uint32_t adac_processor_checkmlistinclude(const char *mlist_address,
	const char *account_address, BOOL *pb_include);

uint32_t adac_processor_allocdir(uint8_t type, char *dir);

uint32_t adac_processor_freedir(const char *address);

uint32_t adac_processor_setpropvals(const char *address,
	const char *pnamespace, const DAC_VARRAY *ppropvals);

uint32_t adac_processor_getpropvals(const char *address,
	const char *pnamespace, const STRING_ARRAY *pnames,
	DAC_VARRAY *ppropvals);

uint32_t adac_processor_deletepropvals(const char *address,
	const char *pnamespace, const STRING_ARRAY *pnames);

uint32_t adac_processor_opentable(const char *address,
	const char *pnamespace, const char *ptablename,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id);

uint32_t adac_processor_opencelltable(const char *address,
	uint64_t row_instance, const char *pcolname,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id);

uint32_t adac_processor_restricttable(const char *address,
	uint32_t table_id, const DAC_RES *prestrict);

uint32_t adac_processor_updatecells(const char *address,
	uint64_t row_instance, const DAC_VARRAY *pcells);

uint32_t adac_processor_sumtable(const char *address,
	uint32_t table_id, uint32_t *pcount);

uint32_t adac_processor_queryrows(const char *address,
	uint32_t table_id, uint32_t start_pos,
	uint32_t row_count, DAC_VSET *pset);

uint32_t adac_processor_insertrows(const char *address,
	uint32_t table_id, uint32_t flags, const DAC_VSET *pset);
	
uint32_t adac_processor_deleterow(
	const char *address, uint64_t row_instance);

uint32_t adac_processor_deletecells(const char *address,
	uint64_t row_instance, const STRING_ARRAY *pcolnames);

uint32_t adac_processor_deletetable(const char *address,
	const char *pnamespace, const char *ptablename);

uint32_t adac_processor_closetable(
	const char *address, uint32_t table_id);

uint32_t adac_processor_matchrow(const char *address,
	uint32_t table_id, uint16_t type,
	const void *pvalue, DAC_VARRAY *prow);

uint32_t adac_processor_getnamespaces(const char *address,
	STRING_ARRAY *pnamespaces);

uint32_t adac_processor_getpropnames(const char *address,
	const char *pnamespace, STRING_ARRAY *ppropnames);

uint32_t adac_processor_gettablenames(const char *address,
	const char *pnamespace, STRING_ARRAY *ptablenames);

uint32_t adac_processor_readfile(const char *address, const char *path,
	const char *file_name, uint64_t offset, uint32_t length,
	BINARY *pcontent_bin);

uint32_t adac_processor_appendfile(const char *address,
	const char *path, const char *file_name,
	const BINARY *pcontent_bin);

uint32_t adac_processor_removefile(const char *address,
	const char *path, const char *file_name);

uint32_t adac_processor_statfile(const char *address,
	const char *path, const char *file_name,
	uint64_t *psize);

uint32_t adac_processor_checkrow(const char *dir,
	const char *pnamespace, const char *ptablename,
	uint16_t type, const void *pvalue);

uint32_t adac_processor_phprpc(const char *address,
	const char *script_path, const BINARY *pbin_in,
	BINARY *pbin_out);

#endif /* _H_ADAC_PROCESSOR_ */
