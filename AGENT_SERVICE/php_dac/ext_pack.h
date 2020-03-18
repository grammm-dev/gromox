#ifndef _H_EXT_PACK_
#define _H_EXT_PACK_
#include "types.h"
#include "php.h"

typedef struct _PULL_CTX {
	const uint8_t *data;
	uint32_t data_size;
	uint32_t offset;
} PULL_CTX;

typedef struct _PUSH_CTX {
	uint8_t *data;
	uint32_t alloc_size;
	uint32_t offset;
} PUSH_CTX;


#define ext_pack_pull_bool	ext_pack_pull_uint8
#define ext_pack_pusg_bool	ext_pack_push_uint8

void ext_pack_pull_init(PULL_CTX *pctx,
	const uint8_t *pdata, uint32_t data_size);
	
zend_bool ext_pack_pull_advance(PULL_CTX *pctx, uint32_t size);

zend_bool ext_pack_pull_uint8(PULL_CTX *pctx, uint8_t *v);

zend_bool ext_pack_pull_uint16(PULL_CTX *pctx, uint16_t *v);

zend_bool ext_pack_pull_uint32(PULL_CTX *pctx, uint32_t *v);

zend_bool ext_pack_pull_uint64(PULL_CTX *pctx, uint64_t *v);

zend_bool ext_pack_pull_double(PULL_CTX *pctx, double *v);

zend_bool ext_pack_pull_bytes(PULL_CTX *pctx, uint8_t *data, uint32_t n);

zend_bool ext_pack_pull_guid(PULL_CTX *pctx, GUID *r);

zend_bool ext_pack_pull_string(PULL_CTX *pctx, char **ppstr);

zend_bool ext_pack_pull_binary(PULL_CTX *pctx, BINARY *r);

zend_bool ext_pack_pull_longlong_array(PULL_CTX *pctx, LONGLONG_ARRAY *r);

zend_bool ext_pack_pull_string_array(PULL_CTX *pctx, STRING_ARRAY *r);

zend_bool ext_pack_pull_propval(PULL_CTX *pctx, uint16_t type, void **ppval);

zend_bool ext_pack_pull_dac_propval(PULL_CTX *pctx, DAC_PROPVAL *r);

zend_bool ext_pack_pull_dac_varray(PULL_CTX *pctx, DAC_VARRAY *r);

zend_bool ext_pack_pull_dac_vset(PULL_CTX *pctx, DAC_VSET *r);

zend_bool ext_pack_pull_dac_res(PULL_CTX *pctx, DAC_RES *r);

zend_bool ext_pack_push_init(PUSH_CTX *pctx);

void ext_pack_push_free(PUSH_CTX *pctx);

zend_bool ext_pack_push_advance(PUSH_CTX *pctx, uint32_t size);

zend_bool ext_pack_push_bytes(PUSH_CTX *pctx,
	const uint8_t *pdata, uint32_t n);

zend_bool ext_pack_push_uint8(PUSH_CTX *pctx, uint8_t v);

zend_bool ext_pack_push_uint16(PUSH_CTX *pctx, uint16_t v);

zend_bool ext_pack_push_uint32(PUSH_CTX *pctx, uint32_t v);

zend_bool ext_pack_push_uint64(PUSH_CTX *pctx, uint64_t v);

zend_bool ext_pack_push_double(PUSH_CTX *pctx, double v);

zend_bool ext_pack_push_binary(PUSH_CTX *pctx, const BINARY *r);

zend_bool ext_pack_push_guid(PUSH_CTX *pctx, const GUID *r);

zend_bool ext_pack_push_string(PUSH_CTX *pctx, const char *pstr);

zend_bool ext_pack_push_longlong_array(
	PUSH_CTX *pctx, const LONGLONG_ARRAY *r);

zend_bool ext_pack_push_string_array(
	PUSH_CTX *pctx, const STRING_ARRAY *r);

zend_bool ext_pack_push_propval(PUSH_CTX *pctx,
	uint16_t type, const void *pval);

zend_bool ext_pack_push_dac_propval(PUSH_CTX *pctx, const DAC_PROPVAL *r);

zend_bool ext_pack_push_dac_varray(PUSH_CTX *pctx, const DAC_VARRAY *r);

zend_bool ext_pack_push_dac_vset(PUSH_CTX *pctx, const DAC_VSET *r);

zend_bool ext_pack_push_dac_res(PUSH_CTX *pctx, const DAC_RES *r);

#endif /* _H_EXT_PACK_ */
