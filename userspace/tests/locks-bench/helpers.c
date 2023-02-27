// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/stdio.h"
#include "librpc/rpc_client.h"
#include "libuserspace.h"
#include "mos/device/dm_types.h"
#include "mos/syscall/usermode.h"

static rpc_server_stub_t *console_server;

static void do_print_to_console(const char *buf, size_t size)
{
    rpc_call_t *write_call = rpc_call_create(console_server, DM_CONSOLE_WRITE);
    rpc_call_arg(write_call, buf, size);
    rpc_call_exec(write_call, NULL, NULL);
    rpc_call_destroy(write_call);
}

void open_console(void)
{
    syscall_spawn("/drivers/x86_console_driver", 0, NULL); // TODO: remove this once init spawns the console driver

    console_server = rpc_client_create("drivers.x86_text_console");
    rpc_call_t *clear_call = rpc_call_create(console_server, DM_CONSOLE_CLEAR);
    rpc_call_exec(clear_call, NULL, NULL);
    rpc_call_destroy(clear_call);
}

void print_to_console(const char *fmt, ...)
{
    char buf[4 KB];

    va_list args;
    va_start(args, fmt);
    int size = vsnprintf(buf, sizeof(buf), fmt, args);
    do_print_to_console(buf, size);
    va_end(args);
}

void set_console_color(standard_color_t fg, standard_color_t bg)
{
    rpc_call_t *set_color_call = rpc_call_create(console_server, DM_CONSOLE_SET_COLOR);
    rpc_call_arg(set_color_call, &fg, sizeof(fg));
    rpc_call_arg(set_color_call, &bg, sizeof(bg));
    rpc_call_exec(set_color_call, NULL, NULL);
    rpc_call_destroy(set_color_call);
}
