#include "url_wildcard.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sqlite3.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

static sqlite3 *g_sqlite;
static sqlite3_stmt *g_stmt;
static char g_list_path[256];
static pthread_mutex_t g_lock;

static void url_wildcard_extract_host(const char *url, char *host)
{
	char *ptoken;
	char *presult;
	char tmp_url[256];
	
	strcpy(tmp_url, url);
	presult = tmp_url;
	/* trim the "http://", "https://" or "www." of found url */
	if (0 == strncasecmp(presult, "http://", 7)) {
		presult += 7;
	} else if (0 == strncasecmp(presult, "https://", 8)) {
		presult += 8;
	}
	ptoken = strchr(presult, '/');
	if (NULL != ptoken) {
		*ptoken = '\0';
	}
	ptoken = strchr(presult, ':');
	if (NULL != ptoken) {
		*ptoken = '\0';
	}
	ptoken = strchr(presult, '?');
	if (NULL != ptoken) {
		*ptoken = '\0';
	}
	strcpy(host, presult);
}

void url_wildcard_init(const char *list_path)
{
	g_sqlite = NULL;
	strcpy(g_list_path, list_path);
	pthread_mutex_init(&g_lock, NULL);
}

int url_wildcard_run()
{
	if (FALSE == url_wildcard_refresh()) {
		return -1;
	}
	return 0;
}

int url_wildcard_stop()
{
	if (NULL != g_sqlite) {
		sqlite3_finalize(g_stmt);
		g_stmt = NULL;
		sqlite3_close(g_sqlite);
		g_sqlite = NULL;
	}
	return 0;
}

void url_wildcard_free()
{
	pthread_mutex_destroy(&g_lock);
}

BOOL url_wildcard_query(const char *url)
{
	char tmp_host[256];
	const char *pmatch_str;
	
	url_wildcard_extract_host(url, tmp_host);
	pthread_mutex_lock(&g_lock);
	sqlite3_reset(g_stmt);
	sqlite3_bind_text(g_stmt, 1, tmp_host, -1, SQLITE_STATIC);
	while (SQLITE_ROW == sqlite3_step(g_stmt)) {
		pmatch_str = sqlite3_column_text(g_stmt, 0);
		if (0 != wildcard_match(url, pmatch_str, TRUE)) {
			pthread_mutex_unlock(&g_lock);
			return TRUE;
		}
	}
	sqlite3_reset(g_stmt);
	sqlite3_bind_text(g_stmt, 1, "*", -1, SQLITE_STATIC);
	while (SQLITE_ROW == sqlite3_step(g_stmt)) {
		pmatch_str = sqlite3_column_text(g_stmt, 0);
		if (0 != wildcard_match(url, pmatch_str, TRUE)) {
			pthread_mutex_unlock(&g_lock);
			return TRUE;
		}
	}
	pthread_mutex_unlock(&g_lock);
	return FALSE;
}

BOOL url_wildcard_refresh()
{
	FILE *fp;
	int tmp_len;
	sqlite3 *psqlite;
	char tmp_host[256];
	char tmp_line[256];
	sqlite3_stmt *pstmt;
	char sql_string[1024];
	
	fp = fopen(g_list_path, "r");
	if (NULL == fp) {
		return FALSE;
	}
	if (SQLITE_OK != sqlite3_open_v2(":memory:", &psqlite,
		SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, NULL)) {
		fclose(fp);
		return FALSE;
	}
	if (SQLITE_OK != sqlite3_exec(
		psqlite, "CREATE TABLE urls ("
		"url_id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"url_str TEXT COLLATE NOCASE NOT NULL,"
		"host TEXT COLLATE NOCASE NOT NULL); "
		"CREATE INDEX host_index ON urls(host);",
		NULL, NULL, NULL)) {
		sqlite3_close(psqlite);
		fclose(fp);
		return FALSE;
	}
	if (SQLITE_OK != sqlite3_prepare_v2(psqlite,
		"INSERT INTO urls (url_str, host) VALUES(?, ?)",
		-1, &pstmt, NULL)) {
		sqlite3_close(psqlite);
		fclose(fp);
		return FALSE;
	}
	sqlite3_exec(psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
	while (NULL != fgets(tmp_line, 256, fp)) {
		tmp_len = strlen(tmp_line);
		if ('\n' == tmp_line[tmp_len - 1]) {
			tmp_line[tmp_len - 1] = '\0';
			tmp_len --;
		}
		if ('\r' == tmp_line[tmp_len - 1]) {
			tmp_line[tmp_len - 1] = '\0';
		}
		ltrim_string(tmp_line);
		rtrim_string(tmp_line);
		if ('\0' == tmp_line[0]) {
			continue;
		}
		url_wildcard_extract_host(tmp_line, tmp_host);
		sqlite3_reset(pstmt);
		sqlite3_bind_text(pstmt, 1, tmp_line, -1, SQLITE_STATIC);
		sqlite3_bind_text(pstmt, 2, tmp_host, -1, SQLITE_STATIC);
		if (SQLITE_DONE != sqlite3_step(pstmt)) {
			sqlite3_finalize(pstmt);
			sqlite3_exec(psqlite, "ROLLBACK", NULL, NULL, NULL);
			sqlite3_close(psqlite);
			fclose(fp);
			return FALSE;
		}
	}
	fclose(fp);
	sqlite3_finalize(pstmt);
	sqlite3_exec(psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	if (SQLITE_OK != sqlite3_prepare_v2(psqlite,
		"SELECT url_str FROM urls WHERE host=?",
		-1, &pstmt, NULL)) {
		sqlite3_close(psqlite);
		return FALSE;
	}
	pthread_mutex_lock(&g_lock);
	if (NULL != g_sqlite) {
		sqlite3_finalize(g_stmt);
		sqlite3_close(g_sqlite);
	}
	g_sqlite = psqlite;
	g_stmt = pstmt;
	pthread_mutex_unlock(&g_lock);
	return TRUE;
}
