#include "service_common.h"
#include "config_file.h"
#include "fcgi_rpc.h"
#include <string.h>
#include <stdio.h>

DECLARE_API;

static int g_excution_timeout;
static char g_socket_path[256];

static BOOL fcgi_rpc(const uint8_t *pbuff_in, uint32_t in_len,
	uint8_t **ppbuff_out, uint32_t *pout_len, const char *script_path)
{
	return fcgi_rpc_do(g_socket_path, g_excution_timeout,
		pbuff_in, in_len, ppbuff_out, pout_len, script_path);
}

BOOL SVC_LibMain(int reason, void **ppdata)
{
	char tmp_buff[64];
	CONFIG_FILE *pfile;
	char tmp_path[256];
	char file_name[256];
	char *str_value, *psearch;

	switch(reason) {
	case PLUGIN_INIT:
		LINK_API(ppdata);
		/* get the plugin name from system api */
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		sprintf(tmp_path, "%s/%s.cfg", get_config_path(), file_name);
		pfile = config_file_init(tmp_path);
		if (NULL == pfile) {
			printf("[fcgi_rpc]: error to open config file!!!\n");
			return FALSE;
		}
		str_value = config_file_get_value(pfile, "FPM_SOCKET_PATH");
		if (NULL == str_value) {
			strcpy(g_socket_path, "/var/run/php5-fpm.sock");
			config_file_set_value(pfile, "FPM_SOCKET_PATH", g_socket_path);
		} else {
			strcpy(g_socket_path, str_value);
		}
		printf("[fcgi_rpc]: fpm socket path is %s\n", g_socket_path);
		str_value = config_file_get_value(pfile, "EXEC_TIMEOUT");
		if (NULL == str_value) {
			g_excution_timeout = 600;
			config_file_set_value(pfile, "EXEC_TIMEOUT", "10minutes");
		} else {
			g_excution_timeout = atoitvl(str_value);
		}
		itvltoa(g_excution_timeout, tmp_buff);
		printf("[fcgi_rpc]: excution timeout is %s\n", tmp_buff);
		if (FALSE == config_file_save(pfile)) {
			config_file_free(pfile);
			printf("[fcgi_rpc]: fail to save config file\n");
			return FALSE;
		}
		config_file_free(pfile);
		if (FALSE == register_service("fcgi_rpc", fcgi_rpc)) {
			printf("[fcgi_rpc]: fail to register \"fcgi_rpc\" service\n");
			return FALSE;
		}
		return TRUE;
	case PLUGIN_FREE:
		return TRUE;
	}
}
