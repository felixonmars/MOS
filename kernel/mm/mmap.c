// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mm/memops.h>
#include <mos/mm/mm_types.h>
#include <mos/mm/mmap.h>
#include <mos/mm/paging/paging.h>
#include <mos/tasks/process.h>
#include <mos/tasks/task_types.h>

static bool mmap_check(uintptr_t hint_addr, mmap_flags_t mmap_flags)
{
    const bool shared = mmap_flags & MMAP_SHARED;   // when forked, shared between parent and child
    const bool private = mmap_flags & MMAP_PRIVATE; // when forked, make it Copy-On-Write

    if (shared == private)
    {
        pr_warn("mmap_file: shared and private are mutually exclusive, and one of them must be specified");
        return NULL;
    }

    if ((hint_addr == 0) && (mmap_flags & MMAP_EXACT))
    {
        // WTF is this? Trying to map at address 0?
        pr_warn("mmap_anonymous: trying to map at address 0");
        return false;
    }

    return true;
}

uintptr_t mmap_anonymous(uintptr_t hint_addr, mmap_flags_t flags, vm_flags vm_flags, size_t n_pages)
{
    if (!mmap_check(hint_addr, flags))
        return 0;

    const vmap_fork_mode_t fork_mode = (flags & MMAP_SHARED) ? VMAP_FORK_SHARED : VMAP_FORK_PRIVATE;
    const valloc_flags valloc_flags = (flags & MMAP_EXACT) ? VALLOC_EXACT : VALLOC_DEFAULT;

    vmblock_t block = mm_alloc_zeroed_pages(current_process->pagetable, n_pages, hint_addr, valloc_flags, vm_flags);
    pr_info2("mmap_anonymous: allocated %zd pages at " PTR_FMT, block.npages, block.vaddr);

    process_attach_mmap(current_process, block, VMTYPE_MMAP, (vmap_flags_t){ .cow = true, .fork_mode = fork_mode });

    return block.vaddr;
}

uintptr_t mmap_file(uintptr_t hint_addr, mmap_flags_t flags, vm_flags vm_flags, size_t size, io_t *io, off_t offset)
{
    if (!mmap_check(hint_addr, flags))
        return 0;

    MOS_UNUSED(vm_flags);
    MOS_UNUSED(size);
    MOS_UNUSED(io);
    MOS_UNUSED(offset);

    return 0;
}

bool munmap(uintptr_t addr, size_t size)
{
    // will unmap all pages containing the range, even if they are not fully contained
    const uintptr_t range_start = ALIGN_DOWN_TO_PAGE(addr);
    const uintptr_t range_end = ALIGN_UP_TO_PAGE(addr + size);
    const size_t n_pages = (range_end - range_start) / MOS_PAGE_SIZE;

    mm_unmap_pages(current_process->pagetable, range_start, n_pages);

    return true;
}
