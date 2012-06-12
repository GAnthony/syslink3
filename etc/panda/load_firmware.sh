#!/bin/bash
depmod -a
if [ ! -d "/debug" ]; then
	mkdir /debug
fi
mount -t debugfs none /debug
modprobe remoteproc
modprobe omap_remoteproc
modprobe virtio_rpmsg_bus
modprobe rpmsg_proto
