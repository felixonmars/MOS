// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/physical/buddy.h"
#include "mos/mm/physical/pmm.h"

#include <mos/lib/structures/list.h>
#include <stdlib.h>

#define log2(x)                                                                                                                                                          \
    __extension__({                                                                                                                                                      \
        __extension__ __auto_type _x = (x);                                                                                                                              \
        _x ? (sizeof(_x) * 8 - 1 - __builtin_clzl(_x)) : 0;                                                                                                              \
    })
#define log2_ceil(x) (log2(x) + (pow2(log2(x)) < x))

static const size_t orders[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25 };
static const size_t max_order = MOS_ARRAY_SIZE(orders) - 1;
static list_head freelists[MOS_ARRAY_SIZE(orders)] = { 0 };

static void add_to_freelist(size_t order, phyframe_t *frame)
{
    MOS_ASSERT(frame->state == PHYFRAME_FREE);
    frame->order = order;
    list_node_t *frame_node = list_node(frame);
    MOS_ASSERT(list_is_empty(frame_node));

    list_head *head = &freelists[order];
    list_head *node = head->next;

    // performance hot spot, even a binary tree would always be faster
    while (node != head && node < frame_node)
        node = node->next;

    list_node_insert_before(node, frame_node);
}

static pfn_t get_buddy_pfn(size_t page_pfn, size_t order)
{
    // the address of a block's "buddy" is equal to the XOR of the block's address and the block's size.
    return page_pfn ^ pow2(order);
}

static void dump_list(size_t order)
{
    const list_head *head = &freelists[order];
    pr_cont("\nlist of order %zu: ", order);
    list_foreach(phyframe_t, frame, *head)
    {
        pr_cont("\t" PFN_RANGE, phyframe_pfn(frame), phyframe_pfn(frame) + pow2(order) - 1);
    }
}

/**
 * @brief Add [start_pfn, start_pfn + nframes - 1] to the freelist of order `order`
 *
 * @param start_pfn physical frame number of the first frame in the block
 * @param nframes number of frames in the block
 * @param order order of the block, must be in [0, MAX_ORDER]
 */
static void populate_freelist(const size_t start_pfn, const size_t nframes, const size_t order)
{
    const size_t step = pow2(order);

    mos_debug(pmm_buddy, "  order: %zu, step: %zu", order, step);

    pfn_t current = start_pfn;
    size_t nframes_left = nframes;

    for (; current + step <= start_pfn + nframes; current += step)
    {
        phyframe_t *frame = &phyframes[current];
        linked_list_init(list_node(frame));
        frame->state = PHYFRAME_FREE; // free or reserved

        mos_debug(pmm_buddy, "    - " PFN_RANGE, current, current + step - 1);
        frame->order = order;
        add_to_freelist(order, frame);
        nframes_left -= step;
    }

    // we have some frames left over, add them to a smaller order
    if (nframes_left)
        populate_freelist(current, nframes_left, order - 1);
}

static void break_this_pfn(pfn_t this_pfn, size_t this_order)
{
    phyframe_t *const frame = &phyframes[this_pfn];
    MOS_ASSERT(frame->state == PHYFRAME_FREE); // must be free
    list_remove(frame);

    // split this frame into two frames of order-1
    const pfn_t frame2_pfn = this_pfn + pow2(this_order - 1); // pow2(order) / 2
    mos_debug(pmm_buddy, "  breaking order %zu" PFN_RANGE " -> " PFN_RANGE " and " PFN_RANGE, this_order, this_pfn, this_pfn + pow2(this_order) - 1, this_pfn,
              frame2_pfn - 1, frame2_pfn, frame2_pfn + pow2(this_order - 1) - 1);

    phyframe_t *const frame2 = &phyframes[frame2_pfn];
    linked_list_init(list_node(frame2)); // this must not be in any list, so we init it
    frame2->state = frame->state;        // which is PHYFRAME_FREE

    add_to_freelist(this_order - 1, frame);
    add_to_freelist(this_order - 1, frame2);
}

static void extract_exact_range(pfn_t start, size_t nframes, enum phyframe_state state)
{
    size_t last_nframes = 0;

    while (nframes)
    {
        if (last_nframes == nframes)
        {
            phyframe_t *frame = &phyframes[start];
            if (state == PHYFRAME_RESERVED && frame->state == PHYFRAME_RESERVED)
            {
                // XXX: A great hack, there may be overlapped reserved ranges.
                MOS_ASSERT(frame->order == 0);
                frame->is_compound_tail = false;
                frame->compound_head = NULL;
                start++;
                nframes--;
            }
            else
            {
                mos_panic("infinite loop detected");
            }
        }

        last_nframes = nframes;

        MOS_ASSERT_X(start <= buddy_max_nframes, "insane!");
        mos_debug(pmm_buddy, "  extracting, n left: %zu, start: " PFN_FMT, nframes, start);

        for (size_t order = max_order; order != (size_t) -1; order--)
        {
            // find if the freelist of this order contains something [start]
            list_head *freelist = &freelists[order];
            if (list_is_empty(freelist))
                continue; // no, the frame must be at a lower order

            list_foreach(phyframe_t, f, *freelist)
            {
                const pfn_t start_pfn = phyframe_pfn(f);
                const pfn_t end_pfn = start_pfn + pow2(order) - 1;

                if (start_pfn == start)
                {
                    // so, we found a set of frame that starts with [start], here are the cases:
                    // - pow2(order) < nframes:   we not only need this frame, but also some more frames
                    // - pow2(order) = nframes:   we need this frame, and we are done
                    // - pow2(order) > nframes:   we need this frame, but we need to break it into two smaller frames so that
                    //                            in the next iteration, a more precise subset of this frame can be found

                    mos_debug(pmm_buddy, "    found a frame that starts with " PFN_FMT "...", start);
                    if (pow2(order) <= nframes)
                    {
                        list_remove(f);
                        f->state = state;
                        f->is_compound_tail = false;
                        f->state = state;
                        f->order = 0;

                        nframes -= pow2(order);
                        start += pow2(order);

                        mos_debug(pmm_buddy, "      done, n left: %zu, start: " PFN_FMT, nframes, start);
                        break; // we're done with the current order
                    }
                    else
                    {
                        mos_debug(pmm_buddy, "      narrowing down...");
                        // break this frame into two smaller frames, so that in the next iteration
                        // we can find a frame that exactly ends with [start + nframes - 1]
                        break_this_pfn(start_pfn, order);
                        break;
                    }
                }

                if (start_pfn <= start && end_pfn >= start)
                {
                    mos_debug(pmm_buddy, "    found a frame that contains " PFN_FMT, start);
                    // we found a frame that contains [start]
                    // so we will break it into two frames,
                    // - one of which contains [start], and
                    // - the other contains [start + 1, end_pfn]

                    // the lower order frames will be broken down in the next iteration of this loop
                    // until we have a frame that exactly starts with [start]
                    break_this_pfn(start_pfn, order);
                    break;
                }
            }

            if (nframes == 0)
                break; // fast exit path
        }
    }
}

static void break_the_order(const size_t order)
{
    if (order > orders[MOS_ARRAY_SIZE(orders) - 1])
        return; // we can't break any further

    const list_head *freelist = &freelists[order];
    if (list_is_empty(freelist))
        break_the_order(order + 1);

    if (list_is_empty(freelist))
    {
        mos_debug(pmm_buddy, "  no free frames of order %zu, can't break", order);
        return; // out of memory!
    }

    phyframe_t *const frame = list_entry(freelist->next, phyframe_t);
    MOS_ASSERT(frame->state == PHYFRAME_FREE);
    list_remove(frame);

    const pfn_t frame_pfn = phyframe_pfn(frame);
    const pfn_t frame2_pfn = frame_pfn + pow2(order - 1); // pow2(order) / 2

    mos_debug(pmm_buddy, "  breaking order %3zu, " PFN_RANGE " -> " PFN_RANGE " and " PFN_RANGE, order, frame_pfn, frame_pfn + pow2(order) - 1, frame_pfn, frame2_pfn - 1,
              frame2_pfn, frame2_pfn + pow2(order - 1) - 1);

    phyframe_t *const frame2 = &phyframes[frame2_pfn];
    linked_list_init(list_node(frame2));
    frame2->state = PHYFRAME_FREE;

    add_to_freelist(order - 1, frame);
    add_to_freelist(order - 1, frame2);
}

/**
 * @brief Try finding a buddy frame and merging it with the given frame
 *
 * @param pfn physical frame number
 * @param order order of the frame, given by log2(nframes)
 * @return true if a buddy was found and merged to a higher order freelist
 * @return false if...
 */
[[nodiscard]] static bool try_merge(const pfn_t pfn, size_t order)
{
    if (order > orders[MOS_ARRAY_SIZE(orders) - 1])
    {
        mos_debug(pmm_buddy, "  order %zu is too large, cannot merge", order);
        return false;
    }

    phyframe_t *const frame = &phyframes[pfn];

    const pfn_t buddy_pfn = get_buddy_pfn(pfn, order);
    if (buddy_pfn >= buddy_max_nframes)
    {
        // mos_debug(pmm_buddy, "  buddy pfn " PFN_FMT " is out of bounds, simply adding to order %zu", buddy_pfn, order);
        // frame->state = PHYFRAME_FREE;
        // add_to_freelist(order, frame);
        // return true;

        return false;
    }

    phyframe_t *const buddy = &phyframes[buddy_pfn];
    if (buddy->state != PHYFRAME_FREE)
    {
        mos_debug(pmm_buddy, "  buddy pfn " PFN_FMT " is not free for pfn " PFN_FMT ", not merging", buddy_pfn, pfn);
        return false;
    }

    if (buddy->order != order)
    {
        mos_debug(pmm_buddy, "  buddy pfn " PFN_FMT " is not the same order (%zu != %zu) as " PFN_FMT ", not merging", buddy_pfn, buddy->order, order, pfn);
        return false;
    }

    list_remove(buddy);
    frame->state = PHYFRAME_FREE;

    mos_debug(pmm_buddy, "  merging order %zu, " PFN_RANGE " and " PFN_RANGE, order, pfn, pfn + pow2(order) - 1, buddy_pfn, buddy_pfn + pow2(order) - 1);

    const size_t high_order_pfn = MIN(pfn, buddy_pfn); // the lower pfn

    if (!try_merge(high_order_pfn, order + 1))
    {
        phyframe_t *const high_order_frame = &phyframes[high_order_pfn];
        linked_list_init(list_node(high_order_frame));
        high_order_frame->state = PHYFRAME_FREE;
        add_to_freelist(order + 1, high_order_frame); // use the lower pfn
    }
    return true;
}

void buddy_dump_all()
{
    for (size_t i = 0; i < MOS_ARRAY_SIZE(freelists); i++)
        dump_list(i);

    pr_info("");
}

void buddy_init(size_t max_nframes)
{
    for (size_t i = 0; i < MOS_ARRAY_SIZE(freelists); i++)
    {
        mos_debug(pmm_buddy, "init freelist[%zu], order: %zu", i, orders[i]);
        linked_list_init(&freelists[i]);
    }

    const size_t order = MIN(log2(max_nframes), orders[MOS_ARRAY_SIZE(orders) - 1]);
    populate_freelist(0, max_nframes, order);
}

void buddy_reserve_n(pfn_t pfn, size_t nframes)
{
    mos_debug(pmm_buddy, "reserving " PFN_RANGE " (%zu frames)", pfn, pfn + nframes - 1, nframes);
    extract_exact_range(pfn, nframes, PHYFRAME_RESERVED);
}

phyframe_t *buddy_alloc_n_exact(size_t nframes)
{
    const size_t order = log2_ceil(nframes);

    // check if this order is too large
    if (order > orders[MOS_ARRAY_SIZE(orders) - 1])
        return NULL;

    mos_debug(pmm_buddy, "allocating %zu contiguous frames (order %zu, which is %zu frames, wasting %zu frames)", nframes, order, pow2(order), pow2(order) - nframes);

    list_head *free = &freelists[order];
    if (list_is_empty(free))
        break_the_order(order + 1);

    if (list_is_empty(free))
    {
        pr_emerg("no free frames of order %zu, can't break", order);
        pr_emerg("out of memory!");
        return NULL; // out of memory!
    }

    phyframe_t *const frame = list_entry(free->next, phyframe_t);
    const pfn_t start = phyframe_pfn(frame);

    extract_exact_range(start, nframes, PHYFRAME_ALLOCATED); // extract the exact range from the freelists

    for (size_t i = 0; i < nframes; i++)
    {
        phyframe_t *const f = &phyframes[start + i];
        f->state = PHYFRAME_ALLOCATED;
        f->order = 0; // so that they can be freed individually
        f->is_compound_tail = false;
        f->compound_head = NULL;
    }

    return frame;
}

phyframe_t *buddy_alloc_n_compound(size_t nframes)
{
    const size_t order = log2_ceil(nframes);

    // check if this order is too large
    if (order > orders[MOS_ARRAY_SIZE(orders) - 1])
        return NULL;

    mos_debug(pmm_buddy, "allocating %zu contiguous frames (order %zu, which is %zu frames, wasting %zu frames)", nframes, order, pow2(order), pow2(order) - nframes);

    list_head *free = &freelists[order];
    if (list_is_empty(free))
        break_the_order(order + 1);

    if (list_is_empty(free))
    {
        pr_emerg("no free frames of order %zu, can't break", order);
        pr_emerg("out of memory!");
        return NULL; // out of memory!
    }

    phyframe_t *const head_frame = list_entry(free->next, phyframe_t);
    list_remove(head_frame); // this frame is what we want
    head_frame->state = PHYFRAME_ALLOCATED;
    head_frame->order = order;
    head_frame->is_compound_tail = false;

    const pfn_t head_pfn = phyframe_pfn(head_frame);

    for (size_t i = head_pfn + 1; i < head_pfn + nframes; i++)
    {
        phyframe_t *const f = &phyframes[i];
        f->state = PHYFRAME_ALLOCATED;
        f->is_compound_tail = true;
        f->compound_head = head_frame;
    }
    return head_frame;
}

void buddy_free_n(pfn_t pfn, size_t nframes)
{
    mos_debug(pmm_buddy, "freeing " PFN_RANGE " (%zu frames)", pfn, pfn + nframes - 1, nframes);

    phyframe_t *const frame = &phyframes[pfn];
    MOS_ASSERT(list_is_empty(list_node(frame)));
    frame->state = PHYFRAME_FREE;

    const size_t order = log2_ceil(nframes);
    if (!try_merge(pfn, order))
        add_to_freelist(order, frame);
}
