// SPDX-License-Identifier: GPL-3.0-or-later

#include "libuserspace++.hpp"
#include "struct_file.h"

#include <mos/lib/sync/mutex.h>
#include <mos/types.h>
#include <stdio.h>
#include <stdlib.h>

void *__dso_handle = 0;

void *operator new(size_t sz) noexcept
{
    return malloc(sz);
}

void *operator new[](size_t sz) noexcept
{
    return malloc(sz);
}

void operator delete(void *ptr) noexcept
{
    free(ptr);
}

void operator delete[](void *ptr) noexcept
{
    free(ptr);
}

void operator delete(void *ptr, long unsigned int) noexcept
{
    free(ptr);
}

void operator delete[](void *ptr, long unsigned int) noexcept
{
    free(ptr);
}

namespace mos
{
    output_stream cout(stdout->fd);
    output_stream cerr(stderr->fd);

    output_stream &endl(output_stream &os)
    {
        return os << "\r\n";
    }

    output_stream &operator<<(output_stream &s, const void *ptr)
    {
        dprintf(s.fd, "%p", ptr);
        return s;
    }

    output_stream &operator<<(output_stream &os, const char c)
    {
        dprintf(os.fd, "%c", c);
        return os;
    }

    output_stream &operator<<(output_stream &s, const char *str)
    {
        dprintf(s.fd, "%s", str);
        return s;
    }

    output_stream &operator<<(output_stream &s, signed int val)
    {
        dprintf(s.fd, "%d", val);
        return s;
    }

    output_stream &operator<<(output_stream &s, unsigned int val)
    {
        dprintf(s.fd, "%u", val);
        return s;
    }

    output_stream &operator<<(output_stream &s, signed long val)
    {
        dprintf(s.fd, "%ld", val);
        return s;
    }

    output_stream &operator<<(output_stream &s, unsigned long val)
    {
        dprintf(s.fd, "%lu", val);
        return s;
    }

    output_stream &operator<<(output_stream &s, signed long long val)
    {
        dprintf(s.fd, "%lld", val);
        return s;
    }

    output_stream &operator<<(output_stream &s, unsigned long long val)
    {
        dprintf(s.fd, "%llu", val);
        return s;
    }

} // namespace mos
