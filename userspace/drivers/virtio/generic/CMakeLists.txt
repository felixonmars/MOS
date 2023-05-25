# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(virtio_generic
    main.c)
setup_userspace_program(virtio_generic /drivers "Virtio Generic Device Placeholder")
