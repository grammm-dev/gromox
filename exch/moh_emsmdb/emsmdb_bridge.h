#ifndef _H_EMSMDB_BRIDGE_
#define _H_EMSMDB_BRIDGE_
#include <gromox/common_types.hpp>

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

extern int (*emsmdb_interface_connect_ex)(uint64_t hrpc, EMSMDB_HANDLE *, const char *user_dn, uint32_t flags, uint32_t con_mode, uint32_t limit, uint32_t cpid, uint32_t lcid_string, uint32_t lcid_sort, uint32_t cxr_link, uint16_t cnvt_cps, uint32_t *max_polls, uint32_t *max_retry, uint32_t *retry_delay, uint16_t *cxr, uint8_t *dn_prefix, uint8_t *displayname, const uint16_t client_vers[3], uint16_t server_vers[3], uint16_t best_vers[3], uint32_t *timestamp, const uint8_t *auxin, uint32_t cb_auxin, uint8_t *auxout, uint32_t *cb_auxout);
extern int (*emsmdb_interface_rpc_ext2)(EMSMDB_HANDLE *, uint32_t *flags, const uint8_t *in, uint32_t cb_in, uint8_t *out, uint32_t *cb_out, const uint8_t *auxin, uint32_t cb_auxin, uint8_t *auxout, uint32_t *cb_auxout, uint32_t *trans_time);
extern int (*emsmdb_interface_disconnect)(EMSMDB_HANDLE *);
extern void (*emsmdb_interface_touch_handle)(EMSMDB_HANDLE *);

#endif /* _H_EMSMDB_BRIDGE_ */
