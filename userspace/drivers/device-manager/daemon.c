// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm.h"

#include <libconfig/libconfig.h>
#include <mos/lib/cmdline.h>
#include <mos/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool start_load_drivers(void)
{
    size_t num_drivers;
    const char **drivers = config_get_all(dm_config, "load", &num_drivers);
    if (!drivers)
        return true; // no drivers to start

    for (size_t i = 0; i < num_drivers; i++)
    {
        const char *driver = drivers[i];

        char *dup = strdup(driver);
        char *saveptr;
        char *driver_path = strtok_r(dup, " ", &saveptr);
        char *driver_args = strtok_r(NULL, " ", &saveptr);

        driver_path = string_trim(driver_path);
        driver_args = string_trim(driver_args);

        if (!driver_path)
            return false; // invalid options

        size_t argc = 0;
        const char **argv = NULL;

        if (driver_args)
            argv = cmdline_parse(NULL, driver_args, strlen(driver_args), &argc);

        const pid_t driver_pid = syscall_spawn(driver_path, argc, argv);

        for (size_t i = 0; i < argc; i++)
            free((void *) argv[i]);
        free(dup);

        if (driver_pid <= 0)
            return false;
    }

    return true;
}

bool try_start_driver(u16 vendor, u16 device, u32 location, const char *pcidev)
{

    char vendor_str[5];
    char vendor_device_str[10];
    snprintf(vendor_str, sizeof(vendor_str), "%04x", vendor);
    snprintf(vendor_device_str, sizeof(vendor_device_str), "%04x:%04x", vendor, device);

    // first try to find a driver for the specific device
    const char *driver = config_get(dm_config, vendor_device_str);

    // if that fails, try to find a driver for the vendor
    if (!driver)
        driver = config_get(dm_config, vendor_str);

    // if the driver is still not found, leave
    if (!driver)
        return false;

    char *dup = strdup(driver);
    char *saveptr;

    char *driver_path = strtok_r(dup, " ", &saveptr);
    char *driver_args = strtok_r(NULL, " ", &saveptr);

    if (!driver_path)
        return false; // invalid options

    driver_path = string_trim(driver_path);

    if (unlikely(driver_args))
    {
        puts("argv for driver is not supported yet");
        free(dup);
        return false;
    }

    char device_str[5];
    char location_str[10];
    snprintf(device_str, sizeof(device_str), "%04x", device);
    snprintf(location_str, sizeof(location_str), "%04x", location);

    const char *argv[4] = { vendor_str, device_str, location_str, pcidev };
    const pid_t driver_pid = syscall_spawn(driver_path, 4, argv);
    free(dup);

    return driver_pid > 0;
}
