#include "tpropval_array.h"
#include "proptag_array.h"
#include "tarray_set.h"
#include "rop_util.h"
#include "str_hash.h"
#include "int_hash.h"
#include "propval.h"
#include "tnef.h"
#include "guid.h"
#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


#define TNEF_LEGACY								0x0001
#define TNEF_VERSION							0x10000

/*
	TRIPLES										0x0000
	STRING										0x0001
	TEXT										0x0002
	DATE										0x0003
	SHORT										0x0004
	LONG										0x0005
	BINARY										0x0006
	CLASS										0x0007
	LONGARRAY									0x0008
*/

#define ATTRIBUTE_ID_OWNER						0x00060000
#define ATTRIBUTE_ID_SENTFOR					0x00060001
#define ATTRIBUTE_ID_DELEGATE					0x00060002
#define ATTRIBUTE_ID_DATESTART					0x00030006
#define ATTRIBUTE_ID_DATEEND					0x00030007
#define ATTRIBUTE_ID_AIDOWNER					0x00050008
#define ATTRIBUTE_ID_REQUESTRES					0x00040009
#define ATTRIBUTE_ID_ORIGNINALMESSAGECLASS		0x00070600
#define ATTRIBUTE_ID_FROM						0x00008000
#define ATTRIBUTE_ID_SUBJECT 					0x00018004
#define ATTRIBUTE_ID_DATESENT					0x00038005
#define ATTRIBUTE_ID_DATERECD					0x00038006
#define ATTRIBUTE_ID_MESSAGESTATUS				0x00068007
#define ATTRIBUTE_ID_MESSAGECLASS				0x00078008
#define ATTRIBUTE_ID_MESSAGEID					0x00018009
#define ATTRIBUTE_ID_BODY						0x0002800C
#define ATTRIBUTE_ID_PRIORITY					0x0004800D
#define ATTRIBUTE_ID_ATTACHDATA					0x0006800F
#define ATTRIBUTE_ID_ATTACHTITLE				0x00018010
#define ATTRIBUTE_ID_ATTACHMETAFILE				0x00068011
#define ATTRIBUTE_ID_ATTACHCREATEDATE			0x00038012
#define ATTRIBUTE_ID_ATTACHMODIFYDATE			0x00038013
#define ATTRIBUTE_ID_DATEMODIFY					0x00038020
#define ATTRIBUTE_ID_ATTACHTRANSPORTFILENAME	0x00069001
#define ATTRIBUTE_ID_ATTACHRENDDATA				0x00069002
#define ATTRIBUTE_ID_MSGPROPS					0x00069003
#define ATTRIBUTE_ID_RECIPTABLE					0x00069004
#define ATTRIBUTE_ID_ATTACHMENT					0x00069005
#define ATTRIBUTE_ID_TNEFVERSION				0x00089006
#define ATTRIBUTE_ID_OEMCODEPAGE				0x00069007
#define ATTRIBUTE_ID_PARENTID					0x0001800A
#define ATTRIBUTE_ID_CONVERSATIONID				0x0001800B

#define LVL_MESSAGE								0x1
#define LVL_ATTACHMENT							0x2

#define ATTACH_TYPE_FILE						0x0001
#define ATTACH_TYPE_OLE							0x0002

#define FILE_DATA_DEFAULT						0x00000000
#define FILE_DATA_MACBINARY						0x00000001

#define FMS_READ								0x20
#define FMS_MODIFIED							0x01
#define FMS_SUBMITTED							0x04
#define FMS_LOCAL								0x02
#define FMS_HASATTACH							0x80

typedef struct _TNEF_ATTRIBUTE {
	uint8_t lvl;
	uint32_t attr_id;
	void *pvalue;
} TNEF_ATTRIBUTE;

typedef struct _TRP_HEADER {
	uint16_t trp_id;
	uint16_t total_len;
	uint16_t displayname_len;
	uint16_t address_len;
} TRP_HEADER;

typedef struct _DTR {
    uint16_t year;
	uint16_t month;
	uint16_t day;
    uint16_t hour;
	uint16_t min;
	uint16_t sec;
    uint16_t dow;
} DTR;

typedef struct _ATTR_ADDR {
	char *displayname;
	char *address;
} ATTR_ADDR;

typedef struct _REND_DATA {
	uint16_t attach_type;
	uint32_t attach_position;
	uint16_t render_width;
	uint16_t render_height;
	uint32_t data_flags;
} REND_DATA;

typedef struct _TNEF_PROPVAL {
	uint16_t proptype;
	uint16_t propid;
	PROPERTY_NAME *ppropname;
	void *pvalue;
} TNEF_PROPVAL;

typedef struct _TNEF_PROPLIST {
	uint32_t count;
	TNEF_PROPVAL *ppropval;
} TNEF_PROPLIST;

typedef struct _TNEF_PROPSET {
	uint32_t count;
	TNEF_PROPLIST **pplist;
} TNEF_PROPSET;

static uint8_t IID_IMessage[] = {
	0x07, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};

static uint8_t IID_IStorage[] = {
	0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};
	
static uint8_t IID_IStream[] = {
	0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};

static uint8_t OLE_TAG[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x14, 0x03, 0x0A,
	0x03, 0x02, 0x01};
	
static uint8_t MACBINARY_ENCODING[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x14, 0x03, 0x0B,
	0x01};

static const uint8_t g_pad_bytes[3] = {0, 0, 0};

static const char* (*tnef_cpid_to_charset)(uint32_t cpid);

static BOOL tnef_serialize_internal(EXT_PUSH *pext, BOOL b_embedded,
	const MESSAGE_CONTENT *pmsg, EXT_BUFFER_ALLOC alloc,
	GET_PROPNAME get_propname);

void tnef_init_library(CPID_TO_CHARSET cpid_to_charset)
{
	tnef_cpid_to_charset = cpid_to_charset;
}
	
static BOOL tnef_username_to_oneoff(const char *username,
	const char *pdisplay_name, BINARY *pbin)
{
	EXT_PUSH ext_push;
	ONEOFF_ENTRYID tmp_entry;
	
	tmp_entry.flags = 0;
	rop_util_get_provider_uid(PROVIDER_UID_ONE_OFF,
							tmp_entry.provider_uid);
	tmp_entry.version = 0;
	tmp_entry.ctrl_flags = CTRL_FLAG_NORICH | CTRL_FLAG_UNICODE;
	if (NULL != pdisplay_name) {
		tmp_entry.pdisplay_name = (char*)pdisplay_name;
	} else {
		tmp_entry.pdisplay_name = "";
	}
	tmp_entry.paddress_type = "SMTP";
	tmp_entry.pmail_address = (char*)username;
	ext_buffer_push_init(&ext_push, pbin->pb, 1280, EXT_FLAG_UTF16);
	if (EXT_ERR_SUCCESS != ext_buffer_push_oneoff_entryid(
		&ext_push, &tmp_entry)) {
		return FALSE;
	}
	pbin->cb = ext_push.offset;
	return TRUE;
}

static uint16_t tnef_generate_checksum(
	const uint8_t *pdata, uint32_t len)
{
	int i;
	uint32_t mysum;

	mysum = 0;
	for ( i=0; i<len; i++ ) {
		mysum = (mysum + pdata[i]) & 0xFFFF;
	}
	return mysum;
}

static uint8_t tnef_align(uint32_t length)
{
    return ((length + 3) & ~3) - length;
}

static int tnef_pull_property_name(EXT_PULL *pext, PROPERTY_NAME *r)
{
	int status;
	uint32_t offset;
	uint32_t tmp_int;
	
	status = ext_buffer_pull_guid(pext, &r->guid);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_pull_uint32(pext, &tmp_int);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (0 == tmp_int) {
		r->kind = KIND_LID;
		r->plid = pext->alloc(sizeof(uint32_t));
		if (NULL == r->plid) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_uint32(pext, r->plid);
	} else if (1 == tmp_int) {
		r->kind = KIND_NAME;
		status = ext_buffer_pull_uint32(pext, &tmp_int);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		offset = pext->offset + tmp_int;
		status = ext_buffer_pull_wstring(pext, &r->pname);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (pext->offset > offset) {
			return EXT_ERR_FORMAT;
		}
		pext->offset = offset;
		return ext_buffer_pull_advance(pext, tnef_align(tmp_int));
	}
	return EXT_ERR_FORMAT;
}

static int tnef_pull_propval(EXT_PULL *pext, TNEF_PROPVAL *r)
{
	int i;
	int status;
	uint32_t offset;
	uint32_t tmp_int;
	uint16_t fake_byte;
	
	status = ext_buffer_pull_uint16(pext, &r->proptype);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_pull_uint16(pext, &r->propid);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	r->ppropname = NULL;
	if (r->propid & 0x8000) {
		r->ppropname = pext->alloc(sizeof(PROPERTY_NAME));
		if (NULL == r->ppropname) {
			return EXT_ERR_ALLOC;
		}
		status = tnef_pull_property_name(pext, r->ppropname);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
	}
	switch (r->proptype) {
	case PROPVAL_TYPE_SHORT:
		r->pvalue = pext->alloc(sizeof(uint16_t));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint16(pext, r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		return ext_buffer_pull_advance(pext, 2);
	case PROPVAL_TYPE_ERROR:
	case PROPVAL_TYPE_LONG:
		r->pvalue = pext->alloc(sizeof(uint32_t));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_uint32(pext, r->pvalue);
	case PROPVAL_TYPE_FLOAT:
		r->pvalue = pext->alloc(sizeof(float));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_float(pext, r->pvalue);
	case PROPVAL_TYPE_DOUBLE:
	case PROPVAL_TYPE_FLOATINGTIME:
		r->pvalue = pext->alloc(sizeof(double));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_double(pext, r->pvalue);
	case PROPVAL_TYPE_BYTE:
		r->pvalue = pext->alloc(sizeof(uint8_t));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint16(pext, &fake_byte);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		*(uint8_t*)r->pvalue = fake_byte;
		return ext_buffer_pull_advance(pext, 2);
	case PROPVAL_TYPE_CURRENCY:
	case PROPVAL_TYPE_LONGLONG:
	case PROPVAL_TYPE_FILETIME:
		r->pvalue = pext->alloc(sizeof(uint64_t));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_uint64(pext, r->pvalue);
	case PROPVAL_TYPE_STRING:
		status = ext_buffer_pull_uint32(pext, &tmp_int);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (1 != tmp_int) {
			return EXT_ERR_FORMAT;
		}
		status = ext_buffer_pull_uint32(pext, &tmp_int);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		offset = pext->offset + tmp_int;
		status = ext_buffer_pull_string(pext, (char**)&r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (pext->offset > offset) {
			return EXT_ERR_FORMAT;
		}
		pext->offset = offset;
		return ext_buffer_pull_advance(pext, tnef_align(tmp_int));
	case PROPVAL_TYPE_WSTRING:
		status = ext_buffer_pull_uint32(pext, &tmp_int);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (1 != tmp_int) {
			return EXT_ERR_FORMAT;
		}
		status = ext_buffer_pull_uint32(pext, &tmp_int);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		offset = pext->offset + tmp_int;
		status = ext_buffer_pull_wstring(pext, (char**)&r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (pext->offset > offset) {
			return EXT_ERR_FORMAT;
		}
		pext->offset = offset;
		return ext_buffer_pull_advance(pext, tnef_align(tmp_int));
	case PROPVAL_TYPE_GUID:
		r->pvalue = pext->alloc(sizeof(GUID));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_guid(pext, r->pvalue);
	case PROPVAL_TYPE_SVREID:
		r->pvalue = pext->alloc(sizeof(SVREID));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		return ext_buffer_pull_svreid(pext, r->pvalue);
	case PROPVAL_TYPE_OBJECT:
		r->pvalue = pext->alloc(sizeof(BINARY));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint32(pext, &tmp_int);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (1 != tmp_int) {
			return EXT_ERR_FORMAT;
		}
		status = ext_buffer_pull_uint32(pext,
					&((BINARY*)r->pvalue)->cb);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (((BINARY*)r->pvalue)->cb < 16 ||
			((BINARY*)r->pvalue)->cb >
			pext->data_size - pext->offset) {
			return EXT_ERR_FORMAT;
		}
		((BINARY*)r->pvalue)->pb =
			pext->alloc(((BINARY*)r->pvalue)->cb);
		if (NULL == ((BINARY*)r->pvalue)->pb) {
			return EXT_ERR_ALLOC;
		}
		offset = pext->offset;
		status = ext_buffer_pull_bytes(pext,
					((BINARY*)r->pvalue)->pb,
					((BINARY*)r->pvalue)->cb);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (0 != memcmp(((BINARY*)r->pvalue)->pb, IID_IMessage, 16) &&
			0 != memcmp(((BINARY*)r->pvalue)->pb, IID_IStorage, 16) &&
			0 != memcmp(((BINARY*)r->pvalue)->pb, IID_IStream, 16)) {
			return EXT_ERR_FORMAT;
		}
		return ext_buffer_pull_advance(pext,
			tnef_align(pext->offset - offset));
	case PROPVAL_TYPE_BINARY:
		r->pvalue = pext->alloc(sizeof(BINARY));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint32(pext, &tmp_int);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (1 != tmp_int) {
			return EXT_ERR_FORMAT;
		}
		status = ext_buffer_pull_uint32(pext,
			&((BINARY*)r->pvalue)->cb);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (((BINARY*)r->pvalue)->cb +
			pext->offset > pext->data_size) {
			return EXT_ERR_FORMAT;
		}
		((BINARY*)r->pvalue)->pb =
			pext->alloc(((BINARY*)r->pvalue)->cb);
		if (NULL == ((BINARY*)r->pvalue)->pb) {
			return EXT_ERR_ALLOC;
		}
		offset = pext->offset;
		status = ext_buffer_pull_bytes(pext,
					((BINARY*)r->pvalue)->pb,
					((BINARY*)r->pvalue)->cb);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		return ext_buffer_pull_advance(pext,
			tnef_align(pext->offset - offset));
	case PROPVAL_TYPE_SHORT_ARRAY:
		r->pvalue = pext->alloc(sizeof(SHORT_ARRAY));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint32(pext,
			&((SHORT_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (((SHORT_ARRAY*)r->pvalue)->count > 0xFFFF) {
			return EXT_ERR_FORMAT;
		}
		if (0 == ((SHORT_ARRAY*)r->pvalue)->count) {
			((SHORT_ARRAY*)r->pvalue)->ps = NULL;
		} else {
			((SHORT_ARRAY*)r->pvalue)->ps = pext->alloc(
				sizeof(uint16_t)*((SHORT_ARRAY*)r->pvalue)->count);
			if (NULL == ((SHORT_ARRAY*)r->pvalue)->ps) {
				return EXT_ERR_ALLOC;
			}
		}
		for (i=0; i<((SHORT_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_pull_uint16(pext, 
				((SHORT_ARRAY*)r->pvalue)->ps + i);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			status = ext_buffer_pull_advance(pext, 2);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	case PROPVAL_TYPE_LONG_ARRAY:
		r->pvalue = pext->alloc(sizeof(LONG_ARRAY));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint32(pext,
			&((LONG_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (((LONG_ARRAY*)r->pvalue)->count > 0xFFFF) {
			return EXT_ERR_FORMAT;
		}
		if (0 == ((LONG_ARRAY*)r->pvalue)->count) {
			((LONG_ARRAY*)r->pvalue)->pl = NULL;
		} else {
			((LONG_ARRAY*)r->pvalue)->pl = pext->alloc(
				sizeof(uint32_t)*((LONG_ARRAY*)r->pvalue)->count);
			if (NULL == ((LONG_ARRAY*)r->pvalue)->pl) {
				return EXT_ERR_ALLOC;
			}
		}
		for (i=0; i<((LONG_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_pull_uint32(pext, 
				((LONG_ARRAY*)r->pvalue)->pl + i);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	case PROPVAL_TYPE_LONGLONG_ARRAY:
		r->pvalue = pext->alloc(sizeof(LONGLONG_ARRAY));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint32(pext,
			&((LONGLONG_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (((LONGLONG_ARRAY*)r->pvalue)->count > 0xFFFF) {
			return EXT_ERR_FORMAT;
		}
		if (0 == ((LONGLONG_ARRAY*)r->pvalue)->count) {
			((LONGLONG_ARRAY*)r->pvalue)->pll = NULL;
		} else {
			((LONGLONG_ARRAY*)r->pvalue)->pll = pext->alloc(
				sizeof(uint64_t)*((LONGLONG_ARRAY*)r->pvalue)->count);
			if (NULL == ((LONGLONG_ARRAY*)r->pvalue)->pll) {
				return EXT_ERR_ALLOC;
			}
		}
		for (i=0; i<((LONGLONG_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_pull_uint64(pext, 
				((LONGLONG_ARRAY*)r->pvalue)->pll + i);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	case PROPVAL_TYPE_STRING_ARRAY:
		r->pvalue = pext->alloc(sizeof(STRING_ARRAY));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint32(pext,
			&((STRING_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (((STRING_ARRAY*)r->pvalue)->count > 0xFFFF) {
			return EXT_ERR_FORMAT;
		}
		if (0 == ((STRING_ARRAY*)r->pvalue)->count) {
			((STRING_ARRAY*)r->pvalue)->ppstr = NULL;
		} else {
			((STRING_ARRAY*)r->pvalue)->ppstr = pext->alloc(
				sizeof(void*)*((STRING_ARRAY*)r->pvalue)->count);
			if (NULL == ((STRING_ARRAY*)r->pvalue)->ppstr) {
				return EXT_ERR_ALLOC;
			}
		}
		for (i=0; i<((STRING_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_pull_uint32(pext, &tmp_int);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			offset = pext->offset + tmp_int;
			status = ext_buffer_pull_string(pext,
				&((STRING_ARRAY*)r->pvalue)->ppstr[i]);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			if (pext->offset > offset) {
				return EXT_ERR_FORMAT;
			}
			pext->offset = offset;
			status = ext_buffer_pull_advance(pext, tnef_align(tmp_int));
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	case PROPVAL_TYPE_WSTRING_ARRAY:
		r->pvalue = pext->alloc(sizeof(STRING_ARRAY));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint32(pext,
			&((STRING_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (((STRING_ARRAY*)r->pvalue)->count > 0xFFFF) {
			return EXT_ERR_FORMAT;
		}
		if (0 == ((STRING_ARRAY*)r->pvalue)->count) {
			((STRING_ARRAY*)r->pvalue)->ppstr = NULL;
		} else {
			((STRING_ARRAY*)r->pvalue)->ppstr = pext->alloc(
				sizeof(void*)*((STRING_ARRAY*)r->pvalue)->count);
			if (NULL == ((STRING_ARRAY*)r->pvalue)->ppstr) {
				return EXT_ERR_ALLOC;
			}
		}
		for (i=0; i<((STRING_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_pull_uint32(pext, &tmp_int);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			offset = pext->offset + tmp_int;
			status = ext_buffer_pull_wstring(pext,
				&((STRING_ARRAY*)r->pvalue)->ppstr[i]);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			if (pext->offset > offset) {
				return EXT_ERR_FORMAT;
			}
			pext->offset = offset;
			status = ext_buffer_pull_advance(pext, tnef_align(tmp_int));
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	case PROPVAL_TYPE_GUID_ARRAY:
		r->pvalue = pext->alloc(sizeof(GUID_ARRAY));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint32(pext,
			&((GUID_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (((GUID_ARRAY*)r->pvalue)->count > 0xFFFF) {
			return EXT_ERR_FORMAT;
		}
		if (0 == ((GUID_ARRAY*)r->pvalue)->count) {
			((GUID_ARRAY*)r->pvalue)->pguid = NULL;
		} else {
			((GUID_ARRAY*)r->pvalue)->pguid = pext->alloc(
				sizeof(GUID*)*((GUID_ARRAY*)r->pvalue)->count);
			if (NULL == ((GUID_ARRAY*)r->pvalue)->pguid) {
				return EXT_ERR_ALLOC;
			}
		}
		for (i=0; i<((GUID_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_pull_guid(pext,
				((GUID_ARRAY*)r->pvalue)->pguid + i);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	case PROPVAL_TYPE_BINARY_ARRAY:
		r->pvalue = pext->alloc(sizeof(BINARY_ARRAY));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint32(pext,
			&((BINARY_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (((BINARY_ARRAY*)r->pvalue)->count > 0xFFFF) {
			return EXT_ERR_FORMAT;
		}
		if (0 == ((BINARY_ARRAY*)r->pvalue)->count) {
			((BINARY_ARRAY*)r->pvalue)->pbin = NULL;
		} else {
			((BINARY_ARRAY*)r->pvalue)->pbin = pext->alloc(
				sizeof(BINARY)*((BINARY_ARRAY*)r->pvalue)->count);
			if (NULL == ((BINARY_ARRAY*)r->pvalue)->pbin) {
				return EXT_ERR_ALLOC;
			}
		}
		for (i=0; i<((BINARY_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_pull_uint32(pext,
				&((BINARY_ARRAY*)r->pvalue)->pbin[i].cb);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			if (((BINARY_ARRAY*)r->pvalue)->pbin[i].cb +
				pext->offset > pext->data_size) {
				return EXT_ERR_FORMAT;
			}
			if (0 == ((BINARY_ARRAY*)r->pvalue)->pbin[i].cb) {
				((BINARY*)r->pvalue)->pb = NULL;
			} else {
				((BINARY_ARRAY*)r->pvalue)->pbin[i].pb =
					pext->alloc(((BINARY_ARRAY*)r->pvalue)->pbin[i].cb);
				if (NULL == ((BINARY*)r->pvalue)->pb) {
					return EXT_ERR_ALLOC;
				}
			}
			offset = pext->offset;
			status = ext_buffer_pull_bytes(pext,
				((BINARY_ARRAY*)r->pvalue)->pbin[i].pb,
				((BINARY_ARRAY*)r->pvalue)->pbin[i].cb);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			status = ext_buffer_pull_advance(pext,
				tnef_align(pext->offset - offset));
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	}
	return EXT_ERR_BAD_SWITCH;
}

static int tnef_pull_attribute(EXT_PULL *pext, TNEF_ATTRIBUTE *r)
{
	int i, j;
	int status;
	DTR tmp_dtr;
	uint32_t len;
	uint32_t offset;
	uint32_t offset1;
	uint16_t tmp_len;
	struct tm tmp_tm;
    uint16_t checksum;
	TRP_HEADER header;

    status = ext_buffer_pull_uint8(pext, &r->lvl);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (LVL_MESSAGE != r->lvl &&
		LVL_ATTACHMENT != r->lvl) {
		debug_info("[tnef]: attribute level error");
		return EXT_ERR_FORMAT;
	}
	status = ext_buffer_pull_uint32(pext, &r->attr_id);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (LVL_MESSAGE == r->lvl) {
		switch (r->attr_id) {
		case ATTRIBUTE_ID_MSGPROPS:
		case ATTRIBUTE_ID_OWNER:
		case ATTRIBUTE_ID_SENTFOR:
		case ATTRIBUTE_ID_DELEGATE:
		case ATTRIBUTE_ID_DATESTART:
		case ATTRIBUTE_ID_DATEEND:
		case ATTRIBUTE_ID_AIDOWNER:
		case ATTRIBUTE_ID_REQUESTRES:
		case ATTRIBUTE_ID_ORIGNINALMESSAGECLASS:
		case ATTRIBUTE_ID_FROM:
		case ATTRIBUTE_ID_SUBJECT:
		case ATTRIBUTE_ID_DATESENT:
		case ATTRIBUTE_ID_DATERECD:
		case ATTRIBUTE_ID_MESSAGESTATUS:
		case ATTRIBUTE_ID_MESSAGECLASS:
		case ATTRIBUTE_ID_MESSAGEID:
		case ATTRIBUTE_ID_BODY:
		case ATTRIBUTE_ID_PRIORITY:
		case ATTRIBUTE_ID_DATEMODIFY:
		case ATTRIBUTE_ID_RECIPTABLE:
		case ATTRIBUTE_ID_TNEFVERSION:
		case ATTRIBUTE_ID_OEMCODEPAGE:
		case ATTRIBUTE_ID_PARENTID:
		case ATTRIBUTE_ID_CONVERSATIONID:
			break;
		default:
			debug_info("[tnef]: unknown attribute 0x%x", r->attr_id);
			return EXT_ERR_FORMAT;
		}
		
	} else {
		switch (r->attr_id) {
		case ATTRIBUTE_ID_ATTACHMENT:
		case ATTRIBUTE_ID_ATTACHDATA:
		case ATTRIBUTE_ID_ATTACHTITLE:
		case ATTRIBUTE_ID_ATTACHMETAFILE:
		case ATTRIBUTE_ID_ATTACHCREATEDATE:
		case ATTRIBUTE_ID_ATTACHMODIFYDATE:
		case ATTRIBUTE_ID_ATTACHTRANSPORTFILENAME:
		case ATTRIBUTE_ID_ATTACHRENDDATA:
			break;
		default:
			debug_info("[tnef]: unknown attribute 0x%x", r->attr_id);
			return EXT_ERR_FORMAT;
		}
	}
	status = ext_buffer_pull_uint32(pext, &len);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (pext->offset + len > pext->data_size) {
		return EXT_ERR_FORMAT;
	}
	offset = pext->offset;
	switch (r->attr_id) {
	case ATTRIBUTE_ID_FROM:
		r->pvalue = pext->alloc(sizeof(ATTR_ADDR));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint16(pext, &header.trp_id);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (0x0004 != header.trp_id) {
			debug_info("[tnef]: tripidOneOff error");
			return EXT_ERR_FORMAT;
		}
		status = ext_buffer_pull_uint16(pext, &header.total_len);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_pull_uint16(pext, &header.displayname_len);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_pull_uint16(pext, &header.address_len);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (header.total_len != header.displayname_len +
			header.address_len + 16) {
			debug_info("[tnef]: triple header's structure-length error");
			return EXT_ERR_FORMAT;
		}
		offset1 = pext->offset;
		status = ext_buffer_pull_string(pext,
			&((ATTR_ADDR*)r->pvalue)->displayname);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		offset1 += header.displayname_len;
		if (pext->offset > offset1) {
			debug_info("[tnef]: triple header's sender-name-length error");
			return EXT_ERR_FORMAT;
		}
		pext->offset = offset1;
		status = ext_buffer_pull_string(pext,
			&((ATTR_ADDR*)r->pvalue)->address);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		offset1 += header.address_len;
		if (pext->offset > offset1) {
			debug_info("[tnef]: triple header's sender-email-length error");
			return EXT_ERR_FORMAT;
		}
		pext->offset = offset1;
		status = ext_buffer_pull_advance(pext, 8);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_SUBJECT:
	case ATTRIBUTE_ID_MESSAGEID:
	case ATTRIBUTE_ID_ATTACHTITLE:
	case ATTRIBUTE_ID_ORIGNINALMESSAGECLASS:
	case ATTRIBUTE_ID_MESSAGECLASS:
	case ATTRIBUTE_ID_ATTACHTRANSPORTFILENAME:
	case ATTRIBUTE_ID_PARENTID:
	case ATTRIBUTE_ID_CONVERSATIONID:
		status = ext_buffer_pull_string(pext, (char**)&r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_DATESTART:
	case ATTRIBUTE_ID_DATEEND:
	case ATTRIBUTE_ID_DATESENT:
	case ATTRIBUTE_ID_DATERECD:
	case ATTRIBUTE_ID_ATTACHCREATEDATE:
	case ATTRIBUTE_ID_ATTACHMODIFYDATE:
	case ATTRIBUTE_ID_DATEMODIFY:
		r->pvalue = pext->alloc(sizeof(uint64_t));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint16(pext, &tmp_dtr.year);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_pull_uint16(pext, &tmp_dtr.month);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_pull_uint16(pext, &tmp_dtr.day);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_pull_uint16(pext, &tmp_dtr.hour);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_pull_uint16(pext, &tmp_dtr.min);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_pull_uint16(pext, &tmp_dtr.sec);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_pull_uint16(pext, &tmp_dtr.dow);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		tmp_tm.tm_sec = tmp_dtr.sec;
		tmp_tm.tm_min = tmp_dtr.min;
		tmp_tm.tm_hour = tmp_dtr.hour;
		tmp_tm.tm_mday = tmp_dtr.day;
		tmp_tm.tm_mon = tmp_dtr.month - 1;
		tmp_tm.tm_year = tmp_dtr.year - 1900;
		tmp_tm.tm_wday = tmp_dtr.dow - 1;
		tmp_tm.tm_yday = 0;
		tmp_tm.tm_isdst = 0;
		*(uint64_t*)r->pvalue = rop_util_unix_to_nttime(mktime(&tmp_tm));
		break;
	case ATTRIBUTE_ID_REQUESTRES:
	case ATTRIBUTE_ID_PRIORITY:
		r->pvalue = pext->alloc(sizeof(uint16_t));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint16(pext, r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_AIDOWNER:
		r->pvalue = pext->alloc(sizeof(uint32_t));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint32(pext, r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_BODY:
		r->pvalue = pext->alloc(len + 1);
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_bytes(pext, r->pvalue, len);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		((char*)r->pvalue)[len] = '\0';
		break;
	case ATTRIBUTE_ID_MSGPROPS:
	case ATTRIBUTE_ID_ATTACHMENT:
		r->pvalue = pext->alloc(sizeof(TNEF_PROPLIST));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint32(pext,
			&((TNEF_PROPLIST*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (((TNEF_PROPLIST*)r->pvalue)->count > 0xFFFF) {
			return EXT_ERR_FORMAT;
		}
		if (0 == ((TNEF_PROPLIST*)r->pvalue)->count) {
			((TNEF_PROPLIST*)r->pvalue)->ppropval = NULL;
		} else {
			((TNEF_PROPLIST*)r->pvalue)->ppropval = pext->alloc(
				sizeof(TNEF_PROPVAL)*((TNEF_PROPLIST*)r->pvalue)->count);
			if (NULL == ((TNEF_PROPLIST*)r->pvalue)->ppropval) {
				return EXT_ERR_ALLOC;
			}
		}
		for (i=0; i<((TNEF_PROPLIST*)r->pvalue)->count; i++) {
			status = tnef_pull_propval(pext,
				((TNEF_PROPLIST*)r->pvalue)->ppropval + i);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		break;
	case ATTRIBUTE_ID_RECIPTABLE:
		r->pvalue = pext->alloc(sizeof(TNEF_PROPSET));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint32(pext,
			&((TNEF_PROPSET*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (((TNEF_PROPSET*)r->pvalue)->count > 0xFFFF) {
			return EXT_ERR_FORMAT;
		}
		if (0 == ((TNEF_PROPSET*)r->pvalue)->count) {
			((TNEF_PROPSET*)r->pvalue)->pplist = NULL;
		} else {
			((TNEF_PROPSET*)r->pvalue)->pplist = pext->alloc(
				sizeof(TNEF_PROPLIST*)*((TNEF_PROPSET*)r->pvalue)->count);
			if (NULL == ((TNEF_PROPSET*)r->pvalue)->pplist) {
				return EXT_ERR_ALLOC;
			}
		}
		for (i=0; i<((TNEF_PROPSET*)r->pvalue)->count; i++) {
			((TNEF_PROPSET*)r->pvalue)->pplist[i] =
				pext->alloc(sizeof(TNEF_PROPLIST));
			if (NULL == ((TNEF_PROPSET*)r->pvalue)->pplist[i]) {
				return EXT_ERR_ALLOC;
			}
			status = ext_buffer_pull_uint32(pext,
				&((TNEF_PROPSET*)r->pvalue)->pplist[i]->count);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			if (((TNEF_PROPSET*)r->pvalue)->pplist[i]->count > 0xFFFF) {
				return EXT_ERR_FORMAT;
			}
			if (0 == ((TNEF_PROPSET*)r->pvalue)->pplist[i]->count) {
				((TNEF_PROPSET*)r->pvalue)->pplist[i]->ppropval = NULL;
			} else {
				((TNEF_PROPSET*)r->pvalue)->pplist[i]->ppropval =
					pext->alloc(sizeof(TNEF_PROPVAL)*
					((TNEF_PROPSET*)r->pvalue)->pplist[i]->count);
				if (NULL == ((TNEF_PROPSET*)r->pvalue)->pplist[i]->ppropval) {
					return EXT_ERR_ALLOC;
				}
			}
			for (j=0; j<((TNEF_PROPSET*)r->pvalue)->pplist[i]->count; j++) {
				status = tnef_pull_propval(pext, ((TNEF_PROPSET*)
							r->pvalue)->pplist[i]->ppropval + j);
				if (EXT_ERR_SUCCESS != status) {
					return status;
				}
			}
		}
		break;
	case ATTRIBUTE_ID_OWNER:
	case ATTRIBUTE_ID_SENTFOR:
		r->pvalue = pext->alloc(sizeof(ATTR_ADDR));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint16(pext, &tmp_len);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		offset1 = pext->offset + tmp_len;
		status = ext_buffer_pull_string(pext,
			&((ATTR_ADDR*)r->pvalue)->displayname);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (pext->offset > offset1) {
			debug_info("[tnef]: owner's display-name-length error");
			return EXT_ERR_FORMAT;
		}
		pext->offset = offset1;
		status = ext_buffer_pull_uint16(pext, &tmp_len);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		offset1 = pext->offset + tmp_len;
		status = ext_buffer_pull_string(pext,
			&((ATTR_ADDR*)r->pvalue)->address);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (pext->offset > offset1) {
			debug_info("[tnef]: owner's address-length error");
			return EXT_ERR_FORMAT;
		}
		pext->offset = offset1;
		break;
	case ATTRIBUTE_ID_ATTACHRENDDATA:
		r->pvalue = pext->alloc(sizeof(REND_DATA));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_uint16(pext,
			&((REND_DATA*)r->pvalue)->attach_type);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_pull_uint32(pext,
			&((REND_DATA*)r->pvalue)->attach_position);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_pull_uint16(pext,
			&((REND_DATA*)r->pvalue)->render_width);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_pull_uint16(pext,
			&((REND_DATA*)r->pvalue)->render_height);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_pull_uint32(pext,
			&((REND_DATA*)r->pvalue)->data_flags);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_DELEGATE:
	case ATTRIBUTE_ID_ATTACHDATA:
	case ATTRIBUTE_ID_ATTACHMETAFILE:
	case ATTRIBUTE_ID_MESSAGESTATUS:
		r->pvalue = pext->alloc(sizeof(BINARY));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		((BINARY*)r->pvalue)->cb = len;
		((BINARY*)r->pvalue)->pb = pext->alloc(len);
		if (NULL == ((BINARY*)r->pvalue)->pb) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_bytes(pext,
			((BINARY*)r->pvalue)->pb, len);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_TNEFVERSION:
	case ATTRIBUTE_ID_OEMCODEPAGE:
		r->pvalue = pext->alloc(sizeof(LONG_ARRAY));
		if (NULL == r->pvalue) {
			return EXT_ERR_ALLOC;
		}
		((LONG_ARRAY*)r->pvalue)->count = len / sizeof(uint32_t);
		((LONG_ARRAY*)r->pvalue)->pl = pext->alloc(len);
		if (NULL == ((LONG_ARRAY*)r->pvalue)->pl) {
			return EXT_ERR_ALLOC;
		}
		for (i=0; i<((LONG_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_pull_uint32(pext,
				((LONG_ARRAY*)r->pvalue)->pl + i);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		break;
	}
	if (pext->offset > offset + len) {
		debug_info("[tnef]: attribute data length error");
		return EXT_ERR_FORMAT;
	}
	pext->offset = offset + len;
	status = ext_buffer_pull_uint16(pext, &checksum);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
#ifdef _DEBUG_UMTA
	if (checksum != tnef_generate_checksum(
		pext->data + offset, len)) {
		debug_info("[tnef]: invalid checksum");
	}
#endif
	return EXT_ERR_SUCCESS;
}

static const char *tnef_to_msgclass(const char *str_class)
{
	if (0 == strcasecmp("IPM.Microsoft Mail.Note", str_class)) {
		return "IPM.Note";
	} else if (0 == strcasecmp("IPM.Microsoft Mail.Read Receipt",
		str_class)) {
		return "Report.IPM.Note.IPNRN";
	} else if (0 == strcasecmp("IPM.Microsoft Mail.Non-Delivery",
		str_class)) {
		return "Report.IPM.Note.NDR";
	} else if (0 == strcasecmp("IPM.Microsoft Schedule.MtgRespP",
		str_class)) {
		return "IPM.Schedule.Meeting.Resp.Pos";
	} else if (0 == strcasecmp("IPM.Microsoft Schedule.MtgRespN",
		str_class)) {
		return "IPM.Schedule.Meeting.Resp.Neg";
	} else if (0 == strcasecmp("IPM.Microsoft Schedule.MtgRespA",
		str_class)) {
		return "IPM.Schedule.Meeting.Resp.Tent";
	} else if (0 == strcasecmp("IPM.Microsoft Schedule.MtgReq",
		str_class)) {
		return "IPM.Schedule.Meeting.Request";
	} else if (0 == strcasecmp("IPM.Microsoft Schedule.MtgCncl",
		str_class)) {
		return "IPM.Schedule.Meeting.Canceled";
	}
	return str_class;
}

static BOOL tnef_set_attribute_address(TPROPVAL_ARRAY *pproplist,
	uint32_t proptag1, uint32_t proptag2, uint32_t proptag3,
	ATTR_ADDR *paddr)
{
	char *ptr;
	TAGGED_PROPVAL propval;
	
	propval.proptag = proptag1;
	propval.pvalue = paddr->displayname;
	if (FALSE == tpropval_array_set_propval(
		pproplist, &propval)) {
		return FALSE;
	}
	ptr = strchr(paddr->address, ':');
	if (NULL == ptr) {
		return FALSE;
	}
	*ptr = '\0';
	ptr ++;
	propval.proptag = proptag2;
	propval.pvalue = paddr->address;
	if (FALSE == tpropval_array_set_propval(
		pproplist, &propval)) {
		return FALSE;
	}
	propval.proptag = proptag3;
	propval.pvalue = ptr;
	return tpropval_array_set_propval(pproplist, &propval);
}

static void tnef_convert_from_propname(const PROPERTY_NAME *ppropname,
	char *tag_string)
{
	char tmp_guid[64];
	
	guid_to_string(&ppropname->guid, tmp_guid, 64);
	if (KIND_LID == ppropname->kind) {
		snprintf(tag_string, 256, "%s:lid:%u", tmp_guid, *ppropname->plid);
	} else {
		snprintf(tag_string, 256, "%s:name:%s", tmp_guid, ppropname->pname);
	}
	lower_string(tag_string);
}

static BOOL tnef_convert_to_propname(char *tag_string,
	PROPERTY_NAME *ppropname, EXT_BUFFER_ALLOC alloc)
{
	int len;
	char *ptr;
	
	ptr = strchr(tag_string, ':');
	if (NULL == ptr) {
		return FALSE;
	}
	*ptr = '\0';
	if (FALSE == guid_from_string(&ppropname->guid, tag_string)) {
		return FALSE;
	}
	ptr ++;
	if (0 == strncmp(ptr, "lid:", 4)) {
		ppropname->kind = KIND_LID;
		ppropname->pname = NULL;
		ppropname->plid = alloc(sizeof(uint32_t));
		if (NULL == ppropname->plid) {
			return FALSE;
		}
		*ppropname->plid = atoi(ptr + 4);
		return TRUE;
	} else if (0 == strncmp(ptr, "name:", 5)) {
		ppropname->kind = KIND_NAME;
		ppropname->plid = NULL;
		len = strlen(ptr + 5) + 1;
		ppropname->pname = alloc(len);
		if (NULL == ppropname->pname) {
			return FALSE;
		}
		strcpy(ppropname->pname, ptr + 5);
		return TRUE;
	}
	return FALSE;
}

void tnef_replace_propid(TPROPVAL_ARRAY *pproplist, INT_HASH_TABLE *phash)
{
	int i;
	uint16_t propid;
	uint32_t proptag;
	uint16_t *ppropid;
	
	for (i=0; i<pproplist->count; i++) {
		proptag = pproplist->ppropval[i].proptag;
		propid = proptag >> 16;
		if (0 == (propid & 0x8000)) {
			continue;
		}
		ppropid = int_hash_query(phash, propid);
		if (NULL == ppropid || 0 == *ppropid) {
			tpropval_array_remove_propval(pproplist, proptag);
			i --;
			continue;
		}
		proptag = *ppropid;
		proptag <<= 16;
		pproplist->ppropval[i].proptag &= 0xFFFF;
		pproplist->ppropval[i].proptag |= proptag;
	}
}

char* tnef_duplicate_string_to_unicode(
	const char *charset, const char *pstring)
{
	char *pstr_out;
	
	pstr_out = malloc(2*strlen(pstring) + 2);
	if (NULL == pstr_out) {
		return NULL;
	}
	if (FALSE == string_to_utf8(charset, pstring, pstr_out)) {
		free(pstr_out);
		return NULL;
	}
	return pstr_out;
}

STRING_ARRAY* tnef_duplicate_string_array_to_unicode(
	const char *charset, STRING_ARRAY *parray)
{
	int i;
	STRING_ARRAY *parray_out;
	
	parray_out = malloc(sizeof(STRING_ARRAY));
	if (NULL == parray_out) {
		return NULL;
	}
	parray_out->count = parray->count;
	if (parray->count > 0) {
		parray_out->ppstr = malloc(sizeof(void*)*parray->count);
		if (NULL == parray_out->ppstr) {
			free(parray_out);
			return NULL;
		}
	} else {
		parray_out->ppstr = NULL;
	}
	for (i=0; i<parray->count; i++) {
		parray_out->ppstr[i] =
			tnef_duplicate_string_to_unicode(
			charset, parray->ppstr[i]);
		if (NULL == parray_out->ppstr[i]) {
			for (i-=1; i>=0; i--) {
				free(parray_out->ppstr[i]);
			}
			free(parray_out->ppstr);
			free(parray_out);
			return NULL;
		}
	}
	return parray_out;
}

static void tnef_tpropval_array_to_unicode(
	const char *charset, TPROPVAL_ARRAY *pproplist)
{
	int i;
	void *pvalue;
	uint16_t proptype;
	STRING_ARRAY *parray;
	
	for (i=0; i<pproplist->count; i++) {
		proptype = pproplist->ppropval[i].proptag & 0xFFFF;
		if (PROPVAL_TYPE_STRING == proptype) {
			pvalue = tnef_duplicate_string_to_unicode(
				charset, pproplist->ppropval[i].pvalue);
			proptype = PROPVAL_TYPE_WSTRING;
		} else if (PROPVAL_TYPE_STRING_ARRAY == proptype) {
			pvalue = tnef_duplicate_string_array_to_unicode(
					charset, pproplist->ppropval[i].pvalue);
			proptype = PROPVAL_TYPE_WSTRING_ARRAY;
		} else {
			continue;
		}
		if (NULL == pvalue) {
			continue;
		}
		propval_free(proptype, pproplist->ppropval[i].pvalue);
		pproplist->ppropval[i].pvalue = pvalue;
		pproplist->ppropval[i].proptag &= 0xFFFF0000;
		pproplist->ppropval[i].proptag |= proptype;
	}
}

static void tnef_message_to_unicode(
	uint32_t cpid, MESSAGE_CONTENT *pmsg)
{
	int i;
	const char *charset;
	
	charset = tnef_cpid_to_charset(cpid);
	if (NULL == charset) {
		charset = "CP1252";
	}
	tnef_tpropval_array_to_unicode(charset, &pmsg->proplist);
	if (NULL != pmsg->children.prcpts) {
		for (i=0; i<pmsg->children.prcpts->count; i++) {
			tnef_tpropval_array_to_unicode(charset,
				pmsg->children.prcpts->pparray[i]);
		}
	}
	if (NULL != pmsg->children.pattachments) {
		for (i=0; i<pmsg->children.pattachments->count; i++) {
			tnef_tpropval_array_to_unicode(charset,
				&pmsg->children.pattachments->pplist[i]->proplist);
		}
	}
}

static MESSAGE_CONTENT* tnef_deserialize_internal(const void *pbuff,
	uint32_t length, BOOL b_embedded, EXT_BUFFER_ALLOC alloc,
	GET_PROPIDS get_propids, USERNAME_TO_ENTRYID username_to_entryid)
{
	int i, j;
	int count;
	char *psmtp;
	BOOL b_props;
	uint32_t cpid;
	BINARY tmp_bin;
	uint8_t cur_lvl;
	uint8_t tmp_byte;
	uint16_t *ppropid;
	ATTR_ADDR *powner;
	EXT_PULL ext_pull;
	uint16_t tmp_int16;
	uint32_t tmp_int32;
	TARRAY_SET *prcpts;
	char *pdisplay_name;
	STR_HASH_ITER *iter;
	char tmp_buff[1280];
	char tmp_string[256];
	uint16_t last_propid;
	PROPID_ARRAY propids;
	PROPID_ARRAY propids1;
	STR_HASH_TABLE *phash;
	MESSAGE_CONTENT *pmsg;
	INT_HASH_TABLE *phash1;
	TAGGED_PROPVAL propval;
	PROPNAME_ARRAY propnames;
	TNEF_ATTRIBUTE attribute;
	const char *message_class;
	TPROPVAL_ARRAY *pproplist;
	MESSAGE_CONTENT *pembedded;
	TNEF_PROPVAL *ptnef_propval;
	TNEF_PROPLIST *ptnef_proplist;
	ATTACHMENT_LIST *pattachments;
	ATTACHMENT_CONTENT *pattachment;
	
	
	ext_buffer_pull_init(&ext_pull, pbuff,
		length, alloc, EXT_FLAG_UTF16);
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint32(
		&ext_pull, &tmp_int32)) {
		return NULL;
	}
	if (tmp_int32 != 0x223e9f78) {
		debug_info("[tnef]: TNEF SIGNATURE error");
		return NULL;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_pull_uint16(
		&ext_pull, &tmp_int16)) {
		return NULL;
	}
	if (EXT_ERR_SUCCESS != tnef_pull_attribute(
		&ext_pull, &attribute)) {
		return NULL;
	}
	if (ATTRIBUTE_ID_TNEFVERSION != attribute.attr_id) {
		debug_info("[tnef]: cannot find idTnefVersion");
		return NULL;
	}
	if (EXT_ERR_SUCCESS != tnef_pull_attribute(
		&ext_pull, &attribute)) {
		return NULL;
	}
	if (ATTRIBUTE_ID_OEMCODEPAGE != attribute.attr_id) {
		debug_info("[tnef]: cannot find idOEMCodePage");
		return NULL;
	}
	if (0 == ((LONG_ARRAY*)attribute.pvalue)->count) {
		debug_info("[tnef]: cannot find PrimaryCodePage");
		return NULL;
	}
	cpid = ((LONG_ARRAY*)attribute.pvalue)->pl[0];
	b_props = FALSE;
	cur_lvl = LVL_MESSAGE;
	powner = NULL;
	message_class = NULL;
	pmsg = message_content_init();
	if (NULL == pmsg) {
		return NULL;
	}
	last_propid = 0x8000;
	phash = str_hash_init(0x1000, sizeof(uint16_t), NULL);
	if (NULL == phash) {
		message_content_free(pmsg);
		return NULL;
	}
	do {
		if (EXT_ERR_SUCCESS != tnef_pull_attribute(
			&ext_pull, &attribute)) {
			if (0 == pmsg->proplist.count) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		}
		if (attribute.lvl != cur_lvl) {
			if (ATTRIBUTE_ID_ATTACHRENDDATA == attribute.attr_id) {
				cur_lvl = LVL_ATTACHMENT;
				break;
			} else {
				debug_info("[tnef]: attachment should "
					"begin with attAttachRendData");
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
		}
		if (TRUE == b_props) {
			debug_info("[tnef]: attMsgProps should be "
				"the last attribute in message level");
			str_hash_free(phash);
			message_content_free(pmsg);
			return NULL;
		}
		switch (attribute.attr_id) {
		case ATTRIBUTE_ID_MSGPROPS:
			count = ((TNEF_PROPLIST*)attribute.pvalue)->count;
			for (i=0; i<count; i++) {
				ptnef_propval = ((TNEF_PROPLIST*)
					attribute.pvalue)->ppropval + i;
				if (NULL != ptnef_propval->ppropname) {
					tnef_convert_from_propname(
						ptnef_propval->ppropname,
						tmp_string);
					ppropid = str_hash_query(phash, tmp_string);
					if (NULL == ppropid) {
						if (1 != str_hash_add(phash,
							tmp_string, &last_propid)) {
							str_hash_free(phash);
							message_content_free(pmsg);
							return NULL;
						}
						ptnef_propval->propid = last_propid;
						last_propid ++;
					} else {
						ptnef_propval->propid = *ppropid;
					}
				}
				propval.proptag = ptnef_propval->propid;
				propval.proptag <<= 16;
				propval.proptag |= ptnef_propval->proptype;
				propval.pvalue = ptnef_propval->pvalue;
				if (FALSE == tpropval_array_set_propval(
					&pmsg->proplist, &propval)) {
					str_hash_free(phash);
					message_content_free(pmsg);
					return NULL;
				}
			}
			b_props = TRUE;
			break;
		case ATTRIBUTE_ID_OWNER:
			powner = attribute.pvalue;
			break;
		case ATTRIBUTE_ID_SENTFOR:
			if (FALSE == tnef_set_attribute_address(&pmsg->proplist,
				PROP_TAG_SENTREPRESENTINGNAME_STRING8,
				PROP_TAG_SENTREPRESENTINGADDRESSTYPE_STRING8,
				PROP_TAG_SENTREPRESENTINGEMAILADDRESS_STRING8,
				attribute.pvalue)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_DELEGATE:
			propval.proptag = PROP_TAG_RECEIVEDREPRESENTINGENTRYID;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_DATESTART:
			propval.proptag = PROP_TAG_STARTDATE;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_DATEEND:
			propval.proptag = PROP_TAG_ENDDATE;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_AIDOWNER:
			propval.proptag = PROP_TAG_OWNERAPPOINTMENTID;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_REQUESTRES:
			if (0 == (uint16_t*)attribute.pvalue) {
				tmp_byte = 0;
			} else {
				tmp_byte = 1;
			}
			propval.proptag = PROP_TAG_RESPONSEREQUESTED;
			propval.pvalue = &tmp_byte;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_ORIGNINALMESSAGECLASS:
			propval.proptag = PROP_TAG_ORIGINALMESSAGECLASS_STRING8;
			propval.pvalue = (char*)tnef_to_msgclass(attribute.pvalue);
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_FROM:
			if (FALSE == tnef_set_attribute_address(&pmsg->proplist,
				PROP_TAG_SENDERNAME_STRING8,
				PROP_TAG_SENDERADDRESSTYPE_STRING8,
				PROP_TAG_SENDEREMAILADDRESS_STRING8,
				attribute.pvalue)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_SUBJECT:
			propval.proptag = PROP_TAG_SUBJECT_STRING8;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_DATESENT:
			propval.proptag = PROP_TAG_CLIENTSUBMITTIME;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_DATERECD:
			propval.proptag = PROP_TAG_MESSAGEDELIVERYTIME;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_MESSAGESTATUS:
			if (TRUE == b_embedded && 0 != ((BINARY*)attribute.pvalue)->cb) {
				tmp_int32 = 0;
				if (FMS_LOCAL & (*((BINARY*)attribute.pvalue)->pb)) {
					tmp_int32 |= MESSAGE_FLAG_UNSENT;
				}
				if (FMS_SUBMITTED & (*((BINARY*)attribute.pvalue)->pb)) {
					tmp_int32 |= MESSAGE_FLAG_SUBMITTED;
				}
				if (0 != tmp_int32) {
					propval.proptag = PROP_TAG_MESSAGEFLAGS;
					propval.pvalue = &tmp_int32;
					if (FALSE == tpropval_array_set_propval(
						&pmsg->proplist, &propval)) {
						str_hash_free(phash);
						message_content_free(pmsg);
						return NULL;
					}
				}
			}
			break;
		case ATTRIBUTE_ID_MESSAGECLASS:
			message_class = tnef_to_msgclass(attribute.pvalue);
			propval.proptag = PROP_TAG_MESSAGECLASS_STRING8;
			propval.pvalue = (char*)message_class;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_MESSAGEID:
			propval.proptag = PROP_TAG_SEARCHKEY;
			tmp_bin.cb = strlen(attribute.pvalue)/2;
			if (tmp_bin.cb > 0) { 
				tmp_bin.pb = alloc(tmp_bin.cb);
				if (NULL == tmp_bin.pb) {
					str_hash_free(phash);
					message_content_free(pmsg);
					return NULL;
				}
				if (FALSE == decode_hex_binary(attribute.pvalue,
					tmp_bin.pb, tmp_bin.cb)) {
					str_hash_free(phash);
					message_content_free(pmsg);
					return NULL;
				}
				propval.pvalue = &tmp_bin;
				if (FALSE == tpropval_array_set_propval(
					&pmsg->proplist, &propval)) {
					str_hash_free(phash);
					message_content_free(pmsg);
					return NULL;
				}
			}
			break;
		case ATTRIBUTE_ID_BODY:
			propval.proptag = PROP_TAG_BODY_STRING8;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_PRIORITY:
			propval.proptag = PROP_TAG_IMPORTANCE;
			switch (*(uint16_t*)attribute.pvalue) {
			case 3:
				tmp_int32 = 0;
				break;
			case 2:
				tmp_int32 = 1;
				break;
			case 1:
				tmp_int32 = 2;
				break;
			default:
				debug_info("[tnef]: attPriority error");
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			propval.pvalue = &tmp_int32;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_DATEMODIFY:
			propval.proptag = PROP_TAG_LASTMODIFICATIONTIME;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_RECIPTABLE:
			if (NULL != pmsg->children.prcpts) {
				debug_info("[tnef]: idRecipTable already met");
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			prcpts = tarray_set_init();
			if (NULL == prcpts) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			message_content_set_rcpts_internal(pmsg, prcpts);
			for (i=0; i<((TNEF_PROPSET*)attribute.pvalue)->count; i++) {
				ptnef_proplist = ((TNEF_PROPSET*)attribute.pvalue)->pplist[i];
				pproplist = tpropval_array_init();
				if (NULL == pproplist) {
					str_hash_free(phash);
					message_content_free(pmsg);
					return NULL;
				}
				if (FALSE == tarray_set_append_internal(
					prcpts, pproplist)) {
					tpropval_array_free(pproplist);
					str_hash_free(phash);
					message_content_free(pmsg);
					return NULL;
				}
				for (j=0; j<ptnef_proplist->count; j++) {
					ptnef_propval = ptnef_proplist->ppropval + j;
					if (NULL != ptnef_propval->ppropname) {
						tnef_convert_from_propname(
							ptnef_propval->ppropname,
							tmp_string);
						ppropid = str_hash_query(phash, tmp_string);
						if (NULL == ppropid) {
							if (1 != str_hash_add(phash,
								tmp_string, &last_propid)) {
								str_hash_free(phash);
								message_content_free(pmsg);
								return NULL;
							}
							ptnef_propval->propid = last_propid;
							last_propid ++;
						} else {
							ptnef_propval->propid = *ppropid;
						}
					}
					propval.proptag = ptnef_propval->propid;
					propval.proptag <<= 16;
					propval.proptag |= ptnef_propval->proptype;
					propval.pvalue = ptnef_propval->pvalue;
					if (FALSE == tpropval_array_set_propval(
						pproplist, &propval)) {
						str_hash_free(phash);
						message_content_free(pmsg);
						return NULL;
					}
				}
				tpropval_array_remove_propval(pproplist, PROP_TAG_ENTRYID);
				psmtp = tpropval_array_get_propval(
					pproplist, PROP_TAG_SMTPADDRESS);
				pdisplay_name = tpropval_array_get_propval(
							pproplist, PROP_TAG_DISPLAYNAME);
				if (NULL != psmtp) {
					tmp_bin.cb = 0;
					tmp_bin.pb = tmp_buff;
					if (FALSE == username_to_entryid(psmtp,
						pdisplay_name, &tmp_bin, NULL)) {
						str_hash_free(phash);
						message_content_free(pmsg);
						return NULL;
					}
					propval.proptag = PROP_TAG_ENTRYID;
					propval.pvalue = &tmp_bin;
					if (FALSE == tpropval_array_set_propval(
						pproplist, &propval)) {
						str_hash_free(phash);
						message_content_free(pmsg);
						return NULL;
					}
				}
			}
			break;
		case ATTRIBUTE_ID_PARENTID:
		case ATTRIBUTE_ID_CONVERSATIONID:
			/* have been deprecated in Exchange Server */
			break;
		default:
			debug_info("[tnef]: illegal attribute ID %x", attribute.attr_id);
			str_hash_free(phash);
			message_content_free(pmsg);
			return NULL;
		}
	} while (ext_pull.offset < length);
	
	if (NULL != powner && NULL != message_class) {
		if (0 == strcasecmp(message_class,
			"IPM.Schedule.Meeting.Request") ||
			0 == strcasecmp(message_class,
			"IPM.Schedule.Meeting.Canceled")) {
			if (FALSE == tnef_set_attribute_address(&pmsg->proplist,
				PROP_TAG_SENTREPRESENTINGNAME_STRING8,
				PROP_TAG_SENTREPRESENTINGADDRESSTYPE_STRING8,
				PROP_TAG_SENTREPRESENTINGEMAILADDRESS_STRING8, powner)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
		} else if (0 == strcasecmp(message_class,
			"IPM.Schedule.Meeting.Resp.Pos") ||
			0 == strcasecmp(message_class,
			"IPM.Schedule.Meeting.Resp.Neg") ||
			0 == strcasecmp(message_class,
			"IPM.Schedule.Meeting.Resp.Tent")) {
			if (FALSE == tnef_set_attribute_address(&pmsg->proplist,
				PROP_TAG_RECEIVEDREPRESENTINGNAME_STRING8,
				PROP_TAG_RECEIVEDREPRESENTINGADDRESSTYPE_STRING8,
				PROP_TAG_RECEIVEDREPRESENTINGEMAILADDRESS_STRING8, powner)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
		}
		
	}
	
	if (LVL_MESSAGE == cur_lvl) {
		goto FETCH_PROPNAME;
	}
	
	pattachments = attachment_list_init();
	if (NULL == pattachments) {
		str_hash_free(phash);
		message_content_free(pmsg);
		return NULL;
	}
	message_content_set_attachments_internal(pmsg, pattachments);
	while (TRUE) {
		if (TRUE == b_props && attribute.attr_id !=
			ATTRIBUTE_ID_ATTACHRENDDATA) {
			debug_info("[tnef]: attAttachment should be "
				"the last attribute in attachment level");
			str_hash_free(phash);
			message_content_free(pmsg);
			return NULL;
		}
		switch (attribute.attr_id) {
		case ATTRIBUTE_ID_ATTACHMENT:
			count = ((TNEF_PROPLIST*)attribute.pvalue)->count;
			for (i=0; i<count; i++) {
				ptnef_propval = ((TNEF_PROPLIST*)
					attribute.pvalue)->ppropval + i;
				if (NULL != ptnef_propval->ppropname) {
					tnef_convert_from_propname(
						ptnef_propval->ppropname,
						tmp_string);
					ppropid = str_hash_query(phash, tmp_string);
					if (NULL == ppropid) {
						if (1 != str_hash_add(phash,
							tmp_string, &last_propid)) {
							str_hash_free(phash);
							message_content_free(pmsg);
							return NULL;
						}
						ptnef_propval->propid = last_propid;
						last_propid ++;
					} else {
						ptnef_propval->propid = *ppropid;
					}
				}
				if (PROPVAL_TYPE_OBJECT == ptnef_propval->proptype) {
					if (0 == memcmp(IID_IMessage, ((BINARY*)
						ptnef_propval->pvalue)->pb, 16)) {
						pembedded = tnef_deserialize_internal(
							((BINARY*)ptnef_propval->pvalue)->pb + 16,
							((BINARY*)ptnef_propval->pvalue)->cb - 16,
							TRUE, alloc, get_propids, username_to_entryid);
						if (NULL == pembedded) {
							str_hash_free(phash);
							message_content_free(pmsg);
							return NULL;
						}
						attachment_content_set_embeded_internal(
							pattachment, pembedded);
					} else {
						((BINARY*)ptnef_propval->pvalue)->cb -= 16;
						memmove(((BINARY*)ptnef_propval->pvalue)->pb,
							((BINARY*)ptnef_propval->pvalue)->pb + 16,
						((BINARY*)ptnef_propval->pvalue)->cb);
					}
					continue;
				}
				propval.proptag = ptnef_propval->propid;
				propval.proptag <<= 16;
				propval.proptag |= ptnef_propval->proptype;
				propval.pvalue = ptnef_propval->pvalue;
				if (FALSE == tpropval_array_set_propval(
					&pattachment->proplist, &propval)) {
					str_hash_free(phash);
					message_content_free(pmsg);
					return NULL;
				}
			}
			b_props = TRUE;
			break;
		case ATTRIBUTE_ID_ATTACHDATA:
			propval.proptag = PROP_TAG_ATTACHDATABINARY;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pattachment->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_ATTACHTITLE:
			propval.proptag = PROP_TAG_ATTACHLONGFILENAME_STRING8;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pattachment->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_ATTACHMETAFILE:
			propval.proptag = PROP_TAG_ATTACHRENDERING;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pattachment->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_ATTACHCREATEDATE:
			propval.proptag = PROP_TAG_CREATIONTIME;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pattachment->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_ATTACHMODIFYDATE:
			propval.proptag = PROP_TAG_LASTMODIFICATIONTIME;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pattachment->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_ATTACHTRANSPORTFILENAME:
			propval.proptag = PROP_TAG_ATTACHTRANSPORTNAME_STRING8;
			propval.pvalue = attribute.pvalue;
			if (FALSE == tpropval_array_set_propval(
				&pattachment->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		case ATTRIBUTE_ID_ATTACHRENDDATA:
			pattachment = attachment_content_init();
			if (NULL == pattachment) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			if (FALSE == attachment_list_append_internal(
				pattachments, pattachment)) {
				attachment_content_free(pattachment);
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			if (ATTACH_TYPE_OLE == ((REND_DATA*)
				attribute.pvalue)->attach_type) {
				propval.proptag = PROP_TAG_ATTACHTAG;
				propval.pvalue = &tmp_bin;
				tmp_bin.cb = 11;
				tmp_bin.pb = OLE_TAG;
				if (FALSE == tpropval_array_set_propval(
					&pattachment->proplist, &propval)) {
					str_hash_free(phash);
					message_content_free(pmsg);
					return NULL;
				}
			}
			if (FILE_DATA_MACBINARY == ((REND_DATA*)
				attribute.pvalue)->attach_type) {
				propval.proptag = PROP_TAG_ATTACHENCODING;
				propval.pvalue = &tmp_bin;
				tmp_bin.cb = 9;
				tmp_bin.pb = MACBINARY_ENCODING;
				if (FALSE == tpropval_array_set_propval(
					&pattachment->proplist, &propval)) {
					str_hash_free(phash);
					message_content_free(pmsg);
					return NULL;
				}
			}
			propval.proptag = PROP_TAG_RENDERINGPOSITION;
			propval.pvalue = &((REND_DATA*)attribute.pvalue)->attach_position;
			if (FALSE == tpropval_array_set_propval(
				&pattachment->proplist, &propval)) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			b_props = FALSE;
			break;
		}
		if (ext_pull.offset == length) {
			break;
		}
		if (EXT_ERR_SUCCESS != tnef_pull_attribute(
			&ext_pull, &attribute)) {
			if (0 == pmsg->proplist.count) {
				str_hash_free(phash);
				message_content_free(pmsg);
				return NULL;
			}
			break;
		}
	}
FETCH_PROPNAME:
	propids.count = 0;
	propids.ppropid = alloc(sizeof(uint16_t)*phash->item_num);
	if (NULL == propids.ppropid) {
		str_hash_free(phash);
		message_content_free(pmsg);
		return NULL;
	}
	propnames.count = 0;
	propnames.ppropname = alloc(sizeof(PROPERTY_NAME)*phash->item_num);
	if (NULL == propnames.ppropname) {
		str_hash_free(phash);
		message_content_free(pmsg);
		return NULL;
	}
	iter = str_hash_iter_init(phash);
	for (str_hash_iter_begin(iter); !str_hash_iter_done(iter);
		str_hash_iter_forward(iter)) {
		ppropid = str_hash_iter_get_value(iter, tmp_string);
		propids.ppropid[propids.count] = *ppropid;
		if (FALSE == tnef_convert_to_propname(tmp_string,
			propnames.ppropname + propnames.count, alloc)) {
			str_hash_iter_free(iter);
			str_hash_free(phash);
			message_content_free(pmsg);
			return NULL;
		}
		propids.count ++;
		propnames.count ++;
	}
	str_hash_iter_free(iter);
	str_hash_free(phash);
	
	if (FALSE == get_propids(&propnames, &propids1)) {
		message_content_free(pmsg);
		return NULL;
	}
	
	phash1 = int_hash_init(0x1000, sizeof(uint16_t), NULL);
	if (NULL == phash1) {
		message_content_free(pmsg);
		return NULL;
	}
	for (i=0; i<propids.count; i++) {
		int_hash_add(phash1, propids.ppropid[i], propids1.ppropid + i);
	}
	tnef_replace_propid(&pmsg->proplist, phash1);
	if (NULL != pmsg->children.prcpts) {
		for (i=0; i<pmsg->children.prcpts->count; i++) {
			tnef_replace_propid(pmsg->children.prcpts->pparray[i], phash1);
		}
	}
	if (NULL != pmsg->children.pattachments) {
		for (i=0; i<pmsg->children.pattachments->count; i++) {
			tnef_replace_propid(
				&pmsg->children.pattachments->pplist[i]->proplist, phash1);
		}
	}
	int_hash_free(phash1);
	
	if (NULL == tpropval_array_get_propval(
		&pmsg->proplist, PROP_TAG_INTERNETCODEPAGE)) {
		propval.proptag = PROP_TAG_INTERNETCODEPAGE;
		propval.pvalue = &cpid;
		tpropval_array_set_propval(&pmsg->proplist, &propval);
	}
	tnef_message_to_unicode(cpid, pmsg);
	tpropval_array_remove_propval(&pmsg->proplist, PROP_TAG_MID);
	tpropval_array_remove_propval(&pmsg->proplist, PROP_TAG_ENTRYID);
	tpropval_array_remove_propval(&pmsg->proplist, PROP_TAG_SEARCHKEY);
	return pmsg;
}

MESSAGE_CONTENT* tnef_deserialize(const void *pbuff,
	uint32_t length, EXT_BUFFER_ALLOC alloc, GET_PROPIDS get_propids,
	USERNAME_TO_ENTRYID username_to_entryid)
{
	return tnef_deserialize_internal(pbuff, length, FALSE,
			alloc, get_propids, username_to_entryid);
}

static int tnef_push_property_name(EXT_PUSH *pext, const PROPERTY_NAME *r)
{
	int status;
	uint32_t offset;
	uint32_t offset1;
	uint32_t tmp_int;
	
	status = ext_buffer_push_guid(pext, &r->guid);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (KIND_LID == r->kind) {
		tmp_int = 0;
	} else if (KIND_NAME == r->kind) {
		tmp_int = 1;
	} else {
		return EXT_ERR_FORMAT;
	}
	status = ext_buffer_push_uint32(pext, tmp_int);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (0 == tmp_int) {
		return ext_buffer_push_uint32(pext, *r->plid);
	} else if (1 == tmp_int) {
		offset = pext->offset;
		status = ext_buffer_push_advance(pext, sizeof(uint32_t));
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_wstring(pext, r->pname);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		offset1 = pext->offset;
		tmp_int = offset1 - (offset + sizeof(uint32_t));
		pext->offset = offset;
		status = ext_buffer_push_uint32(pext, tmp_int);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		pext->offset = offset1;
		return ext_buffer_push_bytes(pext,
			g_pad_bytes, tnef_align(tmp_int));
	}
}

static int tnef_push_propval(EXT_PUSH *pext, const TNEF_PROPVAL *r,
	EXT_BUFFER_ALLOC alloc, GET_PROPNAME get_propname)
{
	int i;
	int status;
	uint32_t offset;
	uint32_t offset1;
	uint32_t tmp_int;
	
	status = ext_buffer_push_uint16(pext, r->proptype);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint16(pext, r->propid);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (NULL != r->ppropname) {
		status = tnef_push_property_name(pext, r->ppropname);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
	}
	switch (r->proptype) {
	case PROPVAL_TYPE_SHORT:
		status = ext_buffer_push_uint16(pext, *(uint16_t*)r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		return ext_buffer_push_bytes(pext, g_pad_bytes, 2);
	case PROPVAL_TYPE_ERROR:
	case PROPVAL_TYPE_LONG:
		return ext_buffer_push_uint32(pext, *(uint32_t*)r->pvalue);
	case PROPVAL_TYPE_FLOAT:
		return ext_buffer_push_float(pext, *(float*)r->pvalue);
	case PROPVAL_TYPE_DOUBLE:
	case PROPVAL_TYPE_FLOATINGTIME:
		return ext_buffer_push_double(pext, *(double*)r->pvalue);
	case PROPVAL_TYPE_BYTE:
		status = ext_buffer_push_uint16(pext, *(uint8_t*)r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		return ext_buffer_push_bytes(pext, g_pad_bytes, 2);
	case PROPVAL_TYPE_CURRENCY:
	case PROPVAL_TYPE_LONGLONG:
	case PROPVAL_TYPE_FILETIME:
		return ext_buffer_push_uint64(pext, *(uint64_t*)r->pvalue);
	case PROPVAL_TYPE_STRING:
		status = ext_buffer_push_uint32(pext, 1);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		offset = pext->offset;
		status = ext_buffer_push_advance(pext, sizeof(uint32_t));
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_string(pext, r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		offset1 = pext->offset;
		tmp_int = offset1 - (offset + sizeof(uint32_t));
		pext->offset = offset;
		status = ext_buffer_push_uint32(pext, tmp_int);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		pext->offset = offset1;
		return ext_buffer_push_bytes(pext,
			g_pad_bytes, tnef_align(tmp_int));
	case PROPVAL_TYPE_WSTRING:
		status = ext_buffer_push_uint32(pext, 1);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		offset = pext->offset;
		status = ext_buffer_push_advance(pext, sizeof(uint32_t));
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_wstring(pext, r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		offset1 = pext->offset;
		tmp_int = offset1 - (offset + sizeof(uint32_t));
		pext->offset = offset;
		status = ext_buffer_push_uint32(pext, tmp_int);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		pext->offset = offset1;
		return ext_buffer_push_bytes(pext,
			g_pad_bytes, tnef_align(tmp_int));
	case PROPVAL_TYPE_GUID:
		return ext_buffer_push_guid(pext, r->pvalue);
	case PROPVAL_TYPE_SVREID:
		return ext_buffer_push_svreid(pext, r->pvalue);
	case PROPVAL_TYPE_OBJECT:
		status = ext_buffer_push_uint32(pext, 1);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		if (0xFFFFFFFF != ((BINARY*)r->pvalue)->cb) {
			status = ext_buffer_push_uint32(pext,
					((BINARY*)r->pvalue)->cb + 16);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			status = ext_buffer_push_bytes(pext, IID_IStorage, 16);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			status = ext_buffer_push_bytes(pext,
					((BINARY*)r->pvalue)->pb,
					((BINARY*)r->pvalue)->cb);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			return ext_buffer_push_bytes(pext, g_pad_bytes,
				tnef_align(((BINARY*)r->pvalue)->cb + 16));
		} else {
			offset = pext->offset;
			status = ext_buffer_push_advance(pext, sizeof(uint32_t));
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			status = ext_buffer_push_bytes(pext, IID_IMessage, 16);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			if (FALSE == tnef_serialize_internal(pext, TRUE,
				(MESSAGE_CONTENT*)((BINARY*)r->pvalue)->pb,
				alloc, get_propname)) {
				return EXT_ERR_FORMAT;
			}
			offset1 = pext->offset;
			tmp_int = offset1 - (offset + sizeof(uint32_t));
			pext->offset = offset;
			status = ext_buffer_push_uint32(pext, tmp_int);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			pext->offset = offset1;
			return ext_buffer_push_bytes(pext,
				g_pad_bytes, tnef_align(tmp_int));
		}
	case PROPVAL_TYPE_BINARY:
		status = ext_buffer_push_uint32(pext, 1);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint32(pext,
			((BINARY*)r->pvalue)->cb);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_bytes(pext,
					((BINARY*)r->pvalue)->pb,
					((BINARY*)r->pvalue)->cb);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		return ext_buffer_push_bytes(pext, g_pad_bytes,
			tnef_align(((BINARY*)r->pvalue)->cb));
	case PROPVAL_TYPE_SHORT_ARRAY:
		status = ext_buffer_push_uint32(pext,
			((SHORT_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		for (i=0; i<((SHORT_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_push_uint16(pext, 
				((SHORT_ARRAY*)r->pvalue)->ps[i]);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			status = ext_buffer_push_bytes(pext, g_pad_bytes, 2);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	case PROPVAL_TYPE_LONG_ARRAY:
		status = ext_buffer_push_uint32(pext,
			((LONG_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		for (i=0; i<((LONG_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_push_uint32(pext, 
				((LONG_ARRAY*)r->pvalue)->pl[i]);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	case PROPVAL_TYPE_LONGLONG_ARRAY:
		status = ext_buffer_push_uint32(pext,
			((LONGLONG_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		for (i=0; i<((LONGLONG_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_push_uint64(pext, 
				((LONGLONG_ARRAY*)r->pvalue)->pll[i]);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	case PROPVAL_TYPE_STRING_ARRAY:
		status = ext_buffer_push_uint32(pext,
			((STRING_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		for (i=0; i<((STRING_ARRAY*)r->pvalue)->count; i++) {
			offset = pext->offset;
			status = ext_buffer_push_advance(pext, sizeof(uint32_t));
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			status = ext_buffer_push_string(pext,
				((STRING_ARRAY*)r->pvalue)->ppstr[i]);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			offset1 = pext->offset;
			tmp_int = offset1 - (offset + sizeof(uint32_t));
			pext->offset = offset;
			status = ext_buffer_push_uint32(pext, tmp_int);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			pext->offset = offset1;
			status = ext_buffer_push_bytes(pext,
				g_pad_bytes, tnef_align(tmp_int));
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	case PROPVAL_TYPE_WSTRING_ARRAY:
		status = ext_buffer_push_uint32(pext,
			((STRING_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		for (i=0; i<((STRING_ARRAY*)r->pvalue)->count; i++) {
			offset = pext->offset;
			status = ext_buffer_push_advance(pext, sizeof(uint32_t));
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			status = ext_buffer_push_wstring(pext,
				((STRING_ARRAY*)r->pvalue)->ppstr[i]);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			offset1 = pext->offset;
			tmp_int = offset1 - (offset + sizeof(uint32_t));
			pext->offset = offset;
			status = ext_buffer_push_uint32(pext, tmp_int);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			pext->offset = offset1;
			status = ext_buffer_push_bytes(pext,
				g_pad_bytes, tnef_align(tmp_int));
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	case PROPVAL_TYPE_GUID_ARRAY:
		status = ext_buffer_push_uint32(pext,
			((GUID_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		for (i=0; i<((GUID_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_push_guid(pext,
				((GUID_ARRAY*)r->pvalue)->pguid + i);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	case PROPVAL_TYPE_BINARY_ARRAY:
		status = ext_buffer_push_uint32(pext,
			((BINARY_ARRAY*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		for (i=0; i<((BINARY_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_push_uint32(pext,
				((BINARY_ARRAY*)r->pvalue)->pbin[i].cb);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			status = ext_buffer_push_bytes(pext,
				((BINARY_ARRAY*)r->pvalue)->pbin[i].pb,
				((BINARY_ARRAY*)r->pvalue)->pbin[i].cb);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			status = ext_buffer_push_bytes(pext, g_pad_bytes,
				tnef_align(((BINARY_ARRAY*)r->pvalue)->pbin[i].cb));
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		return EXT_ERR_SUCCESS;
	}
	return EXT_ERR_BAD_SWITCH;
}

static int tnef_push_attribute(EXT_PUSH *pext, const TNEF_ATTRIBUTE *r,
	EXT_BUFFER_ALLOC alloc, GET_PROPNAME get_propname)
{
	int i, j;
	int status;
	DTR tmp_dtr;
	uint32_t offset;
	uint32_t offset1;
	time_t unix_time;
	struct tm tmp_tm;
	uint16_t tmp_len;
	uint32_t tmp_len1;
    uint16_t checksum;
	TRP_HEADER header;
	static uint8_t empty_bytes[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    status = ext_buffer_push_uint8(pext, r->lvl);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, r->attr_id);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	offset = pext->offset;
	status = ext_buffer_push_advance(pext, sizeof(uint32_t));
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	switch (r->attr_id) {
	case ATTRIBUTE_ID_FROM:
		status = ext_buffer_push_uint16(pext, 0x0004);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		header.displayname_len =
			strlen(((ATTR_ADDR*)r->pvalue)->displayname) + 1;
		header.address_len = strlen(((ATTR_ADDR*)r->pvalue)->address) + 1;
		header.total_len = header.displayname_len + header.address_len + 16;
		status = ext_buffer_push_uint16(pext, header.total_len);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint16(pext, header.displayname_len);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint16(pext, header.address_len);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_string(pext,
			((ATTR_ADDR*)r->pvalue)->displayname);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_string(pext,
			((ATTR_ADDR*)r->pvalue)->address);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_bytes(pext, empty_bytes, 8);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_SUBJECT:
	case ATTRIBUTE_ID_MESSAGEID:
	case ATTRIBUTE_ID_ATTACHTITLE:
	case ATTRIBUTE_ID_ORIGNINALMESSAGECLASS:
	case ATTRIBUTE_ID_MESSAGECLASS:
	case ATTRIBUTE_ID_ATTACHTRANSPORTFILENAME:
		status = ext_buffer_push_string(pext, r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_DATESTART:
	case ATTRIBUTE_ID_DATEEND:
	case ATTRIBUTE_ID_DATESENT:
	case ATTRIBUTE_ID_DATERECD:
	case ATTRIBUTE_ID_ATTACHCREATEDATE:
	case ATTRIBUTE_ID_ATTACHMODIFYDATE:
	case ATTRIBUTE_ID_DATEMODIFY:
		unix_time = rop_util_nttime_to_unix(*(uint64_t*)r->pvalue);
		localtime_r(&unix_time, &tmp_tm);
		tmp_dtr.sec = tmp_tm.tm_sec;
		tmp_dtr.min = tmp_tm.tm_min;
		tmp_dtr.hour = tmp_tm.tm_hour;
		tmp_dtr.day = tmp_tm.tm_mday;
		tmp_dtr.month = tmp_tm.tm_mon + 1;
		tmp_dtr.year = tmp_tm.tm_year + 1900;
		tmp_dtr.dow = tmp_tm.tm_wday + 1;
		status = ext_buffer_push_uint16(pext, tmp_dtr.year);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint16(pext, tmp_dtr.month);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint16(pext, tmp_dtr.day);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint16(pext, tmp_dtr.hour);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint16(pext, tmp_dtr.min);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint16(pext, tmp_dtr.sec);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint16(pext, tmp_dtr.dow);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_REQUESTRES:
	case ATTRIBUTE_ID_PRIORITY:
		status = ext_buffer_push_uint16(pext, *(uint16_t*)r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_AIDOWNER:
		status = ext_buffer_push_uint32(pext, *(uint32_t*)r->pvalue);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_BODY:
		status = ext_buffer_push_bytes(pext, r->pvalue, strlen(r->pvalue));
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_MSGPROPS:
	case ATTRIBUTE_ID_ATTACHMENT:
		status = ext_buffer_push_uint32(pext,
			((TNEF_PROPLIST*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		for (i=0; i<((TNEF_PROPLIST*)r->pvalue)->count; i++) {
			status = tnef_push_propval(pext, ((TNEF_PROPLIST*)
				r->pvalue)->ppropval + i, alloc, get_propname);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		break;
	case ATTRIBUTE_ID_RECIPTABLE:
		status = ext_buffer_push_uint32(pext,
			((TNEF_PROPSET*)r->pvalue)->count);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		for (i=0; i<((TNEF_PROPSET*)r->pvalue)->count; i++) {
			status = ext_buffer_push_uint32(pext,
				((TNEF_PROPSET*)r->pvalue)->pplist[i]->count);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
			for (j=0; j<((TNEF_PROPSET*)r->pvalue)->pplist[i]->count; j++) {
				status = tnef_push_propval(pext, ((TNEF_PROPSET*)
							r->pvalue)->pplist[i]->ppropval + j,
							alloc, get_propname);
				if (EXT_ERR_SUCCESS != status) {
					return status;
				}
			}
		}
		break;
	case ATTRIBUTE_ID_OWNER:
	case ATTRIBUTE_ID_SENTFOR:
		tmp_len = strlen(((ATTR_ADDR*)r->pvalue)->displayname) + 1;
		status = ext_buffer_push_uint16(pext, tmp_len);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_string(pext,
			((ATTR_ADDR*)r->pvalue)->displayname);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		tmp_len = strlen(((ATTR_ADDR*)r->pvalue)->address) + 1;
		status = ext_buffer_push_uint16(pext, tmp_len);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_string(pext,
			((ATTR_ADDR*)r->pvalue)->address);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_ATTACHRENDDATA:
		status = ext_buffer_push_uint16(pext,
			((REND_DATA*)r->pvalue)->attach_type);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint32(pext,
			((REND_DATA*)r->pvalue)->attach_position);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint16(pext,
			((REND_DATA*)r->pvalue)->render_width);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint16(pext,
			((REND_DATA*)r->pvalue)->render_height);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		status = ext_buffer_push_uint32(pext,
			((REND_DATA*)r->pvalue)->data_flags);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_DELEGATE:
	case ATTRIBUTE_ID_ATTACHDATA:
	case ATTRIBUTE_ID_ATTACHMETAFILE:
	case ATTRIBUTE_ID_MESSAGESTATUS:
		status = ext_buffer_push_bytes(pext,
					((BINARY*)r->pvalue)->pb,
					((BINARY*)r->pvalue)->cb);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
		break;
	case ATTRIBUTE_ID_TNEFVERSION:
	case ATTRIBUTE_ID_OEMCODEPAGE:
		for (i=0; i<((LONG_ARRAY*)r->pvalue)->count; i++) {
			status = ext_buffer_push_uint32(pext,
				((LONG_ARRAY*)r->pvalue)->pl[i]);
			if (EXT_ERR_SUCCESS != status) {
				return status;
			}
		}
		break;
	default:
		return EXT_ERR_BAD_SWITCH;
	}
	offset1 = pext->offset;
	tmp_len1 = offset1 - (offset + sizeof(uint32_t));
	pext->offset = offset;
	status = ext_buffer_push_uint32(pext, tmp_len1);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	pext->offset = offset1;
	offset += sizeof(uint32_t);
	checksum = tnef_generate_checksum(pext->data + offset, tmp_len1);
	return ext_buffer_push_uint16(pext, checksum);
}

static const char* tnef_from_msgclass(const char *str_class)
{
	if (0 == strcasecmp("IPM.Note", str_class)) {
		return "IPM.Microsoft Mail.Note";
	} else if (0 == strcasecmp("Report.IPM.Note.IPNRN", str_class)) {
		return "IPM.Microsoft Mail.Read Receipt";
	} else if (0 == strcasecmp("Report.IPM.Note.NDR", str_class)) {
		return "IPM.Microsoft Mail.Non-Delivery";
	} else if (0 == strcasecmp("IPM.Schedule.Meeting.Resp.Pos",
		str_class)) {
		return "IPM.Microsoft Schedule.MtgRespP";
	} else if (0 == strcasecmp("IPM.Schedule.Meeting.Resp.Neg",
		str_class)) {
		return "IPM.Microsoft Schedule.MtgRespN";
	} else if (0 == strcasecmp("IPM.Schedule.Meeting.Resp.Tent",
		str_class)) {
		return "IPM.Microsoft Schedule.MtgRespA";
	} else if (0 == strcasecmp("IPM.Schedule.Meeting.Request",
		str_class)) {
		return "IPM.Microsoft Schedule.MtgReq";
	} else if (0 == strcasecmp("IPM.Schedule.Meeting.Canceled",
		str_class)) {
		return "IPM.Microsoft Schedule.MtgCncl";
	}
	return str_class;
}

static TNEF_PROPLIST* tnef_convert_recipient(TPROPVAL_ARRAY *pproplist,
	EXT_BUFFER_ALLOC alloc, GET_PROPNAME get_propname)
{
	int i;
	char *psmtp;
	BINARY *pbin;
	BINARY tmp_bin;
	char *pdisplay_name;
	char tmp_buff[1280];
	TNEF_PROPLIST *ptnef_proplist;
	
	ptnef_proplist = alloc(sizeof(TNEF_PROPLIST));
	if (NULL == ptnef_proplist) {
		return NULL;
	}
	ptnef_proplist->count = 0;
	if (0 == pproplist->count) {
		ptnef_proplist->ppropval = NULL;
	} else {
		ptnef_proplist->ppropval =
			alloc(sizeof(TNEF_PROPVAL)*
			(pproplist->count + 1));
		if (NULL == ptnef_proplist->ppropval) {
			return NULL;
		}
	}
	psmtp = tpropval_array_get_propval(pproplist, PROP_TAG_SMTPADDRESS);
	pdisplay_name = tpropval_array_get_propval(
				pproplist, PROP_TAG_DISPLAYNAME);
	for (i=0; i<pproplist->count; i++) {
		ptnef_proplist->ppropval[ptnef_proplist->count].propid =
							pproplist->ppropval[i].proptag >> 16;
		ptnef_proplist->ppropval[ptnef_proplist->count].proptype =
							pproplist->ppropval[i].proptag & 0xFFFF;
		if (NULL != psmtp && PROP_TAG_ENTRYID ==
			pproplist->ppropval[i].proptag) {
			continue;
		}
		if (ptnef_proplist->ppropval[ptnef_proplist->count].propid & 0x8000) {
			if (FALSE == get_propname(
				ptnef_proplist->ppropval[ptnef_proplist->count].propid,
				&ptnef_proplist->ppropval[ptnef_proplist->count].ppropname)) {
				return NULL;
			}
		} else {
			ptnef_proplist->ppropval[ptnef_proplist->count].ppropname = NULL;
		}
		ptnef_proplist->ppropval[ptnef_proplist->count].pvalue =
									pproplist->ppropval[i].pvalue;
		ptnef_proplist->count ++;
	}
	if (NULL != psmtp) {
		pbin = alloc(sizeof(BINARY));
		if (NULL == pbin) {
			return NULL;
		}
		tmp_bin.cb = 0;
		tmp_bin.pb = tmp_buff;
		if (FALSE == tnef_username_to_oneoff(psmtp, pdisplay_name, &tmp_bin)) {
			return NULL;
		}
		pbin->cb = tmp_bin.cb;
		pbin->pb = alloc(tmp_bin.cb);
		if (NULL == pbin->pb) {
			return NULL;
		}
		memcpy(pbin->pb, tmp_bin.pb, tmp_bin.cb);
		ptnef_proplist->ppropval[ptnef_proplist->count].propid =
											PROP_TAG_ENTRYID >> 16;
		ptnef_proplist->ppropval[ptnef_proplist->count].proptype =
										PROP_TAG_ENTRYID & 0xFFFF;
		ptnef_proplist->ppropval[ptnef_proplist->count].ppropname = NULL;
		ptnef_proplist->ppropval[ptnef_proplist->count].pvalue = pbin;
		ptnef_proplist->count ++;
	}
	return ptnef_proplist;
}

static BOOL tnef_serialize_internal(EXT_PUSH *pext, BOOL b_embedded,
	const MESSAGE_CONTENT *pmsg, EXT_BUFFER_ALLOC alloc,
	GET_PROPNAME get_propname)
{
	int i, j;
	BOOL b_key;
	void *pvalue;
	void *pvalue1;
	void *pvalue2;
	BINARY tmp_bin;
	BINARY key_bin;
	uint8_t tmp_byte;
	uint32_t *pmethod;
	REND_DATA tmp_rend;
	ATTR_ADDR tmp_addr;
	uint16_t tmp_int16;
	uint32_t tmp_int32;
	char tmp_buff[4096];
	uint32_t tmp_cpids[2];
	LONG_ARRAY tmp_larray;
	TNEF_ATTRIBUTE attribute;
	const char *message_class;
	TNEF_PROPSET tnef_propset;
	uint32_t proptag_buff[32];
	PROPTAG_ARRAY tmp_proptags;
	TNEF_PROPLIST tnef_proplist;
	ATTACHMENT_CONTENT *pattachment;
	
	
	tmp_proptags.count = 0;
	tmp_proptags.pproptag = proptag_buff;
	
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint32(pext, 0x223e9f78)) {
		return FALSE;
	}
	if (EXT_ERR_SUCCESS != ext_buffer_push_uint16(pext, TNEF_LEGACY)) {
		return FALSE;
	}
	/* ATTRIBUTE_ID_TNEFVERSION */
	attribute.attr_id = ATTRIBUTE_ID_TNEFVERSION;
	attribute.lvl = LVL_MESSAGE;
	attribute.pvalue = &tmp_larray;
	tmp_larray.count = 1;
	tmp_larray.pl = &tmp_int32;
	tmp_int32 = TNEF_VERSION;
	if (EXT_ERR_SUCCESS != tnef_push_attribute(
		pext, &attribute, alloc, get_propname)) {
		return FALSE;
	}
	/* ATTRIBUTE_ID_OEMCODEPAGE */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_INTERNETCODEPAGE);
	if (NULL == pvalue) {
		debug_info("[tnef]: cannot find PROP_TAG_INTERNETCODEPAGE");
		return FALSE;
	}
	attribute.attr_id = ATTRIBUTE_ID_OEMCODEPAGE;
	attribute.lvl = LVL_MESSAGE;
	attribute.pvalue = &tmp_larray;
	tmp_larray.count = 2;
	tmp_larray.pl = tmp_cpids;
	tmp_cpids[0] = *(uint32_t*)pvalue;
	tmp_cpids[1] = 0;
	if (EXT_ERR_SUCCESS != tnef_push_attribute(
		pext, &attribute, alloc, get_propname)) {
		return FALSE;
	}
	/* ATTRIBUTE_ID_MESSAGESTATUS */
	if (TRUE == b_embedded) {
		tmp_byte = 0;
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_MESSAGEFLAGS);
		if (NULL != pvalue) {
			if ((*(uint32_t*)pvalue) & MESSAGE_FLAG_UNSENT) {
				tmp_byte |= FMS_LOCAL;
			}
			if ((*(uint32_t*)pvalue) & MESSAGE_FLAG_SUBMITTED) {
				tmp_byte | FMS_SUBMITTED;
			}
		}
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
							&pmsg->proplist, PROP_TAG_READ);
		if (NULL != pvalue && 0 != *(uint8_t*)pvalue) {
			tmp_byte |= FMS_READ;
		}
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_CREATIONTIME);
		pvalue1 = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_LASTMODIFICATIONTIME);
		if (NULL != pvalue && NULL != pvalue1 &&
			*(uint64_t*)pvalue1 > *(uint64_t*)pvalue) {
			tmp_byte |= FMS_MODIFIED;
		}
		if (NULL != pmsg->children.pattachments) {
			tmp_byte |= FMS_HASATTACH;
		}
		attribute.attr_id = ATTRIBUTE_ID_MESSAGESTATUS;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = &tmp_bin;
		tmp_bin.cb = 1;
		tmp_bin.pb = &tmp_byte;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
	}
	tmp_proptags.pproptag[tmp_proptags.count] =
							PROP_TAG_MESSAGEFLAGS;
	tmp_proptags.count ++;
	/* ATTRIBUTE_ID_FROM */
	if (TRUE == b_embedded) {
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_SENDERNAME_STRING8);
		pvalue1 = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_SENDERADDRESSTYPE_STRING8);
		pvalue2 = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_SENDEREMAILADDRESS_STRING8);
		if (NULL != pvalue && NULL != pvalue1 && NULL != pvalue2) {
			attribute.attr_id = ATTRIBUTE_ID_FROM;
			attribute.lvl = LVL_MESSAGE;
			snprintf(tmp_buff, sizeof(tmp_buff), "%s:%s", pvalue1, pvalue2);
			tmp_addr.displayname = pvalue;
			tmp_addr.address = tmp_buff;
			attribute.pvalue = &tmp_addr;
			if (EXT_ERR_SUCCESS != tnef_push_attribute(
				pext, &attribute, alloc, get_propname)) {
				return FALSE;
			}
			/* keep these properties for attMsgProps */
		}
	}
	/* ATTRIBUTE_ID_MESSAGECLASS */
	message_class = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pmsg->proplist, PROP_TAG_MESSAGECLASS_STRING8);
	if (NULL == message_class) {
		message_class = tpropval_array_get_propval((TPROPVAL_ARRAY*)
							&pmsg->proplist, PROP_TAG_MESSAGECLASS);
		if (NULL == message_class) {
			debug_info("[tnef]: cannot find PROP_TAG_MESSAGECLASS");
			return FALSE;
		}
	}
	attribute.attr_id = ATTRIBUTE_ID_MESSAGECLASS;
	attribute.lvl = LVL_MESSAGE;
	attribute.pvalue = (void*)tnef_from_msgclass(message_class);
	if (EXT_ERR_SUCCESS != tnef_push_attribute(
		pext, &attribute, alloc, get_propname)) {
		return FALSE;
	}
	tmp_proptags.pproptag[tmp_proptags.count] =
				PROP_TAG_MESSAGECLASS_STRING8;
	tmp_proptags.count ++;
	tmp_proptags.pproptag[tmp_proptags.count] =
						PROP_TAG_MESSAGECLASS;
	tmp_proptags.count ++;
	/* ATTRIBUTE_ID_ORIGNINALMESSAGECLASS */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
		&pmsg->proplist, PROP_TAG_ORIGINALMESSAGECLASS_STRING8);
	if (NULL != pvalue) {
		attribute.attr_id = ATTRIBUTE_ID_ORIGNINALMESSAGECLASS;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = (void*)tnef_from_msgclass(pvalue);
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		tmp_proptags.pproptag[tmp_proptags.count] =
			PROP_TAG_ORIGINALMESSAGECLASS_STRING8;
		tmp_proptags.count ++;
		tmp_proptags.pproptag[tmp_proptags.count] =
					PROP_TAG_ORIGINALMESSAGECLASS;
		tmp_proptags.count ++;
	}
	/* ATTRIBUTE_ID_SUBJECT */
	if (FALSE == b_embedded) {
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_SUBJECT_STRING8);
		if (NULL != pvalue) {
			attribute.attr_id = ATTRIBUTE_ID_SUBJECT;
			attribute.lvl = LVL_MESSAGE;
			attribute.pvalue = pvalue;
			if (EXT_ERR_SUCCESS != tnef_push_attribute(
				pext, &attribute, alloc, get_propname)) {
				return FALSE;
			}
			/* keep this property for attMsgProps */
		}
	}
	/* ATTRIBUTE_ID_BODY */
	if (TRUE == b_embedded) {
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_BODY_STRING8);
		if (NULL != pvalue) {
			attribute.attr_id = ATTRIBUTE_ID_BODY;
			attribute.lvl = LVL_MESSAGE;
			attribute.pvalue = pvalue;
			if (EXT_ERR_SUCCESS != tnef_push_attribute(
				pext, &attribute, alloc, get_propname)) {
				return FALSE;
			}
			tmp_proptags.pproptag[tmp_proptags.count] =
								PROP_TAG_BODY_STRING8;
			tmp_proptags.count ++;
		}
	}
	/* ATTRIBUTE_ID_MESSAGEID */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_SEARCHKEY);
	if (NULL != pvalue) {
		if (FALSE == encode_hex_binary(((BINARY*)pvalue)->pb,
			((BINARY*)pvalue)->cb, tmp_buff, sizeof(tmp_buff))) {
			return FALSE;
		}
		attribute.attr_id = ATTRIBUTE_ID_MESSAGEID;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = tmp_buff;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		tmp_proptags.pproptag[tmp_proptags.count] =
								PROP_TAG_SEARCHKEY;
		tmp_proptags.count ++;
	}
	/* ATTRIBUTE_ID_OWNER */
	if (0 == strcasecmp(message_class,
		"IPM.Schedule.Meeting.Request") ||
		0 == strcasecmp(message_class,
		"IPM.Schedule.Meeting.Canceled")) {
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_SENTREPRESENTINGNAME_STRING8);
		pvalue1 = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_SENTREPRESENTINGADDRESSTYPE_STRING8);
		pvalue2 = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_SENTREPRESENTINGEMAILADDRESS_STRING8);
		if (NULL != pvalue && NULL != pvalue1 && NULL != pvalue2) {
			attribute.attr_id = ATTRIBUTE_ID_OWNER;
			attribute.lvl = LVL_MESSAGE;
			snprintf(tmp_buff, sizeof(tmp_buff), "%s:%s", pvalue1, pvalue2);
			tmp_addr.displayname = pvalue;
			tmp_addr.address = tmp_buff;
			attribute.pvalue = &tmp_addr;
			if (EXT_ERR_SUCCESS != tnef_push_attribute(
				pext, &attribute, alloc, get_propname)) {
				return FALSE;
			}
			/* keep these properties for attMsgProps */
		}
	} else if (0 == strcasecmp(message_class,
		"IPM.Schedule.Meeting.Resp.Pos") ||
		0 == strcasecmp(message_class,
		"IPM.Schedule.Meeting.Resp.Neg") ||
		0 == strcasecmp(message_class,
		"IPM.Schedule.Meeting.Resp.Tent")) {
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_RECEIVEDREPRESENTINGNAME_STRING8);
		pvalue1 = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_RECEIVEDREPRESENTINGADDRESSTYPE_STRING8);
		pvalue2 = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_RECEIVEDREPRESENTINGEMAILADDRESS_STRING8);
		if (NULL != pvalue && NULL != pvalue1 && NULL != pvalue2) {
			attribute.attr_id = ATTRIBUTE_ID_OWNER;
			attribute.lvl = LVL_MESSAGE;
			snprintf(tmp_buff, sizeof(tmp_buff), "%s:%s", pvalue1, pvalue2);
			tmp_addr.displayname = pvalue;
			tmp_addr.address = tmp_buff;
			attribute.pvalue = &tmp_addr;
			if (EXT_ERR_SUCCESS != tnef_push_attribute(
				pext, &attribute, alloc, get_propname)) {
				return FALSE;
			}
			/* keep these properties for attMsgProps */
		}
	}
	/* ATTRIBUTE_ID_SENTFOR */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
		&pmsg->proplist, PROP_TAG_SENTREPRESENTINGNAME_STRING8);
	pvalue1 = tpropval_array_get_propval((TPROPVAL_ARRAY*)
		&pmsg->proplist, PROP_TAG_SENTREPRESENTINGADDRESSTYPE_STRING8);
	pvalue2 = tpropval_array_get_propval((TPROPVAL_ARRAY*)
		&pmsg->proplist, PROP_TAG_SENTREPRESENTINGEMAILADDRESS_STRING8);
	if (NULL != pvalue && NULL != pvalue1 && NULL != pvalue2) {
		attribute.attr_id = ATTRIBUTE_ID_SENTFOR;
		attribute.lvl = LVL_MESSAGE;
		snprintf(tmp_buff, sizeof(tmp_buff), "%s:%s", pvalue1, pvalue2);
		tmp_addr.displayname = pvalue;
		tmp_addr.address = tmp_buff;
		attribute.pvalue = &tmp_addr;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		/* keep these properties for attMsgProps */
	}
	/* ATTRIBUTE_ID_DELEGATE */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
		&pmsg->proplist, PROP_TAG_RECEIVEDREPRESENTINGENTRYID);
	if (NULL != pvalue) {
		attribute.attr_id = ATTRIBUTE_ID_DELEGATE;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = pvalue;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		tmp_proptags.pproptag[tmp_proptags.count] =
			PROP_TAG_RECEIVEDREPRESENTINGENTRYID;
		tmp_proptags.count ++;
	}
	/* ATTRIBUTE_ID_DATESTART */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_STARTDATE);
	if (NULL != pvalue) {
		attribute.attr_id = ATTRIBUTE_ID_DATESTART;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = pvalue;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		tmp_proptags.pproptag[tmp_proptags.count] =
								PROP_TAG_STARTDATE;
		tmp_proptags.count ++;
	}
	/* ATTRIBUTE_ID_DATEEND */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
						&pmsg->proplist, PROP_TAG_ENDDATE);
	if (NULL != pvalue) {
		attribute.attr_id = ATTRIBUTE_ID_DATEEND;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = pvalue;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		tmp_proptags.pproptag[tmp_proptags.count] =
								PROP_TAG_ENDDATE;
		tmp_proptags.count ++;
	}
	/* ATTRIBUTE_ID_AIDOWNER */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_OWNERAPPOINTMENTID);
	if (NULL != pvalue) {
		attribute.attr_id = ATTRIBUTE_ID_AIDOWNER;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = pvalue;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		tmp_proptags.pproptag[tmp_proptags.count] =
						PROP_TAG_OWNERAPPOINTMENTID;
		tmp_proptags.count ++;
	}
	/* ATTRIBUTE_ID_REQUESTRES */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_RESPONSEREQUESTED);
	if (NULL != pvalue && 0 != *(uint8_t*)pvalue) {
		attribute.attr_id = ATTRIBUTE_ID_REQUESTRES;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = &tmp_int16;
		tmp_int16 = 1;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		tmp_proptags.pproptag[tmp_proptags.count] =
						PROP_TAG_RESPONSEREQUESTED;
		tmp_proptags.count ++;
	}
	/* ATTRIBUTE_ID_DATESENT */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_CLIENTSUBMITTIME);
	if (NULL != pvalue) {
		attribute.attr_id = ATTRIBUTE_ID_DATESENT;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = pvalue;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		/* keep this property for attMsgProps */
	}
	/* ATTRIBUTE_ID_DATERECD */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_MESSAGEDELIVERYTIME);
	if (NULL != pvalue) {
		attribute.attr_id = ATTRIBUTE_ID_DATERECD;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = pvalue;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		tmp_proptags.pproptag[tmp_proptags.count] =
						PROP_TAG_MESSAGEDELIVERYTIME;
		tmp_proptags.count ++;
	}
	/* ATTRIBUTE_ID_PRIORITY */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_IMPORTANCE);
	if (NULL != pvalue) {
		attribute.attr_id = ATTRIBUTE_ID_PRIORITY;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = &tmp_int16;
		switch (*(uint32_t*)pvalue) {
		case 0:
			tmp_int16 = 3;
			break;
		case 1:
			tmp_int16 = 2;
			break;
		case 2:
			tmp_int16 = 1;
			break;
		default:
			debug_info("[tnef]: PROP_TAG_IMPORTANCE error");
			return FALSE;
		}
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		tmp_proptags.pproptag[tmp_proptags.count] =
						PROP_TAG_MESSAGEDELIVERYTIME;
		tmp_proptags.count ++;
	}
	/* ATTRIBUTE_ID_DATEMODIFY */
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_LASTMODIFICATIONTIME);
	if (NULL != pvalue) {
		attribute.attr_id = ATTRIBUTE_ID_DATEMODIFY;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = pvalue;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		tmp_proptags.pproptag[tmp_proptags.count] =
						PROP_TAG_LASTMODIFICATIONTIME;
		tmp_proptags.count ++;
	}
	/* ATTRIBUTE_ID_RECIPTABLE */
	/* do not generate this attribute for top-level message */
	if (TRUE == b_embedded && NULL != pmsg->children.prcpts) {
		tnef_propset.count = 0;
		if (0 != pmsg->children.prcpts->count) {
			tnef_propset.pplist = alloc(sizeof(TNEF_PROPLIST*)*
								pmsg->children.prcpts->count);
			if (NULL == tnef_propset.pplist) {
				return FALSE;
			}
		}
		for (i=0; i<pmsg->children.prcpts->count; i++) {
			pvalue = tpropval_array_get_propval(
				pmsg->children.prcpts->pparray[i],
				PROP_TAG_RECIPIENTTYPE);
			/* BCC recipients must be excluded */
			if (NULL != pvalue && RECIPIENT_TYPE_BCC == *(uint32_t*)pvalue) {
				continue;
			}
			tnef_propset.pplist[tnef_propset.count] =
				tnef_convert_recipient(pmsg->children.prcpts->pparray[i],
					alloc, get_propname);
			if (NULL == tnef_propset.pplist[tnef_propset.count]) {
				return FALSE;
			}
			tnef_propset.count ++;
		}
		attribute.attr_id = ATTRIBUTE_ID_RECIPTABLE;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = &tnef_propset;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
	}
	/* ATTRIBUTE_ID_MSGPROPS */
	b_key = FALSE;
	tnef_proplist.count = 0;
	tnef_proplist.ppropval = alloc(sizeof(TNEF_PROPVAL)*
							(pmsg->proplist.count + 1));
	if (NULL == tnef_proplist.ppropval) {
		return FALSE;
	}
	for (i=0; i<pmsg->proplist.count; i++) {
		if (TRUE == proptag_array_check(&tmp_proptags,
			pmsg->proplist.ppropval[i].proptag)) {
			continue;
		}
		tnef_proplist.ppropval[tnef_proplist.count].propid =
					pmsg->proplist.ppropval[i].proptag >> 16;
		if (PROP_TAG_MESSAGECLASS == pmsg->proplist.ppropval[i].proptag) {
			tnef_proplist.ppropval[tnef_proplist.count].proptype =
												PROPVAL_TYPE_STRING;
		} else {
			if (PROP_TAG_TNEFCORRELATIONKEY ==
				pmsg->proplist.ppropval[i].proptag) {
				b_key = TRUE;
			}
			tnef_proplist.ppropval[tnef_proplist.count].proptype =
					pmsg->proplist.ppropval[i].proptag & 0xFFFF;
		}
		if (tnef_proplist.ppropval[tnef_proplist.count].propid & 0x8000) {
			if (FALSE == get_propname(
				tnef_proplist.ppropval[tnef_proplist.count].propid,
				&tnef_proplist.ppropval[tnef_proplist.count].ppropname)) {
				return FALSE;
			}
		} else {
			tnef_proplist.ppropval[tnef_proplist.count].ppropname = NULL;
		}
		tnef_proplist.ppropval[tnef_proplist.count].pvalue =
							pmsg->proplist.ppropval[i].pvalue;
		
		tnef_proplist.count ++;
	}
	if (FALSE == b_key) {
		pvalue = tpropval_array_get_propval(
			(TPROPVAL_ARRAY*)&pmsg->proplist,
			PROP_TAG_INTERNETMESSAGEID);
		if (NULL != pvalue) {
			tnef_proplist.ppropval[tnef_proplist.count].propid =
								PROP_TAG_TNEFCORRELATIONKEY >> 16;
			tnef_proplist.ppropval[tnef_proplist.count].proptype =
											PROPVAL_TYPE_BINARY;
			tnef_proplist.ppropval[tnef_proplist.count].ppropname = NULL;
			tnef_proplist.ppropval[tnef_proplist.count].pvalue = &key_bin;
			key_bin.cb = strlen(pvalue) + 1;
			key_bin.pb = pvalue;
			tnef_proplist.count ++;
		}
	}
	if (tnef_proplist.count > 0) {
		attribute.attr_id = ATTRIBUTE_ID_MSGPROPS;
		attribute.lvl = LVL_MESSAGE;
		attribute.pvalue = &tnef_proplist;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
	}
	
	if (NULL == pmsg->children.pattachments) {
		return TRUE;
	}
	
	for (i=0; i<pmsg->children.pattachments->count; i++) {
		pattachment = pmsg->children.pattachments->pplist[i];
		tmp_proptags.count = 0;
		/* ATTRIBUTE_ID_ATTACHRENDDATA */
		pmethod = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pattachment->proplist, PROP_TAG_ATTACHMETHOD);
		if (NULL == pmethod) {
			tmp_rend.attach_type = ATTACH_TYPE_FILE;
			break;
		} else {
			switch (*pmethod) {
			case ATTACH_METHOD_NONE:
			case ATTACH_METHOD_BY_VALUE:
			case ATTACH_METHOD_EMBEDDED:
				tmp_rend.attach_type = ATTACH_TYPE_FILE;
				break;
			case ATTACH_METHOD_STORAGE:
				tmp_rend.attach_type = ATTACH_TYPE_OLE;
				break;
			default:
				debug_info("[tnef]: unsupported type in "
					"PROP_TAG_ATTACHMETHOD by attachment");
				return FALSE;
			}
		}
		pvalue = tpropval_array_get_propval(
			&pattachment->proplist, PROP_TAG_RENDERINGPOSITION);
		if (NULL == pvalue) {
			tmp_rend.attach_position = 0xFFFFFFFF;
		} else {
			tmp_rend.attach_position = *(uint32_t*)pvalue;
		}
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pattachment->proplist, PROP_TAG_ATTACHENCODING);
		if (NULL != pvalue && 9 == ((BINARY*)pvalue)->cb &&
			0 == memcmp(((BINARY*)pvalue)->pb, MACBINARY_ENCODING, 9)) {
			tmp_rend.data_flags = FILE_DATA_MACBINARY;
		} else {
			tmp_rend.data_flags = FILE_DATA_DEFAULT;
		}
		tmp_rend.render_width = 32;
		tmp_rend.render_height = 32;
		attribute.attr_id = ATTRIBUTE_ID_ATTACHRENDDATA;
		attribute.lvl = LVL_ATTACHMENT;
		attribute.pvalue = &tmp_rend;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
		/* ATTRIBUTE_ID_ATTACHDATA */
		if (NULL != pmethod && ATTACH_METHOD_BY_VALUE == *pmethod) {
			pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pattachment->proplist, PROP_TAG_ATTACHDATABINARY);
			if (NULL != pvalue) {
				attribute.attr_id = ATTRIBUTE_ID_ATTACHDATA;
				attribute.lvl = LVL_ATTACHMENT;
				attribute.pvalue = pvalue;
				if (EXT_ERR_SUCCESS != tnef_push_attribute(
					pext, &attribute, alloc, get_propname)) {
					return FALSE;
				}
				tmp_proptags.pproptag[tmp_proptags.count] =
								PROP_TAG_ATTACHDATABINARY;
				tmp_proptags.count ++;
			}
		}
		/* ATTRIBUTE_ID_ATTACHTITLE */
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pattachment->proplist, PROP_TAG_ATTACHLONGFILENAME_STRING8);
		if (NULL != pvalue) {
			attribute.attr_id = ATTRIBUTE_ID_ATTACHTITLE;
			attribute.lvl = LVL_ATTACHMENT;
			attribute.pvalue = pvalue;
			if (EXT_ERR_SUCCESS != tnef_push_attribute(
				pext, &attribute, alloc, get_propname)) {
				return FALSE;
			}
			tmp_proptags.pproptag[tmp_proptags.count] =
					PROP_TAG_ATTACHLONGFILENAME_STRING8;
			tmp_proptags.count ++;
		} else {
			pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pattachment->proplist, PROP_TAG_ATTACHFILENAME_STRING8);
			if (NULL != pvalue) {
				attribute.attr_id = ATTRIBUTE_ID_ATTACHTITLE;
				attribute.lvl = LVL_ATTACHMENT;
				attribute.pvalue = pvalue;
				if (EXT_ERR_SUCCESS != tnef_push_attribute(
					pext, &attribute, alloc, get_propname)) {
					return FALSE;
				}
				tmp_proptags.pproptag[tmp_proptags.count] =
							PROP_TAG_ATTACHFILENAME_STRING8;
				tmp_proptags.count ++;
			}
		}
		/* ATTRIBUTE_ID_ATTACHMETAFILE */
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pattachment->proplist, PROP_TAG_ATTACHRENDERING);
		if (NULL != pvalue) {
			attribute.attr_id = ATTRIBUTE_ID_ATTACHMETAFILE;
			attribute.lvl = LVL_ATTACHMENT;
			attribute.pvalue = pvalue;
			if (EXT_ERR_SUCCESS != tnef_push_attribute(
				pext, &attribute, alloc, get_propname)) {
				return FALSE;
			}
			tmp_proptags.pproptag[tmp_proptags.count] =
								PROP_TAG_ATTACHRENDERING;
			tmp_proptags.count ++;
			
		}
		/* ATTRIBUTE_ID_ATTACHCREATEDATE */
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pattachment->proplist, PROP_TAG_CREATIONTIME);
		if (NULL != pvalue) {
			attribute.attr_id = ATTRIBUTE_ID_ATTACHCREATEDATE;
			attribute.lvl = LVL_ATTACHMENT;
			attribute.pvalue = pvalue;
			if (EXT_ERR_SUCCESS != tnef_push_attribute(
				pext, &attribute, alloc, get_propname)) {
				return FALSE;
			}
			tmp_proptags.pproptag[tmp_proptags.count] =
									PROP_TAG_CREATIONTIME;
			tmp_proptags.count ++;
		}
		/* ATTRIBUTE_ID_ATTACHMODIFYDATE */
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pattachment->proplist, PROP_TAG_LASTMODIFICATIONTIME);
		if (NULL != pvalue) {
			attribute.attr_id = ATTRIBUTE_ID_ATTACHMODIFYDATE;
			attribute.lvl = LVL_ATTACHMENT;
			attribute.pvalue = pvalue;
			if (EXT_ERR_SUCCESS != tnef_push_attribute(
				pext, &attribute, alloc, get_propname)) {
				return FALSE;
			}
			tmp_proptags.pproptag[tmp_proptags.count] =
						PROP_TAG_LASTMODIFICATIONTIME;
			tmp_proptags.count ++;
		}
		/* ATTRIBUTE_ID_ATTACHTRANSPORTFILENAME */
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pattachment->proplist, PROP_TAG_ATTACHTRANSPORTNAME_STRING8);
		if (NULL != pvalue) {
			attribute.attr_id = ATTRIBUTE_ID_ATTACHTRANSPORTFILENAME;
			attribute.lvl = LVL_ATTACHMENT;
			attribute.pvalue = pvalue;
			if (EXT_ERR_SUCCESS != tnef_push_attribute(
				pext, &attribute, alloc, get_propname)) {
				return FALSE;
			}
			tmp_proptags.pproptag[tmp_proptags.count] =
					PROP_TAG_ATTACHTRANSPORTNAME_STRING8;
			tmp_proptags.count ++;
		}
		/* ATTRIBUTE_ID_ATTACHMENT */
		if (0 == pattachment->proplist.count) {
			continue;
		}
		tnef_proplist.count = 0;
		tnef_proplist.ppropval = alloc(sizeof(TNEF_PROPVAL)*
							pattachment->proplist.count + 1);
		if (NULL == tnef_proplist.ppropval) {
			return FALSE;
		}
		for (j=0; j<pattachment->proplist.count; j++) {
			if (TRUE == proptag_array_check(&tmp_proptags,
				pattachment->proplist.ppropval[j].proptag)) {
				continue;
			}
			tnef_proplist.ppropval[tnef_proplist.count].propid =
				pattachment->proplist.ppropval[j].proptag >> 16;
			tnef_proplist.ppropval[tnef_proplist.count].proptype =
				pattachment->proplist.ppropval[j].proptag & 0xFFFF;
			if (tnef_proplist.ppropval[tnef_proplist.count].propid & 0x8000) {
				if (FALSE == get_propname(
					tnef_proplist.ppropval[tnef_proplist.count].propid,
					&tnef_proplist.ppropval[tnef_proplist.count].ppropname)) {
					return FALSE;
				}
			} else {
				tnef_proplist.ppropval[tnef_proplist.count].ppropname = NULL;
			}
			tnef_proplist.ppropval[tnef_proplist.count].pvalue =
						pattachment->proplist.ppropval[j].pvalue;
			
			tnef_proplist.count ++;
		}
		if (NULL != pattachment->pembedded) {
			tnef_proplist.ppropval[tnef_proplist.count].propid =
								PROP_TAG_ATTACHDATAOBJECT >> 16;
			tnef_proplist.ppropval[tnef_proplist.count].proptype =
												PROPVAL_TYPE_OBJECT;
			tnef_proplist.ppropval[tnef_proplist.count].ppropname = NULL;
			tnef_proplist.ppropval[tnef_proplist.count].pvalue = &tmp_bin;
			tmp_bin.cb = 0xFFFFFFFF;
			tmp_bin.pb = (void*)pattachment->pembedded;
			tnef_proplist.count ++;
		}
		attribute.attr_id = ATTRIBUTE_ID_ATTACHMENT;
		attribute.lvl = LVL_ATTACHMENT;
		attribute.pvalue = &tnef_proplist;
		if (EXT_ERR_SUCCESS != tnef_push_attribute(
			pext, &attribute, alloc, get_propname)) {
			return FALSE;
		}
	}
	return TRUE;
}

/* must convert some properties into ansi code before call this function */
BINARY* tnef_serialize(const MESSAGE_CONTENT *pmsg,
	EXT_BUFFER_ALLOC alloc, GET_PROPNAME get_propname)
{
	BINARY *pbin;
	EXT_PUSH ext_push;
	
	if (FALSE == ext_buffer_push_init(&ext_push,
		NULL, 0, EXT_FLAG_UTF16)) {
		return NULL;
	}
	if (FALSE == tnef_serialize_internal(&ext_push, FALSE,
		pmsg, alloc, get_propname)) {
		ext_buffer_push_free(&ext_push);
		return NULL;
	}
	pbin = malloc(sizeof(BINARY));
	if (NULL == pbin) {
		ext_buffer_push_free(&ext_push);
		return NULL;
	}
	pbin->pb = ext_push.data;
	pbin->cb = ext_push.offset;
	return pbin;
}

