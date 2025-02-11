// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/lib/structures/hashmap.h>
#include <mos/printk.h>
#include <mos/tasks/kthread.h>
#include <mos/tasks/process.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <stdlib.h>

static process_t *kthreadd = NULL;

typedef struct kthread_arg
{
    thread_entry_t entry;
    void *arg;
} kthread_arg_t;

static void kthread_entry(void *arg)
{
    kthread_arg_t *kthread_arg = arg;
    kthread_arg->entry(kthread_arg->arg);
    kfree(kthread_arg);
    thread_handle_exit(current_thread);
}

void kthread_init(void)
{
    kthreadd = process_allocate(NULL, "kthreadd");
    MOS_ASSERT_X(kthreadd->pid == 2, "kthreadd should have pid 2");
    hashmap_put(&process_table, kthreadd->pid, kthreadd);
}

thread_t *kthread_create(thread_entry_t entry, void *arg, const char *name)
{
    pr_info2("creating kernel thread '%s'", name);
    kthread_arg_t *kthread_arg = kzalloc(sizeof(kthread_arg_t));
    kthread_arg->entry = entry;
    kthread_arg->arg = arg;
    thread_t *thread = thread_new(kthreadd, THREAD_MODE_KERNEL, name, kthread_entry, kthread_arg);
    thread_setup_complete(thread);
    return thread;
}
