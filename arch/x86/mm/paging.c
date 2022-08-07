// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/paging.h"

#include "lib/containers.h"
#include "lib/string.h"
#include "mos/kconfig.h"
#include "mos/mm/mm_types.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/types.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/x86_platform.h"

extern void x86_enable_paging_impl(void *page_dir);

static const void *x86_paging_area_start = &__MOS_X86_PAGING_AREA_START;
static const void *x86_page_table_start = &__MOS_X86_PAGE_TABLE_START;
static const void *x86_paging_area_end = &__MOS_X86_PAGING_AREA_END;

static pgdir_entry *mm_page_dir;
static pgtable_entry *mm_page_table;

typedef struct
{
    as_linked_list;
    u32 paddr;
    size_t n_bytes;
} free_phymem_desc_t;

static list_node_t mm_free_phymem = LIST_HEAD_INIT(mm_free_phymem);

#define MM_PAGE_MAP_SIZE (X86_MAX_MEM_SIZE / X86_PAGE_SIZE / 8)
static const u32 mm_page_map_size = MM_PAGE_MAP_SIZE;
static u8 mm_page_map[MM_PAGE_MAP_SIZE] = { 0 };

// static memblock_t bootstrap_pg = {
//     .list_node = LIST_HEAD_INIT(mm_free_pages),
//     .vaddr = MOS_X86_HEAP_BASE_VADDR,
//     .size = X86_PAGE_SIZE,
// };

void x86_mm_prepare_paging()
{
    // validate if the memory region calculated from the linker script is correct.
    s64 paging_area_size = (uintptr_t) x86_paging_area_end - (uintptr_t) x86_paging_area_start;
    static const s64 paging_area_size_expected = 1024 * sizeof(pgdir_entry) + 1024 * 1024 * sizeof(pgtable_entry);
    pr_debug("paging: provided size: 0x%llx, minimum required size: 0x%llx", paging_area_size, paging_area_size_expected);
    MOS_ASSERT_X(paging_area_size >= paging_area_size_expected, "allocated paging area size is too small");

    // place the global page directory at somewhere outside of the kernel
    mm_page_dir = (pgdir_entry *) x86_paging_area_start;
    mm_page_table = (pgtable_entry *) x86_page_table_start;

    MOS_ASSERT_X((uintptr_t) mm_page_dir % 4096 == 0, "page directory is not aligned to 4096");
    MOS_ASSERT_X((uintptr_t) mm_page_table % 4096 == 0, "page table is not aligned to 4096");

    // initialize the page directory
    memset(mm_page_dir, 0, sizeof(pgdir_entry) * 1024);

    pr_debug("paging: setting up low 1MB identity mapping... (except the NULL page)");
    x86_mm_map_page(0, 0, PAGING_PRESENT); // ! the zero page is not writable
    for (int addr = X86_PAGE_SIZE; addr < 1 MB; addr += X86_PAGE_SIZE)
        x86_mm_map_page(addr, addr, PAGING_PRESENT | PAGING_WRITABLE);

    pr_debug("paging: mapping kernel space...");
    uintptr_t addr = (x86_kernel_start_addr / X86_PAGE_SIZE) * X86_PAGE_SIZE; // align the address to the page size
    for (; addr < x86_kernel_end_addr; addr += X86_PAGE_SIZE)
        x86_mm_map_page(addr, addr, PAGING_PRESENT | PAGING_WRITABLE);

    // get a proper physical memory address for the kernel heap
    // u64 required_size = bootstrap_pg.size;
    // pr_debug("paging: pre-allocating %llu bytes for the bootstrap page", required_size);
    // for (size_t ri = x86_mem_regions_count - 1; ri < x86_mem_regions_count; ri--)
    // {
    //     memblock_t *region = &x86_mem_regions[ri];
    //     if (!region->available)
    //         continue;
    //     if (region->size >= required_size)
    //     {
    //         // found a region that is big enough, try aligning it to the page size
    //         u64 phys_addr = region->paddr + X86_PAGE_SIZE - 1;
    //         phys_addr = (phys_addr / X86_PAGE_SIZE) * X86_PAGE_SIZE;
    //         if (phys_addr < region->paddr || phys_addr + required_size > region->paddr + region->size)
    //         {
    //             pr_debug("paging: not suitable physical memory address, region: 0x%llx", region->paddr);
    //             continue;
    //         }
    //         bootstrap_pg.paddr = phys_addr;
    //         break;
    //     }
    // }

    // MOS_ASSERT_X(bootstrap_pg.paddr != 0, "failed to find a suitable physical memory address for the bootstrap page");
    // pr_debug("paging: bootstrap page: 0x%llx, vaddr: 0x%llx", bootstrap_pg.paddr, bootstrap_pg.vaddr);

    // for (u64 v = bootstrap_pg.vaddr, p = bootstrap_pg.paddr; v < bootstrap_pg.vaddr + bootstrap_pg.size; v += X86_PAGE_SIZE, p +=
    // X86_PAGE_SIZE)
    //     x86_mm_map_page(v, p, PAGING_PRESENT | PAGING_WRITABLE);
}

void x86_mm_map_page(uintptr_t vaddr, uintptr_t paddr, paging_entry_flags flags)
{
    // ensure the page is aligned to 4096
    MOS_ASSERT_X(vaddr % X86_PAGE_SIZE == 0, "vaddr is not aligned to 4096");

    // ! todo: ensure the offsets are correct for both paddr and vaddr
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff; // mask out the lower 12 bits

    pgdir_entry *page_dir = mm_page_dir + page_dir_index;
    pgtable_entry *page_table;

    if (likely(page_dir->present))
    {
        page_table = ((pgtable_entry *) (page_dir->page_table_addr << 12)) + page_table_index;
    }
    else
    {
        page_table = mm_page_table + page_dir_index * 1024 + page_table_index;
        page_dir->present = true;
        page_dir->page_table_addr = (uintptr_t) page_table >> 12;
    }

    page_dir->writable |= !!(flags & PAGING_WRITABLE);
    page_dir->usermode |= !!(flags & PAGING_USERMODE);

    MOS_ASSERT_X(page_table->present == false, "page is already mapped");

    page_table->present = !!(flags & PAGING_PRESENT);
    page_table->writable = !!(flags & PAGING_WRITABLE);
    page_table->usermode = !!(flags & PAGING_USERMODE);
    page_table->phys_addr = (uintptr_t) paddr >> 12;

    // update the mm_page_map
    u32 pte_index = page_dir_index * 1024 + page_table_index;
    mm_page_map[pte_index / 8] |= 1 << (pte_index % 8);
}

void x86_mm_unmap_page(uintptr_t vaddr)
{
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff;

    pgdir_entry *page_dir = mm_page_dir + page_dir_index;
    if (unlikely(!page_dir->present))
    {
        mos_warn("page '%zx' not mapped", vaddr);
        return;
    }

    pgtable_entry *page_table = ((pgtable_entry *) (page_dir->page_table_addr << 12)) + page_table_index;
    page_table->present = false;

    // update the mm_page_map
    u32 pte_index = page_dir_index * 1024 + page_table_index;
    mm_page_map[pte_index / 8] &= ~(1 << (pte_index % 8));
}

void x86_mm_enable_paging()
{
    pr_info("Page directory is at: %p", (void *) mm_page_dir);
    x86_enable_paging_impl(mm_page_dir);
    pr_info("Paging enabled.");
}

void *x86_mm_alloc_page(size_t n_page)
{
#define BIT_IS_SET(byte, bit) ((byte) & (1 << (bit)))
    // simply rename the variable, we are dealing with bitmaps
    size_t n_bits = n_page;
    size_t n_zero_bits = 0;
    u8 target_bit = 0;

    size_t i = 0;
    while (n_zero_bits < n_bits)
    {
        if (i >= mm_page_map_size)
        {
            mos_warn("failed to allocate %zu pages", n_page);
            return NULL;
        }
        u8 current_byte = mm_page_map[i];
        i++;

        if (current_byte == 0)
        {
            n_zero_bits += 8;
            continue;
        }

        for (int bit = 0; bit < 8; bit++)
        {
            target_bit = bit;
            if (!BIT_IS_SET(current_byte, bit))
                n_zero_bits++;
            else
                n_zero_bits = 0, target_bit = 0;
        }
    }

    u32 page_index = i * 8 + target_bit;
    u32 page_dir_index = page_index / 1024;
    u32 page_table_index = page_index % 1024;
    void *vaddr = (void *) (page_dir_index << 22 | page_table_index << 12);

    // !! id map the page (for now)
    for (size_t p = 0; p < n_page; p++)
        x86_mm_map_page((uintptr_t) vaddr + p * X86_PAGE_SIZE, (uintptr_t) vaddr + p * X86_PAGE_SIZE, PAGING_PRESENT | PAGING_WRITABLE);

    return vaddr;
}

bool x86_mm_free_page(void *vptr, size_t n)
{
    for (size_t p = 0; p < n; p++)
        x86_mm_unmap_page((uintptr_t) vptr + p * X86_PAGE_SIZE);
    return true;
}
