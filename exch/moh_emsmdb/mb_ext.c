#include "mb_ext.h"

int mb_ext_pull_connect_request(EXT_PULL *pext, CONNECT_REQUEST *prequest)
{
	int status;
	
	status = ext_buffer_pull_string(pext, &prequest->puserdn);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_pull_uint32(pext, &prequest->flags);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_pull_uint32(pext, &prequest->cpid);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_pull_uint32(pext, &prequest->lcid_string);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_pull_uint32(pext, &prequest->lcid_sort);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_pull_uint32(pext, &prequest->cb_auxin);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = pext->alloc(prequest->cb_auxin);
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int mb_ext_pull_execute_request(EXT_PULL *pext, EXECUTE_REQUEST *prequest)
{
	int status;
	
	status = ext_buffer_pull_uint32(pext, &prequest->flags);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_pull_uint32(pext, &prequest->cb_in);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (0 == prequest->cb_in) {
		prequest->pin = NULL;
	} else {
		prequest->pin = pext->alloc(prequest->cb_in);
		if (NULL == prequest->pin) {
			return EXT_ERR_ALLOC;
		}
		status = ext_buffer_pull_bytes(pext, prequest->pin, prequest->cb_in);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
	}
	status = ext_buffer_pull_uint32(pext, &prequest->cb_out);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_pull_uint32(pext, &prequest->cb_auxin);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = pext->alloc(prequest->cb_auxin);
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int mb_ext_pull_disconnect_request(EXT_PULL *pext,
	DISCONNECT_REQUEST *prequest)
{
	int status;
	
	status = ext_buffer_pull_uint32(pext, &prequest->cb_auxin);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = pext->alloc(prequest->cb_auxin);
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int mb_ext_pull_notificationwait_request(EXT_PULL *pext,
	NOTIFICATIONWAIT_REQUEST *prequest)
{
	int status;
	
	status = ext_buffer_pull_uint32(pext, &prequest->flags);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_pull_uint32(pext, &prequest->cb_auxin);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (0 == prequest->cb_auxin) {
		prequest->pauxin = NULL;
		return EXT_ERR_SUCCESS;
	}
	prequest->pauxin = pext->alloc(prequest->cb_auxin);
	if (NULL == prequest->pauxin) {
		return EXT_ERR_ALLOC;
	}
	return ext_buffer_pull_bytes(pext,
		prequest->pauxin, prequest->cb_auxin);
}

int mb_ext_push_connect_response(EXT_PUSH *pext,
	const CONNECT_RESPONSE *presponse)
{
	int status;
	
	status = ext_buffer_push_uint32(pext, presponse->status);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, presponse->result);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, presponse->max_polls);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, presponse->max_retry);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, presponse->retry_delay);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_string(pext, presponse->pdn_prefix);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_wstring(pext, presponse->pdisplayname);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, presponse->cb_auxout);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (0 == presponse->cb_auxout) {
		return EXT_ERR_SUCCESS;
	}
	return ext_buffer_push_bytes(pext,
		presponse->pauxout, presponse->cb_auxout);
}

int mb_ext_push_execute_response(EXT_PUSH *pext,
	const EXECUTE_RESPONSE *presponse)
{
	int status;
	
	status = ext_buffer_push_uint32(pext, presponse->status);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, presponse->result);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, presponse->flags);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, presponse->cb_out);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (presponse->cb_out > 0) {
		status = ext_buffer_push_bytes(pext,
			presponse->pout, presponse->cb_out);
		if (EXT_ERR_SUCCESS != status) {
			return status;
		}
	}
	status = ext_buffer_push_uint32(pext, presponse->cb_auxout);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	if (0 == presponse->cb_auxout) {
		return EXT_ERR_SUCCESS;
	}
	return ext_buffer_push_bytes(pext,
		presponse->pauxout, presponse->cb_auxout);
}

int mb_ext_push_disconnect_response(EXT_PUSH *pext,
	const DISCONNECT_RESPONSE *presponse)
{
	int status;
	
	status = ext_buffer_push_uint32(pext, presponse->status);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, presponse->result);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	return ext_buffer_push_uint32(pext, 0);
}

int mb_ext_push_notificationwait_response(EXT_PUSH *pext,
	const NOTIFICATIONWAIT_RESPONSE *presponse)
{
	int status;
	
	status = ext_buffer_push_uint32(pext, presponse->status);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, presponse->result);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, presponse->flags_out);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	return ext_buffer_push_uint32(pext, 0);
}
