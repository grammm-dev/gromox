#ifndef _H_FCGI_RPC_
#define _H_FCGI_RPC_

BOOL fcgi_rpc_do(const char *socket_path, int exec_timeout,
	const uint8_t *pbuff_in, uint32_t in_len, uint8_t **ppbuff_out,
	uint32_t *pout_len, const char *script_path);

#endif /* _H_FCGI_RPC_ */
