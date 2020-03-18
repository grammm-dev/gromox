#ifndef _H_DAC_CLIENT_
#define _H_DAC_CLIENT_
#include "common_util.h"
#include "element_data.h"

#define OPEN_TABLE_FLAG_CREATE					0x00000001

#define INSERT_FLAG_REPLACE						0x00000001	

void dac_client_init(int conn_num, const char *list_path);

int dac_client_run();

int dac_client_stop();

void dac_client_free();

uint32_t dac_client_allocdir(uint8_t type, char *dir);

uint32_t dac_client_freedir(const char *dir);

uint32_t dac_client_setpropvals(const char *dir,
	const char *pnamespace, const DAC_VARRAY *ppropvals);

uint32_t dac_client_getpropvals(const char *dir,
	const char *pnamespace, const STRING_ARRAY *pnames,
	DAC_VARRAY *ppropvals);

uint32_t dac_client_deletepropvals(const char *dir,
	const char *pnamespace, const STRING_ARRAY *pnames);

uint32_t dac_client_opentable(const char *dir,
	const char *pnamespace, const char *ptablename,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id);

uint32_t dac_client_opencelltable(const char *dir,
	uint64_t row_instance, const char *pcolname,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id);

uint32_t dac_client_restricttable(const char *dir,
	uint32_t table_id, const DAC_RES *prestrict);

uint32_t dac_client_updatecells(const char *dir,
	uint64_t row_instance, const DAC_VARRAY *pcells);

uint32_t dac_client_sumtable(const char *dir,
	uint32_t table_id, uint32_t *pcount);

uint32_t dac_client_queryrows(const char *dir,
	uint32_t table_id, uint32_t start_pos,
	uint32_t row_count, DAC_VSET *pset);

uint32_t dac_client_insertrows(const char *dir,
	uint32_t table_id, uint32_t flags, const DAC_VSET *pset);
	
uint32_t dac_client_deleterow(const char *dir, uint64_t row_instance);

uint32_t dac_client_deletecells(const char *dir,
	uint64_t row_instance, const STRING_ARRAY *pcolnames);

uint32_t dac_client_deletetable(const char *dir,
	const char *pnamespace, const char *ptablename);

uint32_t dac_client_closetable(
	const char *dir, uint32_t table_id);

uint32_t dac_client_matchrow(const char *dir,
	uint32_t table_id, uint16_t type,
	const void *pvalue, DAC_VARRAY *prow);

uint32_t dac_client_getnamespaces(const char *dir,
	STRING_ARRAY *pnamespaces);

uint32_t dac_client_getpropnames(const char *dir,
	const char *pnamespace, STRING_ARRAY *ppropnames);

uint32_t dac_client_gettablenames(const char *dir,
	const char *pnamespace, STRING_ARRAY *ptablenames);

uint32_t dac_client_readfile(const char *dir, const char *path,
	const char *file_name, uint64_t offset, uint32_t length,
	BINARY *pcontent_bin);

uint32_t dac_client_appendfile(const char *dir,
	const char *path, const char *file_name,
	const BINARY *pcontent_bin);

uint32_t dac_client_removefile(const char *dir,
	const char *path, const char *file_name);

uint32_t dac_client_statfile(const char *dir,
	const char *path, const char *file_name,
	uint64_t *psize);

uint32_t dac_client_checkrow(const char *dir,
	const char *pnamespace, const char *ptablename,
	uint16_t type, const void *pvalue);

uint32_t dac_client_phprpc(const char *dir, const char *account,
	const char *script_path, const BINARY *pbin_in, BINARY *pbin_out);

#endif /* _H_DAC_CLIENT_ */
