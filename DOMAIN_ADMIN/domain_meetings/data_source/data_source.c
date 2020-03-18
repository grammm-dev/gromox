#include "data_source.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>


static BOOL data_source_exec(char **argv, char *buff_out, int *pout_len)
{
	pid_t pid;
	int status;
	int tmp_len;
	int pipes[2] = {-1, -1};
	
	if (-1 == pipe(pipes)) {
		return FALSE;
	}
	pid = fork();
	if (0 == pid) {
		close(pipes[0]);
		close(1);
		dup2(pipes[1], 1);
		close(pipes[1]);
		chdir("/var/grid-php/tools");
		if (execve(argv[0], argv, NULL) == -1) {
			exit(-1);
		}
	} else if (pid < 0) {
		close(pipes[0]);
		close(pipes[1]);
		return FALSE;
	}
	close(pipes[1]);
	*pout_len = 0;
	while ((tmp_len = read(pipes[0], buff_out
		+ *pout_len, 1024*1024 - *pout_len)) > 0) {
		*pout_len += tmp_len;
		if (*pout_len >= 1024*1024) {
			waitpid(pid, &status, 0);
			close(pipes[0]);
			return FALSE;
		}
	}
	buff_out[*pout_len] = '\0';
	close(pipes[0]);
	waitpid(pid, &status, 0);
	if (0 != status) {
		return FALSE;
	}
	return TRUE;
}

DATA_COLLECT* data_source_collect_init()
{
	DATA_COLLECT *pcollect;

	pcollect = (DATA_COLLECT*)malloc(sizeof(DATA_COLLECT));
	if (NULL == pcollect) {
		return NULL;
	}
	double_list_init(&pcollect->list);
	pcollect->pnode = NULL;
	return pcollect;
}

void data_source_collect_free(DATA_COLLECT *pcollect)
{
	DOUBLE_LIST_NODE *pnode;

	if (NULL == pcollect) {
		return;
	}

	while (pnode = double_list_get_from_head(&pcollect->list)) {
		free(pnode->pdata);
		free(pnode);
	}
	double_list_free(&pcollect->list);
	free(pcollect);
}

void data_source_collect_clear(DATA_COLLECT *pcollect)
{
	DOUBLE_LIST_NODE *pnode;

	if (NULL == pcollect) {
		return;
	}

	while (pnode = double_list_get_from_head(&pcollect->list)) {
		free(pnode->pdata);
		free(pnode);
	}
	pcollect->pnode = NULL;
}

int data_source_collect_total(DATA_COLLECT *pcollect)
{
	return double_list_get_nodes_num(&pcollect->list);
}

void data_source_collect_begin(DATA_COLLECT *pcollect)
{
	pcollect->pnode = double_list_get_head(&pcollect->list);

}

int data_source_collect_done(DATA_COLLECT *pcollect)
{
	if (NULL == pcollect || NULL == pcollect->pnode) {
		return 1;
	}
	return 0;
}

int data_source_collect_forward(DATA_COLLECT *pcollect)
{
	DOUBLE_LIST_NODE *pnode;


	pnode = double_list_get_after(&pcollect->list, pcollect->pnode);
	if (NULL == pnode) {
		pcollect->pnode = NULL;
		return -1;
	}
	pcollect->pnode = pnode;
	return 1;
}

void* data_source_collect_get_value(DATA_COLLECT *pcollect)
{
	if (NULL == pcollect || NULL == pcollect->pnode) {
		return NULL;
	}
	return pcollect->pnode->pdata;
}


void data_source_init()
{
	/* do nothing */
}

int data_source_run()
{

	/* do nothing */
	return 0;
}

int data_source_stop()
{
	/* do nothing */
	return 0;
}

void data_source_free()
{
	/* do nothing */
}

BOOL data_source_add_room(const char *domainname, const char *roomname)
{
	int out_len;
	char *argv[6];
	char tmp_buff[4096];

	argv[0] = "/usr/bin/php";
	argv[1] = "meeting_rooms.php";
	argv[2] = "add-room";
	argv[3] = (void*)domainname;
	argv[4] = (void*)roomname;
	argv[5] = NULL;
	return data_source_exec(argv, tmp_buff, &out_len);
}

BOOL data_source_del_room(const char *domainname, const char *roomname)
{
	int out_len;
	char *argv[6];
	char tmp_buff[4096];

	argv[0] = "/usr/bin/php";
	argv[1] = "meeting_rooms.php";
	argv[2] = "del-room";
	argv[3] = (void*)domainname;
	argv[4] = (void*)roomname;
	argv[5] = NULL;
	return data_source_exec(argv, tmp_buff, &out_len);
}

BOOL data_source_list_rooms(const char *domainname, DATA_COLLECT *pcollect)
{
	int out_len;
	char *argv[5];
	char *ptr, *ptr1;
	DOUBLE_LIST_NODE *pnode;
	char tmp_buff[1024*1024];

	argv[0] = "/usr/bin/php";
	argv[1] = "meeting_rooms.php";
	argv[2] = "list-room";
	argv[3] = (void*)domainname;
	argv[4] = NULL;
	if (FALSE == data_source_exec(argv, tmp_buff, &out_len)) {
		return FALSE;
	}
	ptr = tmp_buff;
	ptr1 = ptr;
	while (ptr = memchr(ptr, '\n', out_len - (ptr - tmp_buff))) {
		*ptr = '\0';
		ptr ++;
		pnode = malloc(sizeof(DOUBLE_LIST_NODE));
		if (NULL == pnode) {
			return FALSE;
		}
		pnode->pdata = strdup(ptr1);
		if (NULL == pnode->pdata) {
			return FALSE;
		}
		double_list_append_as_tail(&pcollect->list, pnode);
		ptr1 = ptr;
	}
	return TRUE;
}

BOOL data_source_list_permissions(const char *domainname,
	const char *roomname, DATA_COLLECT *pcollect)
{
	int out_len;
	char *argv[6];
	char *ptr, *ptr1;
	DOUBLE_LIST_NODE *pnode;
	char tmp_buff[1024*1024];

	argv[0] = "/usr/bin/php";
	argv[1] = "meeting_rooms.php";
	argv[2] = "list-permission";
	argv[3] = (void*)domainname;
	argv[4] = (void*)roomname;
	argv[5] = NULL;
	if (FALSE == data_source_exec(argv, tmp_buff, &out_len)) {
		return FALSE;
	}
	ptr = tmp_buff;
	ptr1 = ptr;
	while (ptr = memchr(ptr, '\n', out_len - (ptr - tmp_buff))) {
		*ptr = '\0';
		ptr ++;
		pnode = malloc(sizeof(DOUBLE_LIST_NODE));
		if (NULL == pnode) {
			return FALSE;
		}
		pnode->pdata = strdup(ptr1);
		if (NULL == pnode->pdata) {
			return FALSE;
		}
		double_list_append_as_tail(&pcollect->list, pnode);
		ptr1 = ptr;
	}
	return TRUE;
}

BOOL data_source_add_permission(const char *domainname,
	const char *roomname, const char *address)
{
	int out_len;
	char *argv[7];
	char tmp_buff[4096];

	argv[0] = "/usr/bin/php";
	argv[1] = "meeting_rooms.php";
	argv[2] = "add-permission";
	argv[3] = (void*)domainname;
	argv[4] = (void*)roomname;
	argv[5] = (void*)address;
	argv[6] = NULL;
	return data_source_exec(argv, tmp_buff, &out_len);
}

BOOL data_source_del_permission(const char *domainname,
	const char *roomname, const char *address)
{
	int out_len;
	char *argv[7];
	char tmp_buff[4096];

	argv[0] = "/usr/bin/php";
	argv[1] = "meeting_rooms.php";
	argv[2] = "del-permission";
	argv[3] = (void*)domainname;
	argv[4] = (void*)roomname;
	argv[5] = (void*)address;
	argv[6] = NULL;
	return data_source_exec(argv, tmp_buff, &out_len);
}
