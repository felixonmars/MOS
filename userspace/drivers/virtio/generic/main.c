// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>

int main(int argc, char **argv)
{
    puts("VirtIO Generic Device Driver for MOS");

    for (int i = 0; i < argc; i++)
        printf("argv[%d] = %s\n", i, argv[i]);

    puts("This generic driver is a placeholder for all virtio devices that are not yet implemented.");
    return 0;
}
