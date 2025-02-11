// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/io/io.h>
#include <mos/io/io_types.h>
#include <mos/mos_global.h>
#include <mos/printk.h>

static size_t _null_read(io_t *io, void *buffer, size_t size)
{
    (void) io;
    (void) buffer;
    (void) size;
    return 0;
}

static size_t _null_write(io_t *io, const void *buffer, size_t size)
{
    (void) io;
    (void) buffer;
    (void) size;
    return 0;
}

static io_t io_null_impl = {
    .refcount = 1, // never gets closed
    .type = IO_NULL,
    .flags = IO_READABLE | IO_WRITABLE,
    .ops =
        &(io_op_t){
            .read = _null_read,
            .write = _null_write,
            .seek = NULL,
            .close = NULL,
        },
};

io_t *const io_null = &io_null_impl;

void io_init(io_t *io, io_type_t type, io_flags_t flags, const io_op_t *ops)
{
    mos_debug(io, "io_init(%p, %d, %d, %p)", (void *) io, type, flags, (void *) ops);

    if (unlikely(!io))
        mos_warn("io is NULL");

    if (unlikely(!ops))
        mos_warn("io->ops is NULL");

    if (unlikely(!ops->close))
        mos_warn("io->ops->close is NULL");

    if (flags & IO_READABLE)
        if (unlikely(!ops->read))
            mos_warn("ops->read is NULL for readable io");

    if (flags & IO_WRITABLE)
        if (unlikely(!ops->write))
            mos_warn("ops->write is NULL for writable io");

    if (flags & IO_SEEKABLE)
        if (unlikely(!ops->seek))
            mos_warn("io->ops->seek is NULL for seekable io");

    io->flags = flags;
    io->type = type;
    io->ops = ops;
    io->closed = false;
    io->refcount = 0;
}

io_t *io_ref(io_t *io)
{
    mos_debug(io, "io_ref(%p)", (void *) io);
    if (unlikely(!io))
    {
        mos_warn("io is NULL");
        return NULL;
    }

    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return 0;
    }

    io->refcount++;
    return io;
}

io_t *io_unref(io_t *io)
{
    mos_debug(io, "io_unref(%p)", (void *) io);
    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return NULL;
    }

    if (unlikely(io->refcount == 0))
    {
        mos_warn("%p has refcount 0", (void *) io);
        return NULL;
    }

    io->refcount--;

    if (io->refcount == 0)
    {
        mos_debug(io, "closing %p", (void *) io);
        io->closed = true;
        io->ops->close(io);
        return NULL;
    }

    return io;
}

bool io_valid(io_t *io)
{
    return io && !io->closed && io->refcount > 0 && io->ops && io->ops->close;
}

size_t io_read(io_t *io, void *buf, size_t count)
{
    mos_debug(io, "io_read(%p, %p, %zu)", (void *) io, buf, count);

    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return 0;
    }

    if (!(io->flags & IO_READABLE))
    {
        pr_info2("%p is not readable\n", (void *) io);
        return 0;
    }

    return io->ops->read(io, buf, count);
}

size_t io_write(io_t *io, const void *buf, size_t count)
{
    mos_debug(io, "io_write(%p, %p, %zu)", (void *) io, buf, count);

    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return 0;
    }

    if (!(io->flags & IO_WRITABLE))
    {
        pr_info2("%p is not writable\n", (void *) io);
        return 0;
    }

    return io->ops->write(io, buf, count);
}

off_t io_seek(io_t *io, off_t offset, io_seek_whence_t whence)
{
    mos_debug(io, "io_seek(%p, %lu, %d)", (void *) io, offset, whence);

    if (unlikely(io->closed))
    {
        mos_warn("%p is already closed", (void *) io);
        return 0;
    }

    if (!(io->flags & IO_SEEKABLE))
    {
        pr_info2("%p is not seekable\n", (void *) io);
        return 0;
    }

    return io->ops->seek(io, offset, whence);
}

off_t io_tell(io_t *io)
{
    mos_debug(io, "io_tell(%p)", (void *) io);
    return io_seek(io, 0, IO_SEEK_CURRENT);
}
