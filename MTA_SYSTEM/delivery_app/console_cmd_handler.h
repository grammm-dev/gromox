#pragma once
#include "common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL cmd_handler_dequeue_control(int argc, char** argv);
BOOL cmd_handler_help(int argc, char** argv);

BOOL cmd_handler_server_control(int argc, char** argv);

BOOL cmd_handler_system_control(int argc, char** argv);

BOOL cmd_handler_mpc_plugins(int argc, char** argv);

BOOL cmd_handler_service_plugins(int argc, char** argv);

#ifdef __cplusplus
} /* extern "C" */
#endif
