#!/bin/bash
depmod -a
if [ ! -d "/debug" ]; then
	mkdir /debug
fi
mount -t debugfs none /debug
echo 8 > /proc/sys/kernel/printk
modprobe remoteproc
modprobe davinci_remoteproc
modprobe virtio_rpmsg_bus
modprobe rpmsg_proto
echo 0 > /proc/sys/kernel/printk
