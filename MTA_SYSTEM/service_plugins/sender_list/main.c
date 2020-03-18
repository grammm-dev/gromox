#include "service_common.h"
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

#define RESPONSE_CODE_SUCCESS      			0x00

#define CALL_ID_CHECKROW					0x30

#define CS_PATH								"/var/grid-agent/token/adac"

DECLARE_API;

static BOOL check_sender(const char *dir, const char *tag_string)
{
	int sockd, len;
	EXT_PUSH ext_push;
	uint32_t request_len;
	struct sockaddr_un un;
	char response_buff[16];
	char request_buff[1024];
	
	ext_buffer_push_init(&ext_push, request_buff, 1024, 0);
	ext_buffer_push_advance(&ext_push, sizeof(uint32_t));
	ext_buffer_push_uint8(&ext_push, CALL_ID_CHECKROW);
	ext_buffer_push_string(&ext_push, dir);
	ext_buffer_push_string(&ext_push, "anti-spam");
	ext_buffer_push_string(&ext_push, "sender-list");
	ext_buffer_push_uint16(&ext_push, PROPVAL_TYPE_STRING);
	ext_buffer_push_string(&ext_push, tag_string);
	request_len = ext_push.offset;
	ext_push.offset = 0;
	ext_buffer_push_uint32(&ext_push, request_len- sizeof(uint32_t));
	
	sockd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockd < 0) {
		return FALSE;
	}
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, CS_PATH);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);
	if (connect(sockd, (struct sockaddr*)&un, len) < 0) {
		close(sockd);
		return FALSE;
	}
	if (request_len != write(sockd, request_buff, request_len)) {
		close(sockd);
		return FALSE;
	}
	if (9 != read(sockd, response_buff, 9)) {
		close(sockd);
		return FALSE;
	}
	close(sockd);
	if (RESPONSE_CODE_SUCCESS != response_buff[0]) {
		return FALSE;
	}
	if (4 != *(uint32_t*)(response_buff + 1)) {
		return FALSE;
	}
	if (0 == *(uint32_t*)(response_buff + 5)) {
		return TRUE;
	}
	return FALSE;
}

BOOL SVC_LibMain(int reason, void **ppdata)
{
	switch(reason) {
	case PLUGIN_INIT:
		LINK_API(ppdata);
		
		if (FALSE == register_service("check_sender", check_sender)) {
			printf("[sender_list]: fail to register services\n");
			return FALSE;
		}
		return TRUE;
	case PLUGIN_FREE:
		return TRUE;
	}
}
