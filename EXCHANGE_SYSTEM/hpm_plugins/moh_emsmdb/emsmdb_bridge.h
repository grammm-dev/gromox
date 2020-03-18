#ifndef _H_EMSMDB_BRIDGE_
#define _H_EMSMDB_BRIDGE_
#include "common_types.h"

#define HANDLE_EXCHANGE_EMSMDB							2

#define HANDLE_EXCHANGE_ASYNCEMSMDB						3

typedef struct _EMSMDB_HANDLE {
	uint32_t handle_type;
	GUID guid;
} EMSMDB_HANDLE;

uint32_t emsmdb_bridge_connect(const char *puserdn, uint32_t flags,
	uint32_t cpid, uint32_t lcid_string, uint32_t lcid_sort,
	uint32_t cb_auxin, const uint8_t *pauxin, GUID *psession_guid,
	uint16_t *pcxr, uint32_t *pmax_polls, uint32_t *pmax_retry,
	uint32_t *pretry_delay, char *pdn_prefix, char *pdisplayname,
	uint32_t *pcb_auxout, uint8_t *pauxout);

uint32_t emsmdb_bridge_execute(GUID session_guid,
	uint32_t cb_in, const uint8_t *pin, uint32_t cb_auxin,
	const uint8_t *pauxin, uint32_t *pflags, uint32_t *pcb_out,
	uint8_t *pout, uint32_t *pcb_auxout, uint8_t *pauxout);

uint32_t emsmdb_bridge_disconnect(GUID session_guid,
	uint32_t cb_auxin, const uint8_t *pauxin);

void emsmdb_bridge_touch_handle(GUID session_guid);

#endif /* _H_EMSMDB_BRIDGE_ */
