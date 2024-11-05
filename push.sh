#!/bin/bash

make clean
ssh root@sevilla "rm -r /root/sync/*"
scp Makefile root@sevilla:/root/sync/Makefile
scp -r src root@sevilla:/root/sync/src
ssh root@sevilla "cd /root/sync; tar czf \"/mount/host/28 Machines/28.04 Sevilla/nixos/sync/release.tar.gz\" --transform 's,^,sync/,' ."
ssh root@sevilla "ls -lh \"/mount/host/28 Machines/28.04 Sevilla/nixos/sync/\""
