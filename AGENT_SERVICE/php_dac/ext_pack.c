#include "ext_pack.h"
#include <stdlib.h>
#include <string.h>
#include <iconv.h>

#define CVAL(buf, pos) ((unsigned int)(((const uint8_t *)(buf))[pos]))
#define CVAL_NC(buf, pos) (((uint8_t *)(buf))[pos])
#define PVAL(buf, pos) (CVAL(buf,pos))
#define SCVAL(buf, pos, val) (CVAL_NC(buf,pos) = (val))
#define SVAL(buf, pos) (PVAL(buf,pos)|PVAL(buf,(pos)+1)<<8)
#define IVAL(buf, pos) (SVAL(buf,pos)|SVAL(buf,(pos)+2)<<16)
#define IVALS(buf, pos) ((int32_t)IVAL(buf,pos))
#define SSVALX(buf, pos, val) (CVAL_NC(buf,pos)=(uint8_t)((val)&0xFF),CVAL_NC(buf,pos+1)=(uint8_t)((val)>>8))
#define SIVALX(buf, pos, val) (SSVALX(buf,pos,val&0xFFFF),SSVALX(buf,pos+2,val>>16))
#define SSVAL(buf, pos, val) SSVALX((buf),(pos),((uint16_t)(val)))
#define SIVAL(buf, pos, val) SIVALX((buf),(pos),((uint32_t)(val)))
#define SIVALS(buf, pos, val) SIVALX((buf),(pos),((int32_t)(val)))

#define GROWING_BLOCK_SIZE				0x1000

void ext_pack_pull_init(PULL_CTX *pctx,
	const uint8_t *pdata, uint32_t data_size)
{
	pctx->data = pdata;
	pctx->data_size = data_size;
	pctx->offset = 0;
}

zend_bool ext_pack_pull_advance(PULL_CTX *pctx, uint32_t size)
{
	pctx->offset += size;
	if (pctx->offset > pctx->data_size) {
		return 0;
	}
	return 1;
}

zend_bool ext_pack_pull_uint8(PULL_CTX *pctx, uint8_t *v)
{
	if (pctx->data_size < sizeof(uint8_t) ||
		pctx->offset + sizeof(uint8_t) > pctx->data_size) {
		return 0;
	}
	*v = CVAL(pctx->data, pctx->offset);
	pctx->offset += sizeof(uint8_t);
	return 1;
}

zend_bool ext_pack_pull_uint16(PULL_CTX *pctx, uint16_t *v)
{
	if (pctx->data_size < sizeof(uint16_t) ||
		pctx->offset + sizeof(uint16_t) > pctx->data_size) {
		return 0;
	}
	*v = SVAL(pctx->data, pctx->offset);
	pctx->offset += sizeof(uint16_t);
	return 1;
}

zend_bool ext_pack_pull_uint32(PULL_CTX *pctx, uint32_t *v)
{
	if (pctx->data_size < sizeof(uint32_t) ||
		pctx->offset + sizeof(uint32_t) > pctx->data_size) {
		return 0;
	}
	*v = IVAL(pctx->data, pctx->offset);
	pctx->offset += sizeof(uint32_t);
	return 1;
}

zend_bool ext_pack_pull_uint64(PULL_CTX *pctx, uint64_t *v)
{
	if (pctx->data_size < sizeof(uint64_t) ||
		pctx->offset + sizeof(uint64_t) > pctx->data_size) {
		return 0;
	}
	*v = IVAL(pctx->data, pctx->offset);
	*v |= (uint64_t)(IVAL(pctx->data, pctx->offset+4)) << 32;
	pctx->offset += sizeof(uint64_t);
	return 1;
}

zend_bool ext_pack_pull_double(PULL_CTX *pctx, double *v)
{
	if (pctx->data_size < sizeof(double) ||
		pctx->offset + sizeof(double) > pctx->data_size) {
		return 0;
	}
	memcpy(v, pctx->data + pctx->offset, sizeof(double));
	pctx->offset += sizeof(double);
	return 1;
}

zend_bool ext_pack_pull_bytes(PULL_CTX *pctx, uint8_t *data, uint32_t n)
{
	if (pctx->data_size < n || pctx->offset + n > pctx->data_size) {
		return 0;
	}
	memcpy(data, pctx->data + pctx->offset, n);
	pctx->offset += n;
	return 1;
}

zend_bool ext_pack_pull_guid(PULL_CTX *pctx, GUID *r)
{
	if (!ext_pack_pull_uint32(pctx, &r->time_low)) {
		return 0;
	}
	if (!ext_pack_pull_uint16(pctx, &r->time_mid)) {
		return 0;
	}
	if (!ext_pack_pull_uint16(pctx, &r->time_hi_and_version)) {
		return 0;
	}
	if (!ext_pack_pull_bytes(pctx, r->clock_seq, 2)) {
		return 0;
	}
	return ext_pack_pull_bytes(pctx, r->node, 6);
}

zend_bool ext_pack_pull_string(PULL_CTX *pctx, char **ppstr)
{
	int len;
	
	if (pctx->offset >= pctx->data_size) {
		return 0;
	}
	len = strnlen(pctx->data + pctx->offset,
			pctx->data_size - pctx->offset);
	if (len + 1 > pctx->data_size - pctx->offset) {
		return 0;
	}
	len ++;
	*ppstr = emalloc(len);
	if (NULL == *ppstr) {
		return 0;
	}
	memcpy(*ppstr, pctx->data + pctx->offset, len);
	return ext_pack_pull_advance(pctx, len);
}

zend_bool ext_pack_pull_binary(PULL_CTX *pctx, BINARY *r)
{
	if (!ext_pack_pull_uint32(pctx, &r->cb)) {
		return 0;
	}
	if (!r->cb) {
		r->pb = NULL;
		return 1;
	}
	r->pb = emalloc(r->cb);
	if (NULL == r->pb) {
		return 0;
	}
	return ext_pack_pull_bytes(pctx, r->pb, r->cb);
}

zend_bool ext_pack_pull_longlong_array(PULL_CTX *pctx, LONGLONG_ARRAY *r)
{
	int i;
	
	if (!ext_pack_pull_uint32(pctx, &r->count)) {
		return 0;
	}
	if (!r->count) {
		r->pll = NULL;
		return 1;
	}
	r->pll = emalloc(sizeof(uint64_t)*r->count);
	if (NULL == r->pll) {
		return 0;
	}
	for (i=0; i<r->count; i++) {
		if (!ext_pack_pull_uint64(pctx, &r->pll[i])) {
			return 0;
		}
	}
	return 1;
}

zend_bool ext_pack_pull_string_array(PULL_CTX *pctx, STRING_ARRAY *r)
{
	int i;
	
	if (!ext_pack_pull_uint32(pctx, &r->count)) {
		return 0;
	}
	if (!r->count) {
		r->ppstr = NULL;
		return 1;
	}
	r->ppstr = emalloc(sizeof(char*)*r->count);
	if (NULL == r->ppstr) {
		return 0;
	}
	for (i=0; i<r->count; i++) {
		if (!ext_pack_pull_string(pctx, &r->ppstr[i])) {
			return 0;
		}
	}
	return 1;
}

zend_bool ext_pack_pull_propval(PULL_CTX *pctx, uint16_t type, void **ppval)
{
	switch (type) {
	case PROPVAL_TYPE_DOUBLE:
		*ppval = emalloc(sizeof(double));
		if (NULL == *ppval) {
			return 0;
		}
		return ext_pack_pull_double(pctx, *ppval);
	case PROPVAL_TYPE_BYTE:
		*ppval = emalloc(sizeof(uint8_t));
		if (NULL == *ppval) {
			return 0;
		}
		return ext_pack_pull_uint8(pctx, *ppval);
	case PROPVAL_TYPE_LONGLONG:
		*ppval = emalloc(sizeof(uint64_t));
		if (NULL == *ppval) {
			return 0;
		}
		return ext_pack_pull_uint64(pctx, *ppval);
	case PROPVAL_TYPE_STRING:
		return ext_pack_pull_string(pctx, (char**)ppval);
	case PROPVAL_TYPE_BINARY:
		*ppval = emalloc(sizeof(BINARY));
		if (NULL == *ppval) {
			return 0;
		}
		return ext_pack_pull_binary(pctx, *ppval);
	case PROPVAL_TYPE_GUID:
		*ppval = emalloc(sizeof(GUID));
		if (NULL == *ppval) {
			return 0;
		}
		return ext_pack_pull_guid(pctx, *ppval);
	case PROPVAL_TYPE_LONGLONG_ARRAY:
		*ppval = emalloc(sizeof(LONGLONG_ARRAY));
		if (NULL == *ppval) {
			return 0;
		}
		return ext_pack_pull_longlong_array(pctx, *ppval);
	case PROPVAL_TYPE_STRING_ARRAY:
		*ppval = emalloc(sizeof(STRING_ARRAY));
		if (NULL == *ppval) {
			return 0;
		}
		return ext_pack_pull_string_array(pctx, *ppval);
	case PROPVAL_TYPE_OBJECT:
		/* no value for object */
		*ppval = NULL;
		return 1;
	default:
		return 0;
	}
}

zend_bool ext_pack_pull_dac_propval(PULL_CTX *pctx, DAC_PROPVAL *r)
{	
	if (!ext_pack_pull_string(pctx, &r->pname)) {
		return 0;
	}
	if (!ext_pack_pull_uint16(pctx, &r->type)) {
		return 0;	
	}
	return ext_pack_pull_propval(pctx, r->type, &r->pvalue);
}

zend_bool ext_pack_pull_dac_varray(PULL_CTX *pctx, DAC_VARRAY *r)
{
	int i;
	
	if (!ext_pack_pull_uint16(pctx, &r->count)) {
		return 0;
	}
	if (!r->count) {
		r->ppropval = NULL;
		return 1;
	}
	r->ppropval = emalloc(sizeof(DAC_PROPVAL)*r->count);
	if (NULL == r->ppropval) {
		return 0;
	}
	for (i=0; i<r->count; i++) {
		if (!ext_pack_pull_dac_propval(pctx, r->ppropval + i)) {
			return 0;
		}
	}
	return 1;
}

zend_bool ext_pack_pull_dac_vset(PULL_CTX *pctx, DAC_VSET *r)
{
	int i;

	if (!ext_pack_pull_uint16(pctx, &r->count)) {
		return 0;
	}
	if (!r->count) {
		r->pparray = NULL;
		return 1;
	}
	r->pparray = emalloc(sizeof(DAC_VARRAY*)*r->count);
	if (NULL == r->pparray) {
		return 0;
	}
	for (i=0; i<r->count; i++) {
		r->pparray[i] = emalloc(sizeof(DAC_VARRAY));
		if (NULL == r->pparray[i]) {
			return 0;
		}
		if (!ext_pack_pull_dac_varray(pctx, r->pparray[i])) {
			return 0;
		}
	}
	return 1;
}

static zend_bool ext_pack_pull_dac_res_and_or(
	PULL_CTX *pctx, DAC_RES_AND_OR *r)
{
	int i;
	
	if (!ext_pack_pull_uint32(pctx, &r->count)) {
		return 0;
	}
	r->pres = emalloc(sizeof(DAC_RES)*r->count);
	if (NULL == r->pres) {
		return 0;
	}
	for (i=0; i<r->count; i++) {
		if (!ext_pack_pull_dac_res(pctx, r->pres + i)) {
			return 0;
		}
	}
	return 1;
}

static zend_bool ext_pack_pull_dac_res_not(
	PULL_CTX *pctx, DAC_RES_NOT *r)
{
	return ext_pack_pull_dac_res(pctx, &r->res);
}

static zend_bool ext_pack_pull_dac_res_content(
	PULL_CTX *pctx, DAC_RES_CONTENT *r)
{
	if (!ext_pack_pull_uint8(pctx, &r->fl)) {
		return 0;
	}
	return ext_pack_pull_dac_propval(pctx, &r->propval);
}

static zend_bool ext_pack_pull_dac_res_property(
	PULL_CTX *pctx, DAC_RES_PROPERTY *r)
{
	if (!ext_pack_pull_uint8(pctx, &r->relop)) {
		return 0;
	}
	return ext_pack_pull_dac_propval(pctx, &r->propval);
}

static zend_bool ext_pack_pull_dac_res_exist(
	PULL_CTX *pctx, DAC_RES_EXIST *r)
{
	if (!ext_pack_pull_string(pctx, &r->pname)) {
		return 0;
	}
	return 1;
}

static zend_bool ext_pack_pull_dac_res_subobj(
	PULL_CTX *pctx, DAC_RES_SUBOBJ *r)
{
	if (!ext_pack_pull_string(pctx, &r->pname)) {
		return 0;
	}
	return ext_pack_pull_dac_res(pctx, &r->res);
}

zend_bool ext_pack_pull_dac_res(PULL_CTX *pctx, DAC_RES *r)
{
	if (!ext_pack_pull_uint8(
		pctx, &r->rt)) {
		return 0;	
	}
	switch (r->rt) {
	case DAC_RES_TYPE_AND:
	case DAC_RES_TYPE_OR:
		r->pres = emalloc(sizeof(DAC_RES_AND_OR));
		if (NULL == r->pres) {
			return 0;
		}
		return ext_pack_pull_dac_res_and_or(pctx, r->pres);
	case DAC_RES_TYPE_NOT:
		r->pres = emalloc(sizeof(DAC_RES_NOT));
		if (NULL == r->pres) {
			return 0;
		}
		return ext_pack_pull_dac_res_not(pctx, r->pres);
	case DAC_RES_TYPE_CONTENT:
		r->pres = emalloc(sizeof(DAC_RES_CONTENT));
		if (NULL == r->pres) {
			return 0;
		}
		return ext_pack_pull_dac_res_content(pctx, r->pres);
	case DAC_RES_TYPE_PROPERTY:
		r->pres = emalloc(sizeof(DAC_RES_PROPERTY));
		if (NULL == r->pres) {
			return 0;
		}
		return ext_pack_pull_dac_res_property(pctx, r->pres);
	case DAC_RES_TYPE_EXIST:
		r->pres = emalloc(sizeof(DAC_RES_EXIST));
		if (NULL == r->pres) {
			return 0;
		}
		return ext_pack_pull_dac_res_exist(pctx, r->pres);
	case DAC_RES_TYPE_SUBOBJ:
		r->pres = emalloc(sizeof(DAC_RES_SUBOBJ));
		if (NULL == r->pres) {
			return 0;
		}
		return ext_pack_pull_dac_res_subobj(pctx, r->pres);
	default:
		return 0;
	}
}

/*---------------------------------------------------------------------------*/

zend_bool ext_pack_push_init(PUSH_CTX *pctx)
{	
	pctx->alloc_size = GROWING_BLOCK_SIZE;
	pctx->data = emalloc(GROWING_BLOCK_SIZE);
	if (NULL == pctx->data) {
		return 0;
	}
	pctx->offset = 0;
	return 1;
}

void ext_pack_push_free(PUSH_CTX *pctx)
{
	efree(pctx->data);
}

static zend_bool ext_pack_push_check_overflow(
	PUSH_CTX *pctx, uint32_t extra_size)
{
	uint32_t size;
	uint8_t *pdata;
	uint32_t alloc_size;
	
	size = extra_size + pctx->offset;
	if (pctx->alloc_size >= size) {
		return 1;
	}
	for (alloc_size=pctx->alloc_size; alloc_size<size;
		alloc_size+=GROWING_BLOCK_SIZE);
	pdata = erealloc(pctx->data, alloc_size);
	if (NULL == pdata) {
		return 0;
	}
	pctx->data = pdata;
	pctx->alloc_size = alloc_size;
	return 1;
}

zend_bool ext_pack_push_advance(PUSH_CTX *pctx, uint32_t size)
{
	if (!ext_pack_push_check_overflow(pctx, size)) {
		return 0;
	}
	pctx->offset += size;
	return 1;
}

zend_bool ext_pack_push_bytes(PUSH_CTX *pctx,
	const uint8_t *pdata, uint32_t n)
{
	if (!ext_pack_push_check_overflow(pctx, n)) {
		return 0;
	}
	memcpy(pctx->data + pctx->offset, pdata, n);
	pctx->offset += n;
	return 1;
}

zend_bool ext_pack_push_uint8(PUSH_CTX *pctx, uint8_t v)
{
	if (!ext_pack_push_check_overflow(pctx, sizeof(uint8_t))) {
		return 0;
	}
	SCVAL(pctx->data, pctx->offset, v);
	pctx->offset += sizeof(uint8_t);
	return 1;
}

zend_bool ext_pack_push_uint16(PUSH_CTX *pctx, uint16_t v)
{
	if (!ext_pack_push_check_overflow(pctx, sizeof(uint16_t))) {
		return 0;
	}
	SSVAL(pctx->data, pctx->offset, v);
	pctx->offset += sizeof(uint16_t);
	return 1;
}

zend_bool ext_pack_push_uint32(PUSH_CTX *pctx, uint32_t v)
{
	if (!ext_pack_push_check_overflow(pctx, sizeof(uint32_t))) {
		return 0;
	}
	SIVAL(pctx->data, pctx->offset, v);
	pctx->offset += sizeof(uint32_t);
	return 1;
}

zend_bool ext_pack_push_uint64(PUSH_CTX *pctx, uint64_t v)
{
	if (!ext_pack_push_check_overflow(pctx, sizeof(uint64_t))) {
		return 0;
	}
	SIVAL(pctx->data, pctx->offset, (v & 0xFFFFFFFF));
	SIVAL(pctx->data, pctx->offset+4, (v>>32));
	pctx->offset += sizeof(uint64_t);
	return 1;
}

zend_bool ext_pack_push_double(PUSH_CTX *pctx, double v)
{
	if (!ext_pack_push_check_overflow(pctx, sizeof(double))) {
		return 0;
	}
	memcpy(pctx->data + pctx->offset, &v, 8);
	pctx->offset += sizeof(double);
	return 1;
}

zend_bool ext_pack_push_binary(PUSH_CTX *pctx, const BINARY *r)
{
	if (!ext_pack_push_uint32(pctx, r->cb)) {
		return 0;
	}
	if (!r->cb) {
		return 1;
	}
	return ext_pack_push_bytes(pctx, r->pb, r->cb);
}

zend_bool ext_pack_push_guid(PUSH_CTX *pctx, const GUID *r)
{
	if (!ext_pack_push_uint32(pctx, r->time_low)) {
		return 0;
	}
	if (!ext_pack_push_uint16(pctx, r->time_mid)) {
		return 0;
	}
	if (!ext_pack_push_uint16(pctx, r->time_hi_and_version)) {
		return 0;
	}
	if (!ext_pack_push_bytes(pctx, r->clock_seq, 2)) {
		return 0;
	}
	return ext_pack_push_bytes(pctx, r->node, 6);
}

zend_bool ext_pack_push_string(PUSH_CTX *pctx, const char *pstr)
{
	return ext_pack_push_bytes(pctx, pstr, strlen(pstr) + 1);
}

zend_bool ext_pack_push_longlong_array(
	PUSH_CTX *pctx, const LONGLONG_ARRAY *r)
{
	int i;
	
	if (!ext_pack_push_uint32(pctx, r->count)) {
		return 0;
	}
	for (i=0; i<r->count; i++) {
		if (!ext_pack_push_uint64(pctx, r->pll[i])) {
			return 0;
		}
	}
	return 1;
}

zend_bool ext_pack_push_string_array(
	PUSH_CTX *pctx, const STRING_ARRAY *r)
{
	int i;
	
	if (!ext_pack_push_uint32(pctx, r->count)) {
		return 0;
	}
	for (i=0; i<r->count; i++) {
		if (!ext_pack_push_string(pctx, r->ppstr[i])) {
			return 0;
		}
	}
	return 1;
}

zend_bool ext_pack_push_propval(PUSH_CTX *pctx,
	uint16_t type, const void *pval)
{
	switch (type) {
	case PROPVAL_TYPE_DOUBLE:
		if (!ext_pack_push_double(pctx, *(double*)pval)) {
			return 0;	
		}
		return 1;
	case PROPVAL_TYPE_BYTE:
		if (!ext_pack_push_uint8(pctx, *(uint8_t*)pval)) {
			return 0;	
		}
		return 1;
	case PROPVAL_TYPE_LONGLONG:
		if (!ext_pack_push_uint64(pctx, *(uint64_t*)pval)) {
			return 0;
		}
		return 1;
	case PROPVAL_TYPE_STRING:
		if (!ext_pack_push_string(pctx, pval)) {
			return 0;	
		}
		return 1;
	case PROPVAL_TYPE_BINARY:
		if (!ext_pack_push_binary(pctx, pval)) {
			return 0;	
		}
		return 1;
	case PROPVAL_TYPE_GUID:
		if (!ext_pack_push_guid(pctx, pval)) {
			return 0;	
		}
		return 1;
	case PROPVAL_TYPE_LONGLONG_ARRAY:
		if (!ext_pack_push_longlong_array(pctx, pval)) {
			return 0;	
		}
		return 1;
	case PROPVAL_TYPE_STRING_ARRAY:
		if (!ext_pack_push_string_array(pctx, pval)) {
			return 0;	
		}
		return 1;
	default:
		return 0;
	}
}

zend_bool ext_pack_push_dac_propval(PUSH_CTX *pctx, const DAC_PROPVAL *r)
{
	if (!ext_pack_push_string(pctx, r->pname)) {
		return 0;	
	}
	if (!ext_pack_push_uint16(pctx, r->type)) {
		return 0;	
	}
	return ext_pack_push_propval(pctx, r->type, r->pvalue);
}

zend_bool ext_pack_push_dac_varray(PUSH_CTX *pctx, const DAC_VARRAY *r)
{
	int i;
	
	if (!ext_pack_push_uint16(pctx, r->count)) {
		return 0;
	}
	for (i=0; i<r->count; i++) {
		if (!ext_pack_push_dac_propval(pctx, r->ppropval + i)) {
			return 0;
		}
	}
	return 1;
}

zend_bool ext_pack_push_dac_vset(PUSH_CTX *pctx, const DAC_VSET *r)
{
	int i;

	if (!ext_pack_push_uint16(pctx, r->count)) {
		return 0;
	}
	for (i=0; i<r->count; i++) {
		if (!ext_pack_push_dac_varray(pctx, r->pparray[i])) {
			return 0;
		}
	}
	return 1;
}

static zend_bool ext_pack_push_dac_res_and_or(
	PUSH_CTX *pctx, const DAC_RES_AND_OR *r)
{
	int i;
	
	if (!ext_pack_push_uint32(pctx, r->count)) {
		return 0;
	}
	for (i=0; i<r->count; i++) {
		if (1 != ext_pack_push_dac_res(pctx, r->pres + i)) {
			return 0;
		}
	}
	return 1;
}

static zend_bool ext_pack_push_dac_res_not(
	PUSH_CTX *pctx, const DAC_RES_NOT *r)
{
	return ext_pack_push_dac_res(pctx, &r->res);
}

static zend_bool ext_pack_push_dac_res_content(
	PUSH_CTX *pctx, const DAC_RES_CONTENT *r)
{
	if (!ext_pack_push_uint8(pctx, r->fl)) {
		return 0;
	}
	return ext_pack_push_dac_propval(pctx, &r->propval);
}

static zend_bool ext_pack_push_dac_res_property(
	PUSH_CTX *pctx, const DAC_RES_PROPERTY *r)
{
	if (!ext_pack_push_uint8(pctx, r->relop)) {
		return 0;
	}
	return ext_pack_push_dac_propval(pctx, &r->propval);
}

static zend_bool ext_pack_push_dac_res_exist(
	PUSH_CTX *pctx, const DAC_RES_EXIST *r)
{
	return ext_pack_push_string(pctx, r->pname);
}

static zend_bool ext_pack_push_dac_res_subobj(
	PUSH_CTX *pctx, const DAC_RES_SUBOBJ *r)
{
	if (!ext_pack_push_string(pctx, r->pname)) {
		return 0;
	}
	return ext_pack_push_dac_res(pctx, &r->res);
}

zend_bool ext_pack_push_dac_res(PUSH_CTX *pctx, const DAC_RES *r)
{
	if (!ext_pack_push_uint8(pctx, r->rt)) {
		return 0;	
	}
	switch (r->rt) {
	case DAC_RES_TYPE_AND:
	case DAC_RES_TYPE_OR:
		return ext_pack_push_dac_res_and_or(pctx, r->pres);
	case DAC_RES_TYPE_NOT:
		return ext_pack_push_dac_res_not(pctx, r->pres);
	case DAC_RES_TYPE_CONTENT:
		return ext_pack_push_dac_res_content(pctx, r->pres);
	case DAC_RES_TYPE_PROPERTY:
		return ext_pack_push_dac_res_property(pctx, r->pres);
	case DAC_RES_TYPE_EXIST:
		return ext_pack_push_dac_res_exist(pctx, r->pres);
	case DAC_RES_TYPE_SUBOBJ:
		return ext_pack_push_dac_res_subobj(pctx, r->pres);
	default:
		return 0;
	}
}
