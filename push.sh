#!/bin/bash

./clean.sh

rsync -r --links . root@192.168.33.20:/root/sync/
