// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/file.h"

#include "lib/string.h"
#include "lib/structures/tree.h"
#include "mos/filesystem/fs_fwd.h"
#include "mos/filesystem/mount.h"
#include "mos/filesystem/path.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

void path_increment_refcount(const tree_node_t *node)
{
    path_t *path = tree_entry(node, path_t);
    mos_debug("Incrementing refcount of path '%s'", path->name);
    path->refcount.atomic++;
}

void file_format_perm(file_permissions_t perms, char buf[10])
{
    buf[0] = perms.owner & FILE_PERM_READ ? 'r' : '-';
    buf[1] = perms.owner & FILE_PERM_WRITE ? 'w' : '-';
    buf[2] = perms.owner & FILE_PERM_EXEC ? 'x' : '-';
    buf[3] = perms.group & FILE_PERM_READ ? 'r' : '-';
    buf[4] = perms.group & FILE_PERM_WRITE ? 'w' : '-';
    buf[5] = perms.group & FILE_PERM_EXEC ? 'x' : '-';
    buf[6] = perms.other & FILE_PERM_READ ? 'r' : '-';
    buf[7] = perms.other & FILE_PERM_WRITE ? 'w' : '-';
    buf[8] = perms.other & FILE_PERM_EXEC ? 'x' : '-';
    buf[9] = '\0';
}

file_t *file_open(const char *path, file_open_flags mode)
{
    path_t *p = construct_path(path);
    MOS_ASSERT(p != NULL);
    mountpoint_t *mp = kmount_find(p);
    if (mp == NULL)
    {
        mos_warn("no filesystem mounted at %s", path);
        return NULL;
    }

    // TODO symlink?
    file_t *file = kcalloc(1, sizeof(file_t));
    const char *ppath = path_get_full_path_string(mp->path, p);

    mos_debug("opening file %s on fs: %s, blockdev: %s", ppath, mp->fs->name, mp->dev->name);
    bool opened = mp->fs->op_open(mp, p, ppath, mode, file);

    kfree(ppath);
    if (!opened)
    {
        mos_warn("failed to open file %s", path);
        kfree(file);
        return NULL;
    }
    tree_trace_to_root(tree_node(p), path_increment_refcount);
    return file;
}

bool file_stat(const char *path, file_stat_t *restrict stat)
{
    path_t *p = construct_path(path);
    mountpoint_t *mp = kmount_find(p);
    if (mp == NULL)
    {
        mos_warn("no filesystem mounted at %s", path);
        return -1;
    }
    const char *ppath = path_get_full_path_string(mp->path, p);
    bool result = mp->fs->op_stat(mp, p, ppath, stat);
    kfree(ppath);
    return result;
}
