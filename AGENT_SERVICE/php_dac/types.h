#ifndef _H_TYPES_
#define _H_TYPES_
#include <stdint.h>

#define EC_SUCCESS									0x00000000
#define EC_ERROR									0x80004005
#define EC_RPC_FAIL									0x80040115
#define EC_ACCESS_DENIED							0x80070005
#define EC_NOT_SUPPORTED							0x80040102
#define EC_OUT_OF_MEMORY							0x8007000E
#define EC_RPC_FORMAT								0x000004B6
#define EC_INVALID_OBJECT							0x80040108
#define EC_INVALID_PARAMETER						0x80070057
#define EC_NOT_IMPLEMENTED							0x80040FFF
#define EC_NOT_FOUND								0x8004010F

#define PROPVAL_TYPE_DOUBLE							0x0005
#define PROPVAL_TYPE_BYTE							0x000b
#define PROPVAL_TYPE_OBJECT							0x000d
#define PROPVAL_TYPE_LONGLONG						0x0014
#define PROPVAL_TYPE_STRING							0x001e
#define PROPVAL_TYPE_GUID							0x0048
#define PROPVAL_TYPE_BINARY							0x0102
#define PROPVAL_TYPE_LONGLONG_ARRAY					0x1014
#define PROPVAL_TYPE_STRING_ARRAY					0x101e

typedef struct _GUID {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_version;
	uint8_t clock_seq[2];
	uint8_t node[6];
} GUID;

typedef struct _BINARY {
	uint32_t cb;
	uint8_t *pb;
} BINARY;

typedef struct _STRING_ARRAY {
	uint32_t count;
	char **ppstr;
} STRING_ARRAY;

typedef struct _LONGLONG_ARRAY {
	uint32_t count;
	uint64_t *pll;
} LONGLONG_ARRAY;


#define DAC_DIR_TYPE_SIMPLE							0
#define DAC_DIR_TYPE_PRIVATE						1
#define DAC_DIR_TYPE_PUBLIC							2


typedef struct _DAC_PROPVAL {
	char *pname;
	uint16_t type;
	void *pvalue;
} DAC_PROPVAL;

typedef struct _DAC_VARRAY {
	uint16_t count;
	DAC_PROPVAL *ppropval;
} DAC_VARRAY;

typedef struct _DAC_VSET {
	uint16_t count;
	DAC_VARRAY **pparray;
} DAC_VSET;

#define DAC_RES_TYPE_AND							0x00
#define DAC_RES_TYPE_OR								0x01
#define DAC_RES_TYPE_NOT							0x02
#define DAC_RES_TYPE_CONTENT						0x03
#define DAC_RES_TYPE_PROPERTY						0x04
#define DAC_RES_TYPE_EXIST							0x05
#define DAC_RES_TYPE_SUBOBJ							0x06

typedef struct _DAC_RES {
	uint8_t rt;
	void *pres;
} DAC_RES;

typedef struct _DAC_RES_AND_OR {
	uint32_t count;
	DAC_RES *pres;
} DAC_RES_AND_OR;

typedef struct _DAC_RES_NOT {
	DAC_RES res;
} DAC_RES_NOT;

#define DAC_FL_FULLSTRING							0x00
#define DAC_FL_SUBSTRING							0x01
#define DAC_FL_PREFIX								0x02
#define DAC_FL_ICASE								0x10

typedef struct _DAC_RES_CONTENT {
	uint8_t fl;
	DAC_PROPVAL propval;
} DAC_RES_CONTENT;

typedef struct _DAC_RES_PROPERTY {
	uint8_t relop;
	DAC_PROPVAL propval;
} DAC_RES_PROPERTY;

typedef struct _DAC_RES_EXIST {
	char *pname;
} DAC_RES_EXIST;

typedef struct _DAC_RES_SUBOBJ {
	char *pname;
	DAC_RES res;
} DAC_RES_SUBOBJ;

#define DAC_DIR_TYPE_SIMPLE							0
#define DAC_DIR_TYPE_PRIVATE						1
#define DAC_DIR_TYPE_PUBLIC							2

#endif /* _H_TYPES_ */
