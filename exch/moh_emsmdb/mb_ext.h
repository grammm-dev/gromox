#ifndef _H_MB_EXT_
#define _H_MB_EXT_
#include "mb_types.h"
#include <gromox/ext_buffer.hpp>

int mb_ext_pull_connect_request(EXT_PULL *pext, CONNECT_REQUEST *prequest);

int mb_ext_pull_execute_request(EXT_PULL *pext, EXECUTE_REQUEST *prequest);

int mb_ext_pull_disconnect_request(EXT_PULL *pext,
	DISCONNECT_REQUEST *prequest);

int mb_ext_pull_notificationwait_request(EXT_PULL *pext,
	NOTIFICATIONWAIT_REQUEST *prequest);

int mb_ext_push_connect_response(EXT_PUSH *pext,
	const CONNECT_RESPONSE *presponse);

int mb_ext_push_execute_response(EXT_PUSH *pext,
	const EXECUTE_RESPONSE *presponse);

int mb_ext_push_disconnect_response(EXT_PUSH *pext,
	const DISCONNECT_RESPONSE *presponse);

int mb_ext_push_notificationwait_response(EXT_PUSH *pext,
	const NOTIFICATIONWAIT_RESPONSE *presponse);

#endif /* _H_MB_EXT_ */
