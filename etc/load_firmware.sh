#!/bin/bash
depmod -a
mount -t debugfs none /debug
modprobe remoteproc
modprobe omap_remoteproc
modprobe virtio_rpmsg_bus
modprobe rpmsg_proto
