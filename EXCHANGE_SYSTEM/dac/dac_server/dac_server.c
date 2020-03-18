#include "system_services.h"
#include "dac_server.h"
#include "ext_buffer.h"
#include "list_file.h"
#include "guid.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

#define DAC_DIR_TYPE_SIMPLE					0
#define DAC_DIR_TYPE_PRIVATE				1
#define DAC_DIR_TYPE_PUBLIC					2


typedef struct _MAP_ITEM {
	char namespace[256];
	char propname[256];
	char path[256];
} MAP_ITEM;

static int g_item_num;
static MAP_ITEM *g_map_items;
static char g_list_path[256];
static LIST_FILE *g_file_list;


static const char* dac_server_get_map_path(
	const char *namespace, const char *propname)
{
	int i;
	
	for (i=0; i<g_item_num; i++) {
		if (0 == strcasecmp(g_map_items[i].namespace, namespace)
			&& 0 == strcasecmp(g_map_items[i].propname, propname)) {
			return g_map_items[i].path;
		}
	}
	return NULL;
}

void dac_server_init(const char *list_path)
{
	strcpy(g_list_path, list_path);
}

int dac_server_run()
{
	g_file_list = list_file_init(g_list_path, "%s:256%s:256%s:256");
	if (NULL == g_file_list) {
		return -1;
	}
	g_item_num = list_file_get_item_num(g_file_list);
	g_map_items = list_file_get_list(g_file_list);
	return 0;
}

int dac_server_stop()
{
	list_file_free(g_file_list);
	return 0;
}

void dac_server_free()
{
	/* do onthing */
}

uint32_t dac_server_infostorage(uint64_t *ptotal_space,
	uint64_t *ptotal_used, uint32_t *ptotal_dir)
{
	
	
}

uint32_t dac_server_allocdir(uint8_t type, char *dir)
{
	
	
	
}

uint32_t dac_server_freedir(const char *dir)
{
	
	
	
}

uint32_t dac_server_setpropvals(const char *dir,
	const char *pnamespace, const DAC_VARRAY *ppropvals)
{
	int i, fd;
	void *pvalue;
	DIR_ITEM *pditem;
	const char *ppath;
	char tmp_path[256];
	char file_name[64];
	sqlite3_stmt *pstmt;
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_exec(pditem->psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"REPLACE INTO propvals (namespace, propname,"
		" type, propval) VALUES (?, ?, ?, ?)", -1,
		&pstmt, NULL)) {
		sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	for (i=0; i<ppropvals->count; i++) {
		pvalue = ppropvals->ppropval[i].pvalue;
		if (PROPVAL_TYPE_BINARY == ppropvals->ppropval[i].type &&
			NULL != (ppath = dac_server_get_map_path(pnamespace,
			ppropvals->ppropval[i].pname))) {
			sprintf(tmp_path, "%s/%s", dir, ppath);
			fd = open(tmp_path, O_CREAT|O_TRUNC|O_WRONLY, 0666);
			if (-1 == fd) {
				sqlite3_finalize(pstmt);
				sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
				common_util_put_dir_item(pditem);
				return EC_ERROR;
			}
			if (((BINARY*)pvalue)->cb != write(
				fd, ((BINARY*)pvalue)->pb,
				((BINARY*)pvalue)->cb)) {
				close(fd);
				remove(tmp_path);
				sqlite3_finalize(pstmt);
				sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
				common_util_put_dir_item(pditem);
				return EC_ERROR;
			}
			close(fd);
			continue;
		}
		sqlite3_reset(pstmt);
		sqlite3_bind_text(pstmt, 1, pnamespace, -1, SQLITE_STATIC);
		sqlite3_bind_text(pstmt, 2, ppropvals->ppropval[i].pname,
			-1, SQLITE_STATIC);
		sqlite3_bind_int64(pstmt, 3, ppropvals->ppropval[i].type);
		if (PROPVAL_TYPE_BINARY == ppropvals->ppropval[i].type) {
			sprintf(file_name, "%lu.%u.dac", time(NULL),
					common_util_get_sequence_id());
			sprintf(tmp_path, "%s/dac/%s", dir, file_name);
			fd = open(tmp_path, O_CREAT|O_TRUNC|O_WRONLY, 0666);
			if (-1 == fd) {
				sqlite3_finalize(pstmt);
				sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
				common_util_put_dir_item(pditem);
				return EC_ERROR;
			}
			if (((BINARY*)pvalue)->cb != write(
				fd, ((BINARY*)pvalue)->pb,
				((BINARY*)pvalue)->cb)) {
				close(fd);
				remove(tmp_path);
				sqlite3_finalize(pstmt);
				sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
				common_util_put_dir_item(pditem);
				return EC_ERROR;
			}
			close(fd);
			sqlite3_bind_text(pstmt, 4, file_name, -1, SQLITE_STATIC);
			if (SQLITE_DONE != sqlite3_step(pstmt)) {
				sqlite3_finalize(pstmt);
				sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
				common_util_put_dir_item(pditem);
				return EC_ERROR;
			}
		} else {
			if (SQLITE_DONE != common_util_bind_value(pstmt,
				4, ppropvals->ppropval[i].type, pvalue)) {
				sqlite3_finalize(pstmt);
				sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
				common_util_put_dir_item(pditem);
				return EC_ERROR;	
			}
		}
	}
	sqlite3_exec(pditem->psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	sqlite3_finalize(pstmt);
	common_util_put_dir_item(pditem);
	return EC_SUCCESS;
}

uint32_t dac_server_getpropvals(const char *dir,
	const char *pnamespace, const STRING_ARRAY *pnames,
	DAC_VARRAY *ppropvals)
{
	int i;
	void *pvalue;
	DIR_ITEM *pditem;
	const char *ppath;
	char tmp_path[256];
	sqlite3_stmt *pstmt;
	struct stat node_stat;
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	ppropvals->count = 0;
	ppropvals->ppropval = common_util_alloc(
		sizeof(DAC_PROPVAL)*pnames->count);
	if (NULL == ppropvals->ppropval) {
		return EC_OUT_OF_MEMORY;
	}
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_exec(pditem->psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT propval, type FROM propvals WHERE"
		" namespace=? AND propname=?", -1, &pstmt, NULL)) {
		sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	for (i=0; i<pnames->count; i++) {
		if (NULL != (ppath = dac_server_get_map_path(
			pnamespace, pnames->ppstr[i]))) {
			sprintf(tmp_path, "%s/%s", dir, ppath);
			if (0 != stat(tmp_path, &node_stat)) {
				continue;
			}
			pvalue = common_util_alloc(sizeof(BINARY));
			if (NULL == pvalue || FALSE == common_util_load_file(
				tmp_path, pvalue)) {
				sqlite3_finalize(pstmt);
				sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
				common_util_put_dir_item(pditem);
				return EC_OUT_OF_MEMORY;
			}
			ppropvals->ppropval[ppropvals->count].pname = pnames->ppstr[i];
			ppropvals->ppropval[ppropvals->count].type = PROPVAL_TYPE_BINARY;
			ppropvals->ppropval[ppropvals->count].pvalue = pvalue;
			ppropvals->count ++;
			continue;
		}
		sqlite3_reset(pstmt);
		sqlite3_bind_text(pstmt, 1, pnamespace, -1, SQLITE_STATIC);
		sqlite3_bind_text(pstmt, 2, pnames->ppstr[i], -1, SQLITE_STATIC);
		if (SQLITE_ROW != sqlite3_step(pstmt)) {
			continue;
		}
		ppropvals->ppropval[ppropvals->count].pname =
									pnames->ppstr[i];
		ppropvals->ppropval[ppropvals->count].type =
						sqlite3_column_int64(pstmt, 1);
		if (PROPVAL_TYPE_BINARY ==
			ppropvals->ppropval[ppropvals->count].type) {
			sprintf(tmp_path, "%s/dac/%s", dir,
				sqlite3_column_text(pstmt, 0));
			if (0 != stat(tmp_path, &node_stat)) {
				continue;
			}
			pvalue = common_util_alloc(sizeof(BINARY));
			if (NULL == pvalue || FALSE == common_util_load_file(
				tmp_path, pvalue)) {
				sqlite3_finalize(pstmt);
				sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
				common_util_put_dir_item(pditem);
				return EC_OUT_OF_MEMORY;
			}
			ppropvals->ppropval[ppropvals->count].pvalue = pvalue;
			ppropvals->count ++;
			continue;
		}
		pvalue = common_util_column_value(pstmt, 0,
			ppropvals->ppropval[ppropvals->count].type);
		if (NULL == pvalue) {
			sqlite3_finalize(pstmt);
			sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
			common_util_put_dir_item(pditem);
			return EC_OUT_OF_MEMORY;
		}
		ppropvals->ppropval[ppropvals->count].pvalue = pvalue;
		ppropvals->count ++;
	}
	sqlite3_exec(pditem->psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	sqlite3_finalize(pstmt);
	common_util_put_dir_item(pditem);
	return EC_SUCCESS;
}

uint32_t dac_server_deletepropvals(const char *dir,
	const char *pnamespace, const STRING_ARRAY *pnames)
{
	int i;
	DIR_ITEM *pditem;
	sqlite3_stmt *pstmt;
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_exec(pditem->psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"DELETE FROM propvals WHERE namespace=?"
		" AND propname=?", -1, &pstmt, NULL)) {
		sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	for (i=0; i<pnames->count; i++) {
		sqlite3_reset(pstmt);
		sqlite3_bind_text(pstmt, 1, pnamespace, -1, SQLITE_STATIC);
		sqlite3_bind_text(pstmt, 2, pnames->ppstr[i], -1, SQLITE_STATIC);
		sqlite3_step(pstmt);
	}
	sqlite3_exec(pditem->psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	sqlite3_finalize(pstmt);
	common_util_put_dir_item(pditem);
	return EC_SUCCESS;
}

uint32_t dac_server_opentable(const char *dir,
	const char *pnamespace, const char *ptablename,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id)
{
	DIR_ITEM *pditem;
	uint32_t table_id;
	uint64_t object_id;
	TABLE_NODE *ptnode;
	uint16_t valid_days;
	sqlite3_stmt *pstmt;
	
	if ('\0' == pnamespace[0] || (NULL !=
		puniquename && '\0' == *puniquename)) {
		return EC_INVALID_PARAMETER;
	}
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT object_id FROM objtbls WHERE"
		" namespace=? AND tablename=?", -1,
		&pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_bind_text(pstmt, 1, pnamespace, -1, SQLITE_STATIC);
	sqlite3_bind_text(pstmt, 2, ptablename, -1, SQLITE_STATIC);
	if (SQLITE_ROW == sqlite3_step(pstmt)) {
		object_id = sqlite3_column_int64(pstmt, 0);
	} else {
		sqlite3_finalize(pstmt);
		if (0 == (flags & OPEN_TABLE_FLAG_CREATE)) {
			common_util_put_dir_item(pditem);
			return EC_NOT_FOUND;
		}
		valid_days = flags >> 16;
		sqlite3_exec(pditem->psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
		if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
			"INSERT INTO objtbls (namespace, tablename, "
			"valid_days, unique_name) VALUES (?, ?, ?, ?)",
			-1, &pstmt, NULL)) {
			common_util_put_dir_item(pditem);
			return EC_ERROR;	
		}
		sqlite3_bind_text(pstmt, 1, pnamespace, -1, SQLITE_STATIC);
		sqlite3_bind_text(pstmt, 2, ptablename, -1, SQLITE_STATIC);
		sqlite3_bind_int64(pstmt, 3, valid_days);
		if (NULL == puniquename) {
			sqlite3_bind_null(pstmt, 4);
		} else {
			sqlite3_bind_text(pstmt, 4, puniquename, -1, SQLITE_STATIC);
		}
		if (SQLITE_DONE != sqlite3_step(pstmt)) {
			sqlite3_finalize(pstmt);
			sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
		object_id = sqlite3_last_insert_rowid(pditem->psqlite);
		sqlite3_exec(pditem->psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	}
	sqlite3_finalize(pstmt);
	ptnode = malloc(sizeof(TABLE_NODE));
	ptnode->node.pdata = ptnode;
	ptnode->pinstances = NULL;
	ptnode->count = -1;
	ptnode->object_id = object_id;
	time(&ptnode->load_time);
	pditem->last_id ++;
	table_id = pditem->last_id;
	ptnode->table_id = pditem->last_id;
	double_list_append_as_tail(&pditem->table_list, &ptnode->node);
	common_util_put_dir_item(pditem);
	*ptable_id = table_id;
	return EC_SUCCESS;
}

uint32_t dac_server_opencelltable(const char *dir,
	uint64_t row_instance, const char *pcolname,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id)
{
	DIR_ITEM *pditem;
	uint32_t table_id;
	uint64_t object_id;
	TABLE_NODE *ptnode;
	sqlite3_stmt *pstmt;
	uint16_t valid_days;
	
	if (NULL != puniquename && '\0' == *puniquename) {
		return EC_INVALID_PARAMETER;
	}
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT type, value FROM cellvals WHERE "
		"row_id=? AND name=?", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_bind_int64(pstmt, 1, row_instance);
	sqlite3_bind_text(pstmt, 2, pcolname, -1, SQLITE_STATIC);
	if (SQLITE_ROW == sqlite3_step(pstmt)) {
		if (PROPVAL_TYPE_OBJECT != sqlite3_column_int64(pstmt, 0)) {
			common_util_put_dir_item(pditem);
			return EC_NOT_SUPPORTED;
		}
		object_id = sqlite3_column_int64(pstmt, 1);
	} else {
		sqlite3_finalize(pstmt);
		if (0 == (flags & OPEN_TABLE_FLAG_CREATE)) {
			common_util_put_dir_item(pditem);
			return EC_NOT_FOUND;
		}
		valid_days = flags >> 16;
		sqlite3_exec(pditem->psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
		if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
			"INSERT INTO objtbls (valid_days, unique_name)"
			" VALUES (?, ?)", -1, &pstmt, NULL)) {
			common_util_put_dir_item(pditem);
			return EC_ERROR;	
		}
		sqlite3_bind_int64(pstmt, 1, valid_days);
		if (NULL == puniquename) {
			sqlite3_bind_null(pstmt, 2);
		} else {
			sqlite3_bind_text(pstmt, 2, puniquename, -1, SQLITE_STATIC);
		}
		if (SQLITE_DONE != sqlite3_step(pstmt)) {
			sqlite3_finalize(pstmt);
			sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
		sqlite3_finalize(pstmt);
		object_id = sqlite3_last_insert_rowid(pditem->psqlite);
		if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
			"INSERT INTO cellvals (row_id, name, type,"
			" value) VALUES (?, ?, 13, ?)", -1, &pstmt,
			NULL)) {
			sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
			common_util_put_dir_item(pditem);
			return EC_ERROR;	
		}
		sqlite3_bind_int64(pstmt, 1, row_instance);
		sqlite3_bind_text(pstmt, 2, pcolname, -1, SQLITE_STATIC);
		sqlite3_bind_int64(pstmt, 3, object_id);
		if (SQLITE_DONE != sqlite3_step(pstmt)) {
			sqlite3_finalize(pstmt);
			sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
		sqlite3_exec(pditem->psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	}
	sqlite3_finalize(pstmt);
	ptnode = malloc(sizeof(TABLE_NODE));
	ptnode->node.pdata = ptnode;
	ptnode->pinstances = NULL;
	ptnode->count = -1;
	ptnode->object_id = object_id;
	time(&ptnode->load_time);
	pditem->last_id ++;
	table_id = pditem->last_id;
	ptnode->table_id = pditem->last_id;
	double_list_append_as_tail(&pditem->table_list, &ptnode->node);
	common_util_put_dir_item(pditem);
	*ptable_id = table_id;
	return EC_SUCCESS;
}

uint32_t dac_server_restricttable(const char *dir,
	uint32_t table_id, const DAC_RES *prestrict)
{
	int count;
	uint64_t row_id;
	DIR_ITEM *pditem;
	uint64_t object_id;
	TABLE_NODE *ptnode;
	sqlite3_stmt *pstmt;
	sqlite3_stmt *pstmt1;
	DOUBLE_LIST tmp_list;
	uint64_t *pinstances;
	DOUBLE_LIST_NODE *pnode;
	
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	for (pnode=double_list_get_head(&pditem->table_list); NULL!=pnode;
		pnode=double_list_get_after(&pditem->table_list, pnode)) {
		ptnode = (TABLE_NODE*)pnode->pdata;
		if (table_id == ptnode->table_id) {
			break;
		}
	}
	if (NULL == pnode) {
		common_util_put_dir_item(pditem);
		return EC_NOT_FOUND;
	}
	if (-1 != ptnode->count) {
		common_util_put_dir_item(pditem);
		return EC_NOT_SUPPORTED;
	}
	object_id = ptnode->object_id;
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT row_id FROM tblrows WHERE object_id=? "
		"ORDER BY row_id ASC", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (NULL != prestrict) {
		if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
			"SELECT value, type FROM cellvals WHERE row_id=?"
			" AND name=?", -1, &pstmt1, NULL)) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
	} else {
		pstmt1 = NULL;
	}
	double_list_init(&tmp_list);
	sqlite3_bind_int64(pstmt, 1, object_id);
	while (SQLITE_ROW == sqlite3_step(pstmt)) {
		row_id = sqlite3_column_int64(pstmt, 0);
		if (NULL != prestrict && FALSE ==
			common_util_evaluate_restriction(
			pditem->psqlite, pstmt1, row_id,
			prestrict)) {
			continue;	
		}
		pnode = common_util_alloc(sizeof(DOUBLE_LIST_NODE));
		if (NULL == pnode) {
			sqlite3_finalize(pstmt);
			if (NULL != pstmt1) {
				sqlite3_finalize(pstmt1);
			}
			common_util_put_dir_item(pditem);
			return EC_OUT_OF_MEMORY;
		}
		pnode->pdata = common_util_alloc(sizeof(uint64_t));
		if (NULL == pnode->pdata) {
			sqlite3_finalize(pstmt);
			if (NULL != pstmt1) {
				sqlite3_finalize(pstmt1);
			}
			common_util_put_dir_item(pditem);
			return EC_OUT_OF_MEMORY;
		}
		*(uint64_t*)pnode->pdata = row_id;
		double_list_append_as_tail(&tmp_list, pnode);
	}
	sqlite3_finalize(pstmt);
	if (NULL != pstmt1) {
		sqlite3_finalize(pstmt1);
	}
	if (0 == double_list_get_nodes_num(&tmp_list)) {
		pinstances = NULL;
		count = 0;
	} else {
		pinstances = malloc(sizeof(uint64_t)*
			double_list_get_nodes_num(&tmp_list));
		if (NULL == pinstances) {
			return EC_OUT_OF_MEMORY;
		}
		count = 0;
		for (pnode=double_list_get_head(&tmp_list); NULL!=pnode;
			pnode=double_list_get_after(&tmp_list, pnode)) {
			pinstances[count] = *(uint64_t*)pnode->pdata;
			count ++;
		}
	}
	ptnode->pinstances = pinstances;
	ptnode->count = count;
	common_util_put_dir_item(pditem);
	return EC_SUCCESS;
}

uint32_t dac_server_updatecells(const char *dir,
	uint64_t row_instance, const DAC_VARRAY *pcells)
{
	int i;
	DIR_ITEM *pditem;
	uint64_t object_id;
	sqlite3_stmt *pstmt;
	sqlite3_stmt *pstmt1;
	sqlite3_stmt *pstmt2;
	const char *puniquename;
	
	for (i=0; i<pcells->count; i++) {
		if (0 == strcasecmp(pcells->ppropval[i].pname, "instance")) {
			return EC_INVALID_PARAMETER;
		}
		switch (pcells->ppropval[i].type) {
		case PROPVAL_TYPE_BYTE:
		case PROPVAL_TYPE_LONGLONG:
		case PROPVAL_TYPE_DOUBLE:
		case PROPVAL_TYPE_STRING:
		case PROPVAL_TYPE_GUID:
		case PROPVAL_TYPE_LONGLONG_ARRAY:
		case PROPVAL_TYPE_STRING_ARRAY:
			break;
		default:
			return EC_INVALID_PARAMETER;
		}
	}
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT objtbls.unique_name FROM tblrows JOIN "
		"objtbls ON objtbls.object_id=tblrows.object_id"
		" AND tblrows.row_id=?", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_bind_int64(pstmt, 1, row_instance);
	if (SQLITE_ROW != sqlite3_step(pstmt)) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_NOT_FOUND;
	}
	puniquename = sqlite3_column_text(pstmt, 0);
	if (NULL != puniquename) {
		puniquename = common_util_dup(puniquename);
		if (NULL == puniquename) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
	}
	sqlite3_finalize(pstmt);
	sqlite3_exec(pditem->psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"REPLACE INTO cellvals (row_id, name, "
		"type, value) VALUES (?, ?, ?, ?)", -1,
		&pstmt, NULL)) {
		sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT value, type FROM cellvals WHERE "
		"row_id=? AND name=?", -1, &pstmt1, NULL)) {
		sqlite3_finalize(pstmt);
		sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	for (i=0; i<pcells->count; i++) {
		sqlite3_reset(pstmt1);
		sqlite3_bind_int64(pstmt1, 1, row_instance);
		sqlite3_bind_text(pstmt1, 2,
			pcells->ppropval[i].pname, -1, SQLITE_STATIC);
		if (SQLITE_ROW == sqlite3_step(pstmt1)) {
			if (PROPVAL_TYPE_OBJECT == sqlite3_column_int64(pstmt1, 1)) {
				if (FALSE == common_util_delete_table(pditem->psqlite,
					sqlite3_column_int64(pstmt1, 0))) {
					sqlite3_finalize(pstmt);
					sqlite3_finalize(pstmt1);
					sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
					common_util_put_dir_item(pditem);
					return EC_ERROR;
				}
			}
		}
		sqlite3_reset(pstmt);
		sqlite3_bind_int64(pstmt, 1, row_instance);
		sqlite3_bind_text(pstmt, 2,
			pcells->ppropval[i].pname, -1, SQLITE_STATIC);
		sqlite3_bind_int64(pstmt, 3, pcells->ppropval[i].type);
		if (SQLITE_DONE != common_util_bind_value(
			pstmt, 4, pcells->ppropval[i].type,
			pcells->ppropval[i].pvalue)) {
			sqlite3_finalize(pstmt);
			sqlite3_finalize(pstmt1);
			sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
			common_util_put_dir_item(pditem);
			return EC_ERROR;	
		}
		if (NULL != puniquename && 0 == strcasecmp(
			puniquename, pcells->ppropval[i].pname)) {
			if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
				"UPDATE tblrows SET unique_val=? WHERE "
				"row_id=?", -1, &pstmt2, NULL)) {
				sqlite3_finalize(pstmt);
				sqlite3_finalize(pstmt1);
				sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
				common_util_put_dir_item(pditem);
				return EC_ERROR;
			}
			sqlite3_bind_int64(pstmt2, 2, row_instance);
			if (SQLITE_DONE != common_util_bind_value(
				pstmt2, 1, pcells->ppropval[i].type,
				pcells->ppropval[i].pvalue)) {
				sqlite3_finalize(pstmt);
				sqlite3_finalize(pstmt1);
				sqlite3_finalize(pstmt2);
				sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
				common_util_put_dir_item(pditem);
				return EC_ERROR;	
			}
			sqlite3_finalize(pstmt2);
		}
	}
	sqlite3_exec(pditem->psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	sqlite3_finalize(pstmt);
	sqlite3_finalize(pstmt1);
	common_util_put_dir_item(pditem);
	return EC_SUCCESS;
}

uint32_t dac_server_sumtable(const char *dir,
	uint32_t table_id, uint32_t *pcount)
{
	DIR_ITEM *pditem;
	uint64_t object_id;
	TABLE_NODE *ptnode;
	sqlite3_stmt *pstmt;
	DOUBLE_LIST_NODE *pnode;
	
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	for (pnode=double_list_get_head(&pditem->table_list); NULL!=pnode;
		pnode=double_list_get_after(&pditem->table_list, pnode)) {
		ptnode = (TABLE_NODE*)pnode->pdata;
		if (table_id == ptnode->table_id) {
			break;
		}
	}
	if (NULL == pnode) {
		common_util_put_dir_item(pditem);
		return EC_NOT_FOUND;
	}
	object_id = ptnode->object_id;
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT count(row_id) FROM tblrows WHERE"
		" object_id=?", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_bind_int64(pstmt, 1, object_id);
	if (SQLITE_ROW != sqlite3_step(pstmt)) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	*pcount = sqlite3_column_int64(pstmt, 0);
	sqlite3_finalize(pstmt);
	common_util_put_dir_item(pditem);
	return EC_SUCCESS;
}

uint32_t dac_server_queryrows(const char *dir,
	uint32_t table_id, uint32_t start_pos,
	uint32_t row_count, DAC_VSET *pset)
{
	int i, j;
	uint64_t row_id;
	uint32_t result;
	DIR_ITEM *pditem;
	uint64_t object_id;
	TABLE_NODE *ptnode;
	sqlite3_stmt *pstmt;
	uint64_t *pinstances;
	DOUBLE_LIST tmp_list;
	DAC_PROPVAL *ppropval;
	DOUBLE_LIST_NODE *pnode;
	
BEGIN_QUERY:
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	for (pnode=double_list_get_head(&pditem->table_list); NULL!=pnode;
		pnode=double_list_get_after(&pditem->table_list, pnode)) {
		ptnode = (TABLE_NODE*)pnode->pdata;
		if (table_id == ptnode->table_id) {
			break;
		}
	}
	if (NULL == pnode) {
		common_util_put_dir_item(pditem);
		return EC_NOT_FOUND;
	}
	object_id = ptnode->object_id;
	if (-1 == ptnode->count) {
		common_util_put_dir_item(pditem);
		result = dac_server_restricttable(dir, table_id, NULL);
		if (EC_SUCCESS != result) {
			return result;
		}
		goto BEGIN_QUERY;
	}
	if (start_pos >= ptnode->count || 0 == row_count) {
		common_util_put_dir_item(pditem);
		pset->count = 0;
		pset->pparray = NULL;
		return EC_SUCCESS;
	}
	pset->count = ptnode->count - start_pos;
	if (pset->count > row_count) {
		pset->count = row_count;
	}
	pinstances = common_util_alloc(sizeof(uint64_t)*pset->count);
	if (NULL == pinstances) {
		common_util_put_dir_item(pditem);
		return EC_OUT_OF_MEMORY;
	}
	memcpy(pinstances, ptnode->pinstances +
		start_pos, ptnode->count*sizeof(uint64_t));
	pset->pparray = common_util_alloc(sizeof(DAC_VARRAY*)*pset->count);
	if (NULL == pset->pparray) {
		common_util_put_dir_item(pditem);
		return EC_OUT_OF_MEMORY;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT name, type, value FROM cellvals"
		" WHERE row_id=?", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	for (i=0; i<pset->count; i++) {
		double_list_init(&tmp_list);
		pnode = common_util_alloc(sizeof(DOUBLE_LIST_NODE));
		if (NULL == pnode) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_OUT_OF_MEMORY;
		}
		ppropval = common_util_alloc(sizeof(DAC_PROPVAL));
		if (NULL == ppropval) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_OUT_OF_MEMORY;
		}
		pnode->pdata = ppropval;
		sqlite3_reset(pstmt);
		sqlite3_bind_int64(pstmt, 1, pinstances[i]);
		ppropval->pname = "instance";
		ppropval->type = PROPVAL_TYPE_LONGLONG;
		ppropval->pvalue = pinstances + i;
		double_list_append_as_tail(&tmp_list, pnode);
		while (SQLITE_ROW == sqlite3_step(pstmt)) {
			pnode = common_util_alloc(sizeof(DOUBLE_LIST_NODE));
			if (NULL == pnode) {
				sqlite3_finalize(pstmt);
				common_util_put_dir_item(pditem);
				return EC_OUT_OF_MEMORY;
			}
			ppropval = common_util_alloc(sizeof(DAC_PROPVAL));
			if (NULL == ppropval) {
				sqlite3_finalize(pstmt);
				common_util_put_dir_item(pditem);
				return EC_OUT_OF_MEMORY;
			}
			pnode->pdata = ppropval;
			ppropval->pname = common_util_dup(
				sqlite3_column_text(pstmt, 0));
			if (NULL == ppropval->pname) {
				sqlite3_finalize(pstmt);
				common_util_put_dir_item(pditem);
				return EC_OUT_OF_MEMORY;
			}
			ppropval->type = sqlite3_column_int64(pstmt, 1);
			ppropval->pvalue = common_util_column_value(
								pstmt, 2, ppropval->type);
			if (NULL == ppropval->pvalue) {
				sqlite3_finalize(pstmt);
				common_util_put_dir_item(pditem);
				return EC_ERROR;
			}
			double_list_append_as_tail(&tmp_list, pnode);
		}
		pset->pparray[i] = common_util_alloc(sizeof(DAC_VARRAY));
		if (NULL == pset->pparray[i]) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
		pset->pparray[i]->count = double_list_get_nodes_num(&tmp_list);
		pset->pparray[i]->ppropval = common_util_alloc(
			pset->pparray[i]->count*sizeof(DAC_PROPVAL));
		if (NULL == pset->pparray[i]->ppropval) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
		for (j=0; j<pset->pparray[i]->count; j++) {
			pnode = double_list_get_from_head(&tmp_list);
			ppropval = pnode->pdata;
			pset->pparray[i]->ppropval[j] = *ppropval;
		}
	}
	sqlite3_finalize(pstmt);
	common_util_put_dir_item(pditem);
	return EC_SUCCESS;
}

uint32_t dac_server_insertrows(const char *dir,
	uint32_t table_id, uint32_t flags, const DAC_VSET *pset)
{
	int i, j;
	int s_result;
	uint64_t row_id;
	DIR_ITEM *pditem;
	GUID unique_guid;
	uint64_t object_id;
	TABLE_NODE *ptnode;
	sqlite3_stmt *pstmt;
	sqlite3_stmt *pstmt1;
	sqlite3_stmt *pstmt2;
	DAC_PROPVAL *ppropval;
	const char *puniquename;
	DOUBLE_LIST_NODE *pnode;
	
	for (i=0; i<pset->count; i++) {
		for (j=0; j<pset->pparray[i]->count; j++) {
			if (0 == strcasecmp("instance",
				pset->pparray[i]->ppropval[j].pname)) {
				return EC_INVALID_PARAMETER;
			}
			switch (pset->pparray[i]->ppropval[j].type) {
			case PROPVAL_TYPE_BYTE:
			case PROPVAL_TYPE_LONGLONG:
			case PROPVAL_TYPE_DOUBLE:
			case PROPVAL_TYPE_STRING:
			case PROPVAL_TYPE_GUID:
			case PROPVAL_TYPE_LONGLONG_ARRAY:
			case PROPVAL_TYPE_STRING_ARRAY:
				break;
			default:
				return EC_INVALID_PARAMETER;
			}
		}
	}
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	for (pnode=double_list_get_head(&pditem->table_list); NULL!=pnode;
		pnode=double_list_get_after(&pditem->table_list, pnode)) {
		ptnode = (TABLE_NODE*)pnode->pdata;
		if (table_id == ptnode->table_id) {
			break;
		}
	}
	if (NULL == pnode) {
		common_util_put_dir_item(pditem);
		return EC_NOT_FOUND;
	}
	object_id = ptnode->object_id;
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT unique_name FROM objtbls WHERE object_id=?",
		-1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;	
	}
	sqlite3_bind_int64(pstmt, 1, object_id);
	if (SQLITE_ROW != sqlite3_step(pstmt)) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_NOT_FOUND;
	}
	puniquename = sqlite3_column_text(pstmt, 0);
	if (NULL != puniquename) {
		puniquename = common_util_dup(puniquename);
		if (NULL == puniquename) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
	}
	sqlite3_finalize(pstmt);
	if (NULL != puniquename) {
		for (i=0; i<pset->count; i++) {
			for (j=0; j<pset->pparray[i]->count; j++) {
				if (0 == strcasecmp(puniquename,
					pset->pparray[i]->ppropval[j].pname)) {
					break;	
				}
			}
			if (j >= pset->pparray[i]->count) {
				common_util_put_dir_item(pditem);
				return EC_INVALID_PARAMETER;
			}
		}
	}
	sqlite3_exec(pditem->psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"INSERT INTO tblrows (unique_val, object_id)"
		" VALUES (?, ?)", -1, &pstmt, NULL)) {
		sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"INSERT INTO cellvals (row_id, name, type, value)"
		" VALUES (?, ?, ?, ?)", -1, &pstmt1, NULL)) {
		sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (NULL != puniquename) {
		if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
			"SELECT row_id FROM tblrows WHERE object_id=?"
			" AND unique_val=?", -1, &pstmt2, NULL)) {
			sqlite3_finalize(pstmt);
			sqlite3_finalize(pstmt1);
			sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
	}
	for (i=0; i<pset->count; i++) {
		if (NULL != puniquename) {
			ppropval = common_util_get_propvals(
				pset->pparray[i], puniquename);
			sqlite3_reset(pstmt2);
			sqlite3_bind_int64(pstmt2, 1, object_id);
			if (SQLITE_ROW == common_util_bind_value(pstmt2,
				2, ppropval->type, ppropval->pvalue)) {
				if (0 == (flags & INSERT_FLAG_REPLACE)) {
					sqlite3_exec(pditem->psqlite,
						"ROLLBACK", NULL, NULL, NULL);
					sqlite3_finalize(pstmt);
					sqlite3_finalize(pstmt1);
					sqlite3_finalize(pstmt2);
					common_util_put_dir_item(pditem);
					return EC_COLLIDING_NAMES;
				}
				if (FALSE == common_util_delete_row(pditem->psqlite,
					sqlite3_column_int64(pstmt2, 0))) {
					sqlite3_exec(pditem->psqlite,
						"ROLLBACK", NULL, NULL, NULL);
					sqlite3_finalize(pstmt);
					sqlite3_finalize(pstmt1);
					sqlite3_finalize(pstmt2);
					common_util_put_dir_item(pditem);
					return EC_ERROR;
				}
			}
		}
		sqlite3_reset(pstmt);
		sqlite3_bind_int64(pstmt, 2, object_id);
		if (NULL == puniquename) {
			unique_guid = guid_random_new();
			s_result = common_util_bind_value(pstmt,
				1, PROPVAL_TYPE_GUID, &unique_guid);
		} else {
			s_result = common_util_bind_value(pstmt,
				1, ppropval->type, ppropval->pvalue);
		}
		if (SQLITE_DONE != s_result) {
			sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
			sqlite3_finalize(pstmt);
			sqlite3_finalize(pstmt1);
			if (NULL != puniquename) {
				sqlite3_finalize(pstmt2);
			}
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
		row_id = sqlite3_last_insert_rowid(pditem->psqlite);
		for (j=0; j<pset->pparray[i]->count; j++) {
			ppropval = pset->pparray[i]->ppropval + j;
			sqlite3_reset(pstmt1);
			sqlite3_bind_int64(pstmt1, 1, row_id);
			sqlite3_bind_text(pstmt1, 2, ppropval->pname, -1, SQLITE_STATIC);
			sqlite3_bind_int64(pstmt1, 3, ppropval->type);
			if (SQLITE_DONE != common_util_bind_value(pstmt1,
				4, ppropval->type, ppropval->pvalue)) {
				sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
				sqlite3_finalize(pstmt);
				sqlite3_finalize(pstmt1);
				if (NULL != puniquename) {
					sqlite3_finalize(pstmt2);
				}
				common_util_put_dir_item(pditem);
				return EC_ERROR;
			}
		}
	}
	sqlite3_exec(pditem->psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	sqlite3_finalize(pstmt);
	sqlite3_finalize(pstmt1);
	if (NULL != puniquename) {
		sqlite3_finalize(pstmt2);
	}
	common_util_put_dir_item(pditem);
	return EC_SUCCESS;
}

uint32_t dac_server_deleterow(const char *dir, uint64_t row_instance)
{
	DIR_ITEM *pditem;
	
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_exec(pditem->psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if (FALSE == common_util_delete_row(pditem->psqlite, row_instance)) {
		sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_exec(pditem->psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	common_util_put_dir_item(pditem);
	return EC_SUCCESS;
}

uint32_t dac_server_deletecells(const char *dir,
	uint64_t row_instance, const STRING_ARRAY *pcolnames)
{
	int i;
	int s_result;
	DIR_ITEM *pditem;
	uint64_t cell_id;
	sqlite3_stmt *pstmt;
	sqlite3_stmt *pstmt1;
	DAC_PROPVAL *ppropval;
	const char *puniquename;
	
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT objtbls.unique_name FROM tblrows JOIN "
		"objtbls ON objtbls.object_id=tblrows.object_id"
		" AND tblrows.row_id=?", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_bind_int64(pstmt, 1, row_instance);
	if (SQLITE_ROW != sqlite3_step(pstmt)) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_SUCCESS;
	}
	puniquename = sqlite3_column_text(pstmt, 0);
	if (NULL != puniquename) {
		for (i=0; i<pcolnames->count; i++) {
			if (0 == strcasecmp(pcolnames->ppstr[i], puniquename)) {
				sqlite3_finalize(pstmt);
				common_util_put_dir_item(pditem);
				return EC_INVALID_PARAMETER;
			}
		}
	}
	sqlite3_finalize(pstmt);
	sqlite3_exec(pditem->psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT cell_id, value, type FROM cellvals"
		" WHERE row_id=? AND name=?", -1, &pstmt,
		NULL)) {
		sqlite3_finalize(pstmt);
		sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"DELETE FROM cellvals WHERE cell_id=?", -1, &pstmt1, NULL)) {
		sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	for (i=0; i<pcolnames->count; i++) {
		sqlite3_reset(pstmt);
		sqlite3_bind_int64(pstmt, 1, row_instance);
		sqlite3_bind_text(pstmt, 2, pcolnames->ppstr[i], -1, SQLITE_STATIC);
		if (SQLITE_ROW == sqlite3_step(pstmt)) {
			cell_id = sqlite3_column_int64(pstmt, 0);
			if (PROPVAL_TYPE_OBJECT == sqlite3_column_int64(pstmt, 2)) {
				if (FALSE == common_util_delete_table(pditem->psqlite,
					sqlite3_column_int64(pstmt, 1))) {
					sqlite3_exec(pditem->psqlite,
						"ROLLBACK", NULL, NULL, NULL);
					sqlite3_finalize(pstmt);
					sqlite3_finalize(pstmt1);
					common_util_put_dir_item(pditem);
					return EC_ERROR;	
				}
			}
			sqlite3_reset(pstmt1);
			sqlite3_bind_int64(pstmt1, 1, cell_id);
			sqlite3_step(pstmt1);
		}
	}
	sqlite3_exec(pditem->psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	sqlite3_finalize(pstmt);
	sqlite3_finalize(pstmt1);
	common_util_put_dir_item(pditem);
	return EC_SUCCESS;
}

uint32_t dac_server_deletetable(const char *dir,
	const char *pnamespace, const char *ptablename)
{
	DIR_ITEM *pditem;
	uint64_t object_id;
	sqlite3_stmt *pstmt;
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT object_id FROM objtbls WHERE"
		" namespace=? AND tablename=?", -1,
		&pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_bind_text(pstmt, 1, pnamespace, -1, SQLITE_STATIC);
	sqlite3_bind_text(pstmt, 2, ptablename, -1, SQLITE_STATIC);
	if (SQLITE_ROW != sqlite3_step(pstmt)) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_SUCCESS;
	}
	object_id = sqlite3_column_int64(pstmt, 0);
	sqlite3_finalize(pstmt);
	sqlite3_exec(pditem->psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if (FALSE == common_util_delete_table(pditem->psqlite, object_id)) {
		sqlite3_exec(pditem->psqlite, "ROLLBACK", NULL, NULL, NULL);
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_exec(pditem->psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	common_util_put_dir_item(pditem);
	return EC_SUCCESS;
}

uint32_t dac_server_closetable(
	const char *dir, uint32_t table_id)
{
	DIR_ITEM *pditem;
	TABLE_NODE *ptnode;
	DOUBLE_LIST_NODE *pnode;
	
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	for (pnode=double_list_get_head(&pditem->table_list); NULL!=pnode;
		pnode=double_list_get_after(&pditem->table_list, pnode)) {
		ptnode = (TABLE_NODE*)pnode->pdata;
		if (table_id == ptnode->table_id) {
			double_list_remove(&pditem->table_list, pnode);
			break;
		}
	}
	common_util_put_dir_item(pditem);
	if (NULL != pnode) {
		if (NULL != ptnode->pinstances) {
			free(ptnode->pinstances);
		}
		free(ptnode);
	}
	return EC_SUCCESS;
}

uint32_t dac_server_matchrow(const char *dir,
	uint32_t table_id, uint16_t type,
	const void *pvalue, DAC_VARRAY *prow)
{
	int i;
	DIR_ITEM *pditem;
	uint64_t object_id;
	TABLE_NODE *ptnode;
	sqlite3_stmt *pstmt;
	DOUBLE_LIST tmp_list;
	uint64_t row_instance;
	DAC_PROPVAL *ppropval;
	const char *puniquename;
	DOUBLE_LIST_NODE *pnode;
	
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	for (pnode=double_list_get_head(&pditem->table_list); NULL!=pnode;
		pnode=double_list_get_after(&pditem->table_list, pnode)) {
		ptnode = (TABLE_NODE*)pnode->pdata;
		if (table_id == ptnode->table_id) {
			break;
		}
	}
	if (NULL == pnode) {
		common_util_put_dir_item(pditem);
		return EC_NOT_FOUND;
	}
	object_id = ptnode->object_id;
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT unique_name FROM objtbls WHERE"
		" object_id=?", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_bind_int64(pstmt, 1, object_id);
	if (SQLITE_ROW != sqlite3_step(pstmt)) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_NOT_FOUND;
	}
	puniquename = sqlite3_column_text(pstmt, 0);
	if (NULL == puniquename) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_NOT_SUPPORTED;
	}
	puniquename = common_util_dup(puniquename);
	sqlite3_finalize(pstmt);
	if (NULL == puniquename) {
		common_util_put_dir_item(pditem);
		return EC_OUT_OF_MEMORY;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT row_id FROM tblrows WHERE object_id=?"
		" AND unique_val=?", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_bind_int64(pstmt, 1, object_id);
	if (SQLITE_ROW != common_util_bind_value(
		pstmt, 2, type, pvalue)) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_NOT_FOUND;
	}
	row_instance = sqlite3_column_int64(pstmt, 0);
	sqlite3_finalize(pstmt);
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT name, type, value FROM cellvals"
		" WHERE row_id=?", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	double_list_init(&tmp_list);
	pnode = common_util_alloc(sizeof(DOUBLE_LIST_NODE));
	if (NULL == pnode) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_OUT_OF_MEMORY;
	}
	ppropval = common_util_alloc(sizeof(DAC_PROPVAL));
	if (NULL == ppropval) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_OUT_OF_MEMORY;
	}
	pnode->pdata = ppropval;
	sqlite3_bind_int64(pstmt, 1, row_instance);
	ppropval->pname = "instance";
	ppropval->type = PROPVAL_TYPE_LONGLONG;
	ppropval->pvalue = common_util_alloc(sizeof(uint64_t));
	if (NULL == ppropval->pvalue) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_OUT_OF_MEMORY;
	}
	*(uint64_t*)ppropval->pvalue = row_instance;
	double_list_append_as_tail(&tmp_list, pnode);
	while (SQLITE_ROW == sqlite3_step(pstmt)) {
		pnode = common_util_alloc(sizeof(DOUBLE_LIST_NODE));
		if (NULL == pnode) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_OUT_OF_MEMORY;
		}
		ppropval = common_util_alloc(sizeof(DAC_PROPVAL));
		if (NULL == ppropval) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_OUT_OF_MEMORY;
		}
		pnode->pdata = ppropval;
		ppropval->pname = common_util_dup(
			sqlite3_column_text(pstmt, 0));
		if (NULL == ppropval->pname) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_OUT_OF_MEMORY;
		}
		ppropval->type = sqlite3_column_int64(pstmt, 1);
		ppropval->pvalue = common_util_column_value(
							pstmt, 2, ppropval->type);
		if (NULL == ppropval->pvalue) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
		double_list_append_as_tail(&tmp_list, pnode);
	}
	sqlite3_finalize(pstmt);
	common_util_put_dir_item(pditem);
	prow->count = double_list_get_nodes_num(&tmp_list);
	prow->ppropval = common_util_alloc(prow->count*sizeof(DAC_PROPVAL));
	if (NULL == prow->ppropval) {
		return EC_OUT_OF_MEMORY;
	}
	for (i=0; i<prow->count; i++) {
		pnode = double_list_get_from_head(&tmp_list);
		ppropval = pnode->pdata;
		prow->ppropval[i] = *ppropval;
	}
	return EC_SUCCESS;
}

uint32_t dac_server_getnamespaces(const char *dir,
	STRING_ARRAY *pnamespaces)
{
	DIR_ITEM *pditem;
	sqlite3_stmt *pstmt;
	DOUBLE_LIST tmp_list;
	DOUBLE_LIST_NODE *pnode;
	
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT DISTINCT namespace FROM propvals",
		-1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	double_list_init(&tmp_list);
	while (SQLITE_ROW == sqlite3_step(pstmt)) {
		pnode = common_util_alloc(sizeof(DOUBLE_LIST_NODE));
		if (NULL == pnode) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
		pnode->pdata = common_util_dup(
			sqlite3_column_text(pstmt, 0));
		if (NULL == pnode->pdata) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
		double_list_append_as_tail(&tmp_list, pnode);
	}
	sqlite3_finalize(pstmt);
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT DISTINCT namespace FROM objtbls WHERE"
		" namespace IS NOT NULL", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	while (SQLITE_ROW == sqlite3_step(pstmt)) {
		pnode = common_util_alloc(sizeof(DOUBLE_LIST_NODE));
		if (NULL == pnode) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
		pnode->pdata = common_util_dup(
			sqlite3_column_text(pstmt, 0));
		if (NULL == pnode->pdata) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_ERROR;
		}
		double_list_append_as_tail(&tmp_list, pnode);
	}
	sqlite3_finalize(pstmt);
	common_util_put_dir_item(pditem);
	pnamespaces->count = double_list_get_nodes_num(&tmp_list);
	if (0 == pnamespaces->count) {
		pnamespaces->ppstr = NULL;
		return EC_SUCCESS;
	}
	pnamespaces->ppstr = common_util_alloc(
		sizeof(void*)*pnamespaces->count);
	if (NULL == pnamespaces->ppstr) {
		return EC_OUT_OF_MEMORY;
	}
	pnamespaces->count = 0;
	for (pnode=double_list_get_head(&tmp_list); NULL!=pnode;
		pnode=double_list_get_after(&tmp_list, pnode)) {
		pnamespaces->ppstr[pnamespaces->count] = pnode->pdata;
		pnamespaces->count ++;
	}
	return EC_SUCCESS;
}

uint32_t dac_server_getpropnames(const char *dir,
	const char *pnamespace, STRING_ARRAY *ppropnames)
{
	int i;
	DIR_ITEM *pditem;
	char tmp_path[256];
	sqlite3_stmt *pstmt;
	DOUBLE_LIST tmp_list;
	struct stat node_stat;
	DOUBLE_LIST_NODE *pnode;
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT propname FROM propvals WHERE "
		"namespace=?", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_bind_text(pstmt, 1, pnamespace, -1, SQLITE_STATIC);
	double_list_init(&tmp_list);
	while (SQLITE_ROW == sqlite3_step(pstmt)) {
		pnode = common_util_alloc(sizeof(DOUBLE_LIST_NODE));
		if (NULL == pnode) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_OUT_OF_MEMORY;
		}
		pnode->pdata = common_util_dup(
			sqlite3_column_text(pstmt, 0));
		if (NULL == pnode->pdata) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_OUT_OF_MEMORY;
		}
		double_list_append_as_tail(&tmp_list, pnode);
	}
	sqlite3_finalize(pstmt);
	common_util_put_dir_item(pditem);
	for (i=0; i<g_item_num; i++) {
		if (0 == strcasecmp(g_map_items[i].namespace, pnamespace)) {
			sprintf(tmp_path, "%s/%s", dir, g_map_items[i].path);
			if (0 != stat(tmp_path, &node_stat)) {
				continue;
			}
			pnode = common_util_alloc(sizeof(DOUBLE_LIST_NODE));
			if (NULL == pnode) {
				return EC_ERROR;
			}
			pnode->pdata = g_map_items[i].propname;
			double_list_append_as_tail(&tmp_list, pnode);
		}	
	}
	ppropnames->count = double_list_get_nodes_num(&tmp_list);
	if (0 == ppropnames->count) {
		ppropnames->ppstr = NULL;
		return EC_SUCCESS;
	}
	ppropnames->ppstr = common_util_alloc(
		sizeof(void*)*ppropnames->count);
	if (NULL == ppropnames->ppstr) {
		return EC_OUT_OF_MEMORY;
	}
	ppropnames->count = 0;
	for (pnode=double_list_get_head(&tmp_list); NULL!=pnode;
		pnode=double_list_get_after(&tmp_list, pnode)) {
		ppropnames->ppstr[ppropnames->count] = pnode->pdata;
		ppropnames->count ++;
	}
	return EC_SUCCESS;
}

uint32_t dac_server_gettablenames(const char *dir,
	const char *pnamespace, STRING_ARRAY *ptablenames)
{
	DIR_ITEM *pditem;
	sqlite3_stmt *pstmt;
	DOUBLE_LIST tmp_list;
	DOUBLE_LIST_NODE *pnode;
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT tablename FROM objtbls WHERE"
		" namespace=?", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_bind_text(pstmt, 1, pnamespace, -1, SQLITE_STATIC);
	double_list_init(&tmp_list);
	while (SQLITE_ROW == sqlite3_step(pstmt)) {
		pnode = common_util_alloc(sizeof(DOUBLE_LIST_NODE));
		if (NULL == pnode) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_OUT_OF_MEMORY;
		}
		pnode->pdata = common_util_dup(
			sqlite3_column_text(pstmt, 0));
		if (NULL == pnode->pdata) {
			sqlite3_finalize(pstmt);
			common_util_put_dir_item(pditem);
			return EC_OUT_OF_MEMORY;
		}
		double_list_append_as_tail(&tmp_list, pnode);
	}
	sqlite3_finalize(pstmt);
	common_util_put_dir_item(pditem);
	ptablenames->count = double_list_get_nodes_num(&tmp_list);
	if (0 == ptablenames->count) {
		ptablenames->ppstr = NULL;
		return EC_SUCCESS;
	}
	ptablenames->ppstr = common_util_alloc(
		sizeof(void*)*ptablenames->count);
	if (NULL == ptablenames->ppstr) {
		return EC_OUT_OF_MEMORY;
	}
	ptablenames->count = 0;
	for (pnode=double_list_get_head(&tmp_list); NULL!=pnode;
		pnode=double_list_get_after(&tmp_list, pnode)) {
		ptablenames->ppstr[ptablenames->count] = pnode->pdata;
		ptablenames->count ++;
	}
	return EC_SUCCESS;
}

uint32_t dac_server_readfile(const char *dir, const char *path,
	const char *file_name, uint64_t offset, uint32_t length,
	BINARY *pcontent_bin)
{
	int fd, len;
	char tmp_path[256];
	struct stat node_stat;
	
	snprintf(tmp_path, sizeof(tmp_path),
		"%s/%s/%s", dir, path, file_name);
	if (0 != stat(tmp_path, &node_stat)) {
		return EC_NOT_FOUND;
	}
	if (offset >= node_stat.st_size) {
		pcontent_bin->cb = 0;
		pcontent_bin->pb = NULL;
		return EC_SUCCESS;
	}
	if (node_stat.st_size < offset + length) {
		length = node_stat.st_size - offset;
	}
	fd = open(tmp_path, O_RDONLY);
	if (-1 == fd) {
		return EC_ERROR;
	}
	if (0 != offset) {
		lseek(fd, offset, SEEK_SET);
	}
	pcontent_bin->pb = common_util_alloc(length);
	if (NULL == pcontent_bin->pb) {
		close(fd);
		return EC_OUT_OF_MEMORY;
	}
	len = read(fd, pcontent_bin->pb, length);
	close(fd);
	if (len < 0) {
		return EC_ERROR;
	}
	pcontent_bin->cb = len;
	return EC_SUCCESS;
}

uint32_t dac_server_appendfile(const char *dir,
	const char *path, const char *file_name,
	const BINARY *pcontent_bin)
{
	int fd;
	char tmp_path[256];
	
	snprintf(tmp_path, sizeof(tmp_path),
		"%s/%s/%s", dir, path, file_name);
	fd = open(tmp_path, O_CREAT|O_APPEND|O_WRONLY, 0666);
	if (-1 == fd) {
		return EC_ERROR;
	}
	if (pcontent_bin->cb != write(fd,
		pcontent_bin->pb, pcontent_bin->cb)) {
		close(fd);
		remove(tmp_path);
		return EC_ERROR;
	}
	close(fd);
	return EC_SUCCESS;
}

uint32_t dac_server_removefile(const char *dir,
	const char *path, const char *file_name)
{
	char tmp_path[256];
	
	snprintf(tmp_path, sizeof(tmp_path),
		"%s/%s/%s", dir, path, file_name);
	remove(tmp_path);
	return EC_SUCCESS;
}

uint32_t dac_server_statfile(const char *dir,
	const char *path, const char *file_name,
	uint64_t *psize)
{
	char tmp_path[256];
	struct stat node_stat;
	
	snprintf(tmp_path, sizeof(tmp_path),
		"%s/%s/%s", dir, path, file_name);
	if (0 != stat(tmp_path, &node_stat)) {
		return EC_NOT_FOUND;
	}
	*psize = node_stat.st_size;
	return EC_SUCCESS;
}

uint32_t dac_server_checkrow(const char *dir,
	const char *pnamespace, const char *ptablename,
	uint16_t type, const void *pvalue)
{
	DIR_ITEM *pditem;
	uint64_t object_id;
	sqlite3_stmt *pstmt;
	const char *puniquename;
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	pditem = common_util_get_dir_item(dir);
	if (NULL == pditem) {
		return EC_ERROR;
	}
	if (NULL == pditem->psqlite) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT object_id, unique_name FROM objtbls WHERE "
		"namespace=? AND tablename=?", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_bind_text(pstmt, 1, pnamespace, -1, SQLITE_STATIC);
	sqlite3_bind_text(pstmt, 2, ptablename, -1, SQLITE_STATIC);
	if (SQLITE_ROW != sqlite3_step(pstmt)) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_NOT_FOUND;
	}
	object_id = sqlite3_column_int64(pstmt, 0);
	puniquename = sqlite3_column_text(pstmt, 0);
	if (NULL == puniquename) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_NOT_SUPPORTED;
	}
	puniquename = common_util_dup(puniquename);
	sqlite3_finalize(pstmt);
	if (NULL == puniquename) {
		common_util_put_dir_item(pditem);
		return EC_OUT_OF_MEMORY;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(pditem->psqlite,
		"SELECT row_id FROM tblrows WHERE object_id=?"
		" AND unique_val=?", -1, &pstmt, NULL)) {
		common_util_put_dir_item(pditem);
		return EC_ERROR;
	}
	sqlite3_bind_int64(pstmt, 1, object_id);
	if (SQLITE_ROW != common_util_bind_value(
		pstmt, 2, type, pvalue)) {
		sqlite3_finalize(pstmt);
		common_util_put_dir_item(pditem);
		return EC_NOT_FOUND;
	}
	sqlite3_finalize(pstmt);
	common_util_put_dir_item(pditem);
	return EC_SUCCESS;
}

uint32_t dac_server_phprpc(const char *dir, const char *account,
	const char *script_path, const BINARY *pbin_in, BINARY *pbin_out)
{
	uint32_t out_len;
	EXT_PUSH ext_push;
	uint8_t *pbuff_out;
	
	if (FALSE == ext_buffer_push_init(&ext_push, NULL, 0, 0)) {
		return EC_OUT_OF_MEMORY;
	}
	ext_buffer_push_string(&ext_push, dir);
	ext_buffer_push_string(&ext_push, account);
	if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
		&ext_push, pbin_in->pb, pbin_in->cb)) {
		ext_buffer_push_free(&ext_push);
		return EC_OUT_OF_MEMORY;
	}
	if (FALSE == system_services_fcgi_rpc(ext_push.data,
		ext_push.offset, &pbuff_out, &out_len, script_path)) {
		ext_buffer_push_free(&ext_push);
		return EC_RPC_FAIL;
	}
	ext_buffer_push_free(&ext_push);
	pbin_out->cb = out_len;
	pbin_out->pb = common_util_alloc(out_len);
	if (NULL == pbin_out->pb) {
		free(pbuff_out);
		return EC_OUT_OF_MEMORY;
	}
	memcpy(pbin_out->pb, pbuff_out, out_len);
	free(pbuff_out);
	return EC_SUCCESS;
}
