// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/tasks/task_types.h>

void tasks_init();
void unblock_scheduler(void);
noreturn void scheduler(void);

void reschedule_for_wait_condition(wait_condition_t *wait_condition);
__nodiscard bool reschedule_for_waitlist(waitlist_t *waitlist);

void reschedule(void);
