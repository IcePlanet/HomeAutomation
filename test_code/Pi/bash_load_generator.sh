#!/bin/bash

queue_10_commands() {
        echo "1 2 3" >> "/tmp/queue.txt"
        sleep 1
        echo "2 2 3" >> "/tmp/queue.txt"
        sleep 1
        echo "3 2 3" >> "/tmp/queue.txt"
        sleep 1
        echo "4 2 3" >> "/tmp/queue.txt"
        sleep 1
        echo "5 2 3" >> "/tmp/queue.txt"
        sleep 1
        echo "6 2 3" >> "/tmp/queue.txt"
        sleep 1
        echo "7 2 3" >> "/tmp/queue.txt"
        sleep 1
        echo "8 2 3" >> "/tmp/queue.txt"
        sleep 1
        echo "9 2 3" >> "/tmp/queue.txt"
        sleep 1
        echo "10 2 3" >> "/tmp/queue.txt"
        sleep 1
}

for i in {1..1000}
do
   queue_10_commands &
done

echo "All commands started waiting for finish"
wait
echo "DONE"
