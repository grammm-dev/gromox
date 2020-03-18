#include "adac_processor.h"
#include "mysql_adaptor.h"
#include "common_util.h"
#include "dac_client.h"
#include "ab_tree.h"
#include <stdio.h>

void adac_processor_init()
{
	/* do nothing */
}

int adac_processor_run()
{
	return 0;
}

int adac_processor_stop()
{
	return 0;
}

void adac_processor_free()
{
	/* do nothing */
}

uint32_t adac_processor_getaddressbook(
	const char *address, char *ab_address)
{
	int org_id;
	int domain_id;
	const char *pdomain;
	char hex_string[32];
	
	pdomain = strchr(address, '@');
	if (NULL == pdomain) {
		pdomain = address;
	} else {
		pdomain ++;
	}
	if (FALSE == mysql_adaptor_get_domain_ids(
		pdomain, &domain_id, &org_id)) {
		return FALSE;	
	}
	if (0 == org_id) {
		encode_hex_int(domain_id, hex_string);
		sprintf(ab_address, "/o=%s/ou=Exchange Administrative"
			" Group (FYDIBOHF23SPDLT)/cn=Share/cn=%s",
			common_util_get_org_name(), hex_string);
	} else {
		encode_hex_int(org_id, hex_string);
		sprintf(ab_address, "/o=%s/ou=Exchange Administrative"
			" Group (FYDIBOHF23SPDLT)/cn=Aggregation/cn=%s",
			common_util_get_org_name(), hex_string);
	}
	return EC_SUCCESS;
}

uint32_t adac_processor_getabhierarchy(
	const char *ab_address, DAC_VSET *pset)
{
	int base_id;
	int essdn_type;
	AB_BASE *pbase;
	int i, end_pos;
	DAC_VSET tmp_set;
	DOMAIN_NODE *pdnode;
	char tmp_buff[1024];
	uint8_t fake_false = 0;
	SINGLE_LIST_NODE *psnode;
	SIMPLE_TREE_NODE *ptnode;
	static uint64_t fake_type = 0xFF;
	
	essdn_type = common_util_get_essdn_type(ab_address);
	if (ESSDN_TYPE_DOMAIN != essdn_type
		&& ESSDN_TYPE_ORG != essdn_type) {
		return EC_NOT_SUPPORTED;	
	}
	if (FALSE == common_util_essdn_to_base_id(ab_address, &base_id)) {
		return EC_ERROR;
	}
	tmp_set.count = 0;
	tmp_set.pparray = common_util_alloc(sizeof(TPROPVAL_ARRAY*)*100);
	if (NULL == tmp_set.pparray) {
		return EC_OUT_OF_MEMORY;
	}
	pbase = ab_tree_get_base(base_id);
	if (NULL == pbase) {
		return EC_ERROR;
	}
	/* make gal item */
	tmp_set.pparray[tmp_set.count] =
		common_util_alloc(sizeof(DAC_VARRAY));
	if (NULL == tmp_set.pparray[tmp_set.count]) {
		ab_tree_put_base(pbase);
		return EC_OUT_OF_MEMORY;
	}
	sprintf(tmp_buff, "%s/cn=GAL", ab_address);
	tmp_set.pparray[tmp_set.count]->count = 6;
	tmp_set.pparray[tmp_set.count]->ppropval =
		common_util_alloc(6*sizeof(DAC_PROPVAL));
	if (NULL == tmp_set.pparray[tmp_set.count]->ppropval) {
		ab_tree_put_base(pbase);
		return EC_OUT_OF_MEMORY;
	}
	tmp_set.pparray[tmp_set.count]->ppropval[0].pname = "display_name";
	tmp_set.pparray[tmp_set.count]->ppropval[0].type = PROPVAL_TYPE_STRING;
	tmp_set.pparray[tmp_set.count]->ppropval[0].pvalue = "Global Address List";
	tmp_set.pparray[tmp_set.count]->ppropval[1].pname = "node_type";
	tmp_set.pparray[tmp_set.count]->ppropval[1].type = PROPVAL_TYPE_LONGLONG;
	/* 0xFF for node type is addressbook GAL */ 
	tmp_set.pparray[tmp_set.count]->ppropval[1].pvalue = &fake_type;
	tmp_set.pparray[tmp_set.count]->ppropval[2].pname = "has_sub";
	tmp_set.pparray[tmp_set.count]->ppropval[2].type = PROPVAL_TYPE_BYTE;
	tmp_set.pparray[tmp_set.count]->ppropval[2].pvalue = &fake_false;
	tmp_set.pparray[tmp_set.count]->ppropval[3].pname = "ex_address";
	tmp_set.pparray[tmp_set.count]->ppropval[3].type = PROPVAL_TYPE_STRING;
	tmp_set.pparray[tmp_set.count]->ppropval[3].pvalue =
								common_util_dup(tmp_buff);
	if (NULL == tmp_set.pparray[tmp_set.count]->ppropval[3].pvalue) {
		ab_tree_put_base(pbase);
		return EC_OUT_OF_MEMORY;
	}
	tmp_set.pparray[tmp_set.count]->ppropval[4].pname = "parent_address";
	tmp_set.pparray[tmp_set.count]->ppropval[4].type = PROPVAL_TYPE_STRING;
	tmp_set.pparray[tmp_set.count]->ppropval[4].pvalue =
							common_util_dup(ab_address);
	if (NULL == tmp_set.pparray[tmp_set.count]->ppropval[4].pvalue) {
		ab_tree_put_base(pbase);
		return EC_OUT_OF_MEMORY;
	}
	tmp_set.pparray[tmp_set.count]->ppropval[5].pname = "content_count";
	tmp_set.pparray[tmp_set.count]->ppropval[5].type = PROPVAL_TYPE_LONGLONG;
	tmp_set.pparray[tmp_set.count]->ppropval[5].pvalue =
						common_util_alloc(sizeof(uint64_t));
	if (NULL == tmp_set.pparray[tmp_set.count]->ppropval[5].pvalue) {
		ab_tree_put_base(pbase);
		return EC_OUT_OF_MEMORY;
	}
	*(uint64_t*)tmp_set.pparray[tmp_set.count]->ppropval[5].pvalue =
						single_list_get_nodes_num(&pbase->gal_list);							
	tmp_set.count ++;
	for (psnode=single_list_get_head(&pbase->list); NULL!=psnode;
		psnode=single_list_get_after(&pbase->list, psnode)) {
		pdnode = (DOMAIN_NODE*)psnode->pdata;
		ptnode = simple_tree_get_root(&pdnode->tree);
		if (FALSE == common_util_get_abhierarchy_from_node(
			ptnode, &tmp_set)) {
			ab_tree_put_base(pbase);
			return EC_ERROR;
		}
	}
	ab_tree_put_base(pbase);
	*pset = tmp_set;
	return EC_SUCCESS;
}

uint32_t adac_processor_getabcontent(const char *folder_address,
	uint32_t start_pos, uint16_t count, DAC_VSET *pset)
{
	int i;
	int org_id;
	int base_id;
	int class_id;
	int group_id;
	int domain_id;
	int essdn_type;
	uint32_t minid;
	AB_BASE *pbase;
	SIMPLE_TREE_NODE *pnode;
	SINGLE_LIST_NODE *psnode;
	static char* tmp_strs[] = {
		"node_type",
		"ex_address",
		"smtp_address",
		"display_name",
		"title",
		"bussiness_phone",
		"mobile_phone"
	};
	static STRING_ARRAY names = {7, tmp_strs};
	
	pset->count = 0;
	pset->pparray = common_util_alloc(count*sizeof(void*));
	if (NULL == pset->pparray) {
		return EC_OUT_OF_MEMORY;
	}
	essdn_type = common_util_get_essdn_type(folder_address);
	switch (essdn_type) {
	case ESSDN_TYPE_GROUP:
		if (FALSE == common_util_essdn_to_group_id(
			folder_address, &domain_id, &group_id)) {
			return EC_INVALID_PARAMETER;	
		}
		if (FALSE == mysql_adaptor_get_org_id(domain_id, &org_id)) {
			return EC_ERROR;
		}
		if (0 == org_id) {
			base_id = (-1)*domain_id; 
		} else {
			base_id = org_id;
		}
		pbase = ab_tree_get_base(base_id);
		if (NULL == pbase) {
			return EC_ERROR;
		}
		minid = ab_tree_make_minid(MINID_TYPE_GROUP, group_id);
		pnode = ab_tree_minid_to_node(pbase, minid);
		break;
	case ESSDN_TYPE_CLASS:
		if (FALSE == common_util_essdn_to_class_id(
			folder_address, &domain_id, &class_id)) {
			return EC_INVALID_PARAMETER;
		}
		if (FALSE == mysql_adaptor_get_org_id(domain_id, &org_id)) {
			return EC_ERROR;
		}
		if (0 == org_id) {
			base_id = (-1)*domain_id; 
		} else {
			base_id = org_id;
		}
		pbase = ab_tree_get_base(base_id);
		if (NULL == pbase) {
			return EC_ERROR;
		}
		minid = ab_tree_make_minid(MINID_TYPE_CLASS, class_id);
		pnode = ab_tree_minid_to_node(pbase, minid);
		break;
	case ESSDN_TYPE_GAL:
		if (FALSE == common_util_essdn_to_base_id(
			folder_address, &base_id)) {
			return EC_INVALID_PARAMETER;	
		}
		pbase = ab_tree_get_base(base_id);
		if (NULL == pbase) {
			return EC_ERROR;
		}
		for (i=0,psnode=single_list_get_head(&pbase->gal_list); NULL!=psnode;
			psnode=single_list_get_after(&pbase->gal_list, psnode),i++) {
			if (i < start_pos) {
				continue;
			}
			pset->pparray[pset->count] =
				common_util_alloc(sizeof(DAC_VARRAY));
			if (NULL == pset->pparray[pset->count]) {
				ab_tree_put_base(pbase);
				return FALSE;
			}
			if (FALSE == ab_tree_fetch_node_properties(psnode->pdata,
				&names, pset->pparray[pset->count])) {
				ab_tree_put_base(pbase);
				return FALSE;	
			}
			pset->count ++;
			if (pset->count == count) {
				break;
			}
		}
		ab_tree_put_base(pbase);
		return EC_SUCCESS;
	default:
		return EC_INVALID_PARAMETER;
	}
	if (NULL == pnode) {
		ab_tree_put_base(pbase);
		return EC_NOT_FOUND;
	}
	do {
		if (ab_tree_get_node_type(pnode) > 0x80) {
			continue;
		}
		pset->pparray[pset->count] =
			common_util_alloc(sizeof(DAC_VARRAY));
		if (NULL == pset->pparray[pset->count]) {
			ab_tree_put_base(pbase);
			return FALSE;
		}
		if (FALSE == ab_tree_fetch_node_properties(pnode,
			&names, pset->pparray[pset->count])) {
			ab_tree_put_base(pbase);
			return FALSE;	
		}
		pset->count ++;
		if (pset->count == count) {
			break;
		}
	} while (pnode=simple_tree_node_get_slibling(pnode));
	ab_tree_put_base(pbase);
	return EC_SUCCESS;
}

uint32_t adac_processor_restrictabcontent(const char *folder_address,
	const DAC_RES *prestrict, uint16_t max_count, DAC_VSET *pset)
{
	int i;
	int org_id;
	int base_id;
	int class_id;
	int group_id;
	int domain_id;
	int essdn_type;
	uint32_t minid;
	AB_BASE *pbase;
	LONG_ARRAY minids;
	SIMPLE_TREE_NODE *pnode;
	static char* tmp_strs[] = {
		"node_type",
		"ex_address",
		"smtp_address",
		"display_name",
		"title",
		"bussiness_phone",
		"mobile_phone"
	};
	static STRING_ARRAY names = {7, tmp_strs};
	
	essdn_type = common_util_get_essdn_type(folder_address);
	switch (essdn_type) {
	case ESSDN_TYPE_GROUP:
		if (FALSE == common_util_essdn_to_group_id(
			folder_address, &domain_id, &group_id)) {
			return EC_INVALID_PARAMETER;	
		}
		if (FALSE == mysql_adaptor_get_org_id(domain_id, &org_id)) {
			return EC_ERROR;
		}
		if (0 == org_id) {
			base_id = (-1)*domain_id; 
		} else {
			base_id = org_id;
		}
		pbase = ab_tree_get_base(base_id);
		if (NULL == pbase) {
			return EC_ERROR;
		}
		minid = ab_tree_make_minid(MINID_TYPE_GROUP, group_id);
		break;
	case ESSDN_TYPE_CLASS:
		if (FALSE == common_util_essdn_to_class_id(
			folder_address, &domain_id, &class_id)) {
			return EC_INVALID_PARAMETER;
		}
		if (FALSE == mysql_adaptor_get_org_id(domain_id, &org_id)) {
			return EC_ERROR;
		}
		if (0 == org_id) {
			base_id = (-1)*domain_id; 
		} else {
			base_id = org_id;
		}
		pbase = ab_tree_get_base(base_id);
		if (NULL == pbase) {
			return EC_ERROR;
		}
		minid = ab_tree_make_minid(MINID_TYPE_CLASS, class_id);
		break;
	case ESSDN_TYPE_GAL:
		if (FALSE == common_util_essdn_to_base_id(
			folder_address, &base_id)) {
			return EC_INVALID_PARAMETER;	
		}
		pbase = ab_tree_get_base(base_id);
		if (NULL == pbase) {
			return EC_ERROR;
		}
		minid = 0;
		break;
	default:
		return EC_INVALID_PARAMETER;
	}
	if (FALSE == ab_tree_match_minids(
		pbase, minid, prestrict, &minids)) {
		ab_tree_put_base(pbase);
		return EC_ERROR;	
	}
	pset->count = 0;
	if (minids.count < max_count) {
		max_count = minids.count;
	}
	if (0 == max_count) {
		pset->pparray = NULL;
		ab_tree_put_base(pbase);
		return EC_SUCCESS;
	}
	pset->pparray = common_util_alloc(max_count*sizeof(void*));
	if (NULL == pset->pparray) {
		ab_tree_put_base(pbase);
		return EC_OUT_OF_MEMORY;
	}
	for (i=0; i<max_count; i++) {
		pset->pparray[pset->count] =
			common_util_alloc(sizeof(DAC_VARRAY));
		if (NULL == pset->pparray[pset->count]) {
			ab_tree_put_base(pbase);
			return EC_ERROR;
		}
		pnode = ab_tree_minid_to_node(pbase, minids.pl[i]);
		if (NULL == pnode) {
			ab_tree_put_base(pbase);
			return EC_ERROR;
		}
		if (FALSE == ab_tree_fetch_node_properties(pnode,
			&names, pset->pparray[pset->count])) {
			ab_tree_put_base(pbase);
			return EC_ERROR;
		}
		pset->count ++;
	}
	ab_tree_put_base(pbase);
	return EC_SUCCESS;
}

uint32_t adac_processor_getexaddress(
	const char *address, char *ex_address)
{
	int org_id;
	int user_id;
	char *ptoken;
	int domain_id;
	int address_type;
	char username[256];
	char hex_string[32];
	char hex_string1[32];
	
	if (NULL == strchr(address, '@')) {
		if (FALSE == mysql_adaptor_get_domain_ids(
			address, &domain_id, &org_id)) {
			return EC_NOT_FOUND;	
		}
		encode_hex_int(domain_id, hex_string);
		sprintf(ex_address, "/o=%s/ou=Exchange Administrative"
			" Group (FYDIBOHF23SPDLT)/cn=Share/cn=%s",
			common_util_get_org_name(), hex_string);
	} else {
		if (FALSE == mysql_adaptor_get_user_ids(address,
			&user_id, &domain_id, &address_type)) {
			return EC_NOT_FOUND;	
		}
		strncpy(username, address, sizeof(username));
		ptoken = strchr(username, '@');
		*ptoken = '\0';
		encode_hex_int(user_id, hex_string);
		encode_hex_int(domain_id, hex_string1);
		sprintf(ex_address, "/o=%s/ou=Exchange Administrative"
			"Group (FYDIBOHF23SPDLT)/cn=Recipients/cn=%s%s-%s",
			common_util_get_org_name(), hex_string1, hex_string,
			username);
	}
	return EC_SUCCESS;
}

uint32_t adac_processor_getexaddresstype(
	const char *address, uint8_t *paddress_type)
{
	*paddress_type = common_util_get_essdn_type(address);
	return EC_SUCCESS;
}

uint32_t adac_processor_checkmlistinclude(const char *mlist_address,
	const char *account_address, BOOL *pb_include)
{
	AB_BASE *pbase;
	uint32_t minid;
	SIMPLE_TREE_NODE *pnode;
	DAC_VARRAY tmp_propvals;
	char mlist_smtp_address[256];
	char account_smtp_address[256];
	int user_id, domain_id, base_id;
	static char *pname = "smtp_address";
	static STRING_ARRAY names = {1, &pname};
	
	if (NULL == strchr(mlist_address, '@')) {
		strncpy(mlist_smtp_address, mlist_address,
						sizeof(mlist_smtp_address));
	} else {
		if (ESSDN_TYPE_USER != common_util_get_essdn_type(
			mlist_address)) {
			return EC_INVALID_PARAMETER;
		}
		if (FALSE == common_util_essdn_to_user_ids(
			mlist_address, &domain_id, &user_id)) {
			return EC_INVALID_PARAMETER;	
		}
		base_id = (-1)*domain_id;
		pbase = ab_tree_get_base(base_id);
		if (NULL == pbase) {
			return EC_ERROR;
		}
		minid = ab_tree_make_minid(MINID_TYPE_ADDRESS, user_id);
		pnode = ab_tree_minid_to_node(pbase, minid);
		if (NULL == pnode) {
			ab_tree_put_base(pbase);
			return EC_INVALID_PARAMETER;
		}
		if (FALSE == ab_tree_fetch_node_properties(pnode,
			&names, &tmp_propvals) || 0 == tmp_propvals.count) {
			ab_tree_put_base(pbase);
			return EC_ERROR;	
		}
		ab_tree_put_base(pbase);
		strncmp(mlist_smtp_address,
			tmp_propvals.ppropval[0].pvalue,
			sizeof(mlist_smtp_address));
	}
	if (NULL == strchr(account_address, '@')) {
		strncpy(account_smtp_address, account_address,
						sizeof(account_smtp_address));
	} else {
		if (ESSDN_TYPE_USER != common_util_get_essdn_type(
			mlist_address)) {
			return EC_INVALID_PARAMETER;
		}
		if (FALSE == common_util_essdn_to_user_ids(
			mlist_address, &domain_id, &user_id)) {
			return EC_INVALID_PARAMETER;	
		}
		base_id = (-1)*domain_id;
		pbase = ab_tree_get_base(base_id);
		if (NULL == pbase) {
			return EC_ERROR;
		}
		minid = ab_tree_make_minid(MINID_TYPE_ADDRESS, user_id);
		pnode = ab_tree_minid_to_node(pbase, minid);
		if (NULL == pnode) {
			ab_tree_put_base(pbase);
			return EC_INVALID_PARAMETER;
		}
		if (FALSE == ab_tree_fetch_node_properties(pnode,
			&names, &tmp_propvals) || 0 == tmp_propvals.count) {
			ab_tree_put_base(pbase);
			return EC_ERROR;	
		}
		ab_tree_put_base(pbase);
		strncmp(account_smtp_address,
			tmp_propvals.ppropval[0].pvalue,
			sizeof(account_smtp_address));
	}
	*pb_include = mysql_adaptor_check_mlist_include(
			mlist_smtp_address, account_smtp_address);
	return EC_SUCCESS;
}

uint32_t adac_processor_allocdir(uint8_t type, char *dir)
{
	//TODO
}

uint32_t adac_processor_freedir(const char *address)
{
	//TODO
}

uint32_t adac_processor_setpropvals(const char *address,
	const char *pnamespace, const DAC_VARRAY *ppropvals)
{
	char dir[256];
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_setpropvals(dir, pnamespace, ppropvals);
}

uint32_t adac_processor_getpropvals(const char *address,
	const char *pnamespace, const STRING_ARRAY *pnames,
	DAC_VARRAY *ppropvals)
{
	int base_id;
	int user_id;
	char dir[256];
	int domain_id;
	int essdn_type;
	AB_BASE *pbase;
	uint32_t minid;
	int address_type;
	SIMPLE_TREE_NODE *pnode;
	
	if (NULL != pnamespace && '\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	if (NULL != strchr(address, '@')) {
		if (FALSE == mysql_adaptor_get_user_ids(address,
			&user_id, &domain_id, &address_type)) {
			return EC_ERROR;	
		}
		if (ADDRESS_TYPE_VIRTUAL == address_type) {
			return EC_NOT_SUPPORTED;
		}
		if (ADDRESS_TYPE_MLIST == address_type && NULL != pnamespace) {
			return EC_NOT_SUPPORTED;
		}
		base_id = (-1)*domain_id;
		pbase = ab_tree_get_base(base_id);
		if (NULL == pbase) {
			return EC_ERROR;
		}
		minid = ab_tree_make_minid(MINID_TYPE_ADDRESS, user_id);
		pnode = ab_tree_minid_to_node(pbase, minid);
	} else {
		essdn_type = common_util_get_essdn_type(address);
		switch (essdn_type) {
		case ESSDN_TYPE_DOMAIN:
			if (FALSE == common_util_essdn_to_base_id(address, &base_id)) {
				return EC_INVALID_PARAMETER;
			}
			pbase = ab_tree_get_base(base_id);
			if (NULL == pbase) {
				return EC_ERROR;
			}
			minid = ab_tree_make_minid(MINID_TYPE_DOMAIN, base_id);
			pnode = ab_tree_minid_to_node(pbase, minid);
			break;
		case ESSDN_TYPE_GROUP:
		case ESSDN_TYPE_CLASS:
			/* TODO support simple dac node later */
			return EC_NOT_SUPPORTED;
		case ESSDN_TYPE_USER:
			if (FALSE == common_util_essdn_to_user_ids(
				address, &domain_id,  &user_id)) {
				return EC_INVALID_PARAMETER;	
			}
			base_id = (-1)*domain_id;
			pbase = ab_tree_get_base(base_id);
			if (NULL == pbase) {
				return EC_ERROR;
			}
			minid = ab_tree_make_minid(MINID_TYPE_ADDRESS, user_id);
			pnode = ab_tree_minid_to_node(pbase, minid);
			break;
		default:
			return EC_NOT_SUPPORTED;
		}
	}
	if (NULL == pnode) {
		ab_tree_put_base(pbase);
		return EC_ERROR;
	}
	if (NULL == pnamespace) {
		if (FALSE == ab_tree_fetch_node_properties(
			pnode, pnames, ppropvals)) {
			ab_tree_put_base(pbase);
			return EC_ERROR;
		}
		ab_tree_put_base(pbase);
		return EC_SUCCESS;
	}
	if (FALSE == ab_tree_get_node_dir(pnode, dir)) {
		ab_tree_put_base(pbase);
		return EC_ERROR;
	}
	ab_tree_put_base(pbase);
	return dac_client_getpropvals(dir, pnamespace, pnames, ppropvals);
}

uint32_t adac_processor_deletepropvals(const char *address,
	const char *pnamespace, const STRING_ARRAY *pnames)
{
	char dir[256];
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_deletepropvals(dir, pnamespace, pnames);
}

uint32_t adac_processor_opentable(const char *address,
	const char *pnamespace, const char *ptablename,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id)
{
	char dir[256];
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_opentable(dir, pnamespace,
		ptablename, flags, puniquename, ptable_id);
}

uint32_t adac_processor_opencelltable(const char *address,
	uint64_t row_instance, const char *pcolname,
	uint32_t flags, const char *puniquename,
	uint32_t *ptable_id)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_opencelltable(dir, row_instance,
			pcolname, flags, puniquename, ptable_id);
}

uint32_t adac_processor_restricttable(const char *address,
	uint32_t table_id, const DAC_RES *prestrict)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_restricttable(dir, table_id, prestrict);
}

uint32_t adac_processor_updatecells(const char *address,
	uint64_t row_instance, const DAC_VARRAY *pcells)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_updatecells(dir, row_instance, pcells);
}

uint32_t adac_processor_sumtable(const char *address,
	uint32_t table_id, uint32_t *pcount)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_sumtable(dir, table_id, pcount);
}

uint32_t adac_processor_queryrows(const char *address,
	uint32_t table_id, uint32_t start_pos,
	uint32_t row_count, DAC_VSET *pset)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_queryrows(dir, table_id, start_pos, row_count, pset);
}

uint32_t adac_processor_insertrows(const char *address,
	uint32_t table_id, uint32_t flags, const DAC_VSET *pset)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_insertrows(dir, table_id, flags, pset);
}
	
uint32_t adac_processor_deleterow(const char *address, uint64_t row_instance)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_deleterow(dir, row_instance);
}

uint32_t adac_processor_deletecells(const char *address,
	uint64_t row_instance, const STRING_ARRAY *pcolnames)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_deletecells(dir, row_instance, pcolnames);
}

uint32_t adac_processor_deletetable(const char *address,
	const char *pnamespace, const char *ptablename)
{
	char dir[256];
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_deletetable(dir, pnamespace, ptablename);
}

uint32_t adac_processor_closetable(
	const char *address, uint32_t table_id)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_closetable(dir, table_id);
}

uint32_t adac_processor_matchrow(const char *address,
	uint32_t table_id, uint16_t type,
	const void *pvalue, DAC_VARRAY *prow)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_matchrow(dir, table_id, type, pvalue, prow);
}

uint32_t adac_processor_getnamespaces(const char *address,
	STRING_ARRAY *pnamespaces)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_getnamespaces(dir, pnamespaces);
}

uint32_t adac_processor_getpropnames(const char *address,
	const char *pnamespace, STRING_ARRAY *ppropnames)
{
	char dir[256];
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_getpropnames(dir, pnamespace, ppropnames);
}

uint32_t adac_processor_gettablenames(const char *address,
	const char *pnamespace, STRING_ARRAY *ptablenames)
{
	char dir[256];
	
	if ('\0' == pnamespace[0]) {
		return EC_INVALID_PARAMETER;
	}
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_gettablenames(dir, pnamespace, ptablenames);
}

uint32_t adac_processor_readfile(const char *address, const char *path,
	const char *file_name, uint64_t offset, uint32_t length,
	BINARY *pcontent_bin)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_readfile(dir, path,
		file_name, offset, length, pcontent_bin);
}

uint32_t adac_processor_appendfile(const char *address,
	const char *path, const char *file_name,
	const BINARY *pcontent_bin)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_appendfile(dir, path, file_name, pcontent_bin);
}

uint32_t adac_processor_removefile(const char *address,
	const char *path, const char *file_name)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_removefile(dir, path, file_name);
}

uint32_t adac_processor_statfile(const char *address,
	const char *path, const char *file_name,
	uint64_t *psize)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_statfile(dir, path, file_name, psize);
}

uint32_t adac_processor_checkrow(const char *dir,
	const char *pnamespace, const char *ptablename,
	uint16_t type, const void *pvalue)
{
	return dac_client_checkrow(dir, pnamespace, ptablename, type, pvalue);
}

uint32_t adac_processor_phprpc(const char *address,
	const char *script_path, const BINARY *pbin_in,
	BINARY *pbin_out)
{
	char dir[256];
	
	if (FALSE == common_util_get_dir(address, dir)) {
		return EC_ERROR;
	}
	return dac_client_phprpc(dir, address, script_path, pbin_in, pbin_out);
}
