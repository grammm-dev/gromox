#ifndef _H_AB_TREE_
#define _H_AB_TREE_
#include "simple_tree.h"
#include "single_list.h"
#include "common_util.h"
#include "int_hash.h"
#include "mem_file.h"
#include <stdint.h>
#include <time.h>

/* PROP_TAG_CONTAINERFLAGS values */
#define	AB_RECIPIENTS						0x1
#define	AB_SUBCONTAINERS					0x2
#define	AB_UNMODIFIABLE						0x8

#define NODE_TYPE_DOMAIN					0x81
#define NODE_TYPE_GROUP						0x82
#define NODE_TYPE_CLASS						0x83
#define NODE_TYPE_PERSOPN					0x1
#define NODE_TYPE_MLIST						0x2
#define NODE_TYPE_ROOM						0x3
#define NODE_TYPE_EQUIPMENT					0x4
#define NODE_TYPE_FOLDER					0x5

#define MINID_TYPE_ADDRESS					0x0
#define MINID_TYPE_DOMAIN					0x4
#define MINID_TYPE_GROUP					0x5
#define MINID_TYPE_CLASS					0x6

typedef struct _DOMAIN_NODE {
	SINGLE_LIST_NODE node;
	int domain_id;
	SIMPLE_TREE tree;
} DOMAIN_NODE;

typedef struct _AB_BASE {
	volatile int status;
	volatile int reference;
	time_t load_time;
	int base_id;
	SINGLE_LIST list;
	SINGLE_LIST gal_list;
	INT_HASH_TABLE *phash;
} AB_BASE;


void ab_tree_init(int base_size, int cache_interval, int file_blocks);

int ab_tree_run();

int ab_tree_stop();

void ab_tree_free();

AB_BASE* ab_tree_get_base(int base_id);

void ab_tree_put_base(AB_BASE *pbase);

BOOL ab_tree_node_to_dn(SIMPLE_TREE_NODE *pnode, char *pbuff, int length);

uint32_t ab_tree_make_minid(uint8_t type, int value);

uint8_t ab_tree_get_minid_type(uint32_t minid);

int ab_tree_get_minid_value(uint32_t minid);

SIMPLE_TREE_NODE* ab_tree_minid_to_node(AB_BASE *pbase, uint32_t minid);

SIMPLE_TREE_NODE* ab_tree_guid_to_node(
	AB_BASE *pbase, GUID guid);

uint32_t ab_tree_get_node_minid(SIMPLE_TREE_NODE *pnode);

uint8_t ab_tree_get_node_type(SIMPLE_TREE_NODE *pnode);

BOOL ab_tree_has_child(SIMPLE_TREE_NODE *pnode);

BOOL ab_tree_get_node_dir(SIMPLE_TREE_NODE *pnode, char *dir);

BOOL ab_tree_fetch_node_properties(SIMPLE_TREE_NODE *pnode,
	const STRING_ARRAY *pnames, DAC_VARRAY *ppropvals);

BOOL ab_tree_resolvename(AB_BASE *pbase,
	char *pstr, SINGLE_LIST *presult_list);

BOOL ab_tree_match_minids(AB_BASE *pbase, uint32_t container_id,
	const DAC_RES *pfilter, LONG_ARRAY *pminids);

#endif /* _H_AB_TREE_ */
