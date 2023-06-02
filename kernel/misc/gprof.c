// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/misc/power.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/setup.h"

#include <mos/mos_global.h>

#if MOS_CONFIG(MOS_GPROF)

typedef struct
{
    ptr_t from_pc, self_pc;
    u64 entry_time;
} __packed call_record_t;

static call_record_t call_stack[10485760] = { 0 };
static size_t call_stack_index = 0;

__no_instrument void _mcount(long frompc, long selfpc)
{
    static bool in = false;
    if (in)
        return;

    in = true;

    u64 tsc = platform_get_timestamp();

    call_record_t *cr = &call_stack[call_stack_index++];
    cr->from_pc = frompc;
    cr->self_pc = selfpc;
    cr->entry_time = tsc;

    in = false;
}

void print_call_stack(void *unused)
{
    MOS_UNUSED(unused);
    for (size_t i = 0; i < call_stack_index; i++)
    {
        call_record_t *cr = &call_stack[i];
        pr_info2("from_pc: " PTR_FMT ", self_pc: " PTR_FMT ", entry_time: %llu", cr->from_pc, cr->self_pc, cr->entry_time);
    }
}

static void gprof_init(void)
{
    pr_info("registering shutdown callback for gprof");
    power_register_shutdown_callback(print_call_stack, NULL);
}

MOS_INIT(POWER, gprof_init);
#endif
