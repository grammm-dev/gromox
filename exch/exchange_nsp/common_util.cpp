// SPDX-License-Identifier: GPL-2.0-only WITH linking exception
#include <cstdint>
#include <gromox/defs.h>
#include "common_util.h"
#include <gromox/fileio.h>
#include <gromox/proc_common.h>
#include <gromox/ndr_stack.hpp>
#include <gromox/guid.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <iconv.h>
#include <cstdio>
#include <ctime>

static GUID g_server_guid;
static const uint8_t g_guid_nspi[] = {0xDC, 0xA7, 0x40, 0xC8,
									   0xC0, 0x42, 0x10, 0x1A,
									   0xB4, 0xB9, 0x08, 0x00,
									   0x2B, 0x2F, 0xE1, 0x82};

static const char* (*cpid_to_charset)(uint32_t cpid);

const uint8_t* common_util_get_nspi_guid()
{
	return g_guid_nspi;
}

GUID common_util_get_server_guid()
{
	return g_server_guid;
}

void common_util_day_to_filetime(const char *day, FILETIME *pftime)
{
	time_t tmp_time;
	struct tm tmp_tm;
	uint64_t file_time;
	
	memset(&tmp_tm, 0, sizeof(tmp_tm));
	strptime(day, "%Y-%m-%d", &tmp_tm);
	tmp_time = mktime(&tmp_tm);
	file_time = ((uint64_t)tmp_time + EPOCH_DIFF)*10000000;
	pftime->low_datetime = file_time & 0xFFFFFFFF;
	pftime->high_datetime = file_time >> 32;
}

int common_util_from_utf8(uint32_t codepage,
	const char *src, char *dst, size_t len)
{
	size_t in_len;
	size_t out_len;
	iconv_t conv_id;
	char *pin, *pout;
	const char *charset;
	
	charset = cpid_to_charset(codepage);
	if (NULL == charset) {
		return -1;
	}
	conv_id = iconv_open(charset, "UTF-8");
	pin = (char*)src;
	pout = dst;
	in_len = strlen(src) + 1;
	memset(dst, 0, len);
	out_len = len;
	if (-1 == iconv(conv_id, &pin, &in_len, &pout, &len)) {
		iconv_close(conv_id);
		return -1;
	} else {
		iconv_close(conv_id);
		return out_len - len;
	}
}

int common_util_to_utf8(uint32_t codepage,
	const char *src, char *dst, size_t len)
{
	size_t in_len;
	size_t out_len;
	char *pin, *pout;
	iconv_t conv_id;
	const char *charset;
	
	charset = cpid_to_charset(codepage);
	if (NULL == charset) {
		return -1;
	}
	conv_id = iconv_open("UTF-8", charset);
	pin = (char*)src;
	pout = dst;
	in_len = strlen(src) + 1;
	memset(dst, 0, len);
	out_len = len;
	if (-1 == iconv(conv_id, &pin, &in_len, &pout, &len)) {
		iconv_close(conv_id);
		return -1;
	} else {
		iconv_close(conv_id);
		return out_len - len;
	}
}

void common_util_guid_to_binary(GUID *pguid, BINARY *pbin)
{
	pbin->cb = 16;
	pbin->pb[0] = pguid->time_low & 0XFF;
	pbin->pb[1] = (pguid->time_low >> 8) & 0XFF;
	pbin->pb[2] = (pguid->time_low >> 16) & 0XFF;
	pbin->pb[3] = (pguid->time_low >> 24) & 0XFF;
	pbin->pb[4] = pguid->time_mid & 0XFF;
	pbin->pb[5] = (pguid->time_mid >> 8) & 0XFF;
	pbin->pb[6] = pguid->time_hi_and_version & 0XFF;
	pbin->pb[7] = (pguid->time_hi_and_version >> 8) & 0XFF;
	memcpy(pbin->pb + 8,  pguid->clock_seq, sizeof(uint8_t) * 2);
	memcpy(pbin->pb + 10, pguid->node, sizeof(uint8_t) * 6);
}

void common_util_set_ephemeralentryid(uint32_t display_type,
	uint32_t minid, EPHEMERAL_ENTRYID *pephid)
{
	pephid->id_type = 0x87;
	pephid->r1 = 0x0;
	pephid->r2 = 0x0;
	pephid->r3 = 0x0;
	pephid->provider_uid.ab[0] =
		g_server_guid.time_low & 0XFF;
	pephid->provider_uid.ab[1] =
		(g_server_guid.time_low >> 8) & 0XFF;
	pephid->provider_uid.ab[2] =
		(g_server_guid.time_low >> 16) & 0XFF;
	pephid->provider_uid.ab[3] =
		(g_server_guid.time_low >> 24) & 0XFF;
	pephid->provider_uid.ab[4] =
		g_server_guid.time_mid & 0XFF;
	pephid->provider_uid.ab[5] =
		(g_server_guid.time_mid >> 8) & 0XFF;
	pephid->provider_uid.ab[6] =
		g_server_guid.time_hi_and_version & 0XFF;
	pephid->provider_uid.ab[7] =
		(g_server_guid.time_hi_and_version >> 8) & 0XFF;
	memcpy(pephid->provider_uid.ab + 8, 
		g_server_guid.clock_seq, sizeof(uint8_t) * 2);
	memcpy(pephid->provider_uid.ab + 10,
		g_server_guid.node, sizeof(uint8_t) * 6);
	pephid->r4 = 0x1;
	pephid->display_type = display_type;
	pephid->mid = minid;
}

BOOL common_util_set_permanententryid(uint32_t display_type,
	const GUID *pobj_guid, const char *pdn, PERMANENT_ENTRYID *ppermeid)
{
	int len;
	char buff[128];
	
	ppermeid->id_type = 0x0;
	ppermeid->r1 = 0x0;
	ppermeid->r2 = 0x0;
	ppermeid->r3 = 0x0;
	memcpy(ppermeid->provider_uid.ab, g_guid_nspi, 16);
	ppermeid->r4 = 0x1;
	ppermeid->display_type = display_type;
	ppermeid->pdn = NULL;
	if (DT_CONTAINER == display_type) {
		if (NULL == pobj_guid) {
			ppermeid->pdn = deconst("/");
		} else {
			len = gx_snprintf(buff, GX_ARRAY_SIZE(buff),
				"/guid=%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
				pobj_guid->time_low, pobj_guid->time_mid,
				pobj_guid->time_hi_and_version,
				pobj_guid->clock_seq[0],
				pobj_guid->clock_seq[1],
				pobj_guid->node[0], pobj_guid->node[1],
				pobj_guid->node[2], pobj_guid->node[3],
				pobj_guid->node[4], pobj_guid->node[5]);
			ppermeid->pdn = ndr_stack_anew<char>(NDR_STACK_OUT, len + 1);
			if (NULL == ppermeid->pdn) {
				return FALSE;
			}
			memcpy(ppermeid->pdn, buff, len + 1);
		}
	}  else {
		len = strlen(pdn);
		ppermeid->pdn = ndr_stack_anew<char>(NDR_STACK_OUT, len + 1);
		if (NULL == ppermeid->pdn) {
			return FALSE;
		}
		memcpy(ppermeid->pdn, pdn, len + 1);
	}
	return TRUE;
}

BOOL common_util_permanent_entryid_to_binary(
	const PERMANENT_ENTRYID *ppermeid, BINARY *pbin)
{
	int len;
	
	len = strlen(ppermeid->pdn) + 1;
	pbin->cb = 28 + len;
	pbin->pv = ndr_stack_alloc(NDR_STACK_OUT, pbin->cb);
	if (pbin->pv == nullptr)
		return FALSE;
	memset(pbin->pb, 0, pbin->cb);
	pbin->pb[0] = ppermeid->id_type;
	pbin->pb[1] = ppermeid->r1;
	pbin->pb[2] = ppermeid->r2;
	pbin->pb[3] = ppermeid->r3;
	memcpy(pbin->pb + 4, ppermeid->provider_uid.ab, 16);
	pbin->pb[20] = (ppermeid->r4 & 0xFF);
	pbin->pb[21] = ((ppermeid->r4 >> 8)  & 0xFF);
	pbin->pb[22] = ((ppermeid->r4 >> 16) & 0xFF);
	pbin->pb[23] = ((ppermeid->r4 >> 24) & 0xFF);
	pbin->pb[24] = (ppermeid->display_type & 0xFF);
	pbin->pb[25] = ((ppermeid->display_type >> 8)  & 0xFF);
	pbin->pb[26] = ((ppermeid->display_type >> 16) & 0xFF);
	pbin->pb[27] = ((ppermeid->display_type >> 24) & 0xFF);
	memcpy(pbin->pb + 28, ppermeid->pdn, len);
	return TRUE;
}

BOOL common_util_ephemeral_entryid_to_binary(
	const EPHEMERAL_ENTRYID *pephid, BINARY *pbin)
{
	pbin->cb = sizeof(EPHEMERAL_ENTRYID);
	pbin->pv = ndr_stack_alloc(NDR_STACK_OUT, pbin->cb);
	if (pbin->pv == nullptr)
		return FALSE;
	memset(pbin->pb, 0, pbin->cb);
	pbin->pb[0] = pephid->id_type;
	pbin->pb[1] = pephid->r1;
	pbin->pb[2] = pephid->r2;
	pbin->pb[3] = pephid->r3;
	memcpy(pbin->pb + 4, pephid->provider_uid.ab, 16);
	pbin->pb[20] = pephid->r4 & 0xFF;
	pbin->pb[21] = (pephid->r4 >> 8)  & 0xFF;
	pbin->pb[22] = (pephid->r4 >> 16) & 0xFF;
	pbin->pb[23] = (pephid->r4 >> 24) & 0xFF;
	pbin->pb[24] = pephid->display_type & 0xFF;
	pbin->pb[25] = (pephid->display_type >> 8)  & 0xFF;
	pbin->pb[26] = (pephid->display_type >> 16) & 0xFF;
	pbin->pb[27] = (pephid->display_type >> 24) & 0xFF;
	pbin->pb[28] = pephid->mid & 0xFF;
	pbin->pb[29] = (pephid->mid >> 8)  & 0xFF;
	pbin->pb[30] = (pephid->mid >> 16) & 0xFF;
	pbin->pb[31] = (pephid->mid >> 24) & 0xFF;
	return TRUE;
}

PROPROW_SET* common_util_proprowset_init()
{
	auto pset = ndr_stack_anew<PROPROW_SET>(NDR_STACK_OUT);
	if (NULL == pset) {
		return NULL;
	}
	memset(pset, 0, sizeof(PROPROW_SET));
	pset->prows = ndr_stack_anew<PROPERTY_ROW>(NDR_STACK_OUT, 100);
	if (NULL == pset->prows) {
		return NULL;
	}
	return pset;
}

PROPERTY_ROW* common_util_proprowset_enlarge(PROPROW_SET *pset)
{
	int count;
	PROPERTY_ROW *prows;
	
	count = (pset->crows/100 + 1) * 100;
	if (pset->crows + 1 >= count) {
		count += 100;
		prows = ndr_stack_anew<PROPERTY_ROW>(NDR_STACK_OUT, count);
		if (NULL == prows) {
			return NULL;
		}
		memcpy(prows, pset->prows, sizeof(PROPERTY_ROW)*pset->crows);
		pset->prows = prows;
	}
	pset->crows ++;
	return &pset->prows[pset->crows - 1]; 
}

PROPERTY_ROW* common_util_propertyrow_init(PROPERTY_ROW *prow)
{
	if (NULL == prow) {
		prow = ndr_stack_anew<PROPERTY_ROW>(NDR_STACK_OUT);
		if (NULL == prow) {
			return NULL;
		}
	}
	memset(prow, 0, sizeof(PROPERTY_ROW));
	prow->pprops = ndr_stack_anew<PROPERTY_VALUE>(NDR_STACK_OUT, 40);
	if (NULL == prow->pprops) {
		return NULL;
	}
	return prow;
}

PROPERTY_VALUE* common_util_propertyrow_enlarge(PROPERTY_ROW *prow)
{
	int count;
	PROPERTY_VALUE *pprops;
	
	count = (prow->cvalues/40 + 1) * 40;
	if (prow->cvalues + 1 >= count) {
		count += 40;
		pprops = ndr_stack_anew<PROPERTY_VALUE>(NDR_STACK_OUT, count);
		if (NULL == pprops) {
			return NULL;
		}
		memcpy(pprops, prow->pprops,
			sizeof(PROPERTY_VALUE)*prow->cvalues);
		prow->pprops = pprops;
	}
	prow->cvalues ++;
	return &prow->pprops[prow->cvalues - 1]; 
}

PROPTAG_ARRAY* common_util_proptagarray_init()
{
	auto pproptags = ndr_stack_anew<PROPTAG_ARRAY>(NDR_STACK_OUT);
	if (NULL == pproptags) {
		return NULL;
	}
	memset(pproptags, 0, sizeof(PROPTAG_ARRAY));
	pproptags->pproptag = ndr_stack_anew<uint32_t>(NDR_STACK_OUT, 100);
	if (NULL == pproptags->pproptag) {
		return NULL;
	}
	return pproptags;
}

uint32_t* common_util_proptagarray_enlarge(PROPTAG_ARRAY *pproptags)
{
	int count;
	uint32_t *pproptag;
	
	count = (pproptags->cvalues/100 + 1) * 100;
	if (pproptags->cvalues + 1 >= count) {
		count += 100;
		pproptag = ndr_stack_anew<uint32_t>(NDR_STACK_OUT, count);
		if (NULL == pproptag) {
			return NULL;
		}
		memcpy(pproptag, pproptags->pproptag,
			sizeof(uint32_t)*pproptags->cvalues);
		pproptags->pproptag = pproptag;
	}
	pproptags->cvalues ++;
	return &pproptags->pproptag[pproptags->cvalues - 1]; 
}

BOOL common_util_load_file(const char *path, BINARY *pbin)
{
	int fd;
	struct stat node_state;
	
	if (0 != stat(path, &node_state)) {
		return FALSE;
	}
	pbin->cb = node_state.st_size;
	pbin->pv = ndr_stack_alloc(NDR_STACK_OUT, node_state.st_size);
	if (pbin->pv == nullptr)
		return FALSE;
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		return FALSE;
	}
	if (read(fd, pbin->pv, node_state.st_size) != node_state.st_size) {
		close(fd);
		return FALSE;
	}
	close(fd);
	return TRUE;
}

int common_util_run()
{
	query_service1(cpid_to_charset);
	if (NULL == cpid_to_charset) {
		printf("[exchange_nsp]: failed to get service \"cpid_to_charset\"\n");
		return -1;
	}
	g_server_guid = guid_random_new();
	return 0;
}
