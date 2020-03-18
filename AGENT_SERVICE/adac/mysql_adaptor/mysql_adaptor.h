#ifndef _H_MYSQL_ADAPTOR_
#define _H_MYSQL_ADAPTOR_
#include "mem_file.h"

#define ADDRESS_TYPE_NORMAL				0
#define ADDRESS_TYPE_ALIAS				1
#define ADDRESS_TYPE_MLIST				2
#define ADDRESS_TYPE_VIRTUAL			3
/* composd value, not in database, means ADDRESS_TYPE_NORMAL and SUB_TYPE_ROOM */
#define ADDRESS_TYPE_ROOM				4
/* composd value, not in database, means ADDRESS_TYPE_NORMAL and SUB_TYPE_EQUIPMENT */
#define ADDRESS_TYPE_EQUIPMENT			5

void mysql_adaptor_init(int conn_num, const char *host, int port,
	const char *user, const char *password, const char *db_name,
	int timeout);

int mysql_adaptor_run();

int mysql_adaptor_stop();

void mysql_adaptor_free();

BOOL mysql_adaptor_get_user_ids(const char *username,
	int *puser_id, int *pdomain_id, int *paddress_type);

BOOL mysql_adaptor_get_user_dir(const char *username, char *maildir);

BOOL mysql_adaptor_get_domain_ids(const char *domainname,
	int *pdomain_id, int *porg_id);

BOOL mysql_adaptor_get_org_id(int domain_id, int *porg_id);

BOOL mysql_adaptor_get_org_domains(int org_id, MEM_FILE *pfile);

BOOL mysql_adaptor_get_domain_info(int domain_id, char *name,
	char *homedir, char *title, char *address, int *pprivilege_bits);

BOOL mysql_adaptor_get_domain_groups(int domain_id, MEM_FILE *pfile);

BOOL mysql_adaptor_get_group_classes(int group_id, MEM_FILE *pfile);

BOOL mysql_adaptor_get_sub_classes(int class_id, MEM_FILE *pfile);

int mysql_adaptor_get_class_users(int class_id, MEM_FILE *pfile);

int mysql_adaptor_get_group_users(int group_id, MEM_FILE *pfile);

int mysql_adaptor_get_domain_users(int domain_id, MEM_FILE *pfile);

BOOL mysql_adaptor_check_mlist_include(
	const char *mlist_name, const char *account);

#endif
