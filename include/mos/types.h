// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"

#include <stdbool.h>
#include <stddef.h>

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef signed long long int s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef long double f80;

typedef unsigned long uintptr_t;
#define PTR_FMT "0x%lx"

typedef union
{
    struct
    {
        bool b0 : 1;
        bool b1 : 1;
        bool b2 : 1;
        bool b3 : 1;
        bool b4 : 1;
        bool b5 : 1;
        bool b6 : 1;
        bool msb : 1;
    } __packed bits;
    u8 byte;
} byte_t;

static_assert(sizeof(byte_t) == sizeof(char), "byte_t is not 1 byte");

// clang-format off
#define newtype(type, name) typedef struct { type name; } name##_t
// clang-format on

typedef u32 id_t;

newtype(id_t, page_dir);
newtype(id_t, uid);
newtype(id_t, gid);

newtype(id_t, process_id);
newtype(id_t, thread_id);
