# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(virtio_blockdev
    main.c)
setup_userspace_program(virtio_blockdev /drivers "Virtio Block Device Driver")
