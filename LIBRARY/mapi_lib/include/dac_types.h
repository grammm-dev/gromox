#ifndef _H_DAC_TYPES_
#define _H_DAC_TYPES_

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


#endif /* _H_DAC_TYPES_ */
