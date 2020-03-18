#include "ndr.h"
#include "util.h"
#include "fcgi_rpc.h"
#include "mail_func.h"
#include "ext_buffer.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>


#define SOCKET_TIMEOUT							180

#define SERVER_SOFTWARE							"grid/2.0"

#define FCGI_VERSION							1

#define FCGI_REQUEST_ID							1


#define RECORD_TYPE_BEGIN_REQUEST				1
#define RECORD_TYPE_ABORT_REQUEST				2
#define RECORD_TYPE_END_REQUEST					3
#define RECORD_TYPE_PARAMS						4
#define RECORD_TYPE_STDIN						5
#define RECORD_TYPE_STDOUT						6
#define RECORD_TYPE_STDERR						7
#define RECORD_TYPE_DATA						8
#define RECORD_TYPE_GET_VALUES					9
#define RECORD_TYPE_GET_VALUES_RESULT			10
#define RECORD_TYPE_UNKNOWN_TYPE				11


#define ROLE_RESPONDER							1
#define ROLE_AUTHORIZER							2
#define ROLE_FILTER								3


#define PROTOCOL_STATUS_REQUEST_COMPLETE		0
#define PROTOCOL_STATUS_CANT_MPX_CONN			1
#define PROTOCOL_STATUS_OVERLOADED				2
#define PROTOCOL_STATUS_UNKNOWN_ROLE			3

typedef struct _FCGI_ENDREQUESTBODY {
	uint32_t app_status;
	uint8_t protocol_status;
	uint8_t reserved[3];
} FCGI_ENDREQUESTBODY;

typedef struct _FCGI_STDSTREAM {
	uint8_t buffer[0xFFFF];
	uint16_t length;
} FCGI_STDSTREAM;

typedef struct _RECORD_HEADER {
	uint8_t version;
	uint8_t type;
	uint16_t request_id;
	uint16_t content_len;
	uint8_t padding_len;
	uint8_t reserved;
} RECORD_HEADER;

static int fcgi_rpc_push_name_value(NDR_PUSH *pndr,
	const uint8_t *pname, const uint8_t *pvalue)
{
	int status;
	uint32_t tmp_len;
	uint32_t val_len;
	uint32_t name_len;
	
	name_len = strlen(pname);
	if (name_len <= 0x7F) {
		status = ndr_push_uint8(pndr, name_len);
	} else {
		tmp_len = name_len | 0x80000000;
		status = ndr_push_uint32(pndr, tmp_len);
	}
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	val_len = strlen(pvalue);
	if (val_len <= 0x7F) {
		status = ndr_push_uint8(pndr, val_len);
	} else {
		tmp_len = val_len | 0x80000000;
		status = ndr_push_uint32(pndr, tmp_len);
	}
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_array_uint8(pndr, pname, name_len);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	return ndr_push_array_uint8(pndr, pvalue, val_len);
}

static int fcgi_rpc_push_begin_request(NDR_PUSH *pndr)
{
	int status;
	
	status = ndr_push_uint8(pndr, FCGI_VERSION);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_uint8(pndr, RECORD_TYPE_BEGIN_REQUEST);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_uint16(pndr, FCGI_REQUEST_ID);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	/* push content length */
	status = ndr_push_uint16(pndr, 8);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	/* push padding length */
	status = ndr_push_uint8(pndr, 0);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	/* reserved */
	status = ndr_push_uint8(pndr, 0);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	/* begin request role */
	status = ndr_push_uint16(pndr, ROLE_RESPONDER);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	/* begin request flags */
	status = ndr_push_uint8(pndr, 0);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	/* begin request reserved bytes */
	return ndr_push_zero(pndr, 5);
}

static int fcgi_rpc_push_params_begin(NDR_PUSH *pndr)
{
	int status;
	
	status = ndr_push_uint8(pndr, FCGI_VERSION);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_uint8(pndr, RECORD_TYPE_PARAMS);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_uint16(pndr, FCGI_REQUEST_ID);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	/* push fake content length */
	status = ndr_push_uint16(pndr, 0);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	/* push fake padding length */
	status = ndr_push_uint8(pndr, 0);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	/* reserved */
	return ndr_push_uint8(pndr, 0);
}

static int fcgi_rpc_push_align_record(NDR_PUSH *pndr)
{
	uint8_t padding_len;
	
	if (0 == (pndr->offset & 7)) {
		return NDR_ERR_SUCCESS;
	}
	padding_len = 8 - (pndr->offset & 7);
	pndr->data[6] = padding_len;
	return ndr_push_zero(pndr, padding_len);
}

static int fcgi_rpc_push_params_end(NDR_PUSH *pndr)
{
	int status;
	uint16_t len;
	uint32_t offset;
	
	offset = pndr->offset;
	len = offset - 8;
	if (len > 0xFFFF) {
		return NDR_ERR_FAILURE;
	}
	pndr->offset = 4;
	status = ndr_push_uint16(pndr, len);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	pndr->offset = offset;
	return fcgi_rpc_push_align_record(pndr);
}

static int fcgi_rpc_push_stdin(NDR_PUSH *pndr,
	const uint8_t *pbuff, uint16_t length)
{
	int status;
	
	status = ndr_push_uint8(pndr, FCGI_VERSION);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_uint8(pndr, RECORD_TYPE_STDIN);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_uint16(pndr, FCGI_REQUEST_ID);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_uint16(pndr, length);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	/* push padding length */
	status = ndr_push_uint8(pndr, 0);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	/* reserved */
	status = ndr_push_uint8(pndr, 0);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_array_uint8(pndr, pbuff, length);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	return fcgi_rpc_push_align_record(pndr);
}

static int fcgi_rpc_pull_end_request(NDR_PULL *pndr,
	uint8_t padding_len, FCGI_ENDREQUESTBODY *pend_request)
{
	int status;
	
	status = ndr_pull_uint32(pndr, &pend_request->app_status);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_uint8(pndr, &pend_request->protocol_status);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_array_uint8(pndr, pend_request->reserved, 3);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	return ndr_pull_advance(pndr, padding_len);
}

static int fcgi_rpc_pull_stdstream(NDR_PULL *pndr,
	uint8_t padding_len, FCGI_STDSTREAM *pstd_stream)
{
	int status;
	
	status = ndr_pull_array_uint8(pndr,
		pstd_stream->buffer, pstd_stream->length);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	return ndr_pull_advance(pndr, padding_len);
}

static int fcgi_rpc_pull_record_header(
	NDR_PULL *pndr, RECORD_HEADER *pheader)
{
	int status;
	
	status = ndr_pull_uint8(pndr, &pheader->version);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_uint8(pndr, &pheader->type);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_uint16(pndr, &pheader->request_id);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_uint16(pndr, &pheader->content_len);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_uint8(pndr, &pheader->padding_len);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	return ndr_pull_uint8(pndr, &pheader->reserved);
}

static int fcgi_rpc_connect_backend(const char *socket_path)
{
	int sockd, len;
	struct sockaddr_un un;

	/* create a UNIX domain socket */
	sockd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockd < 0) {
		return -1;
	}
	/* fill socket address structure with server's address */
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, socket_path);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);
	if (connect(sockd, (struct sockaddr*)&un, len) < 0) {
		close(sockd);
		return -2;
	}
	return sockd;
}

static BOOL fcgi_rpc_build_params(uint8_t *pbuff, int *plength,
	uint32_t content_length, const char *script_path)
{
	int status;
	char *ptoken;
	NDR_PUSH ndr_push;
	char tmp_buff[32];
	char tmp_path[256];
	
	ndr_push_init(&ndr_push, pbuff, *plength,
		NDR_FLAG_NOALIGN|NDR_FLAG_BIGENDIAN);
	status = fcgi_rpc_push_params_begin(&ndr_push);
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(&ndr_push,
				"GATEWAY_INTERFACE", "CGI/1.1");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(&ndr_push,
			"SERVER_SOFTWARE", SERVER_SOFTWARE);
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(
		&ndr_push, "HTTP_HOST", "localhost");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(
		&ndr_push, "SERVER_NAME", "localhost");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(
		&ndr_push, "SERVER_ADDR", "0.0.0.0");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(
			&ndr_push, "SERVER_PORT", "0");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(
		&ndr_push, "REMOTE_ADDR", "0.0.0.0");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(
		&ndr_push, "REMOTE_PORT", "0");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(&ndr_push,
				"SERVER_PROTOCOL", "HTTP/1.1");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	/* leave the request method empty */
	status = fcgi_rpc_push_name_value(
		&ndr_push, "REQUEST_METHOD", "");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(
		&ndr_push, "REQUEST_URI", "/rpc");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(
		&ndr_push, "QUERY_STRING", "");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	strcpy(tmp_path, script_path);
	ptoken = strrchr(tmp_path, '/');
	if (NULL == ptoken) {
		return FALSE;
	}
	*ptoken = '\0';
	ptoken ++;
	status = fcgi_rpc_push_name_value(
		&ndr_push, "DOCUMENT_ROOT", tmp_path);
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	
	status = fcgi_rpc_push_name_value(
		&ndr_push, "SCRIPT_NAME", ptoken);
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(&ndr_push,
				"SCRIPT_FILENAME", script_path);
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(
		&ndr_push, "PATH_INFO", script_path);
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(&ndr_push,
			"HTTP_ACCEPT", "application/rpc");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(&ndr_push,
				"HTTP_USER_AGENT", "GRID-INFO");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(&ndr_push,
			"CONTENT_TYPE", "application/rpc");
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_name_value(&ndr_push,
					"HTTP_CONNECTION", "close");
	
	sprintf(tmp_buff, "%u", content_length);
	status = fcgi_rpc_push_name_value(&ndr_push,
					"CONTENT_LENGTH", tmp_buff);
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	status = fcgi_rpc_push_params_end(&ndr_push);
	if (NDR_ERR_SUCCESS != status) {
		return FALSE;
	}
	*plength = ndr_push.offset;
	return TRUE;
}

static BOOL fcgi_rpc_safe_read(int cli_sockd,
	int exec_timeout, uint8_t *pbuff, int length)
{
	int offset;
	int tv_msec;
	int read_len;
	struct pollfd pfd_read;
	
	offset = 0;
	while (TRUE) {
		tv_msec = exec_timeout * 1000;
		pfd_read.fd = cli_sockd;
		pfd_read.events = POLLIN|POLLPRI;
		if (1 != poll(&pfd_read, 1, tv_msec)) {
			return FALSE;
		}
		read_len = read(cli_sockd, pbuff + offset, length - offset);
		if (read_len <= 0) {
			return FALSE;
		}
		offset += read_len;
		if (length == offset) {
			return TRUE;
		}
	}
}

BOOL fcgi_rpc_do(const char *socket_path, int exec_timeout,
	const uint8_t *pbuff_in, uint32_t in_len, uint8_t **ppbuff_out,
	uint32_t *pout_len, const char *script_path)
{
	int tmp_len;
	void *pbody;
	char *ptoken;
	char *ptoken1;
	int cli_sockd;
	BOOL b_header;
	int ndr_length;
	uint32_t offset;
	EXT_PUSH ext_push;
	NDR_PULL ndr_pull;
	NDR_PUSH ndr_push;
	char tmp_path[256];
	char tmp_buff[80000];
	RECORD_HEADER header;
	uint8_t header_buff[8];
	uint8_t ndr_buff[65800];
	uint8_t status_line[256];
	FCGI_STDSTREAM std_stream;
	FCGI_ENDREQUESTBODY end_request;
	
	ndr_push_init(&ndr_push, tmp_buff, 16,
		NDR_FLAG_NOALIGN|NDR_FLAG_BIGENDIAN);
	if (NDR_ERR_SUCCESS != fcgi_rpc_push_begin_request(
		&ndr_push) || 16 != ndr_push.offset) {
		return FALSE;
	}
	ndr_length = sizeof(ndr_buff);
	if (FALSE == fcgi_rpc_build_params(ndr_buff,
		&ndr_length, in_len, script_path)) {
		return FALSE;	
	}
	cli_sockd = fcgi_rpc_connect_backend(socket_path);
	if (cli_sockd < 0) {
		printf("[fcgi_rpc]: fail to connect to back-end %s\n", socket_path);
		return FALSE;
	}
	if (16 != write(cli_sockd, tmp_buff, 16) ||
		ndr_length != write(cli_sockd, ndr_buff,
		ndr_length)) {
		close(cli_sockd);
		return FALSE;
	}
	ndr_push_init(&ndr_push, tmp_buff, 8,
		NDR_FLAG_NOALIGN|NDR_FLAG_BIGENDIAN);
	if (NDR_ERR_SUCCESS != fcgi_rpc_push_params_begin(&ndr_push) ||
		NDR_ERR_SUCCESS != fcgi_rpc_push_params_end(&ndr_push) ||
		8 != ndr_push.offset || 8 != write(cli_sockd, tmp_buff, 8)) {
		close(cli_sockd);
		return FALSE;
	}
	offset = 0;
	while (in_len > 0) {
		if (in_len < 0xFFFF) {
			tmp_len = in_len;
		} else {
			tmp_len = 0xFFFF;
		}
		in_len -= tmp_len;
		ndr_push_init(&ndr_push, ndr_buff, sizeof(ndr_buff),
					NDR_FLAG_NOALIGN|NDR_FLAG_BIGENDIAN);
		if (NDR_ERR_SUCCESS != fcgi_rpc_push_stdin(
			&ndr_push, pbuff_in + offset, tmp_len)) {
			close(cli_sockd);
			return FALSE;
		}
		offset += tmp_len;
		if (ndr_push.offset != write(cli_sockd,
			ndr_buff, ndr_push.offset)) {
			close(cli_sockd);
			return FALSE;
		}
	}
	ndr_push_init(&ndr_push, ndr_buff, sizeof(ndr_buff),
				NDR_FLAG_NOALIGN|NDR_FLAG_BIGENDIAN);
	if (NDR_ERR_SUCCESS != fcgi_rpc_push_stdin(
		&ndr_push, NULL, 0)) {
		close(cli_sockd);
		return FALSE;
	}
	if (ndr_push.offset != write(cli_sockd,
		ndr_buff, ndr_push.offset)) {
		close(cli_sockd);
		return FALSE;
	}
	if (FALSE == ext_buffer_push_init(&ext_push, NULL, 0, 0)) {
		close(cli_sockd);
		return FALSE;
	}
	b_header = FALSE;
	while (TRUE) {
		if (FALSE == fcgi_rpc_safe_read(cli_sockd,
			exec_timeout, header_buff, 8)) {
			close(cli_sockd);
			ext_buffer_push_free(&ext_push);
			return FALSE;	
		}
		ndr_pull_init(&ndr_pull, header_buff, 8,
			NDR_FLAG_NOALIGN|NDR_FLAG_BIGENDIAN);
		if (NDR_ERR_SUCCESS != fcgi_rpc_pull_record_header(
			&ndr_pull, &header)) {
			close(cli_sockd);
			ext_buffer_push_free(&ext_push);
			return FALSE;
		}
		switch (header.type) {
		case RECORD_TYPE_END_REQUEST:
			if (8 != header.content_len) {
				close(cli_sockd);
				ext_buffer_push_free(&ext_push);
				return FALSE;
			}
			tmp_len = header.padding_len + 8;
			if (FALSE == fcgi_rpc_safe_read(cli_sockd,
				exec_timeout, tmp_buff, tmp_len)) {
				close(cli_sockd);
				ext_buffer_push_free(&ext_push);
				return FALSE;
			}
			close(cli_sockd);
			*ppbuff_out = ext_push.data;
			*pout_len = ext_push.offset;
			return TRUE;
		case RECORD_TYPE_STDOUT:
		case RECORD_TYPE_STDERR:
			tmp_len = header.content_len + header.padding_len;
			if (FALSE == fcgi_rpc_safe_read(cli_sockd,
				exec_timeout, tmp_buff, tmp_len)) {
				close(cli_sockd);
				ext_buffer_push_free(&ext_push);
				return FALSE;	
			}
			ndr_pull_init(&ndr_pull, tmp_buff, tmp_len,
					NDR_FLAG_NOALIGN|NDR_FLAG_BIGENDIAN);
			std_stream.length = header.content_len;
			if (NDR_ERR_SUCCESS != fcgi_rpc_pull_stdstream(
				&ndr_pull, header.padding_len, &std_stream)) {
				close(cli_sockd);
				ext_buffer_push_free(&ext_push);
				return FALSE;	
			}
			if (RECORD_TYPE_STDERR == header.type) {
				memcpy(tmp_buff, std_stream.buffer, std_stream.length);
				tmp_buff[std_stream.length] = '\0';
				printf("[fcgi_rpc]: stderr message \"%s\" from "
					"fastcgi back-end %s", tmp_buff, socket_path);
				continue;
			}
			if (TRUE == b_header) {
				if (0 == std_stream.length) {
					close(cli_sockd);
					ext_buffer_push_free(&ext_push);
					return FALSE;
				}
				if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
					&ext_push, std_stream.buffer, std_stream.length)) {
					close(cli_sockd);
					ext_buffer_push_free(&ext_push);
					return FALSE;
				}
				continue;
			}
			pbody = memmem(std_stream.buffer,
				std_stream.length, "\r\n\r\n", 4);
			if (NULL == pbody) {
				close(cli_sockd);
				ext_buffer_push_free(&ext_push);
				return FALSE;
			}
			b_header = TRUE;
			pbody += 4;
			if (0 == strncasecmp(std_stream.buffer, "Status:", 7)) {
				ptoken = std_stream.buffer + 7;
			} else {
				ptoken = search_string(std_stream.buffer,
					"\r\nStatus:", std_stream.length);
				if (NULL != ptoken) {
					ptoken += 9;
				}
			}
			if (NULL != ptoken) {
				ptoken1 = memmem(ptoken, sizeof(status_line), "\r\n", 2);
				if (NULL == ptoken1) {
					close(cli_sockd);
					ext_buffer_push_free(&ext_push);
					return FALSE;
				}
				tmp_len = ptoken1 - ptoken;
				memcpy(status_line, ptoken, tmp_len);
				status_line[tmp_len] = '\0';
				ltrim_string(status_line);
				if (0 != strncmp(status_line, "200", 3)) {
					printf("[fcgi_rpc]: get response \"%s\" from "
						"back-end %s\n", status_line, socket_path);
					close(cli_sockd);
					ext_buffer_push_free(&ext_push);
					return FALSE;
				}
			}
			tmp_len = std_stream.length - (pbody - (void*)std_stream.buffer);
			if (0 == tmp_len) {
				continue;
			}
			if (EXT_ERR_SUCCESS != ext_buffer_push_bytes(
				&ext_push, pbody, tmp_len)) {
				close(cli_sockd);
				ext_buffer_push_free(&ext_push);
				return FALSE;
			}
			continue;
		default:
			tmp_len = header.content_len + header.padding_len;
			if (FALSE == fcgi_rpc_safe_read(cli_sockd,
				exec_timeout, tmp_buff, tmp_len)) {
				close(cli_sockd);
				ext_buffer_push_free(&ext_push);
				return FALSE;
			}
			printf("[fcgi_rpc]: ignore record %d from fastcgi"
				" back-end %s", (int)header.type, socket_path);
			continue;
		}
	}
}
