// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int isspace(int _c)
{
    return ((_c > 8 && _c < 14) || (_c == 32));
}

s32 abs(s32 x)
{
    return (x < 0) ? -x : x;
}

long labs(long x)
{
    return (x < 0) ? -x : x;
}

s64 llabs(s64 x)
{
    return (x < 0) ? -x : x;
}

s32 atoi(const char *nptr)
{
    s32 c;
    s32 neg = 0;
    s32 val = 0;

    while (isspace(*nptr))
        nptr++;

    if (*nptr == '-')
    {
        neg = 1;
        nptr++;
    }
    else if (*nptr == '+')
    {
        nptr++;
    }

    while ((c = *nptr++) >= '0' && c <= '9')
    {
        val = val * 10 + (c - '0');
    }

    return neg ? -val : val;
}

void format_size(char *buf, size_t buf_size, u64 size)
{
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
    static const char *const units[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB" };
    size_t i = 0;
    size_t diff = 0;
    while (size >= 1024 && i < ARRAY_SIZE(units) - 1)
    {
        diff = size % 1024;
        size /= 1024;
        i++;
    }
    if (unlikely(diff == 0 || i == 0))
    {
        snprintf(buf, buf_size, "%llu %s", size, units[i]);
    }
    else
    {
        snprintf(buf, buf_size, "%llu %s + %zu %s", size, units[i], diff, units[i - 1]);
    }
}

char *string_trim(char *in)
{
    if (in == NULL)
        return NULL;

    char *end;

    // Trim leading space
    while (*in == ' ')
        in++;

    if (*in == 0) // All spaces?
        return in;

    // Trim trailing space
    end = in + strlen(in) - 1;
    while (end > in && *end == ' ')
        end--;

    // Write new null terminator
    *(end + 1) = '\0';
    return in;
}
