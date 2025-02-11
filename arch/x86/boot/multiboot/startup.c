// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/boot/startup.h>
#include <mos/device/console.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/x86_platform.h>

#define X86_VIDEO_DEVICE 0xb8000
#define VIDEO_WIDTH      80
#define VIDEO_HEIGHT     25

extern const char _mos_startup_START[];
extern const char _mos_startup_END[];
extern const char __MOS_KERNEL_CODE_START[];
extern const char __MOS_KERNEL_CODE_END[];
extern const char __MOS_KERNEL_RODATA_START[];
extern const char __MOS_KERNEL_RODATA_END[];
extern const char __MOS_KERNEL_RW_START[];
extern const char __MOS_KERNEL_RW_END[];
extern const char __MOS_KERNEL_END[];

#define startup_start      ((ptr_t) &_mos_startup_START)
#define startup_end        ((ptr_t) &_mos_startup_END)
#define kernel_code_vstart ((ptr_t) &__MOS_KERNEL_CODE_START)
#define kernel_code_vend   ((ptr_t) &__MOS_KERNEL_CODE_END)
#define kernel_ro_vstart   ((ptr_t) &__MOS_KERNEL_RODATA_START)
#define kernel_ro_vend     ((ptr_t) &__MOS_KERNEL_RODATA_END)
#define kernel_rw_vstart   ((ptr_t) &__MOS_KERNEL_RW_START)
#define kernel_rw_vend     ((ptr_t) &__MOS_KERNEL_RW_END)

// 768 KB of pages gives us 768 MB of virtual memory
__startup_rwdata x86_pgdir_entry startup_pgd[1024] __aligned(MOS_PAGE_SIZE) = { 0 };
__startup_rwdata x86_pgtable_entry startup_pgt[768 KB / 4] __aligned(MOS_PAGE_SIZE) = { 0 };

__startup_rwdata ptr_t video_device_address = X86_VIDEO_DEVICE;

#define STARTUP_ASSERT(cond, type)                                                                                                                                       \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (!unlikely(cond))                                                                                                                                             \
        {                                                                                                                                                                \
            print_debug_info('E', type, Red, Gray);                                                                                                                      \
            while (1)                                                                                                                                                    \
                __asm__("hlt");                                                                                                                                          \
        }                                                                                                                                                                \
    } while (0)

#define debug_print_step() print_debug_info('S', step++, LightGreen, Gray)

__startup_code should_inline void print_debug_info(char a, char b, char color1, char color2)
{
    *((volatile char *) video_device_address + 0) = a;
    *((volatile char *) video_device_address + 1) = color1;
    *((volatile char *) video_device_address + 2) = b;
    *((volatile char *) video_device_address + 3) = color2;
    *((volatile char *) video_device_address + 4) = '\0';
    *((volatile char *) video_device_address + 5) = White;
}

__startup_code should_inline void startup_setup_pgd(int pgdid, x86_pgtable_entry *pgtable)
{
    STARTUP_ASSERT(pgdid < 1024, 'r');                // pgdid must be less than 1024
    STARTUP_ASSERT(pgtable != NULL, 't');             // pgtable must not be NULL
    STARTUP_ASSERT((ptr_t) pgtable % 4096 == 0, 'a'); // pgtable must be aligned to 4096
    STARTUP_ASSERT(!startup_pgd[pgdid].present, 'p'); // pgdir entry already present

    mos_startup_memzero((void *) (startup_pgd + pgdid), sizeof(x86_pgdir_entry));
    startup_pgd[pgdid].present = true;
    startup_pgd[pgdid].page_table_paddr = (ptr_t) pgtable >> 12;
}

__startup_code void mos_startup_map_single_page(ptr_t vaddr, ptr_t paddr, vm_flags flags)
{
    const size_t dir_index = vaddr >> 22;
    const size_t table_index = (vaddr >> 12) & 0x3FF;

    __startup_rwdata static int used_pgd = 0;

    x86_pgdir_entry *this_dir = &startup_pgd[dir_index];
    if (!this_dir->present)
    {
        size_t pagedir_entry_table_index = 0;

        if (vaddr - (dir_index << 22) > 0)
            pagedir_entry_table_index = used_pgd * 1024;
        else if (vaddr - (dir_index << 22) == 0)
            pagedir_entry_table_index = ALIGN_UP(used_pgd * 1024, 1024);
        else
            STARTUP_ASSERT(false, 'v');

        startup_setup_pgd(dir_index, &startup_pgt[pagedir_entry_table_index]);
        used_pgd++;
    }

    // this_dir should be present now
    STARTUP_ASSERT(this_dir->present, 'm');
    this_dir->writable = flags & VM_WRITE;

    x86_pgtable_entry *this_table = (x86_pgtable_entry *) (this_dir->page_table_paddr << 12) + table_index;
    if (this_table->present)
    {
        STARTUP_ASSERT(this_table->pfn == (paddr >> 12), 'd'); // fail if the page is mapped to different physical address
        return;
    }
    mos_startup_memzero((void *) this_table, sizeof(x86_pgtable_entry));
    this_table->present = true;
    this_table->pfn = (ptr_t) paddr >> 12;
    this_table->writable = flags & VM_WRITE;
    this_table->global = flags & VM_GLOBAL;
    this_table->cache_disabled = flags & VM_CACHE_DISABLED;
    this_table->write_through = flags & VM_WRITE_THROUGH;

    __asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

// x86_startup does the following:
// 1. Identity map the VGA buffer to kvirt address space
// 2. Identity map the code section '.mos.startup*'
// 3. Map the kernel code, rodata, data, bss and kpage tables
// 4. Enable paging
__startup_code asmlinkage void x86_startup(x86_startup_info *startup)
{
    char step = 'a';

    STARTUP_ASSERT(startup->mb_magic == MULTIBOOT_BOOTLOADER_MAGIC, '1');
    STARTUP_ASSERT(startup->mb_info->flags & MULTIBOOT_INFO_MEM_MAP, '2');

    mos_startup_memzero((void *) startup_pgd, sizeof(x86_pgdir_entry) * 1024);
    mos_startup_memzero((void *) startup_pgt, 512 KB);

    debug_print_step(); // a
    mos_startup_map_identity((ptr_t) startup->mb_info, sizeof(multiboot_info_t), VM_READ);

    // multiboot stuff
    if (startup->mb_info->flags & MULTIBOOT_INFO_CMDLINE)
        mos_startup_map_identity((ptr_t) startup->mb_info->cmdline, mos_startup_strlen(startup->mb_info->cmdline), VM_READ);

    STARTUP_ASSERT(startup->mb_info->mmap_addr, 'm');
    mos_startup_map_identity((ptr_t) startup->mb_info->mmap_addr, startup->mb_info->mmap_length * sizeof(multiboot_memory_map_t), VM_READ);

    if (startup->mb_info->mods_addr)
        mos_startup_map_identity((ptr_t) startup->mb_info->mods_addr, startup->mb_info->mods_count * sizeof(multiboot_module_t), VM_READ);

    // map the VGA buffer, from 0xB8000
    mos_startup_map_bios(X86_VIDEO_DEVICE, VIDEO_WIDTH * VIDEO_HEIGHT * 2, VM_WRITE);

    // map the bios memory regions
    mos_startup_map_bios(X86_BIOS_MEMREGION_PADDR, BIOS_MEMREGION_SIZE, VM_READ);
    mos_startup_map_bios(X86_EBDA_MEMREGION_PADDR, EBDA_MEMREGION_SIZE, VM_READ);

    // ! we do [not] separate the startup part into [code, rodata, data] for simplicity
    // ! this page directory will be removed as soon as the kernel is loaded, it shouldn't be a problem.
    mos_startup_map_identity(startup_start, startup_end - startup_start, VM_RW | VM_EXEC);
    debug_print_step(); // b

    const size_t kernel_code_pgsize = ALIGN_UP_TO_PAGE(kernel_code_vend - kernel_code_vstart) / MOS_PAGE_SIZE;
    mos_startup_map_pages(kernel_code_vstart, kernel_code_vstart - MOS_KERNEL_START_VADDR, kernel_code_pgsize, VM_EXEC);
    debug_print_step(); // c

    const size_t kernel_ro_pgsize = ALIGN_UP_TO_PAGE(kernel_ro_vend - kernel_ro_vstart) / MOS_PAGE_SIZE;
    mos_startup_map_pages(kernel_ro_vstart, kernel_ro_vstart - MOS_KERNEL_START_VADDR, kernel_ro_pgsize, VM_READ);
    debug_print_step(); // d

    const size_t kernel_rw_pgsize = ALIGN_UP_TO_PAGE(kernel_rw_vend - kernel_rw_vstart) / MOS_PAGE_SIZE;
    mos_startup_map_pages(kernel_rw_vstart, kernel_rw_vstart - MOS_KERNEL_START_VADDR, kernel_rw_pgsize, VM_WRITE);
    debug_print_step(); // e

    __asm__ volatile("mov %0, %%cr3" ::"r"(startup_pgd));
    debug_print_step(); // f

    __asm__ volatile("mov %%cr0, %%eax; or $0x80000000, %%eax; mov %%eax, %%cr0" ::: "eax");
    video_device_address = BIOS_VADDR(X86_VIDEO_DEVICE);
    debug_print_step(); // g

    print_debug_info('O', 'k', Green, Green);
}
