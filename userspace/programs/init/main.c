// SPDX-License-Identifier: GPL-3.0-or-later

#include <argparse/libargparse.h>
#include <libconfig/libconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const config_t *config;

static pid_t start_device_manager(void)
{
    const char *dm_path = config_get(config, "device_manager.path");
    if (!dm_path)
        dm_path = "/initrd/programs/device_manager";

    size_t dm_args_count;
    const char **dm_args = config_get_all(config, "device_manager.args", &dm_args_count);

    // start the device manager
    return syscall_spawn(dm_path, dm_args_count, dm_args); // TODO: check if the dm_args are valid
}

static bool create_directories(void)
{
    size_t num_dirs;
    const char **dirs = config_get_all(config, "mkdir", &num_dirs);
    if (!dirs)
        return false;

    for (size_t i = 0; i < num_dirs; i++)
    {
        const char *dir = dirs[i];
        if (!syscall_vfs_mkdir(dir))
            return false;
    }

    return true;
}

static bool mount_filesystems(void)
{
    size_t num_mounts;
    const char **mounts = config_get_all(config, "mount", &num_mounts);
    if (!mounts)
        return false;

    for (size_t i = 0; i < num_mounts; i++)
    {
        // format: <Device> <MountPoint> <Filesystem> <Options>
        const char *mount = mounts[i];

        char *dup = strdup(mount);
        char *device = strtok(dup, " ");
        char *mount_point = strtok(NULL, " ");
        char *filesystem = strtok(NULL, " ");
        char *options = strtok(NULL, " ");

        if (!device || !mount_point || !filesystem || !options)
            return false; // invalid options

        device = string_trim(device);
        mount_point = string_trim(mount_point);
        filesystem = string_trim(filesystem);
        options = string_trim(options);

        if (!syscall_vfs_mount(device, mount_point, filesystem, options))
            return false;
    }

    return true;
}

#define DYN_ERROR_CODE (__COUNTER__ + 1)

static const argparse_arg_t longopts[] = {
    { "help", 'h', ARGPARSE_NONE, "show this help" },
    { "config", 'C', ARGPARSE_REQUIRED, "configuration file, default: /initrd/config/init.conf" },
    { "shell", 'S', ARGPARSE_REQUIRED, "shell to start, default: /initrd/programs/mossh" },
    { 0 },
};

int main(int argc, const char *argv[])
{
    if (syscall_get_pid() != 1)
    {
        printf("init: not running as PID 1\n");

        for (int i = 0; i < argc; i++)
            printf("argv[%d] = %s\n", i, argv[i]);

        return DYN_ERROR_CODE;
    }

    const char *config_file = "/initrd/config/init.conf";
    const char *shell = "/initrd/programs/mossh";
    argparse_state_t state;
    argparse_init(&state, argv);
    while (true)
    {
        const int option = argparse_long(&state, longopts, NULL);
        if (option == -1)
            break;

        switch (option)
        {
            case 'C': config_file = state.optarg; break;
            case 'S': shell = state.optarg; break;
            case 'h': argparse_usage(&state, longopts, "the init program"); return 0;
            default: break;
        }
    }

    config = config_parse_file(config_file);
    if (!config)
        return DYN_ERROR_CODE;

    if (!create_directories())
        return DYN_ERROR_CODE;

    if (!mount_filesystems())
        return DYN_ERROR_CODE;

    pid_t dm_pid = start_device_manager();
    if (dm_pid <= 0)
        return DYN_ERROR_CODE;

    // start the shell
    const char **shell_argv = NULL;
    int shell_argc = 0;

    const char *arg;
    argparse_init(&state, argv); // reset the options
    while ((arg = argparse_arg(&state)))
    {
        shell_argc++;
        shell_argv = realloc(shell_argv, shell_argc * sizeof(char *));
        shell_argv[shell_argc - 1] = arg;
    }

    // TODO: use exec() ?
    pid_t shell_pid = syscall_spawn(shell, shell_argc, shell_argv);
    if (shell_pid <= 0)
        return DYN_ERROR_CODE;

    syscall_wait_for_process(shell_pid);
    return 0;
}
