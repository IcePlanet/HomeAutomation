#!/bin/bash
# This file is to be located in /usr/local/bin/
# Change owner: sudo chown openhab:openhab enqueue.sh
# Change rights: sudo chmod 754 enqueue.sh

echo "$1 $2 $3" >> /tmp/RF24_queue.txt
