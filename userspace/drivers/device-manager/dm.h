// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <libconfig/libconfig.h>
#include <librpc/rpc_server.h>

extern const config_t *dm_config;

void dm_run_server(rpc_server_t *server);

bool try_start_driver(u16 vendor, u16 device, u32 location, const char *pcidev);
bool start_load_drivers(void);
