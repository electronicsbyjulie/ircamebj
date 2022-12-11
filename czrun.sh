#!/bin/bash

echo $$ > /tmp/czpid

while [ 1 ]
do
    isrun=$(ps -ef | grep ctrlr | grep -v grep)
    
    if [ "$isrun" = "" ]
    then
        args=$(cat /tmp/ctrlargs)
        /home/pi/ctrlr $args &
    fi
    
    ./thaw &
    
    sleep 0.75
done
