#!/usr/bin/env bash

mount -t resctrl resctrl /sys/fs/resctrl
mkdir -p /sys/fs/resctrl/group0
chmod 777 -R /sys/fs/resctrl/group0
