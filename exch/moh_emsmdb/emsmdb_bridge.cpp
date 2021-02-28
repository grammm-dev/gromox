#include "emsmdb_bridge.h"
#include <gromox/mapi_types.hpp>

decltype(emsmdb_interface_connect_ex) emsmdb_interface_connect_ex;
decltype(emsmdb_interface_rpc_ext2) emsmdb_interface_rpc_ext2;
decltype(emsmdb_interface_disconnect) emsmdb_interface_disconnect;
decltype(emsmdb_interface_touch_handle) emsmdb_interface_touch_handle;

uint32_t emsmdb_bridge_connect(const char *puserdn, uint32_t flags,
	uint32_t cpid, uint32_t lcid_string, uint32_t lcid_sort,
	uint32_t cb_auxin, const uint8_t *pauxin, GUID *psession_guid,
	uint16_t *pcxr, uint32_t *pmax_polls, uint32_t *pmax_retry,
	uint32_t *pretry_delay, char *pdn_prefix, char *pdisplayname,
	uint32_t *pcb_auxout, uint8_t *pauxout)
{
	uint32_t result;
	uint32_t timestamp;
	uint16_t pbest_vers[3];
	uint16_t pclient_vers[3];
	uint16_t pserver_vers[3];
	EMSMDB_HANDLE session_handle;
	
	result = emsmdb_interface_connect_ex(0, &session_handle, puserdn,
		flags, 0, 0, cpid, lcid_string, lcid_sort, 0, 0, pmax_polls,
		pmax_retry, pretry_delay, pcxr, pdn_prefix, pdisplayname,
		pclient_vers, pserver_vers, pbest_vers, &timestamp, pauxin,
		cb_auxin, pauxout, pcb_auxout);
	if (result != ecSuccess)
		return result;
	*psession_guid = session_handle.guid;
	return ecSuccess;
}

uint32_t emsmdb_bridge_execute(GUID session_guid,
	uint32_t cb_in, const uint8_t *pin, uint32_t cb_auxin,
	const uint8_t *pauxin, uint32_t *pflags, uint32_t *pcb_out,
	uint8_t *pout, uint32_t *pcb_auxout, uint8_t *pauxout)
{
	uint32_t trans_time;
	EMSMDB_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_EMSMDB;
	session_handle.guid = session_guid;
	return emsmdb_interface_rpc_ext2(&session_handle, pflags,
		pin, cb_in, pout, pcb_out, pauxin, cb_auxin, pauxout,
		pcb_auxout, &trans_time);
}

uint32_t emsmdb_bridge_disconnect(GUID session_guid,
	uint32_t cb_auxin, const uint8_t *pauxin)
{
	EMSMDB_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_EMSMDB;
	session_handle.guid = session_guid;
	return emsmdb_interface_disconnect(&session_handle);
}

void emsmdb_bridge_touch_handle(GUID session_guid)
{
	EMSMDB_HANDLE session_handle;
	
	session_handle.handle_type = HANDLE_EXCHANGE_EMSMDB;
	session_handle.guid = session_guid;
	emsmdb_interface_touch_handle(&session_handle);
}
