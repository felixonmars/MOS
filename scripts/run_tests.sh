#!/bin/bash

set -e # exit on error

# get the current directory
DIR=$(pwd)

if [ -z "$MOS_TEST_SMP_COUNT" ]; then
    SMP_COUNT=1
else
    SMP_COUNT=$MOS_TEST_SMP_COUNT
    echo "Using SMP_COUNT=$SMP_COUNT"
fi

QEMU_ARGS="-m 4G -smp $SMP_COUNT"

# test if MOS_TEST_SHOW_UI is set
if [ -z "$MOS_TEST_SHOW_UI" ]; then
    QEMU_ARGS="$QEMU_ARGS -nographic"
fi

qemu-system-i386 \
    -kernel $DIR/mos_multiboot.bin \
    -initrd $DIR/initrd.cpio \
    -monitor "unix:/tmp/monitor.sock,server,nowait" \
    -chardev stdio,id=char0,logfile=test-failure.log \
    $QEMU_ARGS \
    -append "poweroff_on_panic=true $*" \
    -serial chardev:char0

PANIC_MARKER="!!!!! KERNEL PANIC !!!!!"

if grep -q "$PANIC_MARKER" test-failure.log; then
    echo "Kernel panic detected."
    echo "Test failed."
    echo "See:"
    echo " - test-failure.bin   the kernel binary"
    echo " - test-failure.log   the serial log"
    exit 1
fi

echo "Test passed."
