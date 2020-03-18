#include "util.h"
#include "str_hash.h"
#include "list_file.h"
#include "ext_buffer.h"
#include "common_util.h"
#include "alloc_context.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <strings.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

typedef struct _ENVIRONMENT_CONTEXT {
	ALLOC_CONTEXT allocator;
} ENVIRONMENT_CONTEXT;

static BOOL g_wal;
static BOOL g_async;
static int g_table_size;
static uint32_t g_last_id;
static char *g_sql_string;
static BOOL g_notify_stop;
static int g_valid_interval;
static pthread_t g_scan_tid;
static char g_sql_path[256];
static pthread_key_t g_env_key;
static STR_HASH_TABLE *g_item_hash;
static pthread_mutex_t g_hash_lock;
static pthread_mutex_t g_seqence_lock;


static void *scan_work_func(void *param)
{
	int count;
	char htag[256];
	time_t now_time;
	DIR_ITEM *pditem;
	TABLE_NODE *ptnode;
	STR_HASH_ITER *iter;
	DOUBLE_LIST_NODE *ptail;
	DOUBLE_LIST_NODE *pnode;
	
	count = 0;
	while (FALSE == g_notify_stop) {
		sleep(1);
		if (count < 10) {
			count ++;
			continue;
		}
		count = 0;
		pthread_mutex_lock(&g_hash_lock);
		time(&now_time);
		iter = str_hash_iter_init(g_item_hash);
		for (str_hash_iter_begin(iter); FALSE == str_hash_iter_done(iter);
			str_hash_iter_forward(iter)) {
			pditem = (DIR_ITEM*)str_hash_iter_get_value(iter, htag);
			if (0 != pditem->reference) {
				continue;
			}
			ptail = double_list_get_tail(&pditem->table_list);
			while (pnode=double_list_get_from_head(&pditem->table_list)) {
				ptnode = (TABLE_NODE*)pnode->pdata;
				if (now_time - ptnode->load_time > g_valid_interval) {
					if (NULL != ptnode->pinstances) {
						free(ptnode->pinstances);
					}
					free(ptnode);
				}
				if (pnode == ptail) {
					break;
				}
			}
			if (0 == double_list_get_nodes_num(&pditem->table_list)
				&& now_time - pditem->last_time > g_valid_interval) {
				double_list_free(&pditem->table_list);
				if (NULL != pditem->psqlite) {
					sqlite3_close(pditem->psqlite);
					pditem->psqlite = NULL;
				}
				str_hash_iter_remove(iter);
			}
		}
		str_hash_iter_free(iter);
		pthread_mutex_unlock(&g_hash_lock);
	}
	pthread_mutex_lock(&g_hash_lock);
	iter = str_hash_iter_init(g_item_hash);
	for (str_hash_iter_begin(iter); FALSE == str_hash_iter_done(iter);
		str_hash_iter_forward(iter)) {
		pditem = (DIR_ITEM*)str_hash_iter_get_value(iter, htag);
		while (pnode=double_list_get_from_head(&pditem->table_list)) {
			ptnode = (TABLE_NODE*)pnode->pdata;
			if (NULL != ptnode->pinstances) {
				free(ptnode->pinstances);
			}
			free(ptnode);
		}
		if (NULL != pditem->psqlite) {
			sqlite3_close(pditem->psqlite);
		}
		double_list_free(&pditem->table_list);
	}
	str_hash_iter_free(iter);
	pthread_mutex_unlock(&g_hash_lock);
	pthread_exit(0);
}


void common_util_init(int table_size, int valid_interval,
	BOOL b_async, BOOL b_wal, const char *sql_path)
{
	g_table_size = table_size;
	g_valid_interval = valid_interval;
	g_async = b_async;
	g_wal = b_wal;
	pthread_mutex_init(&g_hash_lock, NULL);
	pthread_mutex_init(&g_seqence_lock, NULL);
	pthread_key_create(&g_env_key, NULL);
	strcpy(g_sql_path, sql_path);
}

int common_util_run()
{
	int fd;
	void *pitem;
	int i, item_num;
	LIST_FILE *pfile;
	char tmp_name[256];
	struct stat node_stat;
	
	if (0 != stat(g_sql_path, &node_stat)) {
		return -1;
	}
	g_sql_string = malloc(node_stat.st_size + 1);
	if (NULL == g_sql_string) {
		return -2;
	}
	fd = open(g_sql_path, O_RDONLY);
	if (-1 == fd) {
		free(g_sql_string);
		return -3;
	}
	if (node_stat.st_size != read(fd, g_sql_string, node_stat.st_size)) {
		close(fd);
		free(g_sql_string);
		return -4;
	}
	close(fd);
	g_sql_string[node_stat.st_size] = '\0';
	sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
	sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 0);
	if (SQLITE_OK != sqlite3_initialize()) {
		free(g_sql_string);
		return -5;
	}
	g_item_hash = str_hash_init(g_table_size, sizeof(DIR_ITEM), NULL);
	if (NULL == g_item_hash) {
		free(g_sql_string);
		sqlite3_shutdown();
		return -6;
	}
	g_notify_stop = FALSE;
	if (0 != pthread_create(&g_scan_tid, NULL, scan_work_func, NULL)) {
		free(g_sql_string);
		sqlite3_shutdown();
		str_hash_free(g_item_hash);
		return -7;
	}
	return 0;
}

int common_util_stop()
{
	g_notify_stop = TRUE;
	pthread_join(g_scan_tid, NULL);
	sqlite3_shutdown();
	str_hash_free(g_item_hash);
	free(g_sql_string);
	return 0;
}

void common_util_free()
{
	pthread_key_delete(g_env_key);
	pthread_mutex_destroy(&g_hash_lock);
	pthread_mutex_destroy(&g_seqence_lock);
}

BOOL common_util_build_environment()
{
	ENVIRONMENT_CONTEXT *pctx;
	
	pctx = malloc(sizeof(ENVIRONMENT_CONTEXT));
	if (NULL == pctx) {
		return FALSE;
	}
	alloc_context_init(&pctx->allocator);
	pthread_setspecific(g_env_key, pctx);
}

void common_util_free_environment()
{
	ENVIRONMENT_CONTEXT *pctx;

	pctx = pthread_getspecific(g_env_key);
	pthread_setspecific(g_env_key, NULL);
	alloc_context_free(&pctx->allocator);
	free(pctx);
}

void* common_util_alloc(size_t size)
{
	ENVIRONMENT_CONTEXT *pctx;
	
	pctx = pthread_getspecific(g_env_key);
	return alloc_context_alloc(&pctx->allocator, size);
}

char* common_util_dup(const char *pstr)
{
	int len;
	char *pstr1;
	
	len = strlen(pstr) + 1;
	pstr1 = common_util_alloc(len);
	if (NULL == pstr1) {
		return NULL;
	}
	memcpy(pstr1, pstr, len);
	return pstr1;
}

static BINARY* common_util_dup_binary(const BINARY *pbin)
{
	BINARY *pbin1;
	
	pbin1 = common_util_alloc(sizeof(BINARY));
	if (NULL == pbin1) {
		return NULL;
	}
	pbin1->cb = pbin->cb;
	if (0 == pbin->cb) {
		pbin1->pb = NULL;
		return pbin1;
	}
	pbin1->pb = common_util_alloc(pbin->cb);
	if (NULL == pbin1->pb) {
		return NULL;
	}
	memcpy(pbin1->pb, pbin->pb, pbin->cb);
	return pbin1;
}

BOOL common_util_load_file(const char *path, BINARY *pbin)
{
	int fd;
	struct stat node_stat;
	
	if (0 != stat(path, &node_stat)) {
		return FALSE;
	}
	pbin->cb = node_stat.st_size;
	pbin->pb = common_util_alloc(node_stat.st_size);
	if (NULL == pbin->pb) {
		return FALSE;
	}
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		return FALSE;
	}
	if (node_stat.st_size != read(fd,
		pbin->pb, node_stat.st_size)) {
		close(fd);
		return FALSE;
	}
	close(fd);
	return TRUE;
}

static sqlite3* common_util_open_database(const char *dir)
{
	sqlite3 *psqlite;
	char tmp_path[256];
	struct stat node_stat;
	
	sprintf(tmp_path, "%s/exmdb/dac.sqlite3", dir);
	if (0 != stat(tmp_path, &node_stat)) {
		if (SQLITE_OK != sqlite3_open_v2(tmp_path, &psqlite,
			SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, NULL)) {
			return NULL;
		}
		if (SQLITE_OK != sqlite3_exec(psqlite,
			g_sql_string, NULL, NULL, NULL)) {
			sqlite3_close(psqlite);
			remove(tmp_path);
			return NULL;
		}
		sprintf(tmp_path, "%s/dac", dir);
		mkdir(tmp_path, 0777);
	} else {
		if (SQLITE_OK != sqlite3_open_v2(tmp_path,
			&psqlite, SQLITE_OPEN_READWRITE, NULL)) {
			return NULL;
		}
	}
	sqlite3_exec(psqlite, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
	if (FALSE == g_async) {
		sqlite3_exec(psqlite, "PRAGMA synchronous=OFF", NULL, NULL, NULL);
	} else {
		sqlite3_exec(psqlite, "PRAGMA synchronous=ON", NULL, NULL, NULL);
	}
	if (FALSE == g_wal) {
		sqlite3_exec(psqlite, "PRAGMA journal_mode=DELETE", NULL, NULL, NULL);
	} else {
		sqlite3_exec(psqlite, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
	}
	return psqlite;
}

uint32_t common_util_get_sequence_id()
{
	uint32_t last_id;
	
	pthread_mutex_lock(&g_seqence_lock);
	g_last_id ++;
	if (g_last_id >= 0x7FFFFFFF) {
		g_last_id = 1;
	}
	last_id = g_last_id;
	pthread_mutex_unlock(&g_seqence_lock);
	return last_id;
}

DIR_ITEM* common_util_get_dir_item(const char *dir)
{
	BOOL b_new;
	char htag[256];
	DIR_ITEM *pditem;
	DIR_ITEM tmp_item;
	
	swap_string(htag, dir);
	pthread_mutex_lock(&g_hash_lock);
	pditem = str_hash_query(g_item_hash, htag);
	if (NULL == pditem) {
		if (1 != str_hash_add(g_item_hash, htag, &tmp_item)) {
			pthread_mutex_unlock(&g_hash_lock);
			return NULL;
		}
		pditem = str_hash_query(g_item_hash, htag);
		pthread_mutex_init(&pditem->lock, NULL);
		double_list_init(&pditem->table_list);
		pditem->reference = 0;
		pditem->last_id = 0;
		pditem->last_time = 0;
		pditem->psqlite = NULL;
	}
	if (NULL != pditem) {
		pditem->reference ++;
	}
	pthread_mutex_unlock(&g_hash_lock);
	pthread_mutex_lock(&pditem->lock);
	if (NULL == pditem->psqlite) {
		pditem->psqlite = common_util_open_database(dir);
		if (NULL != pditem->psqlite) {
			time(&pditem->last_time);
		}
	}
	return pditem;
}

void common_util_put_dir_item(DIR_ITEM *pditem)
{
	pthread_mutex_unlock(&pditem->lock);
	pthread_mutex_lock(&g_hash_lock);
	pditem->reference --;
	pthread_mutex_unlock(&g_hash_lock);
}

BOOL common_util_evaluate_restriction(sqlite3 *psqlite,
	sqlite3_stmt *pstmt, uint64_t row_id, const DAC_RES *pres)
{
	int i;
	int len;
	double dval;
	uint8_t bval;
	uint64_t ival;
	uint16_t proptype;
	EXT_PULL ext_pull;
	uint64_t object_id;
	const void *pvalue;
	sqlite3_stmt *pstmt1;
	STRING_ARRAY propstrings;
	
	switch (pres->rt) {
	case DAC_RES_TYPE_OR:
		for (i=0; i<((DAC_RES_AND_OR*)pres->pres)->count; i++) {
			if (TRUE == common_util_evaluate_restriction(psqlite,
				pstmt, row_id, ((DAC_RES_AND_OR*)pres->pres)->pres + i)) {
				return TRUE;
			}
		}
		return FALSE;
	case DAC_RES_TYPE_AND:
		for (i=0; i<((DAC_RES_AND_OR*)pres->pres)->count; i++) {
			if (FALSE == common_util_evaluate_restriction(psqlite,
				pstmt, row_id, ((DAC_RES_AND_OR*)pres->pres)->pres + i)) {
				return FALSE;
			}
		}
		return TRUE;
	case DAC_RES_TYPE_NOT:
		if (TRUE == common_util_evaluate_restriction(psqlite,
			pstmt, row_id, &((DAC_RES_NOT*)pres->pres)->res)) {
			return FALSE;
		}
		return TRUE;
	case DAC_RES_TYPE_CONTENT:
		if (PROPVAL_TYPE_STRING != ((DAC_RES_CONTENT*)
			pres->pres)->propval.type) {
			return FALSE;
		}
		sqlite3_reset(pstmt);
		sqlite3_bind_int64(pstmt, 1, row_id); 
		sqlite3_bind_text(pstmt, 2, ((DAC_RES_CONTENT*)
			pres->pres)->propval.pname, -1, SQLITE_STATIC);
		if (SQLITE_ROW != sqlite3_step(pstmt)) {
			return FALSE;
		}
		proptype = sqlite3_column_int64(pstmt, 1);
		if (PROPVAL_TYPE_STRING == proptype) {
			pvalue = sqlite3_column_text(pstmt, 0);
			if (NULL == pvalue) {
				return FALSE;
			}
			switch (((DAC_RES_CONTENT*)pres->pres)->fl & 0xF) {
			case DAC_FL_FULLSTRING:
				if (((DAC_RES_CONTENT*)pres->pres)->fl & DAC_FL_ICASE) {
					if (0 == strcasecmp(((DAC_RES_CONTENT*)
						pres->pres)->propval.pvalue, pvalue)) {
						return TRUE;
					}
				} else {
					if (0 == strcmp(((DAC_RES_CONTENT*)
						pres->pres)->propval.pvalue, pvalue)) {
						return TRUE;
					}
				}
				return FALSE;
			case DAC_FL_SUBSTRING:
				if (((DAC_RES_CONTENT*)pres->pres)->fl & DAC_FL_ICASE) {
					if (NULL != strcasestr(pvalue, ((DAC_RES_CONTENT*)
						pres->pres)->propval.pvalue)) {
						return TRUE;
					}
				} else {
					if (NULL != strstr(pvalue, ((DAC_RES_CONTENT*)
						pres->pres)->propval.pvalue)) {
						return TRUE;
					}
				}
				return FALSE;
			case DAC_FL_PREFIX:
				len = strlen(((DAC_RES_CONTENT*)pres->pres)->propval.pvalue);
				if (((DAC_RES_CONTENT*)pres->pres)->fl & DAC_FL_ICASE) {
					if (0 == strncasecmp(pvalue, ((DAC_RES_CONTENT*)
						pres->pres)->propval.pvalue, len)) {
						return TRUE;
					}
				} else {
					if (0 == strncmp(pvalue, ((DAC_RES_CONTENT*)
						pres->pres)->propval.pvalue, len)) {
						return TRUE;
					}
				}
				return FALSE;
			}
		} else if (PROPVAL_TYPE_STRING_ARRAY == proptype) {
			ext_buffer_pull_init(&ext_pull,
				sqlite3_column_blob(pstmt, 0),
				sqlite3_column_bytes(pstmt, 0),
				common_util_alloc, 0);
			if (EXT_ERR_SUCCESS != ext_buffer_pull_string_array(
				&ext_pull, &propstrings)) {
				return FALSE;
			}
			for (i=0; i<propstrings.count; i++) {
				pvalue = propstrings.ppstr[i];
				switch (((DAC_RES_CONTENT*)pres->pres)->fl & 0xF) {
				case DAC_FL_FULLSTRING:
					if (((DAC_RES_CONTENT*)pres->pres)->fl & DAC_FL_ICASE) {
						if (0 == strcasecmp(((DAC_RES_CONTENT*)
							pres->pres)->propval.pvalue, pvalue)) {
							return TRUE;
						}
					} else {
						if (0 == strcmp(((DAC_RES_CONTENT*)
							pres->pres)->propval.pvalue, pvalue)) {
							return TRUE;
						}
					}
					break;
				case DAC_FL_SUBSTRING:
					if (((DAC_RES_CONTENT*)pres->pres)->fl & DAC_FL_ICASE) {
						if (NULL != strcasestr(pvalue, ((DAC_RES_CONTENT*)
							pres->pres)->propval.pvalue)) {
							return TRUE;
						}
					} else {
						if (NULL != strstr(pvalue, ((DAC_RES_CONTENT*)
							pres->pres)->propval.pvalue)) {
							return TRUE;
						}
					}
					break;
				case DAC_FL_PREFIX:
					len = strlen(((DAC_RES_CONTENT*)pres->pres)->propval.pvalue);
					if (((DAC_RES_CONTENT*)pres->pres)->fl & DAC_FL_ICASE) {
						if (0 == strncasecmp(pvalue, ((DAC_RES_CONTENT*)
							pres->pres)->propval.pvalue, len)) {
							return TRUE;
						}
					} else {
						if (0 == strncmp(pvalue, ((DAC_RES_CONTENT*)
							pres->pres)->propval.pvalue, len)) {
							return TRUE;
						}
					}
					break;
				}
			}
		}
		return FALSE;
	case RESTRICTION_TYPE_PROPERTY:
		sqlite3_reset(pstmt);
		sqlite3_bind_int64(pstmt, 1, row_id); 
		sqlite3_bind_text(pstmt, 2, ((DAC_RES_PROPERTY*)
			pres->pres)->propval.pname, -1, SQLITE_STATIC);
		if (SQLITE_ROW != sqlite3_step(pstmt)) {
			return FALSE;
		}
		if (((DAC_RES_PROPERTY*)pres->pres)->propval.type
			!= sqlite3_column_int64(pstmt, 1)) {
			return FALSE;
		}
		switch (((DAC_RES_PROPERTY*)pres->pres)->propval.type) {
		case PROPVAL_TYPE_BYTE:
			bval = sqlite3_column_int64(pstmt, 0);
			pvalue = &bval;
			break;
		case PROPVAL_TYPE_LONGLONG:
			ival = sqlite3_column_int64(pstmt, 0);
			pvalue = &ival;
			break;
		case PROPVAL_TYPE_DOUBLE:
			dval = sqlite3_column_double(pstmt, 0);
			pvalue = &dval;
			break;
		case PROPVAL_TYPE_STRING:
			pvalue = sqlite3_column_text(pstmt, 0);
			if (NULL == pvalue) {
				return FALSE;
			}
			break;
		default:
			return FALSE;
		}
		return propval_compare_relop(
			((DAC_RES_PROPERTY*)pres->pres)->relop,
			((DAC_RES_PROPERTY*)pres->pres)->propval.type,
			pvalue, ((DAC_RES_PROPERTY*)pres->pres)->propval.pvalue);
	case RESTRICTION_TYPE_EXIST:
		sqlite3_reset(pstmt);
		sqlite3_bind_int64(pstmt, 1, row_id); 
		sqlite3_bind_text(pstmt, 2, ((DAC_RES_EXIST*)
			pres->pres)->pname, -1, SQLITE_STATIC);
		if (SQLITE_ROW != sqlite3_step(pstmt)) {
			return FALSE;
		}
		return TRUE;
	case DAC_RES_TYPE_SUBOBJ:
		sqlite3_reset(pstmt);
		sqlite3_bind_int64(pstmt, 1, row_id); 
		sqlite3_bind_text(pstmt, 2, ((DAC_RES_SUBOBJ*)
			pres->pres)->pname, -1, SQLITE_STATIC);
		if (SQLITE_ROW != sqlite3_step(pstmt)) {
			return FALSE;
		}
		if (PROPVAL_TYPE_OBJECT != sqlite3_column_int64(pstmt, 1)) {
			return FALSE;
		}
		object_id = sqlite3_column_int64(pstmt, 0);
		if (SQLITE_OK != sqlite3_prepare_v2(psqlite,
			"SELECT row_id FROM tblrows WHERE"
			" object_id=?", -1, &pstmt1, NULL)) {
			return FALSE;
		}
		while (SQLITE_ROW == sqlite3_step(pstmt1)) {
			if (TRUE == common_util_evaluate_restriction(
				psqlite, pstmt, sqlite3_column_int64(pstmt1,
				0), &((DAC_RES_SUBOBJ*)pres->pres)->res)) {
				sqlite3_finalize(pstmt1);
				return TRUE;
			}
		}
		sqlite3_finalize(pstmt1);
		return FALSE;
	}
	return FALSE;
}

int common_util_bind_value(sqlite3_stmt *pstmt,
	int idx, uint16_t type, const void *pvalue)
{
	int status;
	int s_result;
	EXT_PUSH ext_push;
	char temp_buff[16];
	
	switch (type) {
	case PROPVAL_TYPE_BYTE:
		sqlite3_bind_int64(pstmt, idx, *(uint8_t*)pvalue);
		s_result = sqlite3_step(pstmt);
		break;
	case PROPVAL_TYPE_LONGLONG:
		sqlite3_bind_int64(pstmt, idx, *(uint64_t*)pvalue);
		s_result = sqlite3_step(pstmt);
		break;
	case PROPVAL_TYPE_DOUBLE:
		sqlite3_bind_double(pstmt, idx, *(double*)pvalue);
		s_result = sqlite3_step(pstmt);
		break;
	case PROPVAL_TYPE_STRING:
		sqlite3_bind_text(pstmt, idx, pvalue, -1, SQLITE_STATIC);
		s_result = sqlite3_step(pstmt);
		break;
	case PROPVAL_TYPE_GUID:
		ext_buffer_push_init(&ext_push, temp_buff, 16, 0);
		if (EXT_ERR_SUCCESS != ext_buffer_push_guid(
			&ext_push, pvalue)) {
			return FALSE;
		}
		sqlite3_bind_blob(pstmt, idx, ext_push.data,
					ext_push.offset, SQLITE_STATIC);
		s_result = sqlite3_step(pstmt);
		break;
	case PROPVAL_TYPE_LONGLONG_ARRAY:
	case PROPVAL_TYPE_STRING_ARRAY:
		if (FALSE == ext_buffer_push_init(&ext_push, NULL, 0, 0)) {
			return FALSE;
		}
		if (PROPVAL_TYPE_LONGLONG_ARRAY == type) {
			status = ext_buffer_push_longlong_array(&ext_push, pvalue);
		} else {
			status = ext_buffer_push_string_array(&ext_push, pvalue);
		}
		if (EXT_ERR_SUCCESS != status) {
			ext_buffer_push_free(&ext_push);
			return FALSE;
		}
		sqlite3_bind_blob(pstmt, idx, ext_push.data,
				ext_push.offset, SQLITE_STATIC);
		s_result = sqlite3_step(pstmt);
		ext_buffer_push_free(&ext_push);
		break;
	default:
		return FALSE;
	}
	return s_result;
}

void* common_util_column_value(sqlite3_stmt *pstmt,
	int idx, uint16_t type)
{
	int status;
	void *pvalue;
	EXT_PULL ext_pull;
	static uint32_t fake_val = 0;
	
	switch (type) {
	case PROPVAL_TYPE_BYTE:
		pvalue = common_util_alloc(sizeof(uint8_t));
		if (NULL == pvalue) {
			return NULL;
		}
		*(uint8_t*)pvalue = sqlite3_column_int64(pstmt, idx);
		break;
	case PROPVAL_TYPE_LONGLONG:
		pvalue = common_util_alloc(sizeof(uint64_t));
		if (NULL == pvalue) {
			return NULL;
		}
		*(uint64_t*)pvalue = sqlite3_column_int64(pstmt, idx);
		break;
	case PROPVAL_TYPE_DOUBLE:
		pvalue = common_util_alloc(sizeof(double));
		if (NULL == pvalue) {
			return NULL;
		}
		*(double*)pvalue = sqlite3_column_double(pstmt, idx);
		break;
	case PROPVAL_TYPE_STRING:
		pvalue = common_util_dup(sqlite3_column_text(pstmt, idx));
		if (NULL == pvalue) {
			return NULL;
		}
		break;
	case PROPVAL_TYPE_GUID:
		pvalue = common_util_alloc(sizeof(GUID));
		if (NULL != pvalue) {
			ext_buffer_pull_init(&ext_pull,
				sqlite3_column_blob(pstmt, idx),
				sqlite3_column_bytes(pstmt, idx),
				common_util_alloc, 0);
			if (EXT_ERR_SUCCESS != ext_buffer_pull_guid(
				&ext_pull, pvalue)) {
				return NULL;
			}
		}
		break;
	case PROPVAL_TYPE_LONGLONG_ARRAY:
	case PROPVAL_TYPE_STRING_ARRAY:
		if (PROPVAL_TYPE_LONGLONG_ARRAY == type) {
			pvalue = common_util_alloc(sizeof(LONGLONG_ARRAY));
		} else {
			pvalue = common_util_alloc(sizeof(STRING_ARRAY));
		}
		if (NULL == pvalue) {
			return NULL;
		}
		ext_buffer_pull_init(&ext_pull,
			sqlite3_column_blob(pstmt, idx),
			sqlite3_column_bytes(pstmt, idx),
			common_util_alloc, 0);
		if (PROPVAL_TYPE_LONGLONG_ARRAY == type) {
			status = ext_buffer_pull_longlong_array(&ext_pull, pvalue);
		} else {
			status = ext_buffer_pull_string_array(&ext_pull, pvalue);
		}
		if (EXT_ERR_SUCCESS != status) {
			return NULL;
		}
		break;
	case PROPVAL_TYPE_OBJECT:
		pvalue = &fake_val;
		break;
	default:
		return NULL;
	}
	return pvalue;
}

BOOL common_util_delete_table(sqlite3 *psqlite, uint64_t object_id)
{
	sqlite3_stmt *pstmt;
	
	if (SQLITE_OK != sqlite3_prepare_v2(psqlite,
		"SELECT cellvals.value FROM cellvals JOIN "
		"tblrows ON cellvals.row_id=tblrows.row_id "
		"AND cellvals.type=13 AND tblrows.object_id=?",
		-1, &pstmt, NULL)) {
		return FALSE;
	}
	sqlite3_bind_int64(pstmt, 1, object_id);
	if (SQLITE_ROW == sqlite3_step(pstmt)) {
		if (FALSE == common_util_delete_table(psqlite,
			sqlite3_column_int64(pstmt, 0))) {
			return FALSE;	
		}
	}
	sqlite3_finalize(pstmt);
	if (SQLITE_OK != sqlite3_prepare_v2(psqlite,
		"DELETE FROM objtbls WHERE object_id=?",
		-1, &pstmt, NULL)) {
		return FALSE;	
	}
	sqlite3_bind_int64(pstmt, 1, object_id);
	sqlite3_step(pstmt);
	sqlite3_finalize(pstmt);
	return TRUE;
}

BOOL common_util_delete_row(sqlite3 *psqlite, uint64_t row_id)
{
	sqlite3_stmt *pstmt;
	
	if (SQLITE_OK != sqlite3_prepare_v2(psqlite,
		"SELECT value FROM cellvals WHERE type=13"
		" AND row_id=?", -1, &pstmt, NULL)) {
		return FALSE;
	}
	sqlite3_bind_int64(pstmt, 1, row_id);
	if (SQLITE_ROW == sqlite3_step(pstmt)) {
		if (FALSE == common_util_delete_table(psqlite,
			sqlite3_column_int64(pstmt, 0))) {
			return FALSE;	
		}
	}
	sqlite3_finalize(pstmt);
	if (SQLITE_OK != sqlite3_prepare_v2(psqlite,
		"DELETE FROM tblrows WHERE row_id=?",
		-1, &pstmt, NULL)) {
		return FALSE;	
	}
	sqlite3_bind_int64(pstmt, 1, row_id);
	sqlite3_step(pstmt);
	sqlite3_finalize(pstmt);
	return TRUE;
}

DAC_PROPVAL* common_util_get_propvals(
	const DAC_VARRAY *parray, const char *pname)
{
	int i;
	
	for (i=0; i<parray->count; i++) {
		if (0 == strcasecmp(parray->ppropval[i].pname, pname)) {
			return parray->ppropval + i;
		}
	}
	return NULL;
}
